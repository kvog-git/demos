// ================================================================================================
// Simple baseline sequential JPEG decoder. Does no bounds checking on inputs.
//
// ref: https://en.wikipedia.org/wiki/JPEG#Syntax_and_structure
// ref: ITU-T T.81 (1992) — ISO/IEC 10918-1:1994
// ref: https://github.com/richgel999/picojpeg/blob/master/picojpeg.c
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
#include <string.h>

#include <SDL3/SDL.h>

// Table B.1 - Marker code assignments
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
    MC_DRI  = 0xDD,
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

    // Find required image segments: SOF0, DHT, DQT, SOS
    bool got_sof0 = false;
    bool got_dht  = false;
    bool got_dqt  = false;
    bool got_sos  = false;

    // DHT data
    typedef struct HT HT;
    struct HT
    {
        uint8_t        l[16]; // Frequency of each Huffman code length
        size_t         v_len; // Length of huffman code list
        const uint8_t *v;     // Huffman code list
    };
    HT ht_ac[2];
    HT ht_dc[2];

    // DQT data
    uint8_t dqt_q[64]; // Quantization table (assumes only 1 table in file)

    // SOF data
    typedef struct Channel Channel;
    struct Channel
    {
        uint8_t h;   // Horizontal sampling factor
        uint8_t v;   // Vertical sampling factor
        uint8_t tqi; // Quantization table selector
    };
    Channel sof_ch[3];
    uint16_t sof_x;
    uint16_t sof_y;

    // Sanity check, image should start with SOI
    assert(pbuf[0] == 0xFF && pbuf[1] == MC_SOI && "not a jpeg");

    // B.1.1.3: Markers are 0xFF followed by [0x01, 0xFE]
    while (true) {
        const uint8_t marker = pbuf[1];
        printf("Marker code: FF%X\n", pbuf[1]);

        // Don't overrun the buffer when parsing good files, at least
        if (marker == MC_EOI) {
            break;
        }

        // Segment payload to skip while scanning for next marker
        size_t skip = 2;

        // Everything other than SOI and EOI has a payload. EOI doesn't reach here.
        const uint8_t *sbuf = 0;
        size_t         slen = 0;
        if (marker != MC_SOI) {
            sbuf = pbuf + 2;
            slen = pbuf[2] << 8 | pbuf[3];
            skip += slen;
        }

        // Segment-specific parsing
        switch (marker) {
            case MC_COM: {
                printf("COM:\n");
                printf("    %.*s\n", (int)(skip - 4), (const char *)&pbuf[4]);
            } break;
            case MC_DHT: {
                got_dht = true;

                printf("DHT:\n");

                // B.2.4.2
                size_t pos = 2;  // skip Lh
                while (pos < slen) {
                    const uint8_t tc = sbuf[pos] >> 4;
                    const uint8_t th = sbuf[pos] & 0xF;
                    ++pos;

                    assert(th < 2);

                    HT *ht = (tc == 0) ? &ht_dc[th] : &ht_ac[th];

                    // Read huffman code frequencies
                    memcpy(ht->l, sbuf + pos, 16);
                    pos += 16;

                    // Sum all huffman code frequencies
                    ht->v_len = 0;
                    for (size_t i = 0; i < 16; ++i) {
                        ht->v_len += ht->l[i];
                    }

                    // Read huffman codes
                    ht->v = sbuf + pos;
                    pos += ht->v_len;

                    printf("    Table:\n");
                    printf("        Tc: %d\n", tc);
                    printf("        Th: %d\n", th);
                    printf("         L: ");
                    for (size_t i = 0; i < 16; ++i) {
                        printf("%d ", ht->l[i]);
                    }
                    printf("\n");
                    printf("         V:\n            ");
                    for (size_t i = 0; i < ht->v_len; ++i) {
                        printf("%3d ", ht->v[i]);
                        if (i % 8 == 7) {
                            printf("\n            ");
                        }
                    }
                    printf("\n");
                }
            } break;
            case MC_DQT: {
                got_dqt = true;

                // B.2.4.1
                const uint8_t pq = sbuf[2] >> 4;
                const uint8_t tq = sbuf[2] & 0xF;
                assert(pq == 0 && "DQT.Pq must be 0 in baseline configuration");
                assert(tq == 0 && "FIXME only 1 quantization table supported");
                memcpy(dqt_q, sbuf + 3, sizeof(dqt_q));
        
                printf("DQT:\n");
                printf("    Pq: %d\n", pq);
                printf("    Tq: %d\n", tq);
                printf("     Q:\n");
                for (size_t i = 0; i < 64; ++i) {
                    if (i % 8 == 0) { printf("        "); } 
                    printf("%2X ", dqt_q[i]);
                    if (i % 8 == 7) { printf("\n"); }
                }
            } break;
            case MC_SOF0: {
                got_sof0 = true;

                // B.2.2
                const uint8_t  p  = sbuf[2];
                sof_y = sbuf[3] << 8 | sbuf[4];
                sof_x = sbuf[5] << 8 | sbuf[6];
                const uint8_t  nf = sbuf[7];

                assert(p  == 8 && "SOF0.P must be 8 in baseline configuration");
                assert(nf == 3 && "only 3-channel supported");

                size_t pos = 8;
                for (uint8_t i = 0; i < nf; ++i) {
                    const uint8_t ci = sbuf[pos] - 1;
                    assert(ci < 3);
                    pos += 1;
                    sof_ch[ci].h = sbuf[pos] >> 4;
                    sof_ch[ci].v = sbuf[pos] & 0xF;
                    pos += 1;
                    sof_ch[ci].tqi = sbuf[pos];
                    pos += 1;
                }

                printf("SOF:\n");
                printf("     P: %d\n", p);
                printf("     Y: %d\n", sof_y);
                printf("     X: %d\n", sof_x);
                printf("    Nf: %d\n", nf);
                printf("    Ch:\n");
                for (uint8_t i = 0; i < nf; ++i) {
                    Channel *ch = &sof_ch[i];
                    printf("        h = %d v = %d tqi = %d\n", ch->h, ch->v, ch->tqi);
                }

            } break;
            case MC_SOS: {
                got_sos = true;

                // B.2.3
                const uint8_t ns = sbuf[2];

                assert(ns == 3 && "expected 3 planes");

                size_t pos = 3;
                for (uint8_t i = 0; i < ns; ++i) {
                    uint8_t csj = sbuf[pos];
                    pos += 1;
                    uint8_t tdj = sbuf[pos] >> 4;
                    uint8_t taj = sbuf[pos] & 0xF;
                    pos += 1;
                    printf("csj: %d\n", csj);
                    printf("tdj: %d\n", tdj);
                    printf("taj: %d\n", taj);
                    //break;
                }

                uint8_t ss = sbuf[pos];
                pos += 1;

                uint8_t se = sbuf[pos];
                pos += 1;

                uint8_t ah = sbuf[pos] >> 4;
                uint8_t al = sbuf[pos] & 0xF;
                pos += 1;

                assert(ss == 0 && se == 63 && ah == 0 && al == 0 && "not baseline sequential");

                // @@ increment skip so next marker scan doesn't find halfway through the entropy
                // data on a restart marker
                printf("SOS:\n");
                printf("    Ns: %d\n", ns);
                printf("    Ss: %d\n", ss);
                printf("    Se: %d\n", se);
                printf("    Ah: %d\n", ah);
                printf("    Al: %d\n", al);
            } break;
        };

        // Scan for next marker
        for (size_t i = skip; ; ++i) {
            if (pbuf[i] == 0xFF && (pbuf[i + 1] >= 0x01 && pbuf[i + 1] <= 0xFE)) {
                pbuf += i;
                break;
            }
        }
    }

    if (!got_sof0) {
        printf("Missing segment SOF0. Image is not sequential baseline\n");
        return 1;
    }
    if (!got_dht) {
        printf("Missing segment DHT\n");
        return 1;
    }
    if (!got_dqt) {
        printf("Missing segment DQT\n");
        return 1;
    }
    if (!got_sos) {
        printf("Missing segment SOS\n");
        return 1;
    }

    // ref: F.2

    // Build huffman decode trees

    return 0;

    SDL_Init(SDL_INIT_VIDEO);

#define SIDEBAR 250

    SDL_Window   *wnd;
    SDL_Renderer *r;
    if (!SDL_CreateWindowAndRenderer("jpegdec", sof_x + SIDEBAR, sof_y, 0, &wnd, &r)) {
        printf("error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STATIC, sof_x,
                                         sof_y);
    if (!tex) {
        printf("error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_UpdateYUVTexture(tex, 0, 0, 0, 0, 0, 0, 0);

    while (true) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
                case SDL_EVENT_QUIT: {
                    exit(0);
                } break;
            }
        }

        SDL_FRect dst = {
            .w = sof_x,
            .h = sof_y
        };
        SDL_RenderTexture(r, tex, 0, &dst);
        SDL_RenderPresent(r);
    }
}

