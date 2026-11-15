// ================================================================================================
// Macros
// ================================================================================================

#ifdef __APPLE__
#   define LBL(name) L ## name
#   define SYM(name) _##name
#   define SECTION_BSS    .section __DATA,__bss
#   define SECTION_RODATA .section __TEXT,__const
#   define SECTION_TEXT   .section __TEXT,__text
#else
#   define LBL(name) .L ## name
#   define SYM(name) name
#   define SECTION_BSS    .section .bss
#   define SECTION_RODATA .section .rodata
#   define SECTION_TEXT   .section .text
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


