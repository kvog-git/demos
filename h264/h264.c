// Barebones H.264 (baseline) bitstream parser. P-slices only.
//
// SPDX-License-Identifier: 0BSD
//
// ref: https://www.itu.int/rec/T-REC-H.264

// ffmpeg -i ~/badapple.mp4 -c:v libx264 -profile:v baseline -level 3.0 -preset fast -g 1 -f h264 test.264

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond) { if (!(cond)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #cond); abort(); } }

static struct
{
    const uint8_t* buf;
    size_t len;
    size_t cur;
    uint8_t rbsp_buf[1 << 20];
    size_t rbsp_len;
    size_t rbsp_cbyte;
    size_t rbsp_cbit;
} G = { 0 };

// Search for the next Annex-B start code (ref: §B.1)
// Returns the size of the start code
static size_t FindStartCode(size_t start, size_t* off)
{
    for (size_t i = start; i < G.len; ++i)
    {
        const uint8_t* p = &G.buf[i];
        const bool start3 = G.len - i >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01;
        const bool start4 = G.len - i >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01;
        if (start3) { *off = i; return 3; }
        if (start4) { *off = i; return 4; }
    }
    return 0;
}

// Search for the next NAL unit
static bool FindNalUnit(void)
{
    // Find the next start code
    size_t start_len = 0;
    if (!(start_len = FindStartCode(G.cur, &G.cur)))
    {
        return false;
    }
    G.cur += start_len;
    ASSERT(G.cur < G.len);

    // Find end of RBSP
    size_t end = 0;
    if (!FindStartCode(G.cur, &end))
    {
        end = G.len;
    }
    ASSERT(end - G.cur < sizeof(G.rbsp_buf));
    
    // Remove emulation bytes
    size_t z = 0;
    G.rbsp_len = 0;
    G.rbsp_buf[G.rbsp_len++] = G.buf[G.cur];
    for (size_t i = G.cur + 1; i < end; ++i)
    {
        const uint8_t b = G.buf[i];

        // skip third byte of 0x00 0x00 0x03
        if (b == 0x00)
        {
            ++z;
        }
        else
        {
            bool skip = b == 0x03 && z == 2;
            z = 0;
            if (skip)
            {
                continue;
            }
        }

        G.rbsp_buf[G.rbsp_len++] = b;
    }
    ASSERT(G.rbsp_len > 0);

    // Trim trailing bits (ref: §7.3.2.11)
#if 0
    const uint8_t tail = G.rbsp_buf[G.rbsp_len - 1];
    int stop_bit_pos = -1;
    for (int i = 7; i >= 0; --i)
    {
        if (tail & (1 << i))
        {
            stop_bit_pos = i;
            break;
        }
    }
    ASSERT(stop_bit_pos >= 0 && stop_bit_pos < 8);
    G.rbsp_buf[G.rbsp_len - 1] &= ~((1 << stop_bit_pos) - 1);
#endif

    G.cur = end;

    return true;
}

// Read bits from RBSP stream
static uint32_t ReadRbspBits(size_t n)
{
    // @@ check overrun
    uint32_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const uint8_t bit = (G.rbsp_buf[G.rbsp_cbyte] >> (7 - G.rbsp_cbit)) & 0x01;
        if (++G.rbsp_cbit >= 8)
        {
            ++G.rbsp_cbyte;
            G.rbsp_cbit = 0;
        }
        result <<= 1;
        result |= bit;
    }
    return result;
}

// Read bits from RBSP stream without advancing the cursor
static uint32_t PeekRbspBits(size_t n)
{
    const size_t cbyte = G.rbsp_cbyte;
    const size_t cbit = G.rbsp_cbit;
    const uint32_t r = ReadRbspBits(n);
    G.rbsp_cbyte = cbyte;
    G.rbsp_cbit = cbit;
    return r;
}

// Seek forward in RBSP stream
static void SkipRbspBits(size_t n)
{
    // @@ check overrun
    G.rbsp_cbit += n;
    G.rbsp_cbyte += G.rbsp_cbit / 8;
    G.rbsp_cbit %= 8;
}

// Read bits (Exponential-Golomb) from RBSP stream (ref: §9.1)
static uint32_t ReadRbspBitsEG(void)
{
    // @@ check overrun
    uint32_t z = 0;
    while (PeekRbspBits(1) == 0)
    {
        ++z;
        SkipRbspBits(1);
    }

    SkipRbspBits(1);
    if (z == 0)
    {
        return 0;
    }

    const uint32_t suffix = ReadRbspBits(z);
    return (1u << z) - 1u + suffix;
}

static void SpewNalParam(const char* name, uint32_t val)
{
    printf("  %-30s %d\n", name, val);
}
#define SPEW_NAL_PARAM(x) SpewNalParam(#x, x)

int main(int argc, const char* argv[])
{
    (void)argc; (void)argv;

    // To keep things simple, just load the entire file into memory.
    // Bad Apple is only about 16MB
    {
        FILE* f = fopen("test.264", "rb");
        ASSERT(f && "Test file not found");
        fseek(f, 0, SEEK_END);
        G.len = ftell(f);
        fseek(f, 0, SEEK_SET);
        G.buf = malloc(G.len); ASSERT(G.buf);
        fread((void*)G.buf, G.len, 1, f);
        fclose(f);
    }

    for (int i = 0; i < 3; ++i)
    {
        if (!FindNalUnit())
        {
            ASSERT(0);
        }
        // Reset RBSP bit stream
        G.rbsp_cbyte = 0;
        G.rbsp_cbit = 0;

        // Read NAL unit header (ref: §7.3.1)
        if (ReadRbspBits(1) != 0)
        {
            ASSERT(0 && "Got non-zero first bit of NAL header. Something is wrong.");
        }
        SkipRbspBits(2); // nal_ref_idc
        const uint32_t nal_type = ReadRbspBits(5);

        // ref: Table 7-1
        switch (nal_type)
        {
        case 5:
        {
            // ref: §7.3.2.8
            printf("NAL UNIT(IDR slice)\n");
            // Slice header (ref: §7.3.3)
            const uint32_t first_mb_in_slice = ReadRbspBitsEG();
            const uint32_t slice_type = ReadRbspBitsEG();
            const uint32_t pic_parameter_set_id = ReadRbspBitsEG();
            // colour_plane_id not present (?)
            const uint32_t frame_num = ReadRbspBits(2);
            SPEW_NAL_PARAM(first_mb_in_slice);
            SPEW_NAL_PARAM(slice_type);
            SPEW_NAL_PARAM(pic_parameter_set_id);
            SPEW_NAL_PARAM(frame_num);

            break;
        }
        case 7:
        {
            // ref: §7.3.2.1
            printf("NAL UNIT(Sequence parameter set)\n");
            const uint32_t profile_idc = ReadRbspBits(8);
            SkipRbspBits(8); // constraint_setX_flag
            const uint32_t level_idc = ReadRbspBits(8);
            const uint32_t seq_parameter_set_id = ReadRbspBitsEG();
            const uint32_t log2_max_frame_num_minus4 = ReadRbspBitsEG();
            const uint32_t pic_order_cnt_type = ReadRbspBitsEG();
            SPEW_NAL_PARAM(profile_idc);
            SPEW_NAL_PARAM(level_idc);
            SPEW_NAL_PARAM(seq_parameter_set_id);
            SPEW_NAL_PARAM(log2_max_frame_num_minus4);
            SPEW_NAL_PARAM(pic_order_cnt_type);
            ASSERT(profile_idc == 66);
            break;
        }
        case 8:
        {
            // ref: §7.3.2.2
            printf("NAL UNIT(Picture parameter set)\n");
            const uint32_t pic_parameter_set_id = ReadRbspBitsEG();
            const uint32_t seq_parameter_set_id = ReadRbspBitsEG();
            SPEW_NAL_PARAM(pic_parameter_set_id);
            SPEW_NAL_PARAM(seq_parameter_set_id);
            break;
        }
        default:
        {
            printf("NAL UNIT(%d)\n", nal_type);
            break;
        }  
        }
    }

    return EXIT_SUCCESS;
}
