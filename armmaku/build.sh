#!/bin/sh
mkdir -p bin
cc -o bin/armmaku armmaku_main_sdl.S $(pkg-config --libs sdl3)
