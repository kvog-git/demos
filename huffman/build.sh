#!/bin/bash
mkdir -p bin
cc -o bin/huffman -Wall -Wextra -Wpedantic -O0 -g ./huffman.c
