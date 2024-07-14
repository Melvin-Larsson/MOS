[BITS 32]
; Error code is pushed:
%macro interrupt_handler_error_code_m 1
interrupt_handler_%1:
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
%macro interrupt_handler_m 1
interrupt_handler_%1:
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

extern exception_handler
extern exception_handler_error_code

interrupt_handler_m 0
interrupt_handler_m 1
interrupt_handler_m 2
interrupt_handler_m 3
interrupt_handler_m 4
interrupt_handler_m 5
interrupt_handler_m 6
interrupt_handler_m 7
interrupt_handler_error_code_m 8
interrupt_handler_m 9
interrupt_handler_error_code_m 10
interrupt_handler_error_code_m 11
interrupt_handler_error_code_m 12
interrupt_handler_error_code_m 13
interrupt_handler_error_code_m 14
interrupt_handler_m 15
interrupt_handler_m 16
interrupt_handler_error_code_m 17
interrupt_handler_m 18
interrupt_handler_m 19
interrupt_handler_m 20
interrupt_handler_error_code_m 21
interrupt_handler_m 22
interrupt_handler_m 23
interrupt_handler_m 24
interrupt_handler_m 25
interrupt_handler_m 26
interrupt_handler_m 27
interrupt_handler_m 28
interrupt_handler_m 29
interrupt_handler_m 30
interrupt_handler_m 31
interrupt_handler_m 32
interrupt_handler_m 33

global interrupt_addr_table
interrupt_addr_table:
%assign i 0
%rep 34
    dd interrupt_handler_%+i
%assign i i+1
%endrep
