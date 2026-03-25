#ifndef WEEZYDOS_KEYBOARD_H
#define WEEZYDOS_KEYBOARD_H

#include "types.h"

#define KEY_BUFFER_SIZE 64

typedef struct {
    uint8_t (*read_scancode)(void);
} keyboard_ops_t;

void keyboard_init(keyboard_ops_t *ops);
void keyboard_handle_scancode(uint8_t scancode);
char keyboard_getchar(void);
int keyboard_has_key(void);
char scancode_to_ascii(uint8_t scancode);

#endif
