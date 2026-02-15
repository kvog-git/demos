#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/out"

echo "Compiling userspace programs..."
CFLAGS="-static -O2 -s --sysroot $OUT/sysroot -nostdinc -isystem $OUT/sysroot/usr/include"

echo "rlinit"
musl-gcc $CFLAGS rlinit/rlinit.c -o "$OUT/rlinit"

echo "rlws"
musl-gcc $CFLAGS rlws/rlws.c -o "$OUT/rlws"

echo "Creating rootfs..."
ROOTFS="$DIR/out/rootfs"
mkdir -p "$ROOTFS/dev"
mkdir -p "$ROOTFS/proc"
mkdir -p "$ROOTFS/sys"
cp "$OUT/rlinit" "$ROOTFS/init"
cp "$OUT/rlws" "$ROOTFS/rlws"

echo "Creating initranfs..."
(
    cd "$ROOTFS"
    find . -print0 \
        | cpio --null -ov --format=newc \
        | gzip -9 > "$DIR/out/initramfs.cpio.gz"
)