[BITS 16]
xor ax,ax 
mov ss, ax
mov ds, ax
mov sp, 0xec00
%include "s1.inc"
%include "s2.inc"
%include "interrupt.inc"
