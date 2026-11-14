#!/bin/sh
# Fuzz the photo path (fuzz.c), or replay its saved inputs.
#   sh tools/jpeg_host/fuzz.sh [SECONDS]   libFuzzer run, default 15 min (new paths level off by then);
#                                          needs Homebrew llvm
#   sh tools/jpeg_host/fuzz.sh merge       add the run's new paths to fuzz_corpus/ (smallest set)
#   sh tools/jpeg_host/fuzz.sh replay      seeds, corpus and findings once, any compiler (CI)
# A finding lands in build/ as crash-*, timeout-* or oom-*: once fixed, copy it into fuzz_findings/
# so replay keeps checking it.
set -e
cd "$(dirname "$0")/../.."
H=tools/jpeg_host
mkdir -p build build/fuzz_corpus
SOURCES="$H/fuzz.c src/jpeg_convert.c src/exif.c src/palette.c src/cache_format.c lib/tjpgd/tjpgd.c"
FLAGS="-std=c2x -g -O1 -fwrapv -Wall -Wextra -Wno-unused-parameter -Werror -I include -I lib/tjpgd -I lib/e-Paper -I $H/stub"
SANITIZE="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
case "$1" in
replay)
    ${CC:-cc} $FLAGS $SANITIZE -D FUZZ_REPLAY $SOURCES -o build/fuzz_replay
    build/fuzz_replay $H/fuzz_seeds/* $H/fuzz_corpus/* $(ls $H/fuzz_findings/* 2>/dev/null)
    exit ;;
'' | merge) ;;
*[!0-9]*)
    echo "usage: $0 [SECONDS | merge | replay]" >&2
    exit 2 ;;
esac
CLANG=${CLANG:-/opt/homebrew/opt/llvm/bin/clang}
$CLANG $FLAGS $SANITIZE -fsanitize=fuzzer $SOURCES -o build/fuzz
if [ "$1" = merge ]; then
    # Only whether a path is reached counts (no hit counts), which keeps the committed corpus small
    build/fuzz -merge=1 -use_counters=0 $H/fuzz_corpus build/fuzz_corpus $H/fuzz_seeds
    exit
fi
build/fuzz -max_total_time="${1:-900}" -timeout=10 -rss_limit_mb=512 -max_len=65536 \
    -dict=$H/fuzz.dict -use_value_profile=1 -artifact_prefix=build/ -print_final_stats=1 \
    build/fuzz_corpus $H/fuzz_corpus $H/fuzz_seeds
