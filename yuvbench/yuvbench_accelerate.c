#include "yuvbench.h"

#include <Accelerate/Accelerate.h>

typedef struct AccelerateCtx AccelerateCtx;
struct AccelerateCtx
{
    void* rgba_buf;
    vImage_YpCbCrToARGB conv;
};

static bool yuvbench_accelerate_init(Ctx* ctx)
{
    AccelerateCtx* accel = calloc(1, sizeof(AccelerateCtx));
    accel->rgba_buf = calloc(4, ctx->inp_w * ctx->inp_h);
    ctx->user = accel;
    // BT.709
    vImage_YpCbCrToARGBMatrix matrix = { 0 };
    matrix.Yp = 1.0f;
    matrix.Cb_G = -0.1873f;
    matrix.Cb_B = 1.8556f;
    matrix.Cr_R = 1.5748f;
    matrix.Cr_G = -0.4681f;
    //
    vImage_YpCbCrPixelRange pr = { 0 };
    pr.Yp_bias = 16;
    pr.CbCr_bias = 128;
    pr.YpRangeMax = 235;
    pr.CbCrRangeMax = 240;
    pr.YpMax = 235;
    pr.YpMin = 16;
    pr.CbCrMax = 240;
    pr.CbCrMin = 16;
    //
    if (vImageConvert_YpCbCrToARGB_GenerateConversion(&matrix, &pr, &accel->conv, kvImage420Yp8_Cb8_Cr8, kvImageARGB8888, kvImageNoFlags) != kvImageNoError) {
        return false;
    }
    return true;
}

static void yuvbench_accelerate_deinit(Ctx* ctx)
{
    AccelerateCtx* accel = (AccelerateCtx*)ctx->user;
    free(accel->rgba_buf);
    free(accel);
}

static bool yuvbench_accelerate_convert(Ctx* ctx)
{
    AccelerateCtx* accel = (AccelerateCtx*)ctx->user;
    const uint32_t w = ctx->inp_w;
    const uint32_t h = ctx->inp_h;
    const uint8_t* Y  = (const uint8_t*)ctx->inp_buf;
    const uint8_t* Cb = Y + (w * h);
    const uint8_t* Cr = Cb + (w / 2 * h / 2);
    //
    vImage_Buffer ypbuf = { 0 };
    ypbuf.data = (void*)Y;
    ypbuf.width = w;
    ypbuf.height = h;
    ypbuf.rowBytes = w;
    vImage_Buffer cbbuf = { 0 };
    cbbuf.data = (void*)Cb;
    cbbuf.width = w / 2;
    cbbuf.height = h / 2;
    cbbuf.rowBytes = w / 2;
    vImage_Buffer crbuf = { 0 };
    crbuf.data = (void*)Cr;
    crbuf.width = w / 2;
    crbuf.height = h / 2;
    crbuf.rowBytes = w / 2;
    vImage_Buffer rgbabuf = { 0 };
    rgbabuf.data = accel->rgba_buf;
    rgbabuf.width = w;
    rgbabuf.height = h;
    rgbabuf.rowBytes = w * 4;
    //
    uint8_t permute[4] = { 0, 1, 2, 3 };
    //
    if (vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&ypbuf, &cbbuf, &crbuf, &rgbabuf, &accel->conv, permute, 0xFF, kvImageNoFlags) != kvImageNoError) {
        return false;
    }
    //
    vImage_Buffer rgbbuf = { 0 };
    rgbbuf.data = ctx->out_buf;
    rgbbuf.width = w;
    rgbbuf.height = h;
    rgbbuf.rowBytes = w * 3;
    if (vImageConvert_ARGB8888toRGB888(&rgbabuf, &rgbbuf, kvImageNoFlags) != kvImageNoError) {
        return false;
    }
    return true;
}

Backend yuvbench_accelerate(void)
{
  Backend b = { 0 };
  b.init_fn = yuvbench_accelerate_init;
  b.deinit_fn = yuvbench_accelerate_deinit;
  b.convert_fn = yuvbench_accelerate_convert;
  return b;
}
