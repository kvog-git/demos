#include "microshmup_core.S"
#include "microshmup_data.S"
#include "microshmup_render.S"

// ================================================================================================
// Mutable data
// ================================================================================================
SECTION_BSS

g_evt: .zero 128        // SDL_Event
g_rnd: .zero 8          // SDL_Renderer *
g_wnd: .zero 8          // SDL_Window *

// ================================================================================================
// Code
// ================================================================================================
SECTION_TEXT

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
