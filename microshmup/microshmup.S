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

.global SYM(main)

SYM(main):
    stp         x29, x30, [sp, #-16]!
    mov         x29, sp

    mov         w0, #0x30       // SDL_INIT_VIDEO | SDL_INIT_AUDIO
    bl          SYM(SDL_Init)
    tst         x0, #0xFF
    b.eq        LBL(sdl_fail)

    LOADADDR    x0, s_microshmup
    mov         w1, #800
    mov         w2, #600
    mov         x3, #0
    bl          SYM(SDL_CreateWindow)
    cbz         x0, LBL(sdl_fail)

    LOADADDR    x9, g_wnd
    str         x0, [x9]

    LOADADDR    x1, s_software
    bl          SYM(SDL_CreateRenderer)
    cbz         x0, LBL(sdl_fail)

    LOADADDR    x9, g_rnd
    str         x0, [x9]

    // test: blit green
#if 1
LBL(loop):
    LOADADDR    x0, g_rnd
    ldr         x0, [x0]
    mov         w1, #0x00
    mov         w2, #0xFF
    mov         w3, #0x00
    mov         w4, #0xFF
    bl          SYM(SDL_SetRenderDrawColor)

    LOADADDR    x0, g_rnd
    ldr         x0, [x0]
    bl          SYM(SDL_RenderClear)

    LOADADDR    x0, g_rnd
    ldr         x0, [x0]
    bl          SYM(SDL_RenderPresent)

    LOADADDR    x0, g_evt
    bl          SYM(SDL_WaitEvent)

    LOADADDR    x0, g_evt
    ldr         w0, [x0, #0]    // SDL_Event.type
    cmp         w0, #0x100
    b.ne        LBL(loop)
#endif
    
    mov         w0, #0
    b           LBL(end)
LBL(sdl_fail):
    bl          SYM(SDL_GetError)

    mov         x2, x0
    mov         w0, #0x10
    LOADADDR    x1, s_microshmup_error
    LOADADDR    x9, g_wnd
    ldr         x3, [x9]
    bl          SYM(SDL_ShowSimpleMessageBox)

    mov         w0, #1
LBL(end):
    ldp         x29, x30, [sp], #16
    ret
    
