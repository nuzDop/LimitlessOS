; LimitlessOS x86_64 Boot Assembly
; Early boot code and kernel entry point

bits 64
section .text

global _start
extern kernel_main

; Multiboot2 header
section .multiboot
align 8
multiboot_header_start:
    dd 0xe85250d6                    ; Magic number
    dd 0                             ; Architecture (0 = i386)
    dd multiboot_header_end - multiboot_header_start  ; Header length
    dd -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))  ; Checksum

    ; End tag
    dw 0                             ; Type
    dw 0                             ; Flags
    dd 8                             ; Size
multiboot_header_end:

section .bss
align 16
stack_bottom:
    resb 16384                       ; 16 KB stack
stack_top:

; GDT for 64-bit mode
section .data
align 16
gdt64:
    dq 0                             ; Null descriptor
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)  ; Code segment
.data: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41)   ; Data segment
.pointer:
    dw $ - gdt64 - 1                 ; GDT size
    dq gdt64                         ; GDT address

section .text
_start:
    ; We're already in long mode (bootloader handles this)

    ; Load GDT
    lgdt [gdt64.pointer]

    ; Set up segment registers
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack
    mov rsp, stack_top
    mov rbp, rsp

    ; Clear direction flag
    cld

    ; Save multiboot info pointer (in RBX)
    mov rdi, rbx

    ; Call kernel main
    call kernel_main

    ; If kernel returns, hang
.hang:
    cli
    hlt
    jmp .hang

; Context switch function
global context_switch
context_switch:
    ; void context_switch(cpu_context* old, cpu_context* new)
    ; RDI = old context pointer
    ; RSI = new context pointer

    ; Save old context
    test rdi, rdi
    jz .load_new

    mov [rdi + 0],  rax
    mov [rdi + 8],  rbx
    mov [rdi + 16], rcx
    mov [rdi + 24], rdx
    mov [rdi + 32], rsi
    mov [rdi + 40], rdi
    mov [rdi + 48], rbp
    mov [rdi + 56], rsp
    mov [rdi + 64], r8
    mov [rdi + 72], r9
    mov [rdi + 80], r10
    mov [rdi + 88], r11
    mov [rdi + 96], r12
    mov [rdi + 104], r13
    mov [rdi + 112], r14
    mov [rdi + 120], r15

    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 128], rax

    ; Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 136], rax

.load_new:
    ; Load new context
    test rsi, rsi
    jz .done

    mov rax, [rsi + 0]
    mov rbx, [rsi + 8]
    mov rcx, [rsi + 16]
    mov rdx, [rsi + 24]
    ; Skip RSI and RDI for now
    mov rbp, [rsi + 48]
    mov rsp, [rsi + 56]
    mov r8,  [rsi + 64]
    mov r9,  [rsi + 72]
    mov r10, [rsi + 80]
    mov r11, [rsi + 88]
    mov r12, [rsi + 96]
    mov r13, [rsi + 104]
    mov r14, [rsi + 112]
    mov r15, [rsi + 120]

    ; Load RFLAGS
    mov rax, [rsi + 136]
    push rax
    popfq

    ; Load RSI and RDI last
    mov rdi, [rsi + 40]
    mov rax, [rsi + 32]  ; Save RSI value

    ; Jump to new RIP
    mov rcx, [rsi + 128]
    mov rsi, rax         ; Restore RSI
    jmp rcx

.done:
    ret

; CPU halt function
global cpu_halt
cpu_halt:
    cli
    hlt
    jmp cpu_halt

; Port I/O functions
global outb
outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret

global inb
inb:
    mov dx, di
    in al, dx
    ret

global outw
outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

global inw
inw:
    mov dx, di
    in ax, dx
    ret

global outl
outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

global inl
inl:
    mov dx, di
    in eax, dx
    ret
