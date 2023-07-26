[BITS 16]
loads2:
    mov ah, 0x02    ;read sector
    mov al, 5       ;read 5 sectors
    mov ch, 0       ;cylinder 0
    mov cl, 2       ;sector 2
    mov dh,0        ;head 0
    xor bx, bx
    mov es, bx
    mov bx, s2
    int 13h
    ret
