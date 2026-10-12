#!/bin/sh
mkdir -p bin
clang -o bin/microshmup microshmup.S $(pkg-config --libs sdl3)
