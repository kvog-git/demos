#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
IMG="rl-kernel-builder:ubuntu24.04"

if ! docker image inspect "$IMG" > /dev/null 2>&1; then
    echo "Docker image $IMG not found, building..."
    docker build -t "$IMG" -f Dockerfile .
fi

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <script> [args...]"
    exit 1
fi

docker run --rm -it -v "$DIR:/work" -w /work "$IMG" bash -lc "$*"