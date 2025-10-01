; LimitlessOS BIOS Bootloader (MBR)
; Phase 1: Basic boot sector that loads kernel

[BITS 16]
[ORG 0x7C00]

boot_start:
    ; Setup segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Clear screen
    mov ax, 0x0003
    int 0x10

    ; Print banner
    mov si, msg_banner
    call print_string

    mov si, msg_loading
    call print_string

    ; In production, would:
    ; 1. Load kernel from disk sectors
    ; 2. Switch to protected mode
    ; 3. Setup paging for long mode
    ; 4. Jump to kernel

    ; For Phase 1, just display message
    mov si, msg_phase1
    call print_string

    ; Halt
.halt:
    hlt
    jmp .halt

; Print null-terminated string
; SI = string pointer
print_string:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; Data
msg_banner:
    db 13, 10
    db "========================================", 13, 10
    db "LimitlessOS BIOS Bootloader v0.1.0", 13, 10
    db "Phase 1 - Basic Boot Sector", 13, 10
    db "========================================", 13, 10
    db 13, 10, 0

msg_loading:
    db "[*] Initializing...", 13, 10, 0

msg_phase1:
    db "[!] Phase 1: Boot sector loaded", 13, 10
    db "[!] Kernel loading not yet implemented", 13, 10
    db "[!] System halted", 13, 10
    db 13, 10, 0

; Pad to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55
