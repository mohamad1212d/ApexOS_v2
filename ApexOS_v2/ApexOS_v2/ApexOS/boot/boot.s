; =====================================================
;  boot.s - ApexOS Multiboot Entry Point
; =====================================================

MBOOT_MAGIC    equ 0x1BADB002
MBOOT_ALIGN    equ 1<<0
MBOOT_MEMINFO  equ 1<<1
MBOOT_FLAGS    equ MBOOT_ALIGN | MBOOT_MEMINFO
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 32768      ; 32 KB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push ebx        ; multiboot info pointer
    push eax        ; multiboot magic
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
