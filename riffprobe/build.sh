#!/bin/sh
set -euo pipefail
mkdir -p bin
cc -o bin/riffprobe -Wall -Wextra -Wpedantic -O0 -g ./riffprobe.c
