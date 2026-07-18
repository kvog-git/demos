#!/bin/sh
set -euo pipefail
mkdir -p bin
C=$(pkg-config --cflags libavcodec libavformat libswscale libavutil sdl3)
L=$(pkg-config --libs   libavcodec libavformat libswscale libavutil sdl3)
cc -o bin/ffmpeg-shadertoy -Wall -Wextra -Wpedantic -O0 -g $C ./ffmpeg-shadertoy.c $L
