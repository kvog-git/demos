#!/bin/sh
CFLAGS="-Wall -Wextra -Wpedantic -O3 -g -DYUVBENCH_BAD"
LFLAGS=""
mkdir -p build
clang -o build/yuvbench.o $CFLAGS -c ./yuvbench.c
clang -o build/yuvbench_bad.o $CFLAGS -c ./yuvbench_bad.c
clang -o build/yuvbench $LFLAGS build/yuvbench.o build/yuvbench_bad.o
