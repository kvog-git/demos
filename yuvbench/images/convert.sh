#!/bin/sh
DIR="$(dirname "$0")"
for f in $DIR/src/*; do
    ff=$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=0 "$f")
    IFS='x' read -r ffw ffh <<< "$ff"
    fff="$(basename "$f")"
    ffff="$(dirname "$(dirname "$f")")/${fff%.*}-${ffw}x${ffh}.yuv"
    echo "$f -> $ffff"
    ffmpeg -hide_banner -y -i "$f" -pix_fmt yuv420p "$ffff"
done