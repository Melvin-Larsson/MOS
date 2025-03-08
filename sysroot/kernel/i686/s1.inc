[BITS 16]
    %include "s2loader.inc"
    %include "print.inc"
start:
    push dx

    mov si, startStr
    call print_str

    pop dx
    call loads2
    
    mov si, ls2
    call print_str

    jmp s2 

gdt:
    dd 0,0
    db 0xff, 0xff, 0, 0, 0, 10011010b, 11001111b, 0 ;kernel code
    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0 ;kernel data
    db 0xff, 0xff, 0, 0, 0, 11111010b, 11011111b, 0 ;user code
    db 0xff, 0xff, 0, 0, 0, 11110010b, 11011111b, 0 ;user data
global tss
tss:
    db 108, 0x00, 0, 0, 0, 10001001b, 00000000b, 0 ;kernel_tss (placeholder address)
        
gdt_end:
gdtinfo:
    dw gdt_end - gdt - 1
    dd gdt

ls2: db 's2', 0
startStr: db 'St', 0

times 510-96-($-$$) db 0
    dw 0xAA55


