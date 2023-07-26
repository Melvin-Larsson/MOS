[BITS 16]
print_str:
    mov ax, 0xb800
    mov es, ax
print_str_loop:
    lodsb
    cmp al, 0
    je print_str_ret 
    call print_char_fast
    jmp print_str_loop
print_str_ret:
    ret
print_char: 
    mov bl,al
    mov ax,0xb800
    mov es,ax
    mov al,bl
print_char_fast:
    mov ah, 0x0F
    mov cx, ax

    movzx ax, byte [posY]
    mov bx, 160
    mul bx
    movzx bx, byte [posX]
    shl bx, 1
    mov di, 0
    add di, ax
    add di, bx
    mov ax, cx
    
    stosw
    add byte[posX], 1
    ret

posX: db 0
posY: db 0

