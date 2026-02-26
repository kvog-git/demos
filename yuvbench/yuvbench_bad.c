#include "yuvbench.h"

static inline uint8_t clamp8(int32_t x)
{
    if (x < 0) {
        return 0;
    }
    if (x > 255) {
        return 255;
    }
    return x;
}

static bool yuvbench_bad_init(Ctx* ctx)
{
    return (ctx->inp_w % 2 == 0) && (ctx->inp_h % 2 == 0);
}

static bool yuvbench_bad_convert(Ctx* ctx)
{
    const uint32_t w = ctx->inp_w;
    const uint32_t h = ctx->inp_h;
    
    // Input planes
    const uint8_t* Y  = (const uint8_t*)ctx->inp_buf;
    const uint8_t* Cb = Y + (w * h);
    const uint8_t* Cr = Cb + (w / 2 * h / 2);

    // Output plane
    uint8_t* RGB = (uint8_t*)ctx->out_buf;

    // Iterate over 2x2 input blocks
    for (uint32_t y = 0; y < h; y += 2) {
    for (uint32_t x = 0; x < w; x += 2) {
        uint8_t cb = Cb[(y / 2) * (w / 2) + (x / 2)]; // 0.5x sample
        uint8_t cr = Cr[(y / 2) * (w / 2) + (x / 2)]; // 0.5x sample
        for (uint32_t dy = 0; dy < 2; ++dy) {
        for (uint32_t dx = 0; dx < 2; ++dx) {
            uint32_t py = y + dy;
            uint32_t px = x + dx;

            const uint8_t yval = Y[py * w + px]; // 1.0x sample

            // ref: https://en.wikipedia.org/wiki/Y%E2%80%B2UV#HDTV_with_BT.709
            int32_t y2 = (int32_t)yval - 16;
            int32_t cb2 = (int32_t)cb - 128;
            int32_t cr2 = (int32_t)cr - 128;
            int32_t r = 1164 * y2 +  1856 * cr2;
            int32_t g = 1164 * y2 -  187  * cb2 - 468 * cr2;
            int32_t b = 1164 * y2 +  2148 * cb2;
            r = (r + 500) / 1000;
            g = (g + 500) / 1000;
            b = (b + 500) / 1000;
            size_t idx = (py * w + px) * 3;
            RGB[idx + 0] = clamp8(r);
            RGB[idx + 1] = clamp8(g);
            RGB[idx + 2] = clamp8(b);
        }
        }
    }
    }
    return true;
}

Backend yuvbench_bad(void)
{
  Backend b = { 0 };
  b.init_fn = yuvbench_bad_init;
  b.convert_fn = yuvbench_bad_convert;
  return b;
}
