#!/bin/sh
mkdir -p bin
cc -o bin/microshmup microshmup_main_sdl.S $(pkg-config --libs sdl3)
