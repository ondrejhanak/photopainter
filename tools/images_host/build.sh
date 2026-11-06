#!/bin/sh
# Build and run the host tests of the photo selection and the wake log (build/images_test): images.c,
# cache.c and wake_log.c on the real FatFs over a RAM disk, sanitizers on; the log rotates at 300 bytes.
# The firmware's log goes to build/images_test.log.
set -e
cd "$(dirname "$0")/../.."
mkdir -p build
FATFS=lib/FatFs_SPI
cc -std=c2x -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Wall -Wextra -Wno-unused-parameter \
    -I tools/images_host/stub -I tools/jpeg_host/stub -I include -I lib/e-Paper \
    -I $FATFS/ff14a/source -I $FATFS/include \
    tools/images_host/test.c src/images.c src/image_order.c src/cache.c src/cache_format.c src/palette.c \
    src/wake_log.c -D WAKE_LOG_MAX_BYTES=300 \
    $FATFS/src/ff.c $FATFS/src/ffsystem.c $FATFS/src/ffunicode.c $FATFS/src/f_util.c \
    -o build/images_test
build/images_test > build/images_test.log
