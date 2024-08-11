[BITS 16]
mov ax, 0x2000
mov ss, ax
xor ax,ax 
mov ds, ax
mov sp, 0xfc00
mov si, drive_index
mov [ds:si], dl
jmp start
drive_index: db 0
%include "s1.inc"
%include "s2.inc"
%include "interrupt.inc"
