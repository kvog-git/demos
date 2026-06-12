// ================================================================================================
// Basic JPEG decoder. Does no bounds checking on inputs.
//
// ref: https://en.wikipedia.org/wiki/JPEG#Syntax_and_structure
// ref: ITU-T T.81 (1992) — ISO/IEC 10918-1:1994
//
// Test files:
//     $ ffmpeg -f lavfi -i testsrc=size=800x600:rate=1 -frames:v 1 test.jpg
//
// Changelog:
//     ??/??/2026: Initial release
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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Marker codes - Table B.1
enum
{
    MC_SOF0 = 0xC0,
    MC_SOF1 = 0xC1,
    MC_SOF2 = 0xC2,
    MC_SOF3 = 0xC3,
    MC_SOF5 = 0xC5,
    MC_SOF6 = 0xC6,
    MC_SOF7 = 0xC7,
    MC_SOF8 = 0xC8,
    MC_SOF9 = 0xC9,
    MC_SOFA = 0xCA,
    MC_SOFB = 0xCB,
    MC_SOFC = 0xCC,
    MC_SOFD = 0xCD,
    MC_SOFE = 0xCE,
    MC_DHT  = 0xC4,
    MC_DAC  = 0xCC,
    MC_RST0 = 0xD0,
    MC_RST1 = 0xD1,
    MC_RST2 = 0xD2,
    MC_RST3 = 0xD3,
    MC_RST4 = 0xD4,
    MC_RST5 = 0xD5,
    MC_RST6 = 0xD6,
    MC_RST7 = 0xD7,
    MC_SOI  = 0xD8,
    MC_EOI  = 0xD9,
    MC_SOS  = 0xDA,
    MC_DQT  = 0xDB,
    MC_DNL  = 0xDC,
    MC_DRI  = 0xFF,
    MC_DHP  = 0xDE,
    MC_EXP  = 0xDF,
    MC_APP0 = 0xE0,
    MC_APP1 = 0xE1,
    MC_APP2 = 0xE2,
    MC_APP3 = 0xE3,
    MC_APP4 = 0xE4,
    MC_APP5 = 0xE5,
    MC_APP6 = 0xE6,
    MC_APP7 = 0xE7,
    MC_APP8 = 0xE8,
    MC_APP9 = 0xE9,
    MC_APPA = 0xEA,
    MC_APPB = 0xEB,
    MC_APPC = 0xEC,
    MC_APPD = 0xED,
    MC_APPE = 0xEE,
    MC_APPF = 0xEF,
    MC_COM  = 0xFE,
};

int main(int argc, const char **argv)
{
    uint8_t *bbuf = 0;
    size_t   blen = 0;
    {
        if (argc != 2) {
            printf("Supply a file\n");
            return 0;
        }
        FILE *fp = fopen(argv[1], "rb");
        if (!fp) {
            perror("Couldn't open file");
        }

        fseek(fp, 0, SEEK_END);
        blen = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        bbuf = malloc(blen);
        fread(bbuf, 1, blen, fp);

        fclose(fp);
    }

    const uint8_t *pbuf = bbuf;

    // File should start with SOI
    assert(pbuf[0] == 0xFF && pbuf[1] == MC_SOI && "not a jpeg");
    pbuf += 2;

    #define READ_LEN() ((pbuf[0] << 8) | pbuf[1])

    while (true) {
        uint8_t mc0 = pbuf[0];
        uint8_t mc1 = pbuf[1];
        pbuf += 2;
        assert(mc0 == 0xFF);
        switch (mc1) {
            case MC_APP0:
            case MC_APP1:
            case MC_APP2:
            case MC_APP3:
            case MC_APP4:
            case MC_APP5:
            case MC_APP6:
            case MC_APP7:
            case MC_APP8:
            case MC_APP9:
            case MC_APPA:
            case MC_APPB:
            case MC_APPC:
            case MC_APPD:
            case MC_APPE:
            case MC_APPF: {
                printf("MC_APP%d\n", mc1 & 0xF);
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_COM: {
                printf("MC_COM\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_DHT: {
                printf("MC_DHT\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_DQT: {
                printf("MC_DQT\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_SOF0: {
                printf("MC_SOF0 (Baseline DCT)\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_SOF2: {
                printf("MC_SOF2 (Progressive DCT)\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            case MC_SOS: {
                // Section B.2.3
                printf("MC_SOS\n");
                uint16_t len = READ_LEN();
                pbuf += len;
            } break;
            default: {
                printf("Got unknown marker code 0x%X\n", mc1);
                exit(1);
            } break;
        }
    }
}

