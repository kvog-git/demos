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

#include "../_common/aarch64/common.S"

TEXT_SEGMENT

// example 1

.global SYM(i32_add_sub)
.global SYM(i64_add_sub)

SYM(i32_add_sub):
    // w0: a
    // w1: b
    // w2: c
#if 0
    add w3, w0, w1
    sub w0, w3, w2
#else
    add w0, w0, w1
    sub w0, w0, w2
#endif
    // w0: return
    ret

SYM(i64_add_sub):
    // x0: a
    // x1: b
    // x2: c
    add x3, x0, x1
    sub x0, x3, x2
    // x0: return
    ret

// example 2

.global SYM(i32_mul)
.global SYM(i32_mul_safe)

SYM(i32_mul):
    // w0: a
    // w1: b
    mul w0, w0, w1
    // w0: return
    ret

SYM(i32_mul_safe):
    // w0: a
    // w1: b
    smull x0, w0, w1
    // x0: ret
    ret

// example 3

.global SYM(i32_quo_rem)

SYM(i32_quo_rem):
    // w0: num
    // w1: den
    // x2: *quo
    // x3: *den

    // calculate quotient
    sdiv w4, w0, w1
    str w4, [x2]

    // calculate remainder
    mul w5, w1, w4
    sub w6, w0, w5
    str w6, [x3]

    ret

// example 4

.global SYM(test_ldr1)

SYM(test_ldr1):
    // w0: i
    // x1: j
    // x2: tab

    ldr w3, [x2, w0, uxtw #2]   // tab[i] (32-bit index)
    ldr w4, [x2, x1, lsl  #2]   // tab[j] (64-bit index)

    add w0, w3, w4

    ret

// example 5

.global SYM(test_mov1)

SYM(test_mov1):
    // x0: *out

    .equ BIGNUM,0xABCD1234
    .equ BIGNUM_LO16,((BIGNUM & 0x0000FFFF) >>  0)
    .equ BIGNUM_HI16,((BIGNUM & 0xFFFF0000) >> 16)

    mov  w2, #BIGNUM_LO16
    movk w2, #BIGNUM_HI16, lsl 16
    str  w2, [x0]                       // out[0] = w2

    movz w3, #BIGNUM_LO16, lsl 16
    str  w3, [x0, #4]                   // out[1] = w3

    movz w4, 0x1234
    str  w4, [x0, #8]                   // out[2] = w4

    ret

