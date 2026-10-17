#!/bin/sh
mkdir -p bin
cc -o bin/microshmup microshmup.S $(pkg-config --libs sdl3)
