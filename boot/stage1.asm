; weezyDOS Stage 1 Bootloader
; Loaded by BIOS at 0x7C00
; Job: Load Stage 2 from disk sectors 2-17 to 0x8000, then jump there.

[BITS 16]
[ORG 0x7C00]

STAGE2_ADDR   equ 0x8000
STAGE2_SECTORS equ 16

start:
    mov [boot_drive], dl

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x0E
    mov al, 'W'
    int 0x10

    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, STAGE2_ADDR
    int 0x13

    jc disk_error

    cmp al, STAGE2_SECTORS
    jne disk_error

    mov ah, 0x0E
    mov al, '1'
    int 0x10

    mov dl, [boot_drive]
    jmp STAGE2_ADDR

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt
    jmp $

boot_drive: db 0

times 510 - ($ - $$) db 0
dw 0xAA55
