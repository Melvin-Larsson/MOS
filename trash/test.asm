[ORG 0x7c00]      ; add to offsets
 
start:   xor ax, ax   ; make it zero
   mov ds, ax   ; DS=0
   mov ss, ax   ; stack starts at 0
   mov sp, 0x9c00   ; 2000h past code start
 
   cli      ; no interrupt
 
   lgdt [gdtinfo]   ; load gdt register
 
   mov  eax, cr0   ; switch to pmode by
   or al,1         ; set pmode bit
   mov  cr0, eax
 
;   and al,0xFE     ; back to realmode
;   mov  cr0, eax   ; by toggling bit again
 
   sti
 
   jmp $      ; loop forever
 
gdtinfo:
   dw gdt_end - gdt - 1   ;last byte in table
   dd gdt         ;start of table
 
gdt        dd 0,0  ; entry 0 is always unused
flatdesc    db 0xff, 0xff, 0, 0, 0, 10010010b, 11001111b, 0
gdt_end:
 
   times 510-($-$$) db 0  ; fill sector w/ 0's
   db 0x55          ; req'd by some BIOSes
   db 0xAA
