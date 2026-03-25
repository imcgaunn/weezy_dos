#include "idt.h"
#include "port_io.h"
#include "keyboard.h"

#ifndef WEEZYDOS_TEST

#define IDT_ENTRIES 256
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

extern void keyboard_isr_stub(void);

static void idt_set_entry(int index, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[index].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[index].selector    = selector;
    idt[index].zero        = 0;
    idt[index].type_attr   = type_attr;
    idt[index].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

static void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT); io_wait();
    outb(PIC2_COMMAND, ICW1_INIT); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void keyboard_interrupt_handler(void) {
    uint8_t scancode = inb(0x60);
    keyboard_handle_scancode(scancode);
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, 0, 0, 0);
    }

    pic_remap();
    idt_set_entry(33, (uint32_t)keyboard_isr_stub, 0x08, 0x8E);

    outb(PIC1_DATA, 0xFD);
    outb(PIC2_DATA, 0xFF);

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    __asm__ volatile ("sti");
}

#else
void idt_init(void) {}
void keyboard_interrupt_handler(void) {}
#endif
