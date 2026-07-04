// ================================================================================================
// Code written while reading chapter 11 of Modern Arm Assembly Language Programming
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

#include <stdint.h>
#include <stdio.h>

// example 1
int32_t i32_add_sub(int32_t a, int32_t b, int32_t c);
int64_t i64_add_sub(int64_t a, int64_t b, int64_t c);

// example 2
int32_t i32_mul(int32_t a, int32_t b);
int64_t i32_mul_safe(int32_t a, int32_t b);

// example 3
void i32_quo_rem(int32_t num, int32_t den, int32_t *quo, int32_t *rem);

// example 4
int32_t test_ldr1(int32_t i, int64_t j, int32_t *tab);

// example 5
void test_mov1(uint32_t *tab);

int main(int argc, const char* argv[])
{
    (void)argc; (void)argv;
#define X(FMT, CODE) printf(#CODE ": " FMT "\n", CODE)

    // example 1
    X("%d", i32_add_sub(5, 3, 1));
    X("%lld", i64_add_sub(5, 3, 1));

    // example 2
    X("%d", i32_mul(5, 6));
    X("%d", i32_mul(2000000000, 4));
    X("%lld", i32_mul_safe(5, 6));
    X("%lld", i32_mul_safe(2000000000, 4));

#undef X
#define X(CODE) CODE; printf(#CODE ": quo=%d rem=%d\n", quo, rem)

    // example 3
    int32_t quo = 0;
    int32_t rem = 0;
    X(i32_quo_rem(6, 3, &quo, &rem));
    X(i32_quo_rem(7, 3, &quo, &rem));

#undef X
#define X(FMT, CODE) printf(#CODE ": " FMT "\n", CODE)

    // example 4
    int32_t tab[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    X("%d", test_ldr1(0, 0, tab));

    // example 5
    uint32_t out[3] = { 0, 0, 0 };
    test_mov1(out);
    printf("test_mov1(out):\n");
    printf("    out[0] = 0x%X\n", out[0]);
    printf("    out[1] = 0x%X\n", out[1]);
    printf("    out[2] = 0x%X\n", out[2]);
}

