// ================================================================================================
// Constants
// ================================================================================================

.equ VP_W, 800
.equ VP_H, 600

// ================================================================================================
// Read-only data
// ================================================================================================
#ifdef __APPLE__
    .section __TEXT,__const
#else
    .section .rodata
#endif

s_microshmup:           .asciz "microshmup"
s_microshmup_error:     .asciz "microshmup - error"
s_software:             .asciz "software"

// ================================================================================================
// Mutable data
// ================================================================================================
#ifdef __APPLE__
    .section __DATA,__bss
#else
    .section .bss
#endif
    .p2align 3

g_evt: .zero 128        // SDL_Event
g_rnd: .zero 8          // SDL_Renderer *
g_wnd: .zero 8          // SDL_Window *

g_fbufmap: .zero 8      // frame buffer data
g_fstride: .zero 4      // frame buffer stride

// ================================================================================================
// Code
// ================================================================================================

#ifdef __APPLE__
    .section __TEXT,__text
#else
    .section .text
#endif

#ifdef __APPLE__
#   define LBL(name) L ## name
#   define SYM(name) _##name
#else
#   define LBL(name) .L ## name
#   define SYM(name) name
#endif

.macro LOADADDR reg, sym
#ifdef __APPLE__
    adrp        \reg, \sym\()@PAGE
    add         \reg, \reg, \sym\()@PAGEOFF
#else
    adrp        \reg, \sym
    add         \reg, \reg, :lo12:\sym
#endif
.endm

// ================================================================================================
// Code
// ================================================================================================

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

.global SYM(main)
// entry point
SYM(main):
    stp         x29, x30, [sp, #-48]!
    mov         x29, sp
    stp         x19, x20, [sp, #16]
    stp         x21, x22, [sp, #32]
    mov         x19, #0         // x19: SDL_Window *
    mov         x20, #0         // x20: SDL_Renderer *
    mov         x21, #0         // x21: SDL_Texture *
    mov         x22, #0         // x22: spare

    mov         w0, #0x30       // SDL_INIT_VIDEO | SDL_INIT_AUDIO
    bl          SYM(SDL_Init)
    tst         x0, #0xFF
    b.eq        LBL(sdl_fail)

    LOADADDR    x0, s_microshmup
    mov         w1, VP_W
    mov         w2, VP_H
    mov         x3, #0
    bl          SYM(SDL_CreateWindow)
    cbz         x0, LBL(sdl_fail)

    mov         x19, x0
    LOADADDR    x9, g_wnd
    str         x0, [x9]

    LOADADDR    x1, s_software
    bl          SYM(SDL_CreateRenderer)
    cbz         x0, LBL(sdl_fail)

    mov         x20, x0
    LOADADDR    x9, g_rnd
    str         x0, [x9]

    mov         x0, x20
    mov         w1, #0x2004             // SDL_PIXELFORMAT_ARGB8888
    movk        w1, #0x1636, lsl 16
    mov         w2, #1                  // SDL_TEXTUREACCESS_STREAMING
    mov         w3, VP_W
    mov         w4, VP_H
    bl          SYM(SDL_CreateTexture)
    cbz         x0, LBL(sdl_fail)

    mov         x21, x0

// main loop start
LBL(main_loop):
    LOADADDR    x0, g_evt
    bl          SYM(SDL_PollEvent)
    tst         x0, #0xFF
    b.eq        LBL(main_loop_render)

    LOADADDR    x0, g_evt
    ldr         w0, [x0, #0]            // SDL_Event.type
    cmp         w0, #0x100
    b.eq        LBL(main_loop_done)
    b           LBL(main_loop)

// main loop start
LBL(main_loop_render):
    mov         x0, x21
    mov         x1, #0
    LOADADDR    x2, g_fbufmap
    LOADADDR    x3, g_fstride
    bl          SYM(SDL_LockTexture)
    tst         x0, #0xFF
    b.eq        LBL(sdl_fail)

    // clear screen
    mov         w0, #0
    mov         w1, #0
    mov         w2, VP_W
    mov         w3, VP_H
    //                 AARRGGBB
    mov         w4, #0xFF000000
    bl          SYM(draw_rect)

    mov         x0, x21
    bl          SYM(SDL_UnlockTexture)

    mov         x0, x20
    mov         x1, x21
    mov         x2, #0
    mov         x3, #0
    bl          SYM(SDL_RenderTexture)
    tst         x0, #0xFF
    b.eq        LBL(sdl_fail)

    mov         x0, x20
    bl          SYM(SDL_RenderPresent)
    
    b           LBL(main_loop)
LBL(main_loop_done):
    mov         w0, #0
    b           LBL(end)
LBL(sdl_fail):
    bl          SYM(SDL_GetError)
    mov         x22, x0

    mov         x2, x0
    mov         w0, #0x10
    LOADADDR    x1, s_microshmup_error
    mov         x3, x19
    bl          SYM(SDL_ShowSimpleMessageBox)

    mov         x0, x22
    bl          SYM(puts)

    mov         w0, #1
LBL(end):
    ldp         x21, x22, [sp, #32]
    ldp         x19, x20, [sp, #16]
    ldp         x29, x30, [sp], #48
    ret
    
