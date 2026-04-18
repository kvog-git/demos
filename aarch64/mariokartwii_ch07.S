// https://mariokartwii.com/arm64/ch7.html

.section __TEXT,__text
.global _main

_main:
    // Add
    mov x0, #1
    mov x1, #2
    add x2, x0, x1

    // Add immediate
    add x3, x2, #0x100

    // Subtract immediate
    mov x4, x3
    sub x4, x4, #0x100

    // Multiply
    mov x5, #5
    mul x5, x4, x5

    // Divide
    udiv x6, x5, x4
    mov x7, #-1
    sdiv x7, x6, x7

    neg x8, x7

    brk #0 // (lldb) register read
