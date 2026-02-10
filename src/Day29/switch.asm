[BITS 32]

global switch_to

; switch_to(task_t *prev, task_t *next)
switch_to:
    mov eax, [esp + 4]  ; prev
    mov edx, [esp + 8]  ; next
    
    push ebp
    push ebx
    push esi
    push edi
    
    mov [eax + 4], esp
    
    mov esp, [edx + 4] 
    mov ecx, [edx + 12]
    mov cr3, ecx
    
    pop edi
    pop esi
    pop ebx
    pop ebp
    
    ret