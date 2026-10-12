// ================================================================================================
// Read-only data
// ================================================================================================
#ifdef __APPLE__
    .section __TEXT,__const
#else
    .section .rodata
#endif

s_microshmup: .asciz "microshmup"
s_microshmup_error: .asciz "microshmup - error"

// ================================================================================================
// Mutable data
// ================================================================================================
#ifdef __APPLE__
    .section __DATA,__bss
#else
    .section .bss
#endif
    .p2align 3

g_rnd: .zero 8  // SDL_Renderer *
g_wnd: .zero 8  // SDL_Window *

// ================================================================================================
// Code
// ================================================================================================

#ifdef __APPLE__
    .section __TEXT,__text
#else
    .section .text
#endif

#ifdef __APPLE__
#   define SYM(name) _##name
#else
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
    tst         w0, #0xFF
    b.eq        sdl_fail

    LOADADDR    x0, s_microshmup
    mov         w1, #800
    mov         w2, #600
    mov         x3, #0
    LOADADDR    x4, g_wnd
    LOADADDR    x5, g_rnd
    bl          SYM(SDL_CreateWindowAndRenderer)
    tst         w0, #0xFF
    b.eq        sdl_fail

    mov         w0, #0
    ldp         x29, x30, [sp], #16
    ret
sdl_fail:
    bl          SYM(SDL_GetError)

    mov         x2, x0
    mov         w0, #0x10
    LOADADDR    x1, s_microshmup_error
    LOADADDR    x9, g_wnd
    ldr         x3, [x9]
    bl          SYM(SDL_ShowSimpleMessageBox)

    mov         w0, #1
    ldp         x29, x30, [sp], #16
    ret
    
