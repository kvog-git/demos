#!/bin/bash
set -euo pipefail

[ $# -eq 1 ] || { echo "supply a file" >&2; exit 1; }

mkdir -p tmp/bench
git show HEAD:flacdec/flacdec.c > tmp/bench/base.c
cc -O3 -DNO_SPEW -o tmp/bench/flacdec_base tmp/bench/base.c
cc -O3 -DNO_SPEW -o tmp/bench/flacdec_new flacdec.c

hyperfine -N -w 3 -r 20 \
    -n HEAD   "tmp/bench/flacdec_base --dry \"$1\"" \
    -n STAGED "tmp/bench/flacdec_new --dry \"$1\""

