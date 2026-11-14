#!/usr/bin/env python3
"""Make the committed test photos from originals, without re-encoding them.

    python3 tools/jpeg_host/make_test_photos.py [SOURCE_DIR]   # default ~/_eink_ci; needs jpegtran

Each photo is cropped losslessly (jpegtran keeps the coefficients, tables, sampling and EXIF and
re-encodes only the entropy coding, with the source's restart interval), so the set keeps every
encoder's quirks while staying small; crop sizes are chosen so the frame decodes at every scale. GPS, MakerNote and serial numbers are zeroed in place, in every EXIF
block of the file (Apple appends a second image with its own). Upright camera photos are made by
setting the orientation tag of a landscape original, which is all a camera does when held upright.
Then each output is checked: no GPS left, and the frame's decoder gives the expected result.
"""
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'tools', 'jpeg_host', 'photos')
HOST = os.path.join(ROOT, 'build', 'jpeg_host')

# output name, source, lossless crop (WxH+X+Y) or None, orientation to set or None, expected decode
PHOTOS = [
    ('dslr_nikon_d70s.jpg', 'dslr_landscape.jpg', '1200x800+700+500', None, 'ok'),
    ('dslr_nikon_d70s_o6.jpg', 'dslr_landscape.jpg', '1200x800+700+500', 6, 'ok'),
    ('compact_canon_a540.jpg', 'compact_landscape.jpg', '1024x768+900+600', None, 'ok'),
    ('compact_canon_a540_o8.jpg', 'compact_landscape.jpg', '1024x768+900+600', 8, 'ok'),
    ('oldest_olympus_c990z.jpg', 'oldest.jpg', '1280x960+160+120', None, 'ok'),
    ('iphone7.jpg', 'iphone.jpg', '1600x1200+1200+900', None, 'ok'),
    ('iphone16pro_heic_export.jpg', 'heic_to_jpeg.jpeg', None, None, 'ok'),
    ('iphone3gs_export.jpg', 'exported.jpeg', None, None, 'ok'),
    ('no_exif.jpg', '03.jpg', '800x600+240+176', None, 'ok'),
    # Rotated losslessly by a photo app: 4:2:2 turned into 4:4:0, which TJpgDec cannot decode. Rejected
    # at the frame header, so their Huffman tables (replaced by the crop) don't matter
    ('rotated_440_nikon.jpg', 'dslr_portrait.jpg', '256x256+800+1200', None, 'unsupported'),
    ('rotated_440_canon.jpg', 'compact_portrait.jpg', '256x256+900+1300', None, 'unsupported'),
]

TYPE_SIZES = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 7: 1, 9: 4, 10: 8}
GPS_IFD, EXIF_IFD, ORIENTATION = 0x8825, 0x8769, 0x0112
ZEROED_EXIF_TAGS = {0x927C, 0xA420, 0xA430, 0xA431, 0xA435}  # MakerNote, unique ID, owner, serials


class Tiff:
    """The TIFF structure of one EXIF block inside the file's bytes, edited in place"""

    def __init__(self, data, start):
        self.data, self.start = data, start
        self.e = '>' if data[start:start + 2] == b'MM' else '<'

    def u16(self, at):
        return struct.unpack_from(self.e + 'H', self.data, self.start + at)[0]

    def u32(self, at):
        return struct.unpack_from(self.e + 'I', self.data, self.start + at)[0]

    def entries(self, ifd):
        """(tag, type, count, entry offset, value offset) of each entry in the IFD at ifd"""
        for i in range(self.u16(ifd)):
            at = ifd + 2 + 12 * i
            tag, typ, count = self.u16(at), self.u16(at + 2), self.u32(at + 4)
            size = TYPE_SIZES.get(typ, 1) * count
            yield tag, typ, count, at, at + 8 if size <= 4 else self.u32(at + 8), size

    def find(self, ifd, tag):
        return next((e for e in self.entries(ifd) if e[0] == tag), None)

    def zero(self, value_at, size):
        """A value of up to 4 bytes sits in the entry's 4-byte field: clear the whole field"""
        size = max(size, 4) if size <= 4 else size
        a = self.start + value_at
        self.data[a:a + size] = bytes(size)

    def values(self, value_at, size):
        a = self.start + value_at
        return bytes(self.data[a:a + size])


def exif_blocks(data):
    """Every EXIF block in the file: the photo's and those of appended images"""
    at = 0
    while True:
        at = data.find(b'Exif\0\0', at)
        if at < 0:
            return
        start = at + 6
        if data[start:start + 4] in (b'MM\0*', b'II*\0'):
            yield Tiff(data, start)
        at = start


def scrub(data):
    """Zero GPS, MakerNote and serial numbers in place; returns how many EXIF blocks were found"""
    blocks = 0
    for t in exif_blocks(data):
        blocks += 1
        ifd0 = t.u32(4)
        gps = t.find(ifd0, GPS_IFD)
        if gps:
            for entry in t.entries(t.u32(gps[3] + 8)):
                t.zero(entry[4], entry[5])
        exif = t.find(ifd0, EXIF_IFD)
        if exif:
            for entry in t.entries(t.u32(exif[3] + 8)):
                if entry[0] in ZEROED_EXIF_TAGS:
                    t.zero(entry[4], entry[5])
    return blocks


