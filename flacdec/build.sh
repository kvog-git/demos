#!/bin/sh
set -euo pipefail
mkdir -p bin
cc -o bin/flacdec -Wall -Wextra -Wpedantic -O0 -g ./flacdec.c
