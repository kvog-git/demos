#!/bin/sh
CFLAGS="-Wall -Wextra -Wpedantic -O3 -g -DYUVBENCH_ACCELERATE -DYUVBENCH_BAD"
LFLAGS="-framework Accelerate"
mkdir -p build
set -x
clang -o build/yuvbench.o $CFLAGS -c ./yuvbench.c
clang -o build/yuvbench_accelerate.o $CFLAGS -c ./yuvbench_accelerate.c
clang -o build/yuvbench_accelerate.S $CFLAGS -S ./yuvbench_accelerate.c
clang -o build/yuvbench_bad.o $CFLAGS -c ./yuvbench_bad.c
clang -o build/yuvbench_bad.S $CFLAGS -S ./yuvbench_bad.c
clang -o build/yuvbench $LFLAGS build/yuvbench.o build/yuvbench_accelerate.o build/yuvbench_bad.o
