    jmp main
    %include "print.inc"
    %include "protected.inc"
main:
   call enter_protected

[BITS 32]
%include "printp.inc"
pmode_main:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov esp, 0x90000
    mov si, str
    call printp_str
halt: jmp halt

str: db 'Successfully entered protected mode!',0

