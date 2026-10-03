// ================================================================================================
// Toy FLAC decoder. Only support s16 stereo inputs.
//
// ffmpeg -i inp.mp3 -ac 2 -sample_fmt s16 out.flac
//
// ref: https://www.rfc-editor.org/rfc/rfc9639.pdf
//
// Changelog:
//     10/03/2026: Initial release
//
// License:
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#ifdef _MSC_VER
#   pragma warning(disable: 4244)
#   pragma warning(disable: 4267)
#   define _CRT_SECURE_NO_WARNINGS
#   define popen(A, B) _popen(A, "wb")
#   define pclose _pclose
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); abort(); } while (0)

#if 1
#   define SPEW(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#   define SPEW(...)
#endif

typedef struct BitR BitR;
struct BitR
{
    const uint8_t *bbuf;
    size_t         blen;
    size_t         cbyte;
    size_t         cbit;
};

uint8_t peek_bit(BitR *br)
{
    assert(br->cbyte < br->blen);
    uint8_t bit = (br->bbuf[br->cbyte] >> (7 - br->cbit)) & 1;
    return bit;
}

uint8_t read_bit(BitR *br)
{
    assert(br->cbyte < br->blen);
    uint8_t bit = (br->bbuf[br->cbyte] >> (7 - br->cbit)) & 1;
    br->cbit += 1;
    br->cbyte += br->cbit / 8;
    br->cbit %= 8;
    return bit;
}

void read_bytes(BitR *br, size_t len, uint8_t *out)
{
    assert(!br->cbit);
    assert(br->cbyte + len <= br->blen);
    for (size_t i = 0; i < len; ++i) {
        out[i] = br->bbuf[br->cbyte];
        br->cbyte += 1;
    }
}

int64_t make_signed(uint64_t u, size_t n)
{
    uint64_t sign = (uint64_t)1 << (n - 1);
    return (int64_t)((u ^ sign) - sign);
}

uint64_t read_bits(BitR *br, uint8_t n)
{
    uint64_t b = 0;
    for (uint8_t i = 0; i < n; ++i) {
        b <<= 1;
        b |= read_bit(br);
    }
    return b;
}

int64_t read_sbits(BitR *br, uint8_t n)
{
    if (n == 0)
        return 0;
    return make_signed(read_bits(br, n), n);
}

void skip_bits(BitR *br, size_t n)
{
    br->cbit += n;
    br->cbyte += br->cbit / 8;
    br->cbit %= 8;
}

void read_coded_residuals(BitR *br, uint32_t block_size, uint8_t order, int32_t *out)
{
    // (9.2.7)
    uint8_t rice_bits  = read_bits(br, 2) ? 5 : 4;
    uint8_t rice_order = read_bits(br, 4);
    size_t  rice_partitions = (size_t)1 << rice_order;
    assert(block_size % rice_partitions == 0);

    for (size_t i = 0; i < rice_partitions; ++i) {
        size_t num = block_size >> rice_order;
        if (i == 0) {
            num -= order;
        }

        size_t param = read_bits(br, rice_bits);
        if ((rice_bits == 4 && param == 0xF) || param == 0x1F) {
            // escaped partition (9.2.7.1)
            uint8_t width = read_bits(br, 5);
            for (size_t j = 0; j < num; ++j) {
                *out = read_sbits(br, width);
                ++out;
            }
        } else {
            for (size_t j = 0; j < num; ++j) {
                // (D.2.7)
                uint32_t quot = 0;
                while (!read_bit(br))
                    ++quot;
                uint32_t folded = (quot << param) | read_bits(br, param);
                if (folded % 2 == 0) {
                    *out = folded >> 1;
                } else {
                    *out = ~(folded >> 1);
                }
                ++out;
            }
        }
    }
}

const char *help =
    "flacdec - toy flac decoder                                 \n"
    "                                                           \n"
    "OPTIONS:                                                   \n"
    "    --dry                          Disable ffplay playback \n"
    "    --dump-samples=<path.raw>      Dump raw audio frames   \n"
    "    --help                         Display this menu       \n";

