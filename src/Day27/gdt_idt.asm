[BITS 32]

gdt_start:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Access Byte
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

global idt_load

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret


extern irq_handler

global irq0
global irq1

irq0:
    pusha
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push 0
    call irq_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

irq1:
    pusha
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push 1
    call irq_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret