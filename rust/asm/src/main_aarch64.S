// ================================================================================================
// Rust->assembly interop demo
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

#include "../../../_common/aarch64/common.S"

TEXT_SEGMENT

.global SYM(add_u32)
.global SYM(dot_f32v3)

SYM(add_u32):
    add w0, w0, w1
    ret

SYM(dot_f32v3):
    ldp s1, s2, [x0]            // a.x, a.y
    ldp s3, s4, [x1]            // b.x, b.y
    ldr s5, [x0, #8]            // a.z
    ldr s6, [x1, #8]            // b.z

    fmul  s0, s1, s3            // a.x * b.x
    fmadd s0, s2, s4, s0        // + a.y * b.y
    fmadd s0, s5, s6, s0        // + a.z * b.z
    
    ret
