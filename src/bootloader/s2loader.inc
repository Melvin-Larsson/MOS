loads2:
    mov ah, 0x02    ;read sector
    mov al, 1       ;read 1 sector
    mov ch, 0       ;cylinder 0
    mov cl, 2       ;sector 2
    mov dh,0        ;head 0
    xor bx, bx
    mov es, bx
    mov bx, 0x7e00
    int 13h
    ret

    
