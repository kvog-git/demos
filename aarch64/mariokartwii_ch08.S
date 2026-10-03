// ================================================================================================
// Code written while following chapter 8 of the MarioKart Wii ARM64 assembly tutorial:
// https://mariokartwii.com/arm64/ch8.html
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

#include "../_common/aarch64/common.S"

TEXT_SEGMENT

.global SYM(main)

SYM(main):
    // move shift
    mov x0, #0xFFFFFFFFFFFFFFFF
    lsl x0, x0, #32

    mov x1, #0xFFFFFFFFFFFFFFFF
    movk x1, #0x1234, lsl 32

    mov x2, #0xFFFFFFFFFFFFFFFF
    movz x2, #0x1234, lsl 32

    mov x3, #0xFFFFFFFFFFFFFFFF
    movk x3, #0x1111, lsl 48
    movk x3, #0x2222, lsl 32
    movk x3, #0x3333, lsl 16
    movk x3, #0x4444

    brk #0 // (lldb) register read
