[BITS 16]
extern kernel_main
s2:
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
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov si, str;

    call printp_str
    call kernel_main

;    mov edi, 0x1000    ; Set the destination index to 0x1000.
;    mov cr3, edi       ; Set control register 3 to the destination index.
;    xor eax, eax       ; Nullify the A-register.
;    mov ecx, 4096      ; Set the C-register to 4096.
;    rep stosd          ; Clear the memory.
;    mov edi, cr3
;
;    mov DWORD [edi], 0x2003      ; Set the uint32_t at the destination index to 0x2003.
;    add edi, 0x1000              ; Add 0x1000 to the destination index.
;    mov DWORD [edi], 0x3003      ; Set the uint32_t at the destination index to 0x3003.
;    add edi, 0x1000              ; Add 0x1000 to the destination index.
;    mov DWORD [edi], 0x4003      ; Set the uint32_t at the destination index to 0x4003.
;    add edi, 0x1000              ; Add 0x1000 to the destination index.
;
;    mov ebx, 0x00000003          ; Set the B-register to 0x00000003.
;    mov ecx, 512                 ; Set the C-register to 512.
;
;.SetEntry:
;    mov DWORD [edi], ebx         ; Set the uint32_t at the destination index to the B-register.
;    add ebx, 0x1000              ; Add 0x1000 to the B-register.
;    add edi, 8                   ; Add eight to the destination index.
;    loop .SetEntry               ; Set the next entry.
;
;    mov eax, cr4                 ; Set the A-register to control register 4.
;    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
;    mov cr4, eax                 ; Set control register 4 to the A-register.
;
;    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
;    rdmsr                        ; Read from the model-specific register.
;    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
;    wrmsr
;
;    mov eax, cr0                 ; Set the A-register to control register 0.
;    or eax, 1 << 31 | 1 << 0     ; Set the PG-bit, which is the 31nd bit, and the PM-bit, which is the 0th bit.
;    mov cr0, eax                 ; Set control register 0 to the A-register.
;    lgdt [GDT64.Pointer]
;    jmp GDT64.Code:m64
;
;[BITS 64]
;m64:
;    mov ax, GDT64.Data            ; Set the A-register to the data descriptor.
;    mov ds, ax                    ; Set the data segment to the A-register.
;    mov es, ax                    ; Set the extra segment to the A-register.
;    mov fs, ax                    ; Set the F-segment to the A-register.
;    mov gs, ax                    ; Set the G-segment to the A-register.
;    mov ss, ax 
;    mov edi, 0xB8000              ; Set the destination index to 0xB8000.
;    mov rax, 0x1F201F201F201F20   ; Set the A-register to 0x1F201F201F201F20.
;    mov ecx, 500                  ; Set the C-register to 500.
;    rep stosq                     ; Clear the screen.
;    call kernel_main

str: db 'Successfully entered protected mode!',0

;PRESENT        equ 1 << 7
;NOT_SYS        equ 1 << 4
;EXEC           equ 1 << 3
;DC             equ 1 << 2
;RW             equ 1 << 1
;ACCESSED       equ 1 << 0
;
;; Flags bits
;GRAN_4K       equ 1 << 7
;SZ_32         equ 1 << 6
;LONG_MODE     equ 1 << 5
;
;GDT64:
;    .Null: equ $ - GDT64
;        dq 0
;    .Code: equ $ - GDT64
;        dd 0xFFFF                                   ; Limit & Base (low, bits 0-15)
;        db 0                                        ; Base (mid, bits 16-23)
;        db PRESENT | NOT_SYS | EXEC | RW            ; Access
;        db GRAN_4K | LONG_MODE | 0xF                ; Flags & Limit (high, bits 16-19)
;        db 0                                        ; Base (high, bits 24-31)
;    .Data: equ $ - GDT64
;        dd 0xFFFF                                   ; Limit & Base (low, bits 0-15)
;        db 0                                        ; Base (mid, bits 16-23)
;        db PRESENT | NOT_SYS | RW                   ; Access
;        db GRAN_4K | SZ_32 | 0xF                    ; Flags & Limit (high, bits 16-19)
;        db 0                                        ; Base (high, bits 24-31)
;    .TSS: equ $ - GDT64
;        dd 0x00000068
;        dd 0x00CF8900
;    .Pointer:
;        dw $ - GDT64 - 1
;        dq GDT64
