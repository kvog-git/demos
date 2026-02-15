#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"



arch="$(uname -m)"

case "$(uname -m)" in
    x86_64)
        # set vga=ask to select VGA modes
        qemu-system-x86_64 \
            -kernel "$DIR/out/kernel/arch/x86_64/boot/bzImage" \
            -initrd "$DIR/out/initramfs.cpio.gz" \
            -append "console=tty0 console=ttyS0,115200 vga=792" \
            -vga std \
            -display gtk \
            -serial mon:stdio
        ;;
    aarch64)
        qemu-system-aarch64 -machine virt -cpu cortex-a72 \
            -kernel "$DIR/out/kernel/arch/arm64/boot/Image" \
            -initrd "$DIR/out/initramfs.cpio.gz" \
            -append "console=ttyAMA0 console=ttyS0,115200 vga=792" \
            -device virtio-gpu-pci,edid=on,xres=1024,yres=768 \
            -display cocoa \
            -serial mon:stdio
        ;;
esac