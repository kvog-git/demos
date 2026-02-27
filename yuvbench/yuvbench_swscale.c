#include "yuvbench.h"

#include <libswscale/swscale.h>

static bool yuvbench_swscale_init(Ctx* ctx)
{
    struct SwsContext* sws = sws_getContext(ctx->inp_w, ctx->inp_h, AV_PIX_FMT_YUV420P,
                                            ctx->inp_w, ctx->inp_h, AV_PIX_FMT_RGB24,
                                            0, 0, 0, 0);
    ctx->user = sws;
    return !!sws;
}

static void yuvbench_swscale_deinit(Ctx* ctx)
{
    sws_freeContext(ctx->user);
}

static bool yuvbench_swscale_convert(Ctx* ctx)
{
    struct SwsContext* sws = ctx->user;
    const uint32_t w = ctx->inp_w;
    const uint32_t h = ctx->inp_h;
    const uint8_t* Y  = (const uint8_t*)ctx->inp_buf;
    const uint8_t* Cb = Y + (w * h);
    const uint8_t* Cr = Cb + (w / 2 * h / 2);
    const uint8_t* const src_slices[] = { Y, Cb, Cr };
    const int src_strides[] = { w, w / 2, w / 2 };
    uint8_t* const dst_slices[] = { ctx->out_buf };
    const int dst_strides[] = { w * 3 };
    return sws_scale(sws, src_slices, src_strides, 0, h, dst_slices, dst_strides) == (int)h;
}

Backend yuvbench_swscale(void)
{
    Backend b = { 0 };
    b.init_fn = yuvbench_swscale_init;
    b.deinit_fn = yuvbench_swscale_deinit;
    b.convert_fn = yuvbench_swscale_convert;
    return b;
}
