%include "A20.asm"
enter_protected:
    call enable_a20
    cmp al, 1
    je enter_protected_if_a20_activated
    mov si, err_a20_not_activated
    call print_str
    jmp enter_protected_halt

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

enter_protected_halt: jmp enter_protected_halt

err_a20_not_activated: db 'Error: A20 is not activated',0 
