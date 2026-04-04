#!/bin/sh
cc -o microshooter_client -O0 -g -fsanitize=address -Wall -Wextra -Wpedantic ./microshooter.c $(pkg-config --cflags --libs sdl3) -DCLIENT
cc -o microshooter_server -O0 -g -fsanitize=address -Wall -Wextra -Wpedantic ./microshooter.c $(pkg-config --cflags --libs sdl3) -DSERVER
