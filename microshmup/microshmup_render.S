// ================================================================================================
// Code
// ================================================================================================
SECTION_TEXT

// draw solid color rect
// w0: x (top-left)
// w1: y (top-left)
// w2: width
// w3: height
// w4: color
SYM(draw_rect):
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
LBL(draw_rect_y):
    mov         w11, #0         // x cursor
    mov         x13, x9         // row
LBL(draw_rect_x):
    str         w4, [x13]
    add         x13, x13, #4
    add         w11, w11, #1
    cmp         w11, w2
    b.lt        LBL(draw_rect_x)

    add         x9, x9, w10, uxtw
    add         w12, w12, #1
    cmp         w12, w3
    b.lt        LBL(draw_rect_y)

    ldp         x29, x30, [sp], #16
    ret

    
