#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

qemu-system-aarch64 -machine virt -cpu cortex-a72 \
  -kernel "$DIR/out/kernel/arch/arm64/boot/Image" \
  -initrd "$DIR/out/initramfs.cpio.gz" \
  -append "console=ttyAMA0 console=ttyS0,115200 vga=792" \
  -device virtio-gpu-pci,edid=on,xres=1024,yres=768 \
  -display cocoa \
  -serial mon:stdio