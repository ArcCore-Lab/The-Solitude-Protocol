[BITS 32]

global syscall_handler

extern syscall_table

syscall_handler:
    push ds
    push es
    push fs
    push gs
    pusha
    
    mov dx, 0x10
    mov ds, dx
    mov es, dx
    
    cmp eax, 5
    jae .invalid
    
    call [syscall_table + eax * 4]
    jmp .done
    
.invalid:
    mov eax, -1
    
.done:
    popa
    pop gs
    pop fs
    pop es
    pop ds
    iret