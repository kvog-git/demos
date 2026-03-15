#!/bin/bash
mkdir -p build
glslc -fshader-stage=vert ./triangle_vs.glsl -o build/triangle_vs.spv
glslc -fshader-stage=frag ./triangle_fs.glsl -o build/triangle_fs.spv
echo "const uint32_t triangle_vert_spv[] = { "$(hexdump -v -e '1/4 "0x%08x, "' build/triangle_vs.spv)" };" > shaders.h
echo "const uint32_t triangle_frag_spv[] = { "$(hexdump -v -e '1/4 "0x%08x, "' build/triangle_vs.spv)" };" >> shaders.h
