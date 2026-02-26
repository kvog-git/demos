#!/bin/sh
CFLAGS="-Wall -Wextra -Wpedantic -O3 -g -DYUVBENCH_BAD"
LFLAGS=""
mkdir -p build
gcc -o build/yuvbench.o $CFLAGS -c ./yuvbench.c
gcc -o build/yuvbench_bad.o $CFLAGS -c ./yuvbench_bad.c
gcc -o build/yuvbench $LFLAGS build/yuvbench.o build/yuvbench_bad.o