int main(int argc, char **argv)
{
    // Parse arguments
    bool        a_dry          = false;
    const char *a_dump_samples = 0;
    const char *a_file         = 0;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--dry")) {
            a_dry = true;
        } else if (!strncmp(a, "--dump-samples=", 15)) {
            a_dump_samples = a + 15;
        } else if (!strcmp(a, "--help")) {
            fprintf(stderr, "%s", help);
            return 0;
        } else if (*a == '-') {
            fprintf(stderr, "Unknown argument: %s\n", a);
            return 1;
        } else if (!a_file) {
            a_file = a;
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", a);
            return 1;
        }
    }
    if (!a_file)
        DIE("no file supplied\n");

    // Read entire input file into memory
    size_t   flen = 0;
    uint8_t *fbuf = 0;
    {
        FILE* f = fopen(a_file, "rb");
        if (!f)
            DIE("failed to open input\n");

        fseek(f, 0, SEEK_END);
        flen = ftell(f);
        fseek(f, 0, SEEK_SET);

        fbuf = malloc(flen);
        fread(fbuf, 1, flen, f);
        fclose(f);
    }

    // Bit-level reader
    BitR br = { fbuf, flen, 0, 0 };

    // Parse header             f L a C
    if (read_bits(&br, 32) != 0x664C6143)
        DIE("invalid input file\n");

    // Metadata fields
    uint16_t min_block_size = 0;
    uint16_t max_block_size = 0;
    uint32_t min_frame_size = 0;
    uint32_t max_frame_size = 0;
    uint32_t sample_rate    = 0;
    uint8_t  num_channels   = 0;
    uint8_t  bit_depth      = 0;
    uint64_t num_samples    = 0;

    // Parse metadata blocks
    for (;;) {
        uint8_t  end  = read_bit(&br);
        uint16_t type = read_bits(&br, 7);
        uint32_t len  = read_bits(&br, 24);
        SPEW("chunk type=%d len=%d end=%d\n", type, len, end);

        switch (type) {
        // streaminfo (8.2)
        case 0: {
            min_block_size = read_bits(&br, 16);
            max_block_size = read_bits(&br, 16);
            min_frame_size = read_bits(&br, 24);
            max_frame_size = read_bits(&br, 24);
            sample_rate    = read_bits(&br, 20);
            num_channels   = read_bits(&br, 3) + 1;
            bit_depth      = read_bits(&br, 5) + 1;
            num_samples    = read_bits(&br, 36);
            skip_bits(&br, 128); // md5
        } break;
        default: {
            skip_bits(&br, len * 8);
        };
        };

        if (end)
            break;
    }

    fprintf(stderr, "block size:  [%d, %d]\n", min_block_size, max_block_size);
    fprintf(stderr, "frame size:  [%d, %d]\n", min_frame_size, max_frame_size);
    fprintf(stderr, "sample rate: %d\n", sample_rate);
    fprintf(stderr, "channels:    %d\n", num_channels);
    fprintf(stderr, "bit depth:   %d\n", bit_depth);
    fprintf(stderr, "num samples: %llu\n", num_samples);

    // Try to open ffplay for playback
    FILE *ffplay = 0;
    if (!a_dry) {
        char ffplay_cmd[256];
        snprintf(ffplay_cmd, sizeof(ffplay_cmd), "ffplay -loglevel quiet -f s16le -ar %u "
                                                 "-ch_layout stereo -nodisp -autoexit -i - ",
                                                sample_rate);
        ffplay = popen(ffplay_cmd, "w");
        if (!ffplay)
            fprintf(stderr, "ffplay not available, no playback\n");
    }

    // Open sample dump file
    FILE *dump = 0;
    if (a_dump_samples) {
        dump = fopen(a_dump_samples, "wb");
        if (!dump)
            DIE("Failed to open %s for writing\n", a_dump_samples);
    }

    // only stero s16 is supported
    assert(num_channels == 2 && bit_depth == 16);

    int32_t *subframe_buf = 0;  // raw subframe samples. side channel can be 17-bit
    int16_t *samples_buf  = 0;  // output samples
    size_t   samples_len  = 0;

    // stats
    int npred_c = 0;
    int npred_v = 0;
    int npred_f = 0;
    int npred_l = 0;

    // Parse frames
    //for (;;) {
    while (br.cbyte < br.blen) {
        // Frames must start on a byte boundary (9.1)
        assert(!br.cbit);
        // Frames must start with a sync code
        uint16_t sync_code = read_bits(&br, 15);
        assert(sync_code == 0x7FFC);
        
        // Assume encoder was good and all frames have the same blocking strategy bit
        bool variable_block_size = read_bit(&br);
        assert(!variable_block_size && "unsupported encoding");

        uint8_t bs_code = read_bits(&br, 4);
        uint8_t sr_code = read_bits(&br, 4);
        uint8_t ch_code = read_bits(&br, 4);
        uint8_t bd_code = read_bits(&br, 3);
        skip_bits(&br, 1); // padding bit

        // (9.1.5)
        uint64_t frame_num = 0;
        {
            // Extended UTF-8 coded number: up to 36 bits unencoded or 7 bytes encoded
            // ref: https://www.rfc-editor.org/info/rfc3629/#section-3
            uint8_t b1 = read_bits(&br, 8);
            uint8_t extra = 0;
                 if (b1 >> 7 == 0x00) { frame_num = b1;                   }
            else if (b1 >> 5 == 0x06) { frame_num = b1 & 0x1F; extra = 1; }
            else if (b1 >> 4 == 0x0E) { frame_num = b1 & 0x0F; extra = 2; }
            else if (b1 >> 3 == 0x1E) { frame_num = b1 & 0x07; extra = 3; }
            else if (b1 >> 2 == 0x3E) { frame_num = b1 & 0x03; extra = 4; }
            else if (b1 >> 1 == 0x7E) { frame_num = b1 & 0x01; extra = 5; }
            else if (b1      == 0xFE) { frame_num = 0;         extra = 6; }
            else                      { DIE("bad coded frame number\n");  }

            for (uint8_t i = 0; i < extra; ++i) {
                uint8_t b = read_bits(&br, 8);
                assert(b >> 6 == 0x02 && "invalid UTF-8 code");
                frame_num <<= 6;
                frame_num |= (b & 0x3F);
            }
        }
        SPEW("+frame %llu\n", frame_num);

        // Parse block size (9.1.1)
        uint32_t block_size = 0;
        if (bs_code == 0)
            DIE("invalid block size code\n");
        else if (bs_code == 1)
            block_size = 192;
        else if (bs_code <= 5)
            block_size = 144 * (1 << bs_code);
        else if (bs_code == 6)
            block_size = read_bits(&br, 8) + 1;
        else if (bs_code == 7)
            block_size = read_bits(&br, 16) + 1;
        else
            block_size = 1 << bs_code;
        assert(block_size);
        SPEW("  %d samples\n", block_size);

        // Parse sample rate (9.1.2)
        // Don't care, just skip uncommon
        if (sr_code == 12)
            skip_bits(&br, 8);
        if (sr_code == 13 || sr_code == 14)
            skip_bits(&br, 16);

        // Parse bit depth (9.1.4)
        const uint8_t bdtab[] = { bit_depth, 8, 12, 0, 16, 20, 24, 32 };
        assert(bd_code < 8 && bd_code != 3);
        uint8_t frame_bd = bdtab[bd_code];
        // frame and header bit depth must be the same
        assert(frame_bd == bit_depth);

        // Skip CRC
        skip_bits(&br, 8);

        // Output buffer for samples
        // s16 only for now
        if (block_size > samples_len) {
            subframe_buf = realloc(subframe_buf, block_size * sizeof(int32_t) * 2); // 2 channels
            samples_buf  = realloc(samples_buf,  block_size * sizeof(int16_t) * 2);
            samples_len = block_size;
            assert(subframe_buf && samples_buf);
        }

        // Subframes
        for (size_t sf_num = 0; sf_num < 2; ++sf_num) {
            SPEW("  +subframe %zu\n", sf_num);
            // First subframe should start on a byte boundary
            assert(sf_num != 0 || !br.cbit);

            int32_t *samples = subframe_buf + block_size * sf_num;

            // (9.2.1)
            skip_bits(&br, 1);
            uint8_t sf_type_bits = read_bits(&br, 6);

            // Decode predictor type and order
            enum {
                PRED_CONSTANT,
                PRED_VERBATIM,
                PRED_FIXED,
                PRED_LINEAR,
            };
            int     pred_type  = 0;
            uint8_t pred_order = 0;
            if (sf_type_bits == 0) {
                pred_type = PRED_CONSTANT;
            } else if (sf_type_bits == 1) {
                pred_type = PRED_VERBATIM;
            } else if (sf_type_bits >= 8 && sf_type_bits <= 12) {
                pred_type  = PRED_FIXED;
                pred_order = sf_type_bits - 8;
            } else if (sf_type_bits >= 32 && sf_type_bits <= 63) {
                pred_type  = PRED_LINEAR;
                pred_order = sf_type_bits - 32 + 1;
            } else {
                assert(!"invalid predictor type");
            }
            SPEW("    pred=%d order=%d\n", pred_type, pred_order);

            // Wasted bits per sample (9.2.2)
            // If every sample in this subframe has k trailing zero bits (common when audio was
            // converted from a lower bit depth, e.g. 8 bit -> 16 bit) the encoder shifts them all
            // right by k. All samples will need to be shifted left by k on decode.
            uint8_t wbits_k = 0;
            if (read_bit(&br)) {
                do {
                    wbits_k += 1;
                } while (!read_bit(&br));
            }
            SPEW("    wbits:k=%d\n", wbits_k);

            // Calculate bit depth for this subframe.
            // Side channel gets an extra bit.
            bool is_side = 
                (ch_code == 8  && sf_num == 1) ||
                (ch_code == 9  && sf_num == 0) ||
                (ch_code == 10 && sf_num == 1);
            uint8_t sf_bd = frame_bd + is_side - wbits_k;

            // Decode samples
            size_t cur = 0;
            switch (pred_type) {
            case PRED_CONSTANT: {
                // (9.2.3)
                int32_t dc = read_sbits(&br, sf_bd);
                for (uint32_t i = 0; i < block_size; ++i)
                    samples[cur++] = dc;
                ++npred_c;
            } break;
            case PRED_VERBATIM: {
                // (9.2.4)
                for (uint32_t i = 0; i < block_size; ++i)
                    samples[cur++] = read_sbits(&br, sf_bd);
                ++npred_v;
            } break;
            case PRED_FIXED: {
                // (9.2.5)
                assert(pred_order < 5);
                for (uint8_t i = 0; i < pred_order; ++i)
                    samples[cur++] = read_sbits(&br, sf_bd);

                read_coded_residuals(&br, block_size, pred_order, &samples[pred_order]);

                for (uint32_t i = pred_order; i < block_size; ++i) {
                    int32_t *a = &samples[i - pred_order]; // oldest first
                    int32_t s = 0;
                    switch (pred_order) {
                    case 0: { s = 0;                                     } break;
                    case 1: { s = a[0];                                  } break;
                    case 2: { s = 2 * a[1] - a[0];                       } break;
                    case 3: { s = 3 * a[2] - 3 * a[1] + a[0];            } break;
                    case 4: { s = 4 * a[3] - 6 * a[2] + 4 * a[1] - a[0]; } break;
                    }
                    samples[cur] += s;
                    ++cur;
                }

                ++npred_f;
            } break;
            case PRED_LINEAR: {
                // (9.2.6)
                for (uint8_t i = 0; i < pred_order; ++i)
                    samples[cur++] = read_sbits(&br, sf_bd);

                int16_t lpc_coeffs[32];
                uint8_t lpc_coeff_width = read_bits(&br, 4) + 1;
                int8_t  lpc_shift = read_sbits(&br, 5); assert(lpc_shift >= 0);
                for (uint8_t i = 0; i < pred_order; ++i) {
                    lpc_coeffs[i] = read_sbits(&br, lpc_coeff_width);
                }

                read_coded_residuals(&br, block_size, pred_order, &samples[pred_order]);

                for (uint32_t i = pred_order; i < block_size; ++i) {
                    int32_t *a = &samples[i - pred_order]; // oldest first
                    int64_t s = 0;
                    for (uint8_t j = 0; j < pred_order; ++j) {
                        s += a[pred_order - j - 1] * lpc_coeffs[j];
                    }
                    samples[cur] += (s >> lpc_shift);
                    ++cur;
                }

                ++npred_l;
            } break;
            };

            // un-waste bits
            for (size_t i = 0; i < block_size; ++i)
                samples[i] *= (1 << wbits_k);
        }

        // padding after last subframe
        if (br.cbit)
            skip_bits(&br, 8 - br.cbit);

        // crc16, don't care
        skip_bits(&br, 16);

        // undo decorrelation (4.2, 9.1.3)
        int32_t *s1 = subframe_buf;
        int32_t *s2 = subframe_buf + block_size;
        for (size_t i = 0; i < block_size; ++i) {
            int16_t l = 0;
            int16_t r = 0;
            switch (ch_code) {
            case 1: {
                // 2 channels: left, right
                l = s1[i];
                r = s2[i];
            } break;
            case 8: {
                // 2 channels: left, right; stored as left-side stereo
                l = s1[i];
                r = s1[i] - s2[i];
            } break;
            case 9: {
                // 2 channels: left, right; stored as side-right stereo
                l = s2[i] + s1[i];
                r = s2[i];
            } break;
            case 10: {
                // 2 channels: left, right; stored as mid-side stereo
                int32_t m = (s1[i] * 2) | (s2[i] & 1);
                l = (m + s2[i]) >> 1;
                r = (m - s2[i]) >> 1;
            } break;
            default: assert(!"unsupported channel layout");
            }
            // interleaved write
            samples_buf[2 * i + 0] = l;
            samples_buf[2 * i + 1] = r;
        }

        // print some cool stats
        SPEW("  ->%zu\n", br.cbyte);
        SPEW("  predictors: c=%d v=%d f=%d l=%d\n", npred_c, npred_v, npred_f, npred_l);

        // update playback
        if (ffplay)
            fwrite(samples_buf, sizeof(int16_t), block_size * 2, ffplay);
        if (dump)
            fwrite(samples_buf, sizeof(int16_t), block_size * 2, dump);
    }
    if (ffplay)
        pclose(ffplay);
    if (dump)
        fclose(dump);
}

