# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

yuvbench benchmarks YUV 4:2:0 → RGB24 color space conversion across multiple implementations. It loads a raw YUV file, runs each enabled backend through 100 warmup + 2500 timed iterations, and reports min/max/avg per-iteration timing in milliseconds.

## Build Commands

```bash
# macOS (Apple Silicon)
./build-macos-aarch64-clang.sh

# Linux (x86_64)
./build-linux-x86_64-gcc.sh
```

Both scripts create `build/` and produce `build/yuvbench`.

## Running

```bash
./build/yuvbench images/jellybeans-256x256.yuv
./build/yuvbench images/capitol-2950x1528.yuv
./build/yuvbench images/capitol-2950x1528.yuv show   # pipe last frame to ffplay
```

Input filename must encode dimensions as `name-WIDTHxHEIGHT.yuv`.

## Prepare Test Images

```bash
# Convert source images in images/src/ to raw YUV 4:2:0
./images/convert.sh
```

## Architecture

### Backend Plugin System

Each backend is an optional compilation unit implementing the interface in `yuvbench.h`:

```c
typedef struct {
    void (*init_fn)(Ctx *ctx);
    void (*convert_fn)(Ctx *ctx);
    void (*deinit_fn)(Ctx *ctx);
} Backend;
```

`yuvbench.c:run_backend()` drives warmup + timing loops. Backends are compiled in via `-DYUVBENCH_<NAME>` preprocessor flags set in each build script.

### Backends

| Define | File | Platform | Notes |
|--------|------|----------|-------|
| `YUVBENCH_BAD` | `yuvbench_bad.c` | All | Naive BT.709 nested loop; reference baseline |
| `YUVBENCH_ACCELERATE` | `yuvbench_accelerate.c` | macOS | vImage YUV→ARGB→RGB; caches conversion object |
| `YUVBENCH_SWSCALE` | `yuvbench_swscale.c` | All | FFmpeg libswscale; SwsContext created in init |
| `YUVBENCH_LIBYUV` | `yuvbench_libyuv.c` | Linux | Google libyuv `I420ToRAW()`; no init/deinit |

### Timing (`kbench.h`)

- macOS/ARM64: reads `CNTVCT_EL0` / `CNTFRQ_EL0` hardware registers directly
- Linux: `clock_gettime(CLOCK_MONOTONIC)`

### Adding a New Backend

1. Create `yuvbench_<name>.c` implementing `init`, `convert`, `deinit` functions
2. Guard the file body with `#ifdef YUVBENCH_<NAME>`
3. Register it in `yuvbench.c` (see the `backends[]` array)
4. Add `-DYUVBENCH_<NAME>` and any link flags to the relevant build scripts

## Platform Notes

- The `vk-asylum` branch contains a Vulkan compute shader backend (`build-shaders.sh`, `shaders.h`, `main.c`)
- Assembly output for the Accelerate backend is emitted to `build/yuvbench_accelerate.S` on macOS builds
