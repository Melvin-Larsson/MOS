[ORG 0x7c00]
    jmp start
    %include "print.inc"
start:
    xor ax,ax 
    mov ds,ax 
    mov ss, ax
    mov sp, 0x9c00
    mov si, str
    
    cld 
    
    cli
    mov bx, 0x24
    xor ax,ax
    mov gs, ax
    mov[gs:bx], word on_keyboard
    mov [gs:bx+2], ds
    sti

    jmp hang


on_keyboard:
    in al, 0x60
    mov bl, al
    and bl,0x80
    jnz done
    call print_char
done:
    mov al, 0x20
    out 0x20, al
    iret
    
hang:
    jmp hang

str: db 'Hello World',0
