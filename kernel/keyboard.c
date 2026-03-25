#include "keyboard.h"

static char key_buffer[KEY_BUFFER_SIZE];
static int buf_head = 0;
static int buf_tail = 0;
static keyboard_ops_t *ops = NULL;

static const char scancode_table[128] = {
    /* 0x00-0x07 */ 0,   27,  '1', '2', '3', '4', '5', '6',
    /* 0x08-0x0F */ '7', '8', '9', '0', '-', '=', '\b', '\t',
    /* 0x10-0x17 */ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    /* 0x18-0x1F */ 'o', 'p', '[', ']', '\n', 0,   'a', 'b',
    /* 0x20-0x27 */ 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
    /* 0x28-0x2F */ 'k', 'l', ';', '\'', '`', 0,   '\\', 'z',
    /* 0x30-0x37 */ 's', 'x', 'v', 'n', 'm', ',', '.', '/',
    /* 0x38-0x3F */ 0,   ' ', 0,   0,   0,   0,   0,   0,
    /* 0x40-0x47 */ 0,   0,   0,   0,   0,   0,   0,   '7',
    /* 0x48-0x4F */ '8', '9', '-', '4', '5', '6', '+', '1',
    /* 0x50-0x57 */ '2', '3', '0', '.', 0,   0,   0,   0,
};

void keyboard_init(keyboard_ops_t *backend) {
    ops = backend;
    buf_head = 0;
    buf_tail = 0;
}

char scancode_to_ascii(uint8_t scancode) {
    if (scancode & 0x80) return 0;
    if (scancode >= 128) return 0;
    return scancode_table[scancode];
}

static void buffer_push(char c) {
    int next_head = (buf_head + 1) % KEY_BUFFER_SIZE;
    if (next_head == buf_tail) return;
    key_buffer[buf_head] = c;
    buf_head = next_head;
}

void keyboard_handle_scancode(uint8_t scancode) {
    char c = scancode_to_ascii(scancode);
    if (c != 0) buffer_push(c);
}

int keyboard_has_key(void) {
    return buf_head != buf_tail;
}

char keyboard_getchar(void) {
    if (!keyboard_has_key()) return 0;
    char c = key_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}
