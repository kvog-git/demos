#!/bin/sh
CFLAGS="-Wall -Wextra -Wpedantic -O3 -g -DYUVBENCH_BAD -DYUVBENCH_LIBYUV -DYUVBENCH_SWSCALE"
LFLAGS="$(pkg-config --libs libswscale) -lyuv"
mkdir -p build
gcc -o build/yuvbench.o $CFLAGS -c ./yuvbench.c
gcc -o build/yuvbench_bad.o $CFLAGS -c ./yuvbench_bad.c
gcc -o build/yuvbench_libyuv.o $CFLAGS -c ./yuvbench_libyuv.c
gcc -o build/yuvbench_swscale.o $CFLAGS $(pkg-config --cflags libswscale) -c ./yuvbench_swscale.c
gcc -o build/yuvbench $LFLAGS build/yuvbench.o build/yuvbench_bad.o build/yuvbench_libyuv.o build/yuvbench_swscale.o
