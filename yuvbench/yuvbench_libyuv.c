#include "yuvbench.h"

#include <libyuv.h>
#include <libyuv/convert_argb.h>

static bool yuvbench_libyuv_convert(Ctx* ctx)
{
    const uint32_t w = ctx->inp_w;
    const uint32_t h = ctx->inp_h;
    const uint8_t* Y  = (const uint8_t*)ctx->inp_buf;
    const uint8_t* Cb = Y + (w * h);
    const uint8_t* Cr = Cb + (w / 2 * h / 2);
    return I420ToRAW(Y, w, Cb, w / 2, Cr, w / 2, ctx->out_buf, w * 3, w, h) == 0;
}

Backend yuvbench_libyuv(void)
{
  Backend b = { 0 };
  b.convert_fn = yuvbench_libyuv_convert;
  return b;
}
