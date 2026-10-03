#!/bin/bash
mkdir -p bin
cc -o bin/win-resize-st -Wall -Wextra -Wpedantic -O0 -g ./win-resize-st.c -ldwmapi -lgdi32
