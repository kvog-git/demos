// ================================================================================================
// Basic RIFF/WAVE file traversal. Does not do any real parsing.
//
// ref: https://mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf
//
// Changelog:
//     9/20/2026: Initial release
//     9/22/2026: Can now accept multiple files
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

static const char *unfourcc(uint32_t id)
{
    static char b[5] = { 0 };
    b[0] = (id)       & 0xFF;
    b[1] = (id >> 8)  & 0xFF;
    b[2] = (id >> 16) & 0xFF;
    b[3] = (id >> 24) & 0xFF;
    return b;
}

int main(int argc, const char **argv)
{
    if (argc < 2)
        die("no file");

    for (int i = 1; i < argc; ++i)
    {
        printf("%s:\n", argv[i]);

        FILE* fp = fopen(argv[i], "rb");
        if (!fp)
            die("failed to open file: %s", strerror(errno));

        uint8_t buf[256];

        if (fread(buf, 1, 12, fp) != 12)
            return 1;

        if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4))
            die("Invalid file");

        while (!feof(fp))
        {
            if (fread(buf, 1, 8, fp) != 8)
                break;
            uint32_t tag  = *(uint32_t *)buf;
            uint32_t size = *(uint32_t *)(buf + 4);
            printf("    %s\n", unfourcc(tag));
            fseek(fp, size, SEEK_CUR);
        }
    }

    return 0;
}
