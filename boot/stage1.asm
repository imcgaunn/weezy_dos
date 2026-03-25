; weezyDOS Stage 1 Bootloader
; Loaded by BIOS at 0x7C00
; For now: prints 'W' and halts

[BITS 16]
[ORG 0x7C00]

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows down from bootloader

    ; Print 'W' using BIOS teletype
    mov ah, 0x0E            ; BIOS teletype function
    mov al, 'W'             ; Character to print
    mov bh, 0               ; Page number
    int 0x10                ; BIOS video interrupt

    ; Halt
    cli
    hlt
    jmp $                   ; Infinite loop in case of NMI

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
