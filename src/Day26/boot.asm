; nasm -f elf32 boot.asm -o boot.o
section .multiboot
    dd 0x1BADB002           ; magic number
    dd 0x00                 ; flags
    dd -(0x1BADB002 + 0x00) ; checksum

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

align 4096
PML4:
    resb 4096
PDPT:
    resb 4096
PD:
    resb 4096

section .text
global _start
_start:
    mov esp, stack_top

    call check_cpuid
    call check_long_mode

    call setup_page_tables

    ; Enable PAE
    ; cr4 is the control register that controls various aspects of the CPU's operation, including paging. To enable PAE, we need to set the PAE bit (bit 5) in cr4.
    mov eax, cr4
    or eax, 1 << 5 ; Set the PAE bit (bit 5) in cr4
    mov cr4, eax

    ; Load the address of the PML4 into cr3 to enable paging
    ; cr3 is the control register that holds the physical address of the page directory base. By loading the address of our PML4 into cr3, we tell the CPU where to find our page tables.
    mov eax, PML4
    mov cr3, eax ; Tell the CPU where the page tables are located

    ; Enable long mode
    ; MSRs (Model-Specific Registers) are special registers that control various aspects of the CPU's operation. The IA32_EFER MSR (Model-Specific Register) is used to enable long mode. To enable long mode, we need to set the LME bit (bit 8) in the IA32_EFER MSR.
    mov ecx, 0xC0000080 ; IA32_EFER MSR
    rdmsr ; Read the current value of EFER into edx:eax
    or eax, 1 << 8 ; Set the LME bit (bit 8) to enable long mode
    wrmsr ; Write the modified value back to EFER

    ; Enable paging
    ; cr0 is the control register that controls various aspects of the CPU's operation, including paging. To enable paging, we need to set the PG bit (bit 31) in cr0.
    mov eax, cr0
    or eax, 1 << 31 ; Set the PG bit
    mov cr0, eax ; Enter long mode

    lgdt [GDT64.Pointer]
    jmp GDT64.Code:long_mode_start

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push eax
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    hlt

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    hlt

setup_page_tables:
    mov eax, PDPT
    or eax, 0b11
    mov [PML4], eax

    mov eax, PD
    or eax, 0b11
    mov [PDPT], eax

    mov eax, 0b10000011
    mov [PD], eax
    ret

section .rodata
GDT64:
    dq 0
.Code: equ $ - GDT64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
    ; bit 43: long mode code segment
    ; bit 44: code segment is executable
    ; bit 47: code segment is present
    ; bit 53: code segment has a limit of 0xFFFFFFFFF (4GB)
.Pointer:
    dw $ - GDT64 - 1
    dd GDT64

[BITS 64]
long_mode_start:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    extern kernel_main
    call kernel_main

    hlt
