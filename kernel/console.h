#ifndef WEEZYDOS_CONSOLE_H
#define WEEZYDOS_CONSOLE_H

#include "types.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_BLACK      0

typedef struct {
    void (*write_cell)(uint16_t row, uint16_t col, uint8_t ch, uint8_t attr);
    uint16_t (*read_cell)(uint16_t row, uint16_t col);
} console_ops_t;

void console_init(console_ops_t *ops);
void console_putchar(char c);
void console_puts(const char *str);
void console_clear(void);
void console_get_cursor(uint16_t *row, uint16_t *col);
void console_set_cursor(uint16_t row, uint16_t col);

#endif
