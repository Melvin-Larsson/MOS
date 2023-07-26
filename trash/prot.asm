[ORG 0x7c00]
    jmp start
    %include "printp.inc"

start: 
    xor ax,ax
    mov ss,ax
    mov sp,0x9c00
    
    cli
    push ds
    lgdt[gdtinfo]
    mov eax, cr0
    or al,1
    mov cr0, eax

    mov  bx, 0x08   ; select descriptor 1
    mov  ds, bx   ; 8h = 1000b

    and al,0xFE     ; back to realmode
    mov  cr0, eax   ; by toggling bit again
    pop ds

    mov esi, str
    call print_str 
    sti
halt:
    jmp halt




gdtinfo:
    dw gdt_end - gdt - 1
    dd gdt

gdt:
    dd 0,0
    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0
    db 0xff, 0xff, 0, 0, 0, 10011110b, 11001111b, 0
gdt_end:
str: db 'Hello World', 0
