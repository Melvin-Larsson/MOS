xor ax,ax 
mov ss, ax
mov ds, ax
mov sp, 0x9c00
%include "s1.inc"
%include "s2.inc"
