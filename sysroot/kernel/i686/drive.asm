[BITS 16]
    jmp start
    %include "s2loader.asm"
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

    jmp main
    %include "print.inc"
main:
    xor ax,ax 
    mov ss, ax
    mov sp, 0x9c00

   call enter_protected

halt:
    jmp halt

enter_protected:
    call enable_a20
    call check_a20
    cmp al, 1
    je enter_protected_if_a20_activated
    mov si, err_a20_not_activated
    call print_str
    jmp halt

enter_protected_if_a20_activated:
    cli
    call disable_nmi
    lgdt [gdtinfo]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode_main 

disable_nmi:
    in al, 0x70
    or al, 1<<7
    out 0x70, al
    in al, 0x71
    ret
enable_nmi:
    in al, 0x70
    and al, ~(1<<7)
    out 0x70, al
    in al, 0x71


err_a20_not_activated: db 'Error: A20 is not activated',0 
sucess: db 'Success! Entered protected mode',0
debug: db 'Debug',0

[BITS 32]
pmode_main:
    jmp 0x08:pmode_main

    xor ax,ax
    mov ds,ax


