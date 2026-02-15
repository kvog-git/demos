#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

KERNEL_VER="6.19"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VER.tar.xz"
KERNEL_TAR="linux-$KERNEL_VER.tar.xz"
KERNEL_SRC="$DIR/linux-$KERNEL_VER"
KERNEL_OUT="$DIR/out/kernel"
KERNEL_CFG="$KERNEL_SRC/scripts/config"

mkdir -p "$KERNEL_OUT"

# If no kernel sources are found, grab the release tarball
if [ ! -d "$KERNEL_SRC" ]; then
    echo "Kernel sources not found. Downloading a release tarball in 3 seconds..."
    sleep 3
    echo "Downloading $KERNEL_URL..."
    curl -L -o "$DIR/$KERNEL_TAR" "$KERNEL_URL"
    echo "Extracting - this can take a little bit..."
    tar -xf "$KERNEL_TAR" -C "$DIR"
    rm "$DIR/$KERNEL_TAR"

    echo "Generating default kernel config..."
    make -C "$KERNEL_SRC" O="$KERNEL_OUT" defconfig
fi

echo "Setting kernel config options..."

kenable() { "$KERNEL_CFG" --file "$KERNEL_OUT/.config" --enable "$1"; }

kenable DRM
kenable DRM_FBDEV_EMULATION
kenable DRM_VIRTIO_GPU

kenable FB
kenable FB_VESA

kenable FRAMEBUFFER_CONSOLE

echo "Resolving config dependencies..."
make -C "$KERNEL_SRC" O="$KERNEL_OUT" olddefconfig

echo "Building with $(nproc) threads in 3 seconds..."
sleep 3
make -C "$KERNEL_SRC" O="$KERNEL_OUT" -j "$(nproc)"

echo "Exporting kernel headers..."
make -C "$KERNEL_SRC" O="$KERNEL_OUT" -j "$(nproc)" \
    headers_install INSTALL_HDR_PATH="$DIR/out/sysroot/usr"
