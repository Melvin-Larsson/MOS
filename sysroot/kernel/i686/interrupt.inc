[BITS 32]
; Error code is pushed:
%macro exception_handler_error_code_m 1
exception_handler_%1:
    push eax
    mov eax, esp 
    add eax, 12
    pushad
    push eax
    push %1
    cld
    call exception_handler
    add esp, 8
    popad
    pop eax
    add esp, 4
    iret
%endmacro

;No error code is pushed
%macro exception_handler_m 1
exception_handler_%1:
    push eax
    mov eax, esp 
    add eax, 12
    pushad
    push eax
    push %1
    cld
    call exception_handler
    add esp, 8
    popad
    pop eax
    iret
%endmacro

%macro interrupt_handler_m 1
interrupt_handler_%1:
    push eax
    mov eax, esp 
    add eax, 12
    pushad
    push eax
    push %1
    cld
    call interrupt_handler
    add esp, 8
    popad
    pop eax
    iret
%endmacro

extern exception_handler
extern interrupt_handler
extern exception_handler_error_code
extern syscall_handler

exception_handler_m 0
exception_handler_m 1
exception_handler_m 2
exception_handler_m 3
exception_handler_m 4
exception_handler_m 5
exception_handler_m 6
exception_handler_m 7
exception_handler_error_code_m 8
exception_handler_m 9
exception_handler_error_code_m 10
exception_handler_error_code_m 11
exception_handler_error_code_m 12
exception_handler_error_code_m 13
exception_handler_error_code_m 14
exception_handler_m 15
exception_handler_m 16
exception_handler_error_code_m 17
exception_handler_m 18
exception_handler_m 19
exception_handler_m 20
exception_handler_error_code_m 21
exception_handler_m 22
exception_handler_m 23
exception_handler_m 24
exception_handler_m 25
exception_handler_m 26
exception_handler_m 27
exception_handler_m 28
exception_handler_m 29
exception_handler_m 30
exception_handler_m 31
interrupt_handler_m 32
interrupt_handler_m 33
interrupt_handler_m 34
interrupt_handler_m 35

int0x80_handler:
    pushad
    push ecx
    push ebx
    push eax
    call syscall_handler
    add esp, 12
    pop edi
    pop esi
    pop ebp
    add esp, 4
    pop ebx
    pop edx
    pop ecx
    add esp, 4
    iret


extern thread_getNewEsp
global task_switch_handler
task_switch_handler:
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi 
    push esp
    call thread_getNewEsp
    add esp, 4
    mov esp, eax 
    pop edi 
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    iret


global interrupt_addr_table
interrupt_addr_table:
%assign i 0
%rep 32
    dd exception_handler_%+i
%assign i i+1
%endrep

%rep (35 - 32)
    dd interrupt_handler_%+i
%assign i i+1
%endrep

%rep (0x80 - 35)
    dd interrupt_handler_35
%endrep
dd int0x80_handler
