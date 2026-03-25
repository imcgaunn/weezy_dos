#include "types.h"

#ifndef WEEZYDOS_TEST

#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "idt.h"

static volatile uint16_t *vga_mem = (volatile uint16_t *)0xB8000;

static void vga_write_cell(uint16_t row, uint16_t col, uint8_t ch, uint8_t attr) {
    vga_mem[row * VGA_WIDTH + col] = (uint16_t)ch | ((uint16_t)attr << 8);
}

static uint16_t vga_read_cell(uint16_t row, uint16_t col) {
    return vga_mem[row * VGA_WIDTH + col];
}

static console_ops_t vga_ops = {
    .write_cell = vga_write_cell,
    .read_cell = vga_read_cell,
};

static char shell_getchar_impl(void) {
    while (!keyboard_has_key()) {
        __asm__ volatile ("hlt");
    }
    return keyboard_getchar();
}

static shell_io_t os_shell_io = {
    .getchar = shell_getchar_impl,
    .putchar = console_putchar,
    .puts = console_puts,
};

void kmain(void) {
    console_init(&vga_ops);
    console_clear();
    keyboard_init(NULL);
    idt_init();
    shell_init(&os_shell_io);
    shell_run();
}

#endif
