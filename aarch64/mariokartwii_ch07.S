// ================================================================================================
// Code written while following chapter 7 of the MarioKart Wii ARM64 assembly tutorial:
// https://mariokartwii.com/arm64/ch7.html
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

.global SYM(main)

SYM(main):
    // Add
    mov x0, #1
    mov x1, #2
    add x2, x0, x1

    // Add immediate
    add x3, x2, #0x100

    // Subtract immediate
    mov x4, x3
    sub x4, x4, #0x100

    // Multiply
    mov x5, #5
    mul x5, x4, x5

    // Divide
    udiv x6, x5, x4
    mov x7, #-1
    sdiv x7, x6, x7

    neg x8, x7

    brk #0 // (lldb) register read