def leftover_gps(data):
    """Non-zero GPS values in any EXIF block, or location text in XMP"""
    found = []
    for t in exif_blocks(data):
        gps = t.find(t.u32(4), GPS_IFD)
        if gps:
            for tag, _, _, _, value_at, size in t.entries(t.u32(gps[3] + 8)):
                if any(t.values(value_at, size)):
                    found.append('GPS tag 0x%04x' % tag)
    for word in (b'GPSLatitude', b'GPSLongitude', b'exif:GPS'):
        if word in data:
            found.append(word.decode())
    return found


def set_orientation(data, value):
    t = next(exif_blocks(data))
    entry = t.find(t.u32(4), ORIENTATION)
    if not entry:
        sys.exit('no orientation tag to set')
    struct.pack_into(t.e + 'H', data, t.start + entry[4], value)


def structure(data):
    """Quantization and Huffman tables by content, sampling and restart interval, before the scan"""
    i, q, h, sof, dri = 2, {}, {}, None, None
    while i < len(data) - 4 and data[i] == 0xFF:
        marker, length = data[i + 1], data[i + 2] << 8 | data[i + 3]
        p = bytes(data[i + 4:i + 2 + length])
        if marker == 0xDA:
            break
        if marker == 0xDB:
            j = 0
            while j < len(p):
                n = 128 if p[j] >> 4 else 64
                q[p[j] & 15] = p[j + 1:j + 1 + n]
                j += 1 + n
        elif marker == 0xC4:
            j = 0
            while j < len(p):
                n = sum(p[j + 1:j + 17])
                h[p[j]] = p[j + 1:j + 17 + n]
                j += 17 + n
        elif marker in (0xC0, 0xC1, 0xC2):
            sof = p[5:]  # components with their sampling factors and tables
        elif marker == 0xDD:
            dri = p[0] << 8 | p[1]
        i += 2 + length
    return q, h, sof, dri


def decode(path):
    lines = subprocess.run([HOST, path], capture_output=True, text=True).stdout.splitlines()
    return lines[0] if lines else '', lines[1] if len(lines) > 1 else ''


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/_eink_ci')
    jpegtran = shutil.which('jpegtran')
    if not jpegtran or not os.path.exists(HOST):
        sys.exit('needs jpegtran (brew install jpeg-turbo) and build/jpeg_host (sh tools/jpeg_host/build.sh)')
    os.makedirs(OUT, exist_ok=True)
    failed = False
    for name, src, crop, orientation, expect in PHOTOS:
        src_path, out_path = os.path.join(source, src), os.path.join(OUT, name)
        original = open(src_path, 'rb').read()
        if crop:
            restart = structure(original)[3]
            data = bytearray(subprocess.run([jpegtran, '-crop', crop, '-copy', 'all']
                                            + (['-restart', '%dB' % restart] if restart else []) + [src_path],
                                            capture_output=True, check=True).stdout)
        else:
            data = bytearray(original)
        blocks = scrub(data)
        if orientation:
            set_orientation(data, orientation)
        with open(out_path, 'wb') as f:
            f.write(data)

        probe, convert = decode(out_path)
        src_probe, _ = decode(src_path)
        problems = leftover_gps(data)
        before, after = structure(original), structure(data)
        # jpegtran drops quantization tables no component uses; the decoder never reads those
        used = {before[2][3 + 3 * i] for i in range(before[2][0])} if before[2] else set()
        dropped = sorted(set(before[0]) - set(after[0]) - used)
        before = ({t: q for t, q in before[0].items() if t in used or t in after[0]},) + before[1:]
        for what, a, b in zip(('quantization', 'huffman', 'sampling', 'restart interval'), before, after):
            if a != b and not (what == 'huffman' and expect == 'unsupported'):
                problems.append(what + ' changed')
        notes = ['unused quantization table %s dropped' % ', '.join(map(str, dropped))] if dropped else []
        if not convert.startswith('convert: ' + ('ok' if expect == 'ok' else 'unsupported')):
            problems.append('decodes as "%s"' % convert[:40])
        want_orientation = 'orientation %d' % (orientation or 1)
        if want_orientation not in probe:
            problems.append('probe "%s"' % probe[:60])
        if src_probe.split('date ')[-1] != probe.split('date ')[-1]:
            problems.append('date changed')
        failed |= bool(problems)
        print('%-30s %5d KB  exif blocks %d  %s' % (name, len(data) // 1024, blocks,
                                                    '; '.join(problems) if problems else '; '.join(['ok'] + notes)))
    total = sum(os.path.getsize(os.path.join(OUT, p[0])) for p in PHOTOS)
    print('total %d KB' % (total // 1024))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
