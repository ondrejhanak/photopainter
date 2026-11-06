"""Checks the host JPEG harness (build.sh first) on generated edge cases and on a folder of photos.

usage: python3 tools/jpeg_host/check.py PHOTO_DIR SCRATCH_DIR
Needs Pillow. Writes test JPEGs and output BMPs to SCRATCH_DIR; exits 1 if any check fails.
"""
import os
import random
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HARNESS = os.path.join(ROOT, 'build', 'jpeg_host')
LOOK_PHOTOS = ['IMG_7365.jpeg', 'olomouc.jpeg', 'IMG_9077.jpeg'] # landscape photos compared with convert_reference.py
LOOK_MAX_DIFF = 6
BARS = 7 # palette.c: placeholder for the bars (EPD_7IN3F_CLEAN)
STREAM_MAX_DIFF = 1.5 # decoders differ by IDCT rounding only
BOX_MAX_DIFF = 4
# convert_reference.py's exact output colours -> the soft colours our cache BMP shows
SOFT = {(0, 0, 0): (5, 5, 5), (255, 255, 255): (245, 245, 245), (0, 255, 0): (65, 140, 30),
        (0, 0, 255): (30, 65, 240), (255, 0, 0): (190, 0, 0), (255, 255, 0): (255, 245, 55),
        (255, 128, 0): (230, 125, 0)}
failures = []


def run(path, out=None, portrait=False, work=None, grey=False):
    args = [HARNESS] + (['-p'] if portrait else []) + (['-g'] if grey else []) + \
           (['-w', str(work)] if work else []) + [path]
    if out:
        args.append(out)
    p = subprocess.run(args, capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def check(name, ok, detail=''):
    print(('ok   ' if ok else 'FAIL ') + name + ('' if ok else ': ' + detail.strip()))
    if not ok:
        failures.append(name)


def save(img, path, **kw):
    img.save(path, 'JPEG', quality=90, **kw)
    return path


def with_orientation(img, value):
    exif = Image.Exif()
    exif[0x0112] = value
    return exif.tobytes()


def contrast(v):
    return max(0, min(255, int(128 + (v - 128) * 1.1)))


def scaled_reference(img, out_w=800, out_h=480):
    """jpeg_convert's scaling of a greyscale JPEG (8x8 MCUs, size a multiple of 8) done on the whole
    image: 2^k box average as TJpgDec, then cells of integer spans; fill or fit, centred."""
    w, h = img.size
    fill = (w > h) == (out_w > out_h) and w != h
    if fill != (out_w * h <= out_h * w):
        pw, ph = out_w, (h * out_w + w // 2) // w
    else:
        pw, ph = (w * out_h + h // 2) // h, out_h
    k = 3
    while k > 0 and ((w >> k) < pw or (h >> k) < ph):
        k -= 1
    src = img.load()
    dw, dh, b = w >> k, h >> k, 1 << k
    dec = [[sum(src[x * b + i, y * b + j] for i in range(b) for j in range(b)) >> (2 * k) for x in range(dw)]
           for y in range(dh)]
    nx, ny = min(dw, pw), min(dh, ph)
    xs = [c * dw // nx for c in range(nx + 1)]
    ys = [c * dh // ny for c in range(ny + 1)]
    cells = [[0] * nx for _ in range(ny)]
    for cy in range(ny):
        for cx in range(nx):
            vals = [dec[y][x] for y in range(ys[cy], ys[cy + 1]) for x in range(xs[cx], xs[cx + 1])]
            cells[cy][cx] = (sum(vals) + len(vals) // 2) // len(vals)
    ox, oy = int((out_w - pw) / 2), int((out_h - ph) / 2) # C division: towards zero
    ref = Image.new('L', (out_w, out_h), 255)
    px = ref.load()
    for y in range(max(0, oy), min(out_h, oy + ph)):
        cy = ((y - oy + 1) * ny - 1) // ph
        for x in range(max(0, ox), min(out_w, ox + pw)):
            px[x, y] = contrast(cells[cy][((x - ox + 1) * nx - 1) // pw])
    return ref, k


def blurred(path):
    """Output BMP as RGB, blurred so dither patterns from different scan orders compare equal."""
    return Image.open(path).convert('RGB').filter(ImageFilter.BoxBlur(6))


def exif_date(path):
    """DateTimeOriginal, else DateTime, as the frame reads it; 0000-00-00 if neither is a real date."""
    exif = Image.open(path).getexif()
    for value in (exif.get_ifd(0x8769).get(0x9003), exif.get(0x0132)):
        m = re.match(r'(\d{4}):(\d\d):(\d\d)', value or '')
        if m and int(m[1]) >= 1800 and 1 <= int(m[2]) <= 12 and 1 <= int(m[3]) <= 31:
            return f'{m[1]}-{m[2]}-{m[3]}'
    return '0000-00-00'


def mean_diff(a, b):
    pa, pb = a.tobytes(), b.tobytes()
    return sum(abs(x - y) for x, y in zip(pa, pb)) / len(pa)


def main():
    photos, scratch = sys.argv[1], sys.argv[2]
    os.makedirs(scratch, exist_ok=True)
    s = lambda name: os.path.join(scratch, name)

    # A landscape and a portrait source cut from a photo (asymmetric, so a wrong turn shows)
    base = Image.open(os.path.join(photos, sorted(f for f in os.listdir(photos) if f.lower().endswith(('.jpg', '.jpeg')))[0]))
    land = base.convert('RGB').resize((1600, 1200))
    port = land.transpose(Image.Transpose.ROTATE_90).crop((0, 0, 1200, 1600))

    # EXIF orientation: pixels stored turned, tag says how to turn them back
    turn_for = {3: Image.Transpose.ROTATE_180, 6: Image.Transpose.ROTATE_90, 8: Image.Transpose.ROTATE_270}
    for label, src in (('landscape', land), ('portrait', port)):
        ref = save(src, s(f'{label}_o1.jpg'))
        code, log = run(ref, s(f'{label}_o1.bmp'))
        check(f'{label} orientation 1 converts', code == 0, log)
        for value, turn in turn_for.items():
            # stored = view turned the opposite way; Pillow's ROTATE_90 is counter-clockwise
            stored = src.transpose(turn)
            tagged = save(stored, s(f'{label}_o{value}.jpg'), exif=with_orientation(stored, value))
            code, log = run(tagged, s(f'{label}_o{value}.bmp'))
            check(f'{label} orientation {value} converts', code == 0 and f'orientation {value}' in log, log)
            if code:
                continue
            untagged = save(stored, s(f'{label}_o{value}_untagged.jpg'))
            run(untagged, s(f'{label}_o{value}_untagged.bmp'))
            good = mean_diff(blurred(s(f'{label}_o1.bmp')), blurred(s(f'{label}_o{value}.bmp')))
            bad = mean_diff(blurred(s(f'{label}_o1.bmp')), blurred(s(f'{label}_o{value}_untagged.bmp')))
            check(f'{label} orientation {value} matches upright (diff {good:.1f}, ignored tag {bad:.1f})',
                  good < 4 and bad > 3 * good, f'{good:.1f} vs {bad:.1f}')

    # Fill vs fit: a 3:4 portrait in the landscape frame is 360 wide, bars of 220 each side. Bars are
    # the placeholder code (painted in the bar colour when shown), which no photo pixel may use
    def placeholders(path):
        img = Image.open(path) # palette index = panel code
        return [x for x in range(800) for y in range(480) if img.getpixel((x, y)) == BARS]
    bars = placeholders(s('portrait_o1.bmp'))
    check(f'portrait fits between two 220 px bars ({len(bars)} placeholder pixels)',
          len(bars) == 2 * 220 * 480 and all(x < 220 or x >= 580 for x in bars), '')
    n = len(placeholders(s('landscape_o1.bmp')))
    check('landscape fills the frame (no placeholder pixels)', n == 0, f'{n}')

    # Portrait frame (turned counter-clockwise): the cache file holds the panel's buffer, so the photo
    # lies on its side and a counter-clockwise turn shows it upright; compared with Pillow's resize
    tall = save(port.resize((960, 1600)), s('tall.jpg')) # the frame's 3:5, so it fills without a crop
    code, log = run(tall, s('tall_portrait.bmp'), portrait=True)
    check('portrait frame converts', code == 0, log)
    if code == 0:
        ref = Image.open(tall).convert('RGB').resize((480, 800)).filter(ImageFilter.BoxBlur(6))
        out = blurred(s('tall_portrait.bmp'))
        good = mean_diff(out.transpose(Image.Transpose.ROTATE_90), ref)
        bad = mean_diff(out.transpose(Image.Transpose.ROTATE_270), ref)
        check(f'portrait frame upright after a counter-clockwise turn (diff {good:.1f}, other way {bad:.1f})',
              bad > 2 * good, f'{good:.1f} vs {bad:.1f}')

    # Formats TJpgDec handles, and some it rejects
    small = land.resize((800, 600))
    cases = {
        'grayscale': (save(small.convert('L'), s('gray.jpg')), 0),
        '4:4:4': (save(small, s('444.jpg'), subsampling=0), 0),
        '4:2:2': (save(small, s('422.jpg'), subsampling=1), 0),
        'restart markers': (save(land, s('rst.jpg'), restart_marker_blocks=7), 0),
        'odd size 1001x667': (save(land.resize((1001, 667)), s('odd.jpg')), 0),
        'tiny 64x48': (save(land.resize((64, 48)), s('tiny.jpg')), 0),
        'panorama 8000x400': (save(land.resize((8000, 400)), s('pano.jpg')), 0),
        'progressive': (save(small, s('prog.jpg'), progressive=True), 'unsupported'),
        'CMYK': (save(small.convert('CMYK'), s('cmyk.jpg')), 'unsupported'),
    }
    data = open(s('444.jpg'), 'rb').read()
    for name, blob in (('truncated', data[:len(data) // 2]), ('empty', b''), ('not a JPEG', b'BM' + data[2:]),
                       ('garbage after header', data[:400] + bytes(range(256)) * 20)):
        open(s(name.replace(' ', '_') + '.jpg'), 'wb').write(blob)
        cases[name] = (s(name.replace(' ', '_') + '.jpg'), 'error')
    # Sampling factors TJpgDec cannot decode, patched into the frame header (Pillow can't write them)
    sof = data.find(b'\xff\xc0') + 4 # frame header payload
    for name, at, value in (('luma sampling 1x2', sof + 7, 0x12), ('chroma sampling 2x2', sof + 10, 0x22)):
        blob = bytearray(data)
        blob[at] = value
        path = s(name.replace(' ', '_') + '.jpg')
        open(path, 'wb').write(blob)
        cases[name] = (path, 'unsupported')
    for name, (path, expect) in cases.items():
        code, log = run(path, s(os.path.basename(path) + '.bmp'))
        if expect == 0:
            check(f'{name} converts', code == 0, log)
        elif expect == 'unsupported':
            check(f'{name} rejected as unsupported (probe and convert)', code == 1 and 'probe: unsupported' in log
                  and 'convert: unsupported' in log, log)
        else:
            check(f'{name} fails cleanly', code == 1 and 'convert: ok' not in log, log)

    code, log = run(s('odd.jpg'), work=2048)
    check('2 KB work memory is refused', code == 1, log)

    # Scaling, with 256 greys so the output is the scaled photo itself (no dither noise): the
    # streamed result equals the same arithmetic done on the whole image, and at full decode size
    # it is close to Pillow's BOX filter
    rng = random.Random(1)
    for size in ((1200, 720), (2400, 1800), (1000, 1504)):
        texture = Image.new('L', (size[0] // 3, size[1] // 3))
        texture.putdata([rng.randrange(256) for _ in range(texture.width * texture.height)])
        path = save(texture.resize(size, Image.Resampling.NEAREST), s(f'texture_{size[0]}.jpg'))
        code, log = run(path, s(f'texture_{size[0]}.pgm'), grey=True)
        ours = Image.open(s(f'texture_{size[0]}.pgm')) if code == 0 else None
        ref, k = scaled_reference(Image.open(path).convert('L'))
        d = mean_diff(ours, ref) if ours else 999
        check(f'{size[0]}x{size[1]} at 1/{1 << k} streams like the whole-image reference (diff {d:.2f})',
              d < STREAM_MAX_DIFF, log)
        if k == 0 and size[0] * 480 == size[1] * 800:
            box = Image.open(path).convert('L').resize((800, 480), Image.Resampling.BOX).point(contrast)
            d = mean_diff(ours, box) if ours else 999
            check(f'{size[0]}x{size[1]} close to a BOX filter (diff {d:.2f})', d < BOX_MAX_DIFF, log)

    # Unit checks: EXIF parsing in both byte orders, colour enhancement against Pillow's formulas
    p = subprocess.run([os.path.join(ROOT, 'build', 'unit_test')], capture_output=True, text=True)
    check('unit tests', p.returncode == 0, p.stdout + p.stderr)

    # Decoding stops once the visible part is done: a 4:3 photo cropped to 5:3 skips its bottom rows
    code, log = run(s('landscape_o1.jpg'))
    read, size, calls = map(int, re.search(r'read (\d+) of (\d+) bytes in (\d+) calls', log).groups())
    check(f'cropped photo stops early (read {read} of {size} bytes)', read < size * 0.95, log)
    # Reads come in big chunks: on the card each read is a command with a fixed delay
    check(f'reads ahead ({calls} reads for {read} bytes)', read / calls > 4096, log) # 512 without

    # Same look as convert_reference.py: blurred, both dithers average to the same colours
    convert = os.path.join(ROOT, 'tools', 'jpeg_host', 'convert_reference.py')
    for f in LOOK_PHOTOS:
        src = os.path.join(scratch, 'look_' + f)
        shutil.copyfile(os.path.join(photos, f), src)
        subprocess.run([sys.executable, convert, '--dir', 'landscape', src], capture_output=True, check=True)
        ref = Image.open(os.path.splitext(src)[0] + '_output.bmp').convert('RGB')
        px = ref.load()
        for y in range(ref.height):
            for x in range(ref.width):
                px[x, y] = SOFT.get(px[x, y], px[x, y])
        run(src, src + '.bmp')
        n = len(placeholders(src + '.bmp'))
        check(f'{f} fills the frame (no placeholder pixels)', n == 0, f'{n}')
        d = mean_diff(blurred(src + '.bmp'), ref.filter(ImageFilter.BoxBlur(6)))
        check(f'{f} looks like convert_reference.py output (diff {d:.1f})', d < LOOK_MAX_DIFF, f'{d:.1f}')

    # An edited photo: DateTimeOriginal differs from DateTime and lies past 512 bytes of APP1, as in
    # phone photos (Make, Model, Software strings first)
    exif = Image.Exif()
    exif[0x010F], exif[0x0110], exif[0x0131] = 'Maker ' * 40, 'Model ' * 40, 'Editor ' * 20
    exif[0x0132] = '2024:05:06 07:08:09'
    exif.get_ifd(0x8769)[0x9003] = '1985:12:24 18:30:00'
    edited = save(small, s('edited.jpg'), exif=exif.tobytes())
    app1 = open(edited, 'rb').read(8192)
    at = app1.find(b'1985:12:24') - app1.find(b'Exif')
    code, log = run(edited)
    check(f'edited photo shows the date taken (at byte {at} of APP1)',
          at > 512 and 'date 1985-12-24' in log, log)

    # Every photo in the folder, landscape and portrait frame; the date found as Pillow reads it
    for f in sorted(os.listdir(photos)):
        if f.lower().endswith(('.jpg', '.jpeg')):
            for portrait in (False, True):
                code, log = run(os.path.join(photos, f), portrait=portrait)
                check(f'{f} ({"portrait" if portrait else "landscape"} frame)', code == 0, log)
            want = exif_date(os.path.join(photos, f))
            got = re.search(r'date (\d{4}-\d\d-\d\d)', log)
            check(f'{f} date {want}', got and got.group(1) == want, log)

    print(f'\n{len(failures)} failed' if failures else '\nall passed')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
