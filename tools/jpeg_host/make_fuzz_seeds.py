#!/usr/bin/env python3
"""Write the fuzzing seeds: small valid JPEGs covering what the decoder tells apart.

    python3 tools/jpeg_host/make_fuzz_seeds.py   # needs Pillow; writes tools/jpeg_host/fuzz_seeds/

Run once; the seeds are committed. Small files keep each fuzzing run fast.
"""
import io
import os
import struct

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fuzz_seeds')


def picture(size):
    img = Image.new('RGB', size, (200, 180, 120))
    d = ImageDraw.Draw(img)
    w, h = size
    d.rectangle((0, 0, w // 2, h // 2), fill=(30, 90, 200))
    d.ellipse((w // 4, h // 4, w * 3 // 4, h * 3 // 4), fill=(220, 40, 40))
    d.line((0, h - 1, w - 1, 0), fill=(10, 10, 10), width=max(1, w // 16))
    return img


def jpeg(img, **options):
    buf = io.BytesIO()
    img.save(buf, 'JPEG', quality=80, **options)
    return buf.getvalue()


def exif_block(big_endian, orientation, date):
    """APP1 Exif payload: IFD0 with Orientation and a pointer to the Exif IFD with DateTimeOriginal"""
    e = '>' if big_endian else '<'
    tiff = (b'MM' if big_endian else b'II') + struct.pack(e + 'HI', 42, 8)
    ifd0_at, exif_at = 8, 8 + 2 + 2 * 12 + 4
    date_at = exif_at + 2 + 12 + 4
    tiff += struct.pack(e + 'H', 2)
    tiff += struct.pack(e + 'HHI', 0x0112, 3, 1) + struct.pack(e + 'HH', orientation, 0)
    tiff += struct.pack(e + 'HHII', 0x8769, 4, 1, exif_at)
    tiff += struct.pack(e + 'I', 0)
    tiff += struct.pack(e + 'H', 1)
    tiff += struct.pack(e + 'HHII', 0x9003, 2, 20, date_at)
    tiff += struct.pack(e + 'I', 0)
    tiff += date.encode() + b'\0'
    assert ifd0_at == 8
    return b'Exif\0\0' + tiff


def main():
    os.makedirs(OUT, exist_ok=True)
    small = picture((64, 48))
    seeds = {
        '444.jpg': jpeg(small, subsampling=0),
        '422.jpg': jpeg(small, subsampling=1),
        '420.jpg': jpeg(small, subsampling=2),
        'grey.jpg': jpeg(small.convert('L')),
        'odd_size.jpg': jpeg(picture((37, 23)), subsampling=2),
        'portrait.jpg': jpeg(picture((24, 40)), subsampling=2),
        'panorama.jpg': jpeg(picture((200, 8)), subsampling=2),
        'tiny.jpg': jpeg(picture((8, 8)), subsampling=0),
        'too_wide.jpg': jpeg(picture((4000, 1)), subsampling=2),  # needs more work memory than the frame has
        'cmyk.jpg': jpeg(small.convert('CMYK')),  # rejected as unsupported
        'restart.jpg': jpeg(small, subsampling=2, restart_marker_blocks=2),
        'exif_le_o6.jpg': jpeg(small, subsampling=2, exif=exif_block(False, 6, '2019:07:14 10:20:30')),
        'exif_be_o8.jpg': jpeg(small, subsampling=2, exif=exif_block(True, 8, '1985:12:24 18:00:00')),
        'exif_le_o3.jpg': jpeg(small, subsampling=0, exif=exif_block(False, 3, '2024:02:29 00:00:00')),
    }
    for name, data in seeds.items():
        with open(os.path.join(OUT, name), 'wb') as f:
            f.write(data)
        print('%-16s %5d bytes' % (name, len(data)))


if __name__ == '__main__':
    main()
