#!/bin/sh

set -euo pipefail

ROOT="$(dirname "$0")"
BUILD_DIR="$ROOT/build"
ROOTFS_DIR="$ROOT/rootfs"

mkdir -p "$BUILD_DIR"
mkdir -p "$ROOTFS_DIR"

echo "Compiling rlinit"
musl-gcc -static -O2 -s rlinit/rlinit.c -o "$BUILD_DIR/init"

echo "Compiling rlwsd"
musl-gcc -static -O2 -s rlwsd/rlwsd.c -o "$BUILD_DIR/rlwsd"

echo "Building rootfs"
cp "$BUILD_DIR/init" "$ROOTFS_DIR/init"
cp "$BUILD_DIR/rlwsd" "$ROOTFS_DIR/rlwsd"

echo "Building initramfs"
(cd "$ROOTFS_DIR" && find . -print0 | cpio --null -ov --format=newc) > "$BUILD_DIR/initramfs.cpio"

echo "Running QEMU (do CTRL+A then X to quit)"

# set vga=ask to select VGA modes
qemu-system-x86_64 \
	-kernel linux/arch/x86/boot/bzImage \
	-initrd "$BUILD_DIR/initramfs.cpio" \
	-append "console=tty0 console=ttyS0,115200 vga=792" \
	-vga std \
	-display gtk