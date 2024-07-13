[BITS 16]
map_memory:
    pushad

    mov eax, 0x50
    mov es, eax    ;buffer pointer
    mov di, 4      ;---||---
    mov ebx, 0      ;continuation
    mov edx, 0x534D4150 ; "SMAP"

map_memory_loop:
    mov eax, 0xE820 ;function code
    mov ecx, 128    ; buffer size

    int 0x15

    add di, cx

    test ebx, ebx
    jnz map_memory_loop
    
    mov [es:0], di

    popad
    ret
    
