// ================================================================================================
// Code written while reading chapter 12 of Modern Arm Assembly Language Programming
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

#include <stdint.h>
#include <stdio.h>

// example 1
int32_t test_sum10i32(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g,
                      int32_t h, int32_t i, int32_t j);

// example 2
int32_t test_sum4i32(int32_t i1, int32_t i2, int32_t i3, int32_t i4);

int32_t c_sum2i32(int32_t i1, int32_t i2)
{
    // make this function complicated
    printf("c_sum2i32(i1=%d, i2=%d)\n", i1, i2);
    return i1 + i2;
}

int main(int argc, const char* argv[])
{
    (void)argc; (void)argv;
#define X(FMT, CODE) printf(#CODE ": " FMT "\n", CODE)

    // example 1
    X("%d", test_sum10i32(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

    // example 2
    X("%d", test_sum4i32(1, 2, 3, 4));
}

