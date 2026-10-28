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

#include "../_/arm64/common.S"

TEXT_SEGMENT

// example 1

.global SYM(test_sum10i32)

SYM(test_sum10i32):
    // [w0, w7] = [a, h]

#ifdef __APPLE__
    .equ STK_i, 0
    .equ STK_j, 4
#else
    .equ STK_i, 0
    .equ STK_j, 8
#endif

    ldr w8, [sp, STK_i]
    ldr w9, [sp, STK_j]

#if 0
    add w0, w0, w1
    add w0, w0, w2
    add w0, w0, w3
    add w0, w0, w4
    add w0, w0, w5
    add w0, w0, w6
    add w0, w0, w7
    add w0, w0, w8
    add w0, w0, w9
#else
    // better dependency chain:
    add w0, w0, w1
    add w2, w2, w3
    add w4, w4, w5
    add w6, w6, w7
    add w8, w8, w9

    add w0, w0, w2
    add w4, w4, w6

    add w0, w0, w4
    add w0, w0, w8
#endif

    ret

// example 2

.global SYM(test_sum4i32)

SYM(test_sum4i32):
    // w0: i1
    // w1: i2
    // w2: i3
    // w3: i4

    sub sp, sp, #32             // begin stack frame
    stp x29, x30, [sp, #0]      // save fp/lr
    mov x29, sp                 // set frame pointer
    stp x19, x20, [sp, #16]     // save x19/x20

    mov w19, w2                 // w19 = i3
    mov w20, w3                 // w20 = i4

    bl SYM(c_sum2i32)           // w0 = i1 + i2

    mov w3, w0                  // w3 = w0=(i1 + i2)
    mov w0, w19                 // w0 = i3
    mov w1, w20                 // w1 = i4
    mov w19, w3
    bl SYM(c_sum2i32)           // w0 = i3 + i4

    mov w1, w19                 // w1 = i1 + i2
    bl SYM(c_sum2i32)           // w0 = i3 + i4 + i1 + i2

    ldp x19, x20, [sp, #16]     // restore x19/x20
    ldp x29, x30, [sp, #0]      // restore fp/lr
    add sp, sp, #32             // end stack frame

    ret

