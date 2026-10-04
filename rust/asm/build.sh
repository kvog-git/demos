#!/bin/sh
mkdir -p bin
cc -c main_aarch64.S -o bin/asm.o
ar rcs bin/libasm.a bin/asm.o
rustc --edition 2024 main.rs -L bin -l static=asm --out-dir bin
