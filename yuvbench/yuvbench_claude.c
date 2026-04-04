#include "yuvbench.h"

#ifdef YUVBENCH_CLAUDE

#include <arm_neon.h>
#include <dispatch/dispatch.h>

// BT.709 limited range, scale >>6 (factor=64):
//   R = clip(( 75*(Y-16) + 119*(Cr-128) + 32) >> 6)
//   G = clip(( 75*(Y-16) -  12*(Cb-128) -  30*(Cr-128) + 32) >> 6)
//   B = clip(( 75*(Y-16) + 129*(Cb-128) + 32) >> 6)
//
// All int16 intermediates stay within [-16512, 31801] — no overflow.
// vqshrun_n_s16 gives saturating unsigned narrow + right shift in one instruction.

// Coefficients:
//   Y:  75  (1.164 * 64 = 74.5 → 75, err +0.67%)
//   Cr_R: 119 (1.856 * 64 = 118.8 → 119, err +0.17%)
//   Cb_B: 129 (2.016 * 64; libyuv uses 2.016 for BT.709 studio swing)
//   Cb_G:  12 (0.187 * 64 = 12.0)
//   Cr_G:  30 (0.468 * 64 = 30.0)

// Convert 8 luma pixels using pre-widened, bias-subtracted chroma vectors.
// vcb = Cb - 128 (int16x8), vcr = Cr - 128 (int16x8)
static inline void convert_8px(
    const uint8_t* __restrict__ y,
    int16x8_t vcb,
    int16x8_t vcr,
    uint8_t* __restrict__ out)
{
    // Y - 16
    int16x8_t vy = vsubq_s16(
        vreinterpretq_s16_u16(vmovl_u8(vld1_u8(y))),
        vdupq_n_s16(16));

    // ry = 75*(Y-16) + rounding
    int16x8_t base = vaddq_s16(vmulq_n_s16(vy, 75), vdupq_n_s16(32));

    // R = base + 119*Cr
    int16x8_t r = vmlaq_n_s16(base, vcr, 119);
    // G = base - 12*Cb - 30*Cr
    int16x8_t g = vmlsq_n_s16(vmlsq_n_s16(base, vcb, 12), vcr, 30);
    // B = base + 129*Cb
    int16x8_t b = vmlaq_n_s16(base, vcb, 129);

    // Saturating narrow+shift: clamps to [0,255] and packs to uint8
    uint8x8x3_t rgb;
    rgb.val[0] = vqshrun_n_s16(r, 6);
    rgb.val[1] = vqshrun_n_s16(g, 6);
    rgb.val[2] = vqshrun_n_s16(b, 6);
    vst3_u8(out, rgb);  // stores R0G0B0 R1G1B1 ... R7G7B7
}

