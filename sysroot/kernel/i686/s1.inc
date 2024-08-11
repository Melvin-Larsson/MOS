[BITS 16]
    %include "A20.asm"
    %include "s2loader.inc"
    %include "print.inc"
start:
    push dx

    mov si, startStr
    call print_str

    call enable_a20
    cmp al, 1
    jne a20_fail

    mov si, a20
    call print_str

    pop dx
    call loads2
    
    mov si, ls2
    call print_str

    jmp s2 

a20_fail:
    mov si, failedA20
    call print_str
s1_halt:
    jmp s1_halt

gdt:
    dd 0,0
    db 0xff, 0xff, 0, 0, 0, 10011010b, 11001111b, 0 ;code
    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0 ;data
gdt_end:
gdtinfo:
    dw gdt_end - gdt - 1
    dd gdt

a20: db ' a20 enbld ',0
ls2: db ' Ld s2 ', 0
startStr: db ' Start ', 0
failedA20 : db ' Fail a20 ',0

times 510-($-$$) db 0
    dw 0xAA55


