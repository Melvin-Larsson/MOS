[BITS 16]
align 4
dap: 
    db 0x10
    db 0
    dw 32
    dw 0
    dw 0x07E0
    dd 1
    dd 0 
loads2:
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    mov si, no_ext
    jc error

    mov si, dap
    mov ah, 0x42
    int 0x13
    mov si, str1
    jc error
    ret

    mov ah, 0x02    ;read sector
    mov al, 14      ;read 14 sectors
    mov ch, 0       ;cylinder 0
    mov cl, 2       ;sector 2
    mov dh,0        ;head 0
    mov bx, 0x07c0
    mov es, bx
    mov bx, 512
    int 13h

    mov si, str1
    jc error
    mov si, str2
    call print_str

    mov ah, 0x02    ;read sector
    mov al, 14      ;read 15 sectors
    mov ch, 1       ;cylinder 1
    mov cl, 1       ;sector 1
    mov dh,0        ;head 0
    mov bx, 0x07c0
    mov es, bx
    mov bx, 15 * 512
    int 13h

    mov si, str3
    call print_str
    mov si, str1
    jc error

    ret

error:
    call print_str
    shr ax,8
    call print_ax
error_halt:
    jmp error_halt
    

str1: db ' error loading ',0
no_ext: db ' no extension ',0
str2: db '1',0
str3: db '2',0

    
