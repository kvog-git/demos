// https://mariokartwii.com/arm64/ch10.html

.section __TEXT,__text
.global _main

_main:
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
