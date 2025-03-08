[BITS 16]
extern kernel_main
s2:
    jmp main
    %include "A20.asm"
    %include "protected.inc"
    %include "memmap.inc"
main: 
    call enable_a20
    cmp al, 1
    jne a20_fail

    mov si, a20
    call print_str

    mov si, drive_index
    mov dl, [ds:si]
    mov si, dap2
    mov ah, 0x42
    int 0x13

    call map_memory
    call enter_protected

a20_fail:
    mov si, failedA20
    call print_str
s1_halt:
    jmp s1_halt

[BITS 32]
%include "printp.inc"
pmode_main:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x190000

    mov al, 0xff
    out 0xa1, al
    out 0x21, al

    mov si, str;

    call printp_str
    call kernel_main

str: db 'Successfully entered protected mode!',0
a20: db 'a20',0
failedA20 : db 'Fa20',0
dap2: 
    db 0x10
    db 0
    dw 127 ;nr of sectors
    dw 0
    dw 0x17c0  ;assumning a sector is 512 bytes
    dd 128
    dd 0 
