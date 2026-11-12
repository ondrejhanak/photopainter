#!/bin/sh
# Build the host JPEG harness (build/jpeg_host) and unit checks (build/unit_test), sanitizers on;
# pass "-O2" instead for fast runs. -fwrapv as on the device: TJpgDec's IDCT overflows on damaged
# data and must wrap, not be undefined.
set -e
cd "$(dirname "$0")/../.."
mkdir -p build
build() {
    out=$1
    shift
    cc -std=c2x -g ${FLAGS:--O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined} \
        -Wall -Wextra -Wno-unused-parameter -Werror -fwrapv \
        -I include -I lib/tjpgd -I lib/e-Paper -I tools/jpeg_host/stub \
        "$@" src/exif.c src/palette.c src/cache_format.c lib/tjpgd/tjpgd.c -o "build/$out"
}
FLAGS=$1
build jpeg_host src/jpeg_convert.c tools/jpeg_host/main.c
build unit_test tools/jpeg_host/unit_test.c src/messages.c
