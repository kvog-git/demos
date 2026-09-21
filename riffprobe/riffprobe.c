// ================================================================================================
// Basic RIFF/WAVE file traversal. Does not decode media data.
//
// ref: https://mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf
//
// Changelog:
//     9/20/2026: Initial release
//
// License:
//     SPDX-License-Identifier: 0BSD
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#  define ATTR_NORETURN       __attribute__((noreturn))
#  define ATTR_PRINTF(A1, A2) __attribute__((format(printf, A1, A2)))
#else
#  define ATTR_NORETURN
#  define ATTR_PRINTF(A1, A2)
#endif

ATTR_NORETURN
ATTR_PRINTF(1, 2)
static void die(const char *fmt, ...)
{
    va_list va; va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    exit(1);
}

typedef struct Chunk Chunk;
struct Chunk
{
    uint32_t id;
    uint32_t size;
};

// WAVE chunks
typedef struct WAVE_Fmt WAVE_Fmt;
struct WAVE_Fmt
{
    uint16_t fmt_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
};

static bool read_chunk(FILE *fp, Chunk *chunk)
{
    return fread(chunk, sizeof(Chunk), 1, fp) == 1;
}

static const char *unfourcc(uint32_t id)
{
    static char b[5] = { 0 };
    b[0] = (id)       & 0xFF;
    b[1] = (id >> 8)  & 0xFF;
    b[2] = (id >> 16) & 0xFF;
    b[3] = (id >> 24) & 0xFF;
    return b;
}

static bool match_id(uint32_t id, const char *fourcc)
{
    const char *id_fourcc = unfourcc(id);
    for (int i = 0; i < 4; ++i)
    {
        if (tolower(id_fourcc[i]) != tolower(fourcc[i]))
            return false;
    }
    return true;
}

static uint32_t pad_size(uint32_t size)
{
    if (size % 2 != 0)
        return size + 1;
    return size;
}

int main(int argc, const char **argv)
{
    if (argc < 2)
        die("no file");

    FILE* fp = fopen(argv[1], "rb");
    if (!fp)
        die("failed to open file: %s", strerror(errno));

    fseek(fp, 0, SEEK_END);
    size_t flen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    Chunk root;
    if (!read_chunk(fp, &root) || !match_id(root.id, "RIFF"))
        die("unsupported or malformed file");
    if (root.size + sizeof(Chunk) != flen)
        printf("file length mismatch\n");

    uint32_t riff_type = 0;
    fread(&riff_type, sizeof(riff_type), 1, fp);
    printf("type: %s\n", unfourcc(riff_type));

    Chunk chunk;
    while (read_chunk(fp, &chunk))
    {
        size_t next_pos = ftell(fp) + pad_size(chunk.size);
        printf("chunk: %s\n", unfourcc(chunk.id));
        if (match_id(riff_type, "WAVE") && match_id(chunk.id, "FMT "))
        {
            WAVE_Fmt fmt;
            fread(&fmt, sizeof(fmt), 1, fp);

            printf("    fmt category: %d\n", fmt.fmt_tag);
            printf("    channels:     %d\n", fmt.channels);
            printf("    sample rate:  %d\n", fmt.samples_per_sec);
        }
        if (match_id(riff_type, "WAVE") && match_id(chunk.id, "FACT")) {
            uint32_t file_size = 0;
            fread(&file_size, sizeof(file_size), 1, fp);

            printf("    file size: %d\n", file_size);
        }
        if (match_id(chunk.id, "LIST")) {
            uint32_t list_type = 0;
            fread(&list_type, sizeof(list_type), 1, fp);

            printf("    list type: %s\n", unfourcc(list_type));

            size_t list_start = ftell(fp);
            size_t list_end   = list_start + chunk.size - sizeof(list_type);

            Chunk subchunk;
            while ((size_t)ftell(fp) < list_end && read_chunk(fp, &subchunk))
            {
                size_t next_sub_pos = ftell(fp) + pad_size(subchunk.size);
                printf("        subchunk: %s\n", unfourcc(subchunk.id));

                if (match_id(subchunk.id, "IART") ||
                    match_id(subchunk.id, "IGNR") ||
                    match_id(subchunk.id, "IPRD") ||
                    match_id(subchunk.id, "ISFT"))
                {
                    char buf[1024] = { 0 };
                    size_t len = sizeof(buf) - 1;
                    if (subchunk.size < len)
                        len = subchunk.size;
                    fread(buf, len, 1, fp);
                    printf("        data:     %s\n", buf);
                }

                fseek(fp, next_sub_pos, SEEK_SET);
            }
        }
        fseek(fp, next_pos, SEEK_SET);
    }

    fclose(fp);

    return 0;
}
