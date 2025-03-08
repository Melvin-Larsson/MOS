[BITS 16]
extern kernel_main
jmp main

%include "print.inc"
%include "A20.asm"
%include "memmap.inc"

main: 
    mov si, drive_index
    mov [ds:si], dl

    mov si, s2
    call print_str


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

s2: db 's2', 0
dap2: 
    db 0x10
    db 0
    dw 127 ;nr of sectors
    dw 0
    dw 0x17c0  ;assumning a sector is 512 bytes
    dd 192
    dd 0 
%include "protected.inc"
%include "interrupt.inc"

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

drive_index: db 0

gdt:
    dd 0,0
    db 0xff, 0xff, 0, 0, 0, 10011010b, 11001111b, 0 ;kernel code
    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0 ;kernel data
    db 0xff, 0xff, 0, 0, 0, 11111010b, 11011111b, 0 ;user code
    db 0xff, 0xff, 0, 0, 0, 11110010b, 11011111b, 0 ;user data
global tss
tss:
    db 108, 0x00, 0, 0, 0, 10001001b, 00000000b, 0 ;kernel_tss (placeholder address)
        
gdt_end:
gdtinfo:
    dw gdt_end - gdt - 1
    dd gdt

str: db 'Successfully entered protected mode!',0
a20: db 'a20',0
failedA20 : db 'Fa20',0
