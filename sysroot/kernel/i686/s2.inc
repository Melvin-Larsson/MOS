[BITS 16]
extern kernel_main
s2:
    jmp main
    %include "protected.inc"
main:
   call enter_protected

[BITS 32]
%include "printp.inc"
pmode_main:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov al, 0xff
    out 0xa1, al
    out 0x21, al

    mov si, str;

    call printp_str
    call kernel_main

str: db 'Successfully entered protected mode!',0

