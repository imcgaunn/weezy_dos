#include "console.h"

static uint16_t cursor_row = 0;
static uint16_t cursor_col = 0;
static uint8_t default_attr = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY;
static console_ops_t *ops = NULL;

void console_init(console_ops_t *backend) {
    ops = backend;
    cursor_row = 0;
    cursor_col = 0;
}

void console_clear(void) {
    for (uint16_t r = 0; r < VGA_HEIGHT; r++) {
        for (uint16_t c = 0; c < VGA_WIDTH; c++) {
            ops->write_cell(r, c, ' ', default_attr);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

void console_get_cursor(uint16_t *row, uint16_t *col) {
    *row = cursor_row;
    *col = cursor_col;
}

void console_set_cursor(uint16_t row, uint16_t col) {
    cursor_row = row;
    cursor_col = col;
}

static void scroll_up(void) {
    for (uint16_t r = 1; r < VGA_HEIGHT; r++) {
        for (uint16_t c = 0; c < VGA_WIDTH; c++) {
            uint16_t cell = ops->read_cell(r, c);
            ops->write_cell(r - 1, c, (uint8_t)(cell & 0xFF), (uint8_t)(cell >> 8));
        }
    }
    for (uint16_t c = 0; c < VGA_WIDTH; c++) {
        ops->write_cell(VGA_HEIGHT - 1, c, ' ', default_attr);
    }
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        ops->write_cell(cursor_row, cursor_col, (uint8_t)c, default_attr);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT) {
        scroll_up();
        cursor_row = VGA_HEIGHT - 1;
    }
}

void console_puts(const char *str) {
    while (*str) {
        console_putchar(*str);
        str++;
    }
}
