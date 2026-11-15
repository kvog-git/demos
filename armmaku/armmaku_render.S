// ================================================================================================
// Code
// ================================================================================================
SECTION_TEXT

// draw a single character
// w0: x (top-left)
// w1: y (top-left)
// w2: color
// w3: ASCII character
SYM(r_char):
    stp         x29, x30, [sp, #-16]!
    mov         x29, sp

    LOADADDR    x9, g_fbufmap
    ldr         x9, [x9]

    LOADADDR    x10, g_fstride
    ldr         w10, [x10]

    LOADADDR    x11, g_font

    // x offset
    mov         w12, #4
    mul         w12, w12, w0
    add         x9, x9, w12, uxtw

    // y offset
    mul         w12, w10, w1
    add         x9, x9, w12, uxtw

    // glyph offset
    mov         w12, #14
    mul         w12, w12, w3
    add         x11, x11, w12, uxtw

    mov         w14, #0         // y cursor
LBL(r_char_yloop):
    mov         w13, #0         // x cursor
    mov         x15, x9         // dst row
    ldrb        w16, [x11]      // src row
LBL(r_char_xloop):
    tst         w16, #0x80
    b.eq        1f
    str         w2, [x15]
1:
    lsl         w16, w16, #1
    add         x15, x15, #4
    add         w13, w13, #1
    cmp         w13, #8
    b.lt        LBL(r_char_xloop)

    add         x9, x9, w10, uxtw
    add         w14, w14, #1
    add         x11, x11, #1
    cmp         w14, #14
    b.lt        LBL(r_char_yloop)

    ldp         x29, x30, [sp], #16
    ret

// draw a string of characters
// w0: x (top-left)
// w1: y (top-left)
// w2: color
// x3: ASCII string
SYM(r_chars):
    stp         x29, x30, [sp, #-16]!
    mov         x29, sp

    mov         x5, x3
LBL(r_chars_loop):
    ldrb        w3, [x5]
    cmp         w3, #0
    b.eq        1f
    bl          SYM(r_char)
    add         x5, x5, #1
    add         w0, w0, #8
    b           LBL(r_chars_loop)
1:

    ldp         x29, x30, [sp], #16
    ret

// draw solid color rect
// w0: x (top-left)
// w1: y (top-left)
// w2: width
// w3: height
// w4: color
SYM(r_quad):
    stp         x29, x30, [sp, #-16]!
    mov         x29, sp

    LOADADDR    x9, g_fbufmap
    ldr         x9, [x9]

    LOADADDR    x10, g_fstride
    ldr         w10, [x10]

    // x offset
    mov         w11, #4
    mul         w11, w11, w0
    add         x9, x9, w11, uxtw

    // y offset
    mul         w11, w10, w1
    add         x9, x9, w11, uxtw

    mov         w12, #0         // y cursor
LBL(r_quad_yloop):
    mov         w11, #0         // x cursor
    mov         x13, x9         // row
LBL(r_quad_xloop):
    str         w4, [x13]
    add         x13, x13, #4
    add         w11, w11, #1
    cmp         w11, w2
    b.lt        LBL(r_quad_xloop)

    add         x9, x9, w10, uxtw
    add         w12, w12, #1
    cmp         w12, w3
    b.lt        LBL(r_quad_yloop)

    ldp         x29, x30, [sp], #16
    ret