// Process a contiguous range of row-pairs [row_start, row_end).
// row_start and row_end must be even.
static void convert_rows(
    const uint8_t* __restrict__ Y,
    const uint8_t* __restrict__ Cb,
    const uint8_t* __restrict__ Cr,
    uint8_t* __restrict__ RGB,
    uint32_t w,
    uint32_t row_start,
    uint32_t row_end)
{
    for (uint32_t row = row_start; row < row_end; row += 2) {
        const uint8_t* y0   = Y   + row * w;
        const uint8_t* y1   = y0  + w;
        const uint8_t* cb   = Cb  + (row / 2) * (w / 2);
        const uint8_t* cr   = Cr  + (row / 2) * (w / 2);
        uint8_t*       rgb0 = RGB + row * w * 3;
        uint8_t*       rgb1 = rgb0 + w * 3;

        uint32_t col = 0;

        // 16 pixels per inner iteration: 8 chroma samples shared across 2 rows.
        for (; col + 16 <= w; col += 16) {
            // Load 8 Cb/Cr bytes and upsample to 16 via interleave-with-self:
            //   [c0,c1,...,c7] → [c0,c0,c1,c1,...,c7,c7]
            uint8x8x2_t cb_up = vzip_u8(vld1_u8(cb + col/2), vld1_u8(cb + col/2));
            uint8x8x2_t cr_up = vzip_u8(vld1_u8(cr + col/2), vld1_u8(cr + col/2));

            // Widen and bias-subtract chroma for low 8 and high 8 pixels
            int16x8_t vcb_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(cb_up.val[0])), vdupq_n_s16(128));
            int16x8_t vcb_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(cb_up.val[1])), vdupq_n_s16(128));
            int16x8_t vcr_lo = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(cr_up.val[0])), vdupq_n_s16(128));
            int16x8_t vcr_hi = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(cr_up.val[1])), vdupq_n_s16(128));

            // Row 0
            convert_8px(y0 + col,      vcb_lo, vcr_lo, rgb0 + col * 3);
            convert_8px(y0 + col + 8,  vcb_hi, vcr_hi, rgb0 + (col + 8) * 3);
            // Row 1 — same chroma, different luma
            convert_8px(y1 + col,      vcb_lo, vcr_lo, rgb1 + col * 3);
            convert_8px(y1 + col + 8,  vcb_hi, vcr_hi, rgb1 + (col + 8) * 3);
        }

        // Scalar tail for widths not divisible by 16
        for (; col < w; col += 2) {
            int32_t cb2 = (int32_t)cb[col / 2] - 128;
            int32_t cr2 = (int32_t)cr[col / 2] - 128;
            for (uint32_t dy = 0; dy < 2; ++dy) {
                const uint8_t* yr  = (dy == 0) ? y0   : y1;
                uint8_t*       dst = (dy == 0) ? rgb0 : rgb1;
                for (uint32_t dx = 0; dx < 2 && (col + dx) < w; ++dx) {
                    int32_t yv = (int32_t)yr[col + dx] - 16;
                    int32_t base = 75 * yv + 32;
                    int32_t r = base + 119 * cr2;
                    int32_t g = base -  12 * cb2 - 30 * cr2;
                    int32_t bv = base + 129 * cb2;
                    r  = (r  >> 6); r  = r  < 0 ? 0 : r  > 255 ? 255 : r;
                    g  = (g  >> 6); g  = g  < 0 ? 0 : g  > 255 ? 255 : g;
                    bv = (bv >> 6); bv = bv < 0 ? 0 : bv > 255 ? 255 : bv;
                    dst[(col + dx) * 3 + 0] = (uint8_t)r;
                    dst[(col + dx) * 3 + 1] = (uint8_t)g;
                    dst[(col + dx) * 3 + 2] = (uint8_t)bv;
                }
            }
        }
    }
}

static bool yuvbench_claude_init(Ctx* ctx)
{
    return (ctx->inp_w % 2 == 0) && (ctx->inp_h % 2 == 0);
}

static bool yuvbench_claude_convert(Ctx* ctx)
{
    const uint32_t w  = ctx->inp_w;
    const uint32_t h  = ctx->inp_h;
    const uint8_t* Y  = (const uint8_t*)ctx->inp_buf;
    const uint8_t* Cb = Y  + (size_t)w * h;
    const uint8_t* Cr = Cb + (size_t)(w / 2) * (h / 2);
    uint8_t*       RGB = (uint8_t*)ctx->out_buf;

    // Dispatch row-pairs across performance cores.
    // dispatch_apply is synchronous: returns only after all blocks finish.
    static const uint32_t NCHUNKS = 8;
    uint32_t pairs = h / 2;

    dispatch_apply(NCHUNKS,
                   dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0),
                   ^(size_t tid) {
        uint32_t start = (uint32_t)((tid     * pairs / NCHUNKS) * 2);
        uint32_t end   = (uint32_t)(((tid+1) * pairs / NCHUNKS) * 2);
        convert_rows(Y, Cb, Cr, RGB, w, start, end);
    });

    return true;
}

Backend yuvbench_claude(void)
{
    Backend b = { 0 };
    b.init_fn    = yuvbench_claude_init;
    b.convert_fn = yuvbench_claude_convert;
    return b;
}

#endif // YUVBENCH_CLAUDE
