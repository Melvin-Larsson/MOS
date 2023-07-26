printp_str:
    mov eax, [esi]
    lea esi, [esi+1]
    cmp al, 0
    je printp_str_done
    call printp_c
    jmp printp_str
printp_str_done:
    ret    
    
printp_c:
    mov ah, 0x0F
    mov ecx, eax
    movzx eax, byte [ypos]
    mov edx, 160
    mul edx
    movzx ebx, byte[xpos]
    shl ebx, 1

    mov edi, 0xb8000
    add edi, eax
    add edi, ebx

    mov eax, ecx
    mov word [ds:edi], ax
    add byte [xpos], 1

    ret

xpos db 0
ypos db 0     
