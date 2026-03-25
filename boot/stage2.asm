; weezyDOS Stage 2 Bootloader
; Loaded at 0x8000 by Stage 1
; Job: Enable A20, load kernel, set up GDT, switch to protected mode, jump to kmain()

[BITS 16]
[ORG 0x8000]

KERNEL_ADDR    equ 0x10000
KERNEL_SEGMENT equ 0x1000
KERNEL_SECTORS equ 32
KERNEL_START_SECTOR equ 18

start_stage2:
    mov [boot_drive], dl

    mov ah, 0x0E
    mov al, '2'
    int 0x10

    ; === Enable A20 gate via Fast A20 (port 0x92) ===
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al

    ; === Load kernel from disk (must be done in real mode, BIOS not available in PM) ===
    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0
    mov cl, KERNEL_START_SECTOR
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13

    jc .disk_error

    xor ax, ax
    mov es, ax

    mov ah, 0x0E
    mov al, 'K'
    int 0x10

    ; === Switch to protected mode ===
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x01
    mov cr0, eax

    ; Far jump flushes prefetch queue and loads CS with code segment selector
    jmp 0x08:protected_mode_entry

.disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt
    jmp $

boot_drive: db 0

; === GDT ===
; Entry 0: Null descriptor (required)
; Entry 1 (0x08): Code segment — base=0, limit=4GB, executable, readable, 32-bit
; Entry 2 (0x10): Data segment — base=0, limit=4GB, writable, 32-bit

gdt_start:

gdt_null:
    dq 0

gdt_code:
    dw 0xFFFF       ; Limit bits 0-15
    dw 0x0000       ; Base bits 0-15
    db 0x00         ; Base bits 16-23
    db 10011010b    ; Access: present, ring 0, code, executable, readable
    db 11001111b    ; Flags: 4KB granularity, 32-bit + Limit bits 16-19
    db 0x00         ; Base bits 24-31

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; Access: present, ring 0, data, writable
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; === 32-bit Protected Mode Entry ===
[BITS 32]

protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x9FC00

    ; Write 'P' directly to VGA memory to confirm protected mode
    mov byte [0xB8006], 'P'
    mov byte [0xB8007], 0x0A

    jmp KERNEL_ADDR
