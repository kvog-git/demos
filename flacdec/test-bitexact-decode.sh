#!/bin/bash

# release compile
mkdir -p bin
cc -o bin/flacdec_r -Wall -Wextra -Wpedantic -O3 -g ./flacdec.c

mkdir -p tmp

for f in "$@"; do
    #echo "decoding $f to bits1.raw"
    ./bin/flacdec_r --dry --dump-samples=tmp/bits1.raw "$f" 2>/dev/null

    #echo "decoding $f to bits2.raw with reference decoder"
    flac -d -s -f --force-raw-format --endian=little --sign=signed -o tmp/bits2.raw "$f"

    out=$(cmp tmp/bits1.raw tmp/bits2.raw 2>&1)
    if [ $? -eq 0 ]; then
        echo "PASS: $f"
        continue
    fi

    echo "FAIL: $f"
    echo "$out"
done
