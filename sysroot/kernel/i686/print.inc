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

; print the value in ax
print_char: 
    push bx
    mov bl,al
    mov ax,0xb800
    mov es,ax
    mov al,bl
    pop bx
print_char_fast:
    push bx
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
    pop bx
    ret

; print the value in ax as hex
print_ax:
    push si
    push ax
    mov si, hex
    call print_str
    pop ax
    mov bx, ax
    mov cx, 4
print_ax_loop:
    and ax, 0xF000
    shr ax, 12
    cmp ax, 9
    jle print_ax_number
print_ax_letter:
    sub ax, 10
    add ax, 'A'
    jmp print_ax_print
print_ax_number:
    add ax, '0'
print_ax_print:
    push ax
    push cx
    call print_char
    pop cx
    pop ax

    shl bx, 4
    mov ax, bx
    sub cx, 1
    jne print_ax_loop
    pop si
    ret
    
hex: db '0x',0
posX: db 0
posY: db 0

