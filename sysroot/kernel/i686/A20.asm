[BITS 16]
check_a20:
    pushf
    push es
    push di
    push ds
    push si

    cli

    xor ax, ax
    mov es, ax
    not ax
    mov ds, ax

    mov di, 0x0500
    mov si, 0x0510
    
    mov al, byte [es:di]
    push ax

    mov al, byte [ds:si]
    push ax

    mov byte [es:di], 0x00 
    mov byte [ds:si], 0xFF
    
    cmp byte [es:di], 0xFF

    pop ax
    mov byte[ds:si], al

    pop ax
    mov byte[es:di], al

    mov ax, 0
    je check_a20_ret
    mov ax,1

check_a20_ret:
   pop si 
   pop ds
   pop di
   pop es
   popf

   ret

enable_a20:
   call check_a20
   cmp al, 1
   je enable_a20_return

a20_bios:
    mov ax, 0x2403
    int 15h
    jb a20_keyboard_controller
    cmp ah, 0
    jne a20_keyboard_controller

    mov ax,0x2401
    int 0x15
    jb a20_keyboard_controller
    cmp ah, 0
    jne a20_keyboard_controller
    mov al,1
    ret

a20_keyboard_controller:
    mov al,0
    ret

enable_a20_return:
    ret

