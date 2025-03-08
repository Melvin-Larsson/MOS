[BITS 16]
org 0x7c60
jmp start
%include "s2loader.inc"
%include "print.inc"

start:
    mov ax, 0x2000
    mov ss, ax
    xor ax,ax 
    mov ds, ax
    mov sp, 0xfc00

    push dx
    
    mov si, start_str
    call print_str

    pop dx
    push dx
    call loads2

    mov si, load_s2
    call print_str

    pop dx
    jmp s2 

start_str: db 'Kernel started ', 0
load_s2: db 'Stage 2 loaded', 0

times 510-96-($-$$) db 0
    dw 0xAA55

s2:
; Stage 2 is loaded here
