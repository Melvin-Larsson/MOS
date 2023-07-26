[ORG 0x7c00]
[BITS 16]
    jmp start
    %include "s2loader.inc"
start:
    call loads2
    jmp 0x7e00

gdt:
    dd 0,0
    db 0xff, 0xff, 0, 0, 0, 10011010b, 11001111b, 0 ;code
    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0 ;data
gdt_end:
gdtinfo:
    dw gdt_end - gdt - 1
    dd gdt

times 510-($-$$) db 0
    dw 0xAA55


