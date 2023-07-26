mov ax, 0x07c0
mov ds, ax

mov si,msg
cld
loop:
    lodsb
    or al, al
    jz hang
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp loop
hang:
    jmp hang
msg: db 'Hello World', 13, 10, 0

