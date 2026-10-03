#!/bin/bash
mkdir -p bin
cc -o bin/win-resize-mt -Wall -Wextra -Wpedantic -O0 -g ./win-resize-mt.c -ldwmapi -lgdi32
