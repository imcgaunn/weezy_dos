[BITS 32]
[GLOBAL keyboard_isr_stub]
[EXTERN keyboard_interrupt_handler]

keyboard_isr_stub:
    pushad
    call keyboard_interrupt_handler
    mov al, 0x20
    out 0x20, al
    popad
    iret
