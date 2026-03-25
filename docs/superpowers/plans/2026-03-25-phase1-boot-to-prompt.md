# Phase 1: Boot to Prompt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Boot weezyDOS in QEMU, switch from real mode to 32-bit protected mode, display a welcome banner on VGA text mode, accept keyboard input, and show an `A:\>` prompt that echoes typed lines back.

**Architecture:** Two-stage ASM bootloader (Stage 1 loads Stage 2, Stage 2 enters protected mode and loads C kernel). Kernel initializes VGA console, sets up IDT for keyboard interrupts, and enters a read-line/echo shell loop. VGA and keyboard modules use the seam pattern (function pointer structs) so the shell's line-reading logic is host-testable.

**Tech Stack:** NASM (bootloader ASM), Clang/LLVM cross-compilation to i686-elf, ld.lld, QEMU

---

### Task 1: VGA Console Driver — Output

**Files:**
- Create: `kernel/console.h`
- Create: `kernel/console.c`
- Create: `tests/test_console.c`
- Modify: `tests/test_main.c` — add console test suite
- Modify: `Makefile` — add `kernel/console.c` to KERNEL_SRCS and `tests/test_console.c` to TEST_SRCS

The VGA console works by writing to a memory-mapped buffer at 0xB8000. Each character on screen is 2 bytes: ASCII code + attribute byte (color). The screen is 80 columns × 25 rows = 4000 bytes.

For testing, we use the seam pattern: the console writes to a `uint16_t *` buffer pointer. In the OS, this points to 0xB8000. In tests, it points to a stack-allocated array.

- [ ] **Step 1: Write the console header**

Create `kernel/console.h`:

```c
#ifndef WEEZYDOS_CONSOLE_H
#define WEEZYDOS_CONSOLE_H

#include "types.h"

/* VGA text mode constants */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_BLACK      0

/* Console operations struct (seam pattern for testing).
 * In the OS, write_cell writes to VGA memory at 0xB8000.
 * In tests, write_cell writes to a RAM buffer. */
typedef struct {
    /* Write a character+attribute to position (row, col) */
    void (*write_cell)(uint16_t row, uint16_t col, uint8_t ch, uint8_t attr);
    /* Read the character+attribute at position (row, col) */
    uint16_t (*read_cell)(uint16_t row, uint16_t col);
} console_ops_t;

/* Initialize console with the given ops backend */
void console_init(console_ops_t *ops);

/* Print a single character at the current cursor position, advance cursor.
 * Handles '\n' (newline) and scrolling when cursor reaches bottom. */
void console_putchar(char c);

/* Print a null-terminated string */
void console_puts(const char *str);

/* Clear the screen (fill with spaces) */
void console_clear(void);

/* Get current cursor position */
void console_get_cursor(uint16_t *row, uint16_t *col);

/* Set cursor position */
void console_set_cursor(uint16_t row, uint16_t col);

#endif
```

- [ ] **Step 2: Write failing tests**

Create `tests/test_console.c`:

```c
#include "test.h"
#include "../kernel/console.h"

/* Test backend: RAM-backed VGA buffer */
static uint16_t test_vga_buffer[VGA_WIDTH * VGA_HEIGHT];

static void test_write_cell(uint16_t row, uint16_t col, uint8_t ch, uint8_t attr) {
    test_vga_buffer[row * VGA_WIDTH + col] = (uint16_t)ch | ((uint16_t)attr << 8);
}

static uint16_t test_read_cell(uint16_t row, uint16_t col) {
    return test_vga_buffer[row * VGA_WIDTH + col];
}

static console_ops_t test_ops = {
    .write_cell = test_write_cell,
    .read_cell = test_read_cell,
};

/* Helper: extract character from VGA cell */
static char cell_char(uint16_t row, uint16_t col) {
    return (char)(test_vga_buffer[row * VGA_WIDTH + col] & 0xFF);
}

static void setup(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        test_vga_buffer[i] = 0;
    console_init(&test_ops);
}

TEST(console_clear_fills_spaces) {
    setup();
    /* Dirty the buffer */
    test_vga_buffer[0] = 0x0741; /* 'A' with grey-on-black */
    console_clear();
    /* Every cell should be space (0x20) with default attribute */
    ASSERT_EQ(cell_char(0, 0), ' ');
    ASSERT_EQ(cell_char(12, 40), ' ');
    ASSERT_EQ(cell_char(24, 79), ' ');
}

TEST(console_putchar_writes_at_cursor) {
    setup();
    console_clear();
    console_putchar('H');
    ASSERT_EQ(cell_char(0, 0), 'H');
    console_putchar('i');
    ASSERT_EQ(cell_char(0, 1), 'i');
}

TEST(console_putchar_newline_moves_to_next_row) {
    setup();
    console_clear();
    console_putchar('A');
    console_putchar('\n');
    uint16_t row, col;
    console_get_cursor(&row, &col);
    ASSERT_EQ(row, 1);
    ASSERT_EQ(col, 0);
}

TEST(console_puts_writes_string) {
    setup();
    console_clear();
    console_puts("OK");
    ASSERT_EQ(cell_char(0, 0), 'O');
    ASSERT_EQ(cell_char(0, 1), 'K');
}

TEST(console_cursor_tracks_position) {
    setup();
    console_clear();
    console_set_cursor(5, 10);
    uint16_t row, col;
    console_get_cursor(&row, &col);
    ASSERT_EQ(row, 5);
    ASSERT_EQ(col, 10);
    console_putchar('X');
    console_get_cursor(&row, &col);
    ASSERT_EQ(row, 5);
    ASSERT_EQ(col, 11);
}

TEST(console_scroll_when_past_bottom) {
    setup();
    console_clear();
    /* Fill row 0 with 'A' so we can check it scrolls up */
    console_set_cursor(0, 0);
    console_putchar('A');
    /* Move cursor to last row, last col */
    console_set_cursor(24, 79);
    console_putchar('Z');  /* fills last cell, cursor wraps */
    console_putchar('!');  /* should trigger scroll, '!' on new last row */
    /* After scroll, row 0 should now contain what was row 1 (spaces) */
    ASSERT_EQ(cell_char(0, 0), ' ');
    /* '!' should be on row 24 */
    ASSERT_EQ(cell_char(24, 0), '!');
}

void run_console_tests(const char *filter) {
    RUN_TEST_FILTERED(console_clear_fills_spaces, filter);
    RUN_TEST_FILTERED(console_putchar_writes_at_cursor, filter);
    RUN_TEST_FILTERED(console_putchar_newline_moves_to_next_row, filter);
    RUN_TEST_FILTERED(console_puts_writes_string, filter);
    RUN_TEST_FILTERED(console_cursor_tracks_position, filter);
    RUN_TEST_FILTERED(console_scroll_when_past_bottom, filter);
}
```

- [ ] **Step 3: Update tests/test_main.c to include console tests**

Add to `tests/test_main.c`:
- Forward declare: `void run_console_tests(const char *filter);`
- Add in main(): `printf("[console]\n"); run_console_tests(filter);`

- [ ] **Step 4: Update Makefile**

Add `kernel/console.c` to `KERNEL_SRCS` and `tests/test_console.c` to `TEST_SRCS`.

- [ ] **Step 5: Run tests to verify they fail**

```bash
make test
```

Expected: Linker errors — `console_init`, `console_putchar`, etc. are undefined.

- [ ] **Step 6: Implement the console driver**

Create `kernel/console.c`:

```c
#include "console.h"

/* Module state */
static uint16_t cursor_row = 0;
static uint16_t cursor_col = 0;
static uint8_t default_attr = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY; /* 0x07 */
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

/* Scroll the screen up by one row: copy each row up, clear the last row */
static void scroll_up(void) {
    for (uint16_t r = 1; r < VGA_HEIGHT; r++) {
        for (uint16_t c = 0; c < VGA_WIDTH; c++) {
            uint16_t cell = ops->read_cell(r, c);
            ops->write_cell(r - 1, c, (uint8_t)(cell & 0xFF), (uint8_t)(cell >> 8));
        }
    }
    /* Clear last row */
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
```

- [ ] **Step 7: Run tests to verify they pass**

```bash
make test
```

Expected: All console tests pass (6 new + 3 existing string tests = 9 total).

- [ ] **Step 8: Commit**

```bash
git add kernel/console.h kernel/console.c tests/test_console.c tests/test_main.c Makefile
git commit -m "Add VGA console driver with seam pattern and 6 passing tests"
```

---

### Task 2: Keyboard Input Driver

**Files:**
- Create: `kernel/keyboard.h`
- Create: `kernel/keyboard.c`
- Create: `tests/test_keyboard.c`
- Modify: `tests/test_main.c` — add keyboard test suite
- Modify: `Makefile` — add sources

The keyboard works via hardware interrupts. When a key is pressed, the keyboard controller sends a **scancode** to port 0x60, and the PIC fires IRQ1 (interrupt 33). Our ISR reads the scancode and translates it to ASCII using a lookup table.

For testing, we test the scancode-to-ASCII translation table and the key buffer logic on the host. The actual interrupt wiring is QEMU-only.

- [ ] **Step 1: Write the keyboard header**

Create `kernel/keyboard.h`:

```c
#ifndef WEEZYDOS_KEYBOARD_H
#define WEEZYDOS_KEYBOARD_H

#include "types.h"

#define KEY_BUFFER_SIZE 64

/* Keyboard operations struct (seam pattern).
 * In OS: reads scancode from port 0x60.
 * In tests: returns scancodes from a test array. */
typedef struct {
    uint8_t (*read_scancode)(void);
} keyboard_ops_t;

/* Initialize keyboard with the given ops backend */
void keyboard_init(keyboard_ops_t *ops);

/* Process a scancode (called by ISR or test harness).
 * Translates to ASCII and pushes to key buffer. */
void keyboard_handle_scancode(uint8_t scancode);

/* Read a character from the key buffer.
 * Returns 0 if buffer is empty. */
char keyboard_getchar(void);

/* Check if a character is available in the buffer */
int keyboard_has_key(void);

/* Translate a scancode to ASCII. Returns 0 for non-printable keys.
 * Exported for testing. */
char scancode_to_ascii(uint8_t scancode);

#endif
```

- [ ] **Step 2: Write failing tests**

Create `tests/test_keyboard.c`:

```c
#include "test.h"
#include "../kernel/keyboard.h"

/* Scancode set 1 values for common keys */
#define SC_A     0x1E
#define SC_B     0x1F
#define SC_ENTER 0x1C
#define SC_SPACE 0x39
#define SC_1     0x02
#define SC_SHIFT 0x2A
#define SC_A_RELEASE 0x9E  /* key release = scancode | 0x80 */

static void setup(void) {
    keyboard_init(NULL); /* no ops needed for scancode translation tests */
}

TEST(scancode_a_translates_to_a) {
    ASSERT_EQ(scancode_to_ascii(SC_A), 'a');
}

TEST(scancode_enter_translates_to_newline) {
    ASSERT_EQ(scancode_to_ascii(SC_ENTER), '\n');
}

TEST(scancode_space_translates_to_space) {
    ASSERT_EQ(scancode_to_ascii(SC_SPACE), ' ');
}

TEST(scancode_1_translates_to_1) {
    ASSERT_EQ(scancode_to_ascii(SC_1), '1');
}

TEST(scancode_release_returns_zero) {
    /* Key release scancodes (bit 7 set) should return 0 */
    ASSERT_EQ(scancode_to_ascii(SC_A_RELEASE), 0);
}

TEST(keyboard_buffer_stores_keys) {
    setup();
    keyboard_handle_scancode(SC_A);
    keyboard_handle_scancode(SC_B);
    ASSERT(keyboard_has_key());
    ASSERT_EQ(keyboard_getchar(), 'a');
    ASSERT_EQ(keyboard_getchar(), 'b');
    ASSERT(!keyboard_has_key());
}

TEST(keyboard_buffer_empty_returns_zero) {
    setup();
    ASSERT_EQ(keyboard_getchar(), 0);
}

TEST(keyboard_buffer_ignores_releases) {
    setup();
    keyboard_handle_scancode(SC_A);          /* press 'a' */
    keyboard_handle_scancode(SC_A_RELEASE);  /* release 'a' */
    ASSERT_EQ(keyboard_getchar(), 'a');
    ASSERT(!keyboard_has_key()); /* release should not add to buffer */
}

void run_keyboard_tests(const char *filter) {
    RUN_TEST_FILTERED(scancode_a_translates_to_a, filter);
    RUN_TEST_FILTERED(scancode_enter_translates_to_newline, filter);
    RUN_TEST_FILTERED(scancode_space_translates_to_space, filter);
    RUN_TEST_FILTERED(scancode_1_translates_to_1, filter);
    RUN_TEST_FILTERED(scancode_release_returns_zero, filter);
    RUN_TEST_FILTERED(keyboard_buffer_stores_keys, filter);
    RUN_TEST_FILTERED(keyboard_buffer_empty_returns_zero, filter);
    RUN_TEST_FILTERED(keyboard_buffer_ignores_releases, filter);
}
```

- [ ] **Step 3: Update tests/test_main.c**

Add forward declaration: `void run_keyboard_tests(const char *filter);`
Add in main(): `printf("[keyboard]\n"); run_keyboard_tests(filter);`

- [ ] **Step 4: Update Makefile**

Add `kernel/keyboard.c` to `KERNEL_SRCS` and `tests/test_keyboard.c` to `TEST_SRCS`.

- [ ] **Step 5: Run tests to verify they fail**

```bash
make test
```

Expected: Linker errors for undefined keyboard functions.

- [ ] **Step 6: Implement the keyboard driver**

Create `kernel/keyboard.c`:

```c
#include "keyboard.h"

/* Circular key buffer */
static char key_buffer[KEY_BUFFER_SIZE];
static int buf_head = 0;
static int buf_tail = 0;

static keyboard_ops_t *ops = NULL;

/* Scancode set 1 to ASCII lookup table (lowercase only for now).
 * Index = scancode, value = ASCII character (0 = non-printable). */
static const char scancode_table[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',  /* 0x00-0x07 */
    '7', '8', '9', '0', '-', '=', '\b', '\t', /* 0x08-0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  /* 0x10-0x17 */
    'o', 'p', '[', ']', '\n', 0,   'a', 's',  /* 0x18-0x1F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 0x20-0x27 */
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v', /* 0x28-0x2F */
    'b', 'n', 'm', ',', '.', '/', 0,   '*',   /* 0x30-0x37 */
    0,   ' ', 0,   0,   0,   0,   0,   0,     /* 0x38-0x3F */
    0,   0,   0,   0,   0,   0,   0,   '7',   /* 0x40-0x47 */
    '8', '9', '-', '4', '5', '6', '+', '1',   /* 0x48-0x4F */
    '2', '3', '0', '.', 0,   0,   0,   0,     /* 0x50-0x57 */
};

void keyboard_init(keyboard_ops_t *backend) {
    ops = backend;
    buf_head = 0;
    buf_tail = 0;
}

char scancode_to_ascii(uint8_t scancode) {
    /* Key release scancodes have bit 7 set — ignore them */
    if (scancode & 0x80) {
        return 0;
    }
    if (scancode >= 128) {
        return 0;
    }
    return scancode_table[scancode];
}

static void buffer_push(char c) {
    int next_head = (buf_head + 1) % KEY_BUFFER_SIZE;
    if (next_head == buf_tail) {
        return; /* buffer full, drop the key */
    }
    key_buffer[buf_head] = c;
    buf_head = next_head;
}

void keyboard_handle_scancode(uint8_t scancode) {
    char c = scancode_to_ascii(scancode);
    if (c != 0) {
        buffer_push(c);
    }
}

int keyboard_has_key(void) {
    return buf_head != buf_tail;
}

char keyboard_getchar(void) {
    if (!keyboard_has_key()) {
        return 0;
    }
    char c = key_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}
```

- [ ] **Step 7: Run tests to verify they pass**

```bash
make test
```

Expected: All 17 tests pass (3 string + 6 console + 8 keyboard).

- [ ] **Step 8: Commit**

```bash
git add kernel/keyboard.h kernel/keyboard.c tests/test_keyboard.c tests/test_main.c Makefile
git commit -m "Add keyboard driver with scancode translation and key buffer"
```

---

### Task 3: Shell Line Input (Host-Testable)

**Files:**
- Create: `kernel/shell.h`
- Create: `kernel/shell.c`
- Create: `tests/test_shell.c`
- Modify: `tests/test_main.c` — add shell test suite
- Modify: `Makefile` — add sources

The shell's line-reading logic takes characters one at a time and builds a line buffer. When it sees '\n', the line is complete. When it sees '\b' (backspace), it removes the last character. This logic is fully testable on the host — no keyboard or VGA needed.

- [ ] **Step 1: Write the shell header**

Create `kernel/shell.h`:

```c
#ifndef WEEZYDOS_SHELL_H
#define WEEZYDOS_SHELL_H

#include "types.h"

#define SHELL_LINE_MAX 256

/* Shell I/O operations (seam pattern).
 * In OS: reads from keyboard, writes to VGA console.
 * In tests: reads from a test string, writes to a buffer. */
typedef struct {
    /* Get next character (blocks in OS, returns from test string in tests) */
    char (*getchar)(void);
    /* Output a character */
    void (*putchar)(char c);
    /* Output a string */
    void (*puts)(const char *str);
} shell_io_t;

/* Initialize the shell with I/O operations */
void shell_init(shell_io_t *io);

/* Read a line of input into buf (up to max_len-1 chars + null terminator).
 * Handles echoing characters and backspace.
 * Returns the number of characters read (not counting null terminator). */
int shell_readline(char *buf, int max_len);

/* Run the shell loop (blocks forever in OS). */
void shell_run(void);

#endif
```

- [ ] **Step 2: Write failing tests**

Create `tests/test_shell.c`:

```c
#include "test.h"
#include "../kernel/shell.h"

/* Test I/O backend */
static const char *test_input_ptr;
static char test_output[512];
static int test_output_pos;

static char test_getchar(void) {
    if (*test_input_ptr == '\0') return '\n'; /* auto-terminate */
    return *test_input_ptr++;
}

static void test_putchar(char c) {
    if (test_output_pos < (int)sizeof(test_output) - 1) {
        test_output[test_output_pos++] = c;
    }
    test_output[test_output_pos] = '\0';
}

static void test_puts(const char *str) {
    while (*str) {
        test_putchar(*str);
        str++;
    }
}

static shell_io_t test_io = {
    .getchar = test_getchar,
    .putchar = test_putchar,
    .puts = test_puts,
};

static void setup(const char *input) {
    test_input_ptr = input;
    test_output_pos = 0;
    test_output[0] = '\0';
    shell_init(&test_io);
}

TEST(shell_readline_simple) {
    setup("hello\n");
    char buf[SHELL_LINE_MAX];
    int len = shell_readline(buf, SHELL_LINE_MAX);
    ASSERT_STR_EQ(buf, "hello");
    ASSERT_EQ(len, 5);
}

TEST(shell_readline_empty) {
    setup("\n");
    char buf[SHELL_LINE_MAX];
    int len = shell_readline(buf, SHELL_LINE_MAX);
    ASSERT_STR_EQ(buf, "");
    ASSERT_EQ(len, 0);
}

TEST(shell_readline_backspace) {
    setup("helo\bo\n");
    char buf[SHELL_LINE_MAX];
    int len = shell_readline(buf, SHELL_LINE_MAX);
    ASSERT_STR_EQ(buf, "helo");
    ASSERT_EQ(len, 4);
}

TEST(shell_readline_backspace_at_start) {
    setup("\bhello\n");
    char buf[SHELL_LINE_MAX];
    int len = shell_readline(buf, SHELL_LINE_MAX);
    ASSERT_STR_EQ(buf, "hello");
    ASSERT_EQ(len, 5);
}

TEST(shell_readline_echoes_input) {
    setup("ab\n");
    char buf[SHELL_LINE_MAX];
    shell_readline(buf, SHELL_LINE_MAX);
    /* Output should contain the echoed characters */
    ASSERT_EQ(test_output[0], 'a');
    ASSERT_EQ(test_output[1], 'b');
}

TEST(shell_readline_truncates_long_input) {
    setup("abcde\n");
    char buf[4]; /* max 3 chars + null */
    int len = shell_readline(buf, 4);
    ASSERT_STR_EQ(buf, "abc");
    ASSERT_EQ(len, 3);
}

void run_shell_tests(const char *filter) {
    RUN_TEST_FILTERED(shell_readline_simple, filter);
    RUN_TEST_FILTERED(shell_readline_empty, filter);
    RUN_TEST_FILTERED(shell_readline_backspace, filter);
    RUN_TEST_FILTERED(shell_readline_backspace_at_start, filter);
    RUN_TEST_FILTERED(shell_readline_echoes_input, filter);
    RUN_TEST_FILTERED(shell_readline_truncates_long_input, filter);
}
```

- [ ] **Step 3: Update tests/test_main.c**

Add forward declaration: `void run_shell_tests(const char *filter);`
Add in main(): `printf("[shell]\n"); run_shell_tests(filter);`

- [ ] **Step 4: Update Makefile**

Add `kernel/shell.c` to `KERNEL_SRCS` and `tests/test_shell.c` to `TEST_SRCS`.

- [ ] **Step 5: Run tests to verify they fail**

```bash
make test
```

Expected: Linker errors for shell functions.

- [ ] **Step 6: Implement the shell**

Create `kernel/shell.c`:

```c
#include "shell.h"
#include "string.h"

static shell_io_t *io = NULL;

void shell_init(shell_io_t *shell_io) {
    io = shell_io;
}

int shell_readline(char *buf, int max_len) {
    int pos = 0;
    while (1) {
        char c = io->getchar();
        if (c == '\n') {
            io->putchar('\n');
            buf[pos] = '\0';
            return pos;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                /* Erase character on screen: backspace, space, backspace */
                io->putchar('\b');
                io->putchar(' ');
                io->putchar('\b');
            }
        } else {
            if (pos < max_len - 1) {
                buf[pos++] = c;
                io->putchar(c);
            }
        }
    }
}

void shell_run(void) {
    char line[SHELL_LINE_MAX];

    io->puts("weezyDOS v0.1\n");
    io->puts("640K conventional memory\n\n");

    while (1) {
        io->puts("A:\\> ");
        shell_readline(line, SHELL_LINE_MAX);
        /* For Phase 1: just echo the line back */
        if (wdos_strlen(line) > 0) {
            io->puts(line);
            io->putchar('\n');
        }
    }
}
```

- [ ] **Step 7: Run tests to verify they pass**

```bash
make test
```

Expected: All 23 tests pass (3 string + 6 console + 8 keyboard + 6 shell).

- [ ] **Step 8: Commit**

```bash
git add kernel/shell.h kernel/shell.c tests/test_shell.c tests/test_main.c Makefile
git commit -m "Add shell with line input, backspace handling, and echo"
```

---

### Task 4: Rewrite Stage 1 Bootloader to Load Stage 2

**Files:**
- Modify: `boot/stage1.asm` — replace stub with real disk-loading bootloader

This is pure assembly, QEMU-tested only. Stage 1 reads Stage 2 from disk sectors 2+ into memory at 0x8000 using BIOS INT 0x13, then jumps there.

Important x86 boot concepts for this task:
- BIOS stores the boot drive number in DL when it jumps to 0x7C00. We must save it.
- INT 0x13 AH=0x02: read sectors. AL=count, CH=cylinder, CL=sector (1-based), DH=head, DL=drive, ES:BX=destination.
- CHS addressing: sector numbers start at 1 (not 0). Sector 1 is the MBR itself.

- [ ] **Step 1: Rewrite boot/stage1.asm**

```nasm
; weezyDOS Stage 1 Bootloader
; Loaded by BIOS at 0x7C00
; Job: Load Stage 2 from disk sectors 2-17 to 0x8000, then jump there.

[BITS 16]
[ORG 0x7C00]

STAGE2_ADDR   equ 0x8000   ; Where to load Stage 2
STAGE2_SECTORS equ 16      ; Number of sectors to load (8KB — plenty for Stage 2)

start:
    ; BIOS passes boot drive number in DL — save it
    mov [boot_drive], dl

    ; Set up segments and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows downward from 0x7C00

    ; Print 'W' to show Stage 1 is alive
    mov ah, 0x0E
    mov al, 'W'
    int 0x10

    ; Load Stage 2 from disk
    ; INT 0x13 AH=0x02: Read sectors
    ;   AL = number of sectors to read
    ;   CH = cylinder number (0)
    ;   CL = starting sector number (2 — sector 1 is the MBR)
    ;   DH = head number (0)
    ;   DL = drive number (saved from BIOS)
    ;   ES:BX = destination address
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Start at sector 2 (sector 1 = MBR)
    mov dh, 0               ; Head 0
    mov dl, [boot_drive]
    mov bx, STAGE2_ADDR     ; ES:BX = 0x0000:0x8000
    int 0x13

    ; Check for disk read error (carry flag set on error)
    jc disk_error

    ; Verify we read the expected number of sectors
    cmp al, STAGE2_SECTORS
    jne disk_error

    ; Print '1' to show Stage 1 loaded Stage 2 OK
    mov ah, 0x0E
    mov al, '1'
    int 0x10

    ; Jump to Stage 2
    ; Pass boot drive number in DL (convention)
    mov dl, [boot_drive]
    jmp STAGE2_ADDR

disk_error:
    ; Print 'E' for error, then halt
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt
    jmp $

; Data
boot_drive: db 0

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
```

- [ ] **Step 2: Assemble and verify**

```bash
make boot
wc -c build/stage1.bin
```

Expected: Assembles without errors. File is exactly 512 bytes.

- [ ] **Step 3: Commit**

```bash
git add boot/stage1.asm
git commit -m "Stage 1 bootloader loads Stage 2 from disk via BIOS INT 0x13"
```

---

### Task 5: Stage 2 Bootloader — GDT, A20, Protected Mode, Kernel Load

**Files:**
- Create: `boot/stage2.asm`
- Modify: `Makefile` — add stage2 assembly target

This is the most complex assembly in the entire project. Stage 2 runs in real mode, enables the A20 gate, loads the kernel from disk, sets up the GDT, switches to protected mode, and jumps to kmain().

Key concepts:
- **A20 gate:** Address line 20 is disabled by default for backward compatibility. We enable it via the keyboard controller (port 0x64/0x60) so we can address memory above 1MB.
- **GDT:** A table of 8-byte segment descriptors. Entry 0 must be null. We define a code segment (base=0, limit=4GB, executable, readable) and a data segment (base=0, limit=4GB, writable). Both use 32-bit mode and 4KB granularity.
- **CR0 bit 0:** Setting this bit switches from real mode to protected mode. After setting it, the CPU is in a weird hybrid state until we do a far jump to flush the instruction pipeline and load the new code segment selector.
- **Far jump:** `jmp 0x08:label` — jumps to `label` and loads CS with selector 0x08 (our code segment descriptor, which is the 2nd entry in the GDT at offset 8).

- [ ] **Step 1: Create boot/stage2.asm**

```nasm
; weezyDOS Stage 2 Bootloader
; Loaded at 0x8000 by Stage 1
; Job: Enable A20, load kernel, set up GDT, switch to protected mode, jump to kmain()

[BITS 16]
[ORG 0x8000]

KERNEL_ADDR    equ 0x10000  ; Physical address to load kernel (64KB mark)
KERNEL_SEGMENT equ 0x1000   ; Segment for real-mode loading (0x1000 << 4 = 0x10000)
KERNEL_SECTORS equ 32       ; Sectors to load (16KB — enough for Phase 1 kernel)
KERNEL_START_SECTOR equ 18  ; Sector after Stage 1 (1) + Stage 2 (16) = sector 18

start_stage2:
    ; Save boot drive (passed in DL from Stage 1)
    mov [boot_drive], dl

    ; Print '2' to show Stage 2 is running
    mov ah, 0x0E
    mov al, '2'
    int 0x10

    ; =========================================
    ; Step 1: Enable A20 gate
    ; =========================================
    ; The A20 address line is disabled by default (8086 compatibility).
    ; With A20 disabled, addresses above 1MB wrap around to 0.
    ; We use the "Fast A20" method via port 0x92 — simpler than the
    ; keyboard controller method and works on modern hardware/QEMU.

    in al, 0x92             ; Read System Control Port A
    or al, 0x02             ; Set bit 1 (A20 enable)
    and al, 0xFE            ; Clear bit 0 (don't accidentally reset CPU!)
    out 0x92, al            ; Write back — A20 is now enabled

    ; =========================================
    ; Step 2: Load kernel from disk
    ; =========================================
    ; We must load the kernel while still in real mode because BIOS
    ; INT 0x13 disk services are not available in protected mode.
    ;
    ; Loading to 0x10000: We set ES = 0x1000 and BX = 0, so
    ; the BIOS loads to ES:BX = 0x1000:0x0000 = physical 0x10000.

    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor bx, bx             ; ES:BX = 0x1000:0x0000 = 0x10000

    mov ah, 0x02            ; BIOS read sectors
    mov al, KERNEL_SECTORS  ; Number of sectors
    mov ch, 0               ; Cylinder 0
    mov cl, KERNEL_START_SECTOR  ; Starting sector
    mov dh, 0               ; Head 0
    mov dl, [boot_drive]
    int 0x13

    jc .disk_error

    ; Restore ES to 0
    xor ax, ax
    mov es, ax

    ; Print 'K' to show kernel loaded
    mov ah, 0x0E
    mov al, 'K'
    int 0x10

    ; =========================================
    ; Step 3: Switch to protected mode
    ; =========================================

    ; Disable interrupts — we're about to change the interrupt system.
    ; Real-mode BIOS interrupt handlers won't work in protected mode.
    cli

    ; Load the GDT register with the address and size of our GDT
    lgdt [gdt_descriptor]

    ; Set bit 0 of CR0 to enable protected mode
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax

    ; Far jump to 32-bit code segment.
    ; 0x08 is the selector for our code segment (2nd GDT entry, offset 8).
    ; This jump does two critical things:
    ;   1. Loads CS with the new code segment selector
    ;   2. Flushes the CPU prefetch queue (which had 16-bit instructions)
    jmp 0x08:protected_mode_entry

.disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt
    jmp $

; =========================================
; Data: Boot drive
; =========================================
boot_drive: db 0

; =========================================
; GDT (Global Descriptor Table)
; =========================================
; Each entry is 8 bytes. We define 3 entries:
;   0x00: Null descriptor (required, CPU ignores it)
;   0x08: Code segment — base=0, limit=4GB, executable, readable, 32-bit
;   0x10: Data segment — base=0, limit=4GB, writable, 32-bit
;
; This "flat model" makes segmentation transparent: all memory is one
; contiguous 4GB space. Pointers in C just work as linear addresses.

gdt_start:

; Null descriptor (8 bytes of zeros) — required as entry 0
gdt_null:
    dq 0

; Code segment descriptor (selector 0x08)
;   Base = 0x00000000, Limit = 0xFFFFF (with 4KB granularity = 4GB)
;   Access: present(1), ring 0(00), code/data(1), executable(1), readable(1) = 0x9A
;   Flags: 4KB granularity(1), 32-bit(1), not 64-bit(0), no limit high(0) = 0xC
gdt_code:
    dw 0xFFFF       ; Limit bits 0-15
    dw 0x0000       ; Base bits 0-15
    db 0x00         ; Base bits 16-23
    db 10011010b    ; Access byte: present, ring 0, code segment, executable, readable
    db 11001111b    ; Flags (4 bits) + Limit bits 16-19: 4KB gran, 32-bit, limit=0xF
    db 0x00         ; Base bits 24-31

; Data segment descriptor (selector 0x10)
;   Same as code segment but with data access flags
;   Access: present(1), ring 0(00), code/data(1), data(0), writable(1) = 0x92
gdt_data:
    dw 0xFFFF       ; Limit bits 0-15
    dw 0x0000       ; Base bits 0-15
    db 0x00         ; Base bits 16-23
    db 10010010b    ; Access byte: present, ring 0, data segment, writable
    db 11001111b    ; Flags + Limit bits 16-19: same as code
    db 0x00         ; Base bits 24-31

gdt_end:

; GDT descriptor: tells LGDT the size and address of the GDT
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; Size of GDT minus 1
    dd gdt_start                  ; Linear address of GDT

; =========================================
; 32-bit Protected Mode Code
; =========================================
[BITS 32]

protected_mode_entry:
    ; Now running in 32-bit protected mode!
    ; Reload data segment registers with our data segment selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up a 32-bit stack at top of conventional memory
    mov esp, 0x9FC00

    ; Print 'P' directly to VGA memory to show we're in protected mode
    ; VGA text buffer is at 0xB8000. Each cell = 2 bytes (char + attribute)
    mov byte [0xB8006], 'P'     ; Character at position 3 (offset 6)
    mov byte [0xB8007], 0x0A    ; Light green on black

    ; Jump to the C kernel entry point!
    ; The kernel was loaded at KERNEL_ADDR (0x10000) by our disk read above.
    jmp KERNEL_ADDR
```

- [ ] **Step 2: Update Makefile to assemble stage2**

Add to the Makefile, after the stage1.bin target:

```makefile
$(BUILD_DIR)/stage2.bin: boot/stage2.asm | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@
```

Update the `boot` target:

```makefile
.PHONY: boot
boot: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin
	@echo "Boot sectors assembled"
```

- [ ] **Step 3: Assemble and verify**

```bash
make boot
wc -c build/stage2.bin
```

Expected: Assembles without errors. File size should be a few hundred bytes.

- [ ] **Step 4: Commit**

```bash
git add boot/stage2.asm Makefile
git commit -m "Stage 2 bootloader: A20 gate, GDT, protected mode, kernel load"
```

---

### Task 6: Kernel Entry Point and IDT Setup

**Files:**
- Create: `kernel/main.c`
- Create: `kernel/idt.h`
- Create: `kernel/idt.c`
- Create: `kernel/port_io.h`
- Modify: `Makefile` — add sources, add kernel linking and disk image targets

The kernel entry point (`kmain`) initializes all subsystems. The IDT (Interrupt Descriptor Table) and PIC (Programmable Interrupt Controller) setup is necessary for keyboard interrupts.

Key concepts:
- **IDT:** An array of 256 gate descriptors. Each entry maps an interrupt number to a handler function. The CPU loads the IDT address via the `lidt` instruction.
- **PIC remapping:** The 8259A PIC maps hardware IRQs 0-7 to interrupts 8-15 by default, which conflicts with CPU exception interrupts. We remap them to 32-47 so IRQ0=INT 32, IRQ1=INT 33 (keyboard), etc.
- **ISR stub:** The keyboard interrupt handler must be an assembly stub that saves registers, calls our C handler, sends End-of-Interrupt to the PIC, restores registers, and does `iret`.

- [ ] **Step 1: Create kernel/port_io.h**

Inline assembly wrappers for x86 port I/O. These are only used in the OS build (not tests).

```c
#ifndef WEEZYDOS_PORT_IO_H
#define WEEZYDOS_PORT_IO_H

#include "types.h"

#ifdef WEEZYDOS_TEST
/* Stubs for testing — port I/O does nothing */
static inline void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline uint8_t inb(uint16_t port) { (void)port; return 0; }
#else
/* Write a byte to an I/O port */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a byte from an I/O port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Wait for an I/O operation to complete (short delay) */
static inline void io_wait(void) {
    outb(0x80, 0);
}
#endif

#endif
```

- [ ] **Step 2: Create kernel/idt.h**

```c
#ifndef WEEZYDOS_IDT_H
#define WEEZYDOS_IDT_H

#include "types.h"

/* IDT entry (gate descriptor) — 8 bytes per entry */
typedef struct {
    uint16_t offset_low;    /* Offset bits 0-15 */
    uint16_t selector;      /* Code segment selector (0x08) */
    uint8_t  zero;          /* Always 0 */
    uint8_t  type_attr;     /* Type and attributes */
    uint16_t offset_high;   /* Offset bits 16-31 */
} __attribute__((packed)) idt_entry_t;

/* IDT pointer structure for lidt instruction */
typedef struct {
    uint16_t limit;         /* Size of IDT minus 1 */
    uint32_t base;          /* Linear address of IDT */
} __attribute__((packed)) idt_ptr_t;

/* Initialize the IDT and PIC, install keyboard ISR */
void idt_init(void);

/* The keyboard interrupt handler (called from ASM stub) */
void keyboard_interrupt_handler(void);

#endif
```

- [ ] **Step 3: Create kernel/idt.c**

```c
#include "idt.h"
#include "port_io.h"
#include "keyboard.h"

#ifndef WEEZYDOS_TEST

#define IDT_ENTRIES 256

/* PIC ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* ICW (Initialization Command Words) */
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

/* Defined in idt_asm.asm */
extern void keyboard_isr_stub(void);

static void idt_set_entry(int index, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[index].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[index].selector    = selector;
    idt[index].zero        = 0;
    idt[index].type_attr   = type_attr;
    idt[index].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

/* Remap the 8259A PIC.
 * By default, IRQ 0-7 map to INT 8-15, which conflicts with CPU exceptions.
 * We remap them to INT 32-47:
 *   IRQ 0 (timer)    → INT 32
 *   IRQ 1 (keyboard) → INT 33
 *   ...
 */
static void pic_remap(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (ICW1) */
    outb(PIC1_COMMAND, ICW1_INIT);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT);
    io_wait();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, 0x20);   /* PIC1 starts at INT 32 */
    io_wait();
    outb(PIC2_DATA, 0x28);   /* PIC2 starts at INT 40 */
    io_wait();

    /* ICW3: Tell PICs about each other */
    outb(PIC1_DATA, 0x04);   /* PIC1: slave on IRQ2 */
    io_wait();
    outb(PIC2_DATA, 0x02);   /* PIC2: cascade identity */
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void keyboard_interrupt_handler(void) {
    /* Read the scancode from the keyboard controller (port 0x60) */
    uint8_t scancode = inb(0x60);
    keyboard_handle_scancode(scancode);
}

void idt_init(void) {
    /* Set up IDT pointer */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    /* Zero out the IDT */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, 0, 0, 0);
    }

    /* Remap PIC so IRQs don't conflict with CPU exceptions */
    pic_remap();

    /* Install keyboard ISR at INT 33 (IRQ1 after remapping).
     * 0x08 = code segment selector from GDT.
     * 0x8E = present, ring 0, 32-bit interrupt gate. */
    idt_set_entry(33, (uint32_t)keyboard_isr_stub, 0x08, 0x8E);

    /* Unmask IRQ1 (keyboard) on PIC1. Mask all others. */
    outb(PIC1_DATA, 0xFD);  /* 0xFD = 11111101 — only IRQ1 unmasked */
    outb(PIC2_DATA, 0xFF);  /* Mask all IRQs on PIC2 */

    /* Load the IDT */
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));

    /* Enable interrupts */
    __asm__ volatile ("sti");
}

#else
/* Test build: no-op */
void idt_init(void) {}
void keyboard_interrupt_handler(void) {}
#endif
```

- [ ] **Step 4: Create boot/idt_asm.asm**

The keyboard ISR stub — saves registers, calls C handler, sends EOI, restores, returns.

```nasm
; Keyboard interrupt service routine stub
; Saves registers, calls C handler, sends EOI to PIC, restores, returns

[BITS 32]
[GLOBAL keyboard_isr_stub]
[EXTERN keyboard_interrupt_handler]

keyboard_isr_stub:
    ; Save all general-purpose registers
    pushad

    ; Call our C keyboard handler
    call keyboard_interrupt_handler

    ; Send End-of-Interrupt (EOI) to PIC1
    ; This tells the PIC we've handled the interrupt and it can send more
    mov al, 0x20
    out 0x20, al

    ; Restore registers
    popad

    ; Return from interrupt — pops CS, EIP, EFLAGS that the CPU pushed
    iret
```

- [ ] **Step 5: Create kernel/main.c**

```c
#include "types.h"

#ifndef WEEZYDOS_TEST

#include "console.h"
#include "keyboard.h"
#include "shell.h"
#include "idt.h"

/* VGA hardware backend — writes directly to VGA memory at 0xB8000 */
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

/* Keyboard hardware backend — reads scancodes from port 0x60 (handled by ISR) */
/* No ops needed here — the ISR calls keyboard_handle_scancode directly */

/* Shell I/O: connects shell to console + keyboard */
static char shell_getchar_impl(void) {
    /* Busy-wait for keyboard input */
    while (!keyboard_has_key()) {
        __asm__ volatile ("hlt"); /* Halt until next interrupt */
    }
    return keyboard_getchar();
}

static shell_io_t os_shell_io = {
    .getchar = shell_getchar_impl,
    .putchar = console_putchar,
    .puts = console_puts,
};

/* Kernel entry point — called by Stage 2 bootloader */
void kmain(void) {
    /* Initialize console (VGA text mode) */
    console_init(&vga_ops);
    console_clear();

    /* Initialize keyboard */
    keyboard_init(NULL);

    /* Initialize IDT and enable keyboard interrupts */
    idt_init();

    /* Initialize and run shell */
    shell_init(&os_shell_io);
    shell_run(); /* Never returns */
}

#endif
```

- [ ] **Step 6: Update Makefile for full kernel build and disk image**

Replace the Makefile content with the complete build system. The key additions:
- Assemble `boot/idt_asm.asm` as ELF32 object
- Link kernel objects with `ld.lld` using `link.ld`
- Extract flat binary from ELF with `objcopy`
- Concatenate stage1 + stage2 + kernel into disk image
- Pad disk image to proper floppy size (1.44MB)

The Makefile should be updated so these targets exist:

```makefile
# weezyDOS Build System

# --- Toolchain ---
CC       := clang
ASM      := nasm
LD       := ld.lld
OBJCOPY  := llvm-objcopy

# --- Cross-compilation flags (OS build) ---
CFLAGS_KERNEL := -target i686-elf -ffreestanding -nostdlib -Wall -Wextra -std=c11 -O1
ASMFLAGS_BIN  := -f bin
ASMFLAGS_ELF  := -f elf32

# --- Native flags (test build) ---
CFLAGS_TEST   := -DWEEZYDOS_TEST -Wall -Wextra -std=c11 -g -fsanitize=address,undefined

# --- Directories ---
BUILD_DIR := build

# --- Kernel source files ---
KERNEL_C_SRCS := kernel/string.c kernel/console.c kernel/keyboard.c kernel/shell.c kernel/idt.c kernel/main.c
KERNEL_C_OBJS := $(patsubst kernel/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))

# --- ASM kernel objects (32-bit ELF, linked with kernel) ---
KERNEL_ASM_SRCS := boot/idt_asm.asm
KERNEL_ASM_OBJS := $(patsubst boot/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# --- Test source files ---
TEST_SRCS := tests/test_main.c tests/test_string.c tests/test_console.c tests/test_keyboard.c tests/test_shell.c

# --- Disk layout ---
# Sector size = 512 bytes
# Sector 1:     Stage 1 (MBR)
# Sectors 2-17: Stage 2 (16 sectors = 8KB)
# Sectors 18+:  Kernel
STAGE2_SECTORS := 16
FLOPPY_SIZE    := 1474560

# ============================
# OS Build Targets
# ============================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble boot sectors (flat binary)
$(BUILD_DIR)/stage1.bin: boot/stage1.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_BIN) $< -o $@

$(BUILD_DIR)/stage2.bin: boot/stage2.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_BIN) $< -o $@

# Assemble kernel ASM files (ELF objects for linking)
$(BUILD_DIR)/%.o: boot/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

# Cross-compile kernel C files
$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

# Link kernel ELF
$(BUILD_DIR)/kernel.elf: $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS) link.ld | $(BUILD_DIR)
	$(LD) -T link.ld -o $@ $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

# Extract flat binary from ELF
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# Build the disk image
# Layout: [stage1 512B][stage2 padded to 8KB][kernel]
$(BUILD_DIR)/disk.img: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	@# Start with stage1 (512 bytes)
	cp $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/disk.img
	@# Pad stage2 to exactly STAGE2_SECTORS * 512 bytes and append
	dd if=$(BUILD_DIR)/stage2.bin of=$(BUILD_DIR)/stage2_padded.bin bs=512 count=$(STAGE2_SECTORS) conv=sync 2>/dev/null
	cat $(BUILD_DIR)/stage2_padded.bin >> $(BUILD_DIR)/disk.img
	@# Append kernel
	cat $(BUILD_DIR)/kernel.bin >> $(BUILD_DIR)/disk.img
	@# Pad to floppy size (1.44MB) for QEMU
	truncate -s $(FLOPPY_SIZE) $(BUILD_DIR)/disk.img
	@echo "Disk image built: $(BUILD_DIR)/disk.img"

# ============================
# Test Targets
# ============================

$(BUILD_DIR)/test_runner: $(KERNEL_C_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_TEST) -I. -o $@ $(TEST_SRCS) $(KERNEL_C_SRCS)

.PHONY: test
test: $(BUILD_DIR)/test_runner
	./$(BUILD_DIR)/test_runner

# ============================
# Convenience
# ============================

.PHONY: all
all: $(BUILD_DIR)/disk.img

.PHONY: boot
boot: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin
	@echo "Boot sectors assembled"

.PHONY: run
run: all
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/disk.img -nographic -serial mon:stdio

.PHONY: run-gui
run-gui: all
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/disk.img

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
```

- [ ] **Step 7: Verify tests still pass**

```bash
make test
```

Expected: All tests pass (tests don't compile idt.c or main.c code since they're behind `#ifndef WEEZYDOS_TEST`).

- [ ] **Step 8: Build the full disk image**

```bash
make all
```

Expected: Assembles both boot stages, cross-compiles kernel, links, creates disk.img.

Note: if `llvm-objcopy` is not installed, install it or use `objcopy` from binutils. Adjust the `OBJCOPY` variable in the Makefile accordingly.

- [ ] **Step 9: Commit**

```bash
git add kernel/main.c kernel/idt.h kernel/idt.c kernel/port_io.h boot/idt_asm.asm Makefile
git commit -m "Add kernel entry, IDT/PIC setup, keyboard ISR, and full build pipeline"
```

---

### Task 7: Boot Test in QEMU

**Files:** None — verification and fixes only.

This task is the integration test. Boot the disk image in QEMU and verify:
1. Stage 1 prints 'W'
2. Stage 2 prints '2', 'K', and 'P'
3. The screen clears and the welcome banner appears
4. The `A:\>` prompt is displayed
5. Typing characters echoes them
6. Pressing Enter echoes the line back

- [ ] **Step 1: Boot in QEMU with GUI**

```bash
make run-gui
```

Or with the text-mode console:

```bash
make run
```

Watch for the boot sequence characters: W, 1, 2, K (these flash by quickly), then 'P' appears directly in VGA memory, then the screen clears and the shell starts.

- [ ] **Step 2: Test keyboard input**

At the `A:\>` prompt:
- Type `hello` — should echo each character
- Press Enter — should echo "hello" back on the next line
- Press Backspace — should erase the last character
- Press Enter on an empty line — should just show a new prompt

- [ ] **Step 3: Debug any issues**

Common problems:
- **Triple fault / reboot loop:** Usually a GDT or IDT issue. Check that the GDT descriptor base address is correct for ORG 0x8000.
- **No keyboard response:** Check PIC remapping and IRQ1 unmasking. Verify the ISR stub is correctly saving/restoring registers and sending EOI.
- **Garbled text:** Check that VGA write_cell is calculating the buffer offset correctly.
- **Cursor not moving:** Check that port I/O for the VGA cursor register is implemented if needed (for the blinking cursor). The OS works without a blinking cursor — characters still appear.

- [ ] **Step 4: Commit any fixes**

```bash
git add -A
git commit -m "Fix boot issues found during QEMU integration testing"
```

(Only commit if there were fixes needed.)

- [ ] **Step 5: Final verification**

```bash
make clean && make test && make all
```

All tests pass AND the disk image builds. Boot in QEMU one more time to confirm everything works end-to-end.

- [ ] **Step 6: Commit the completed Phase 1**

```bash
git add -A
git commit -m "Phase 1 complete: weezyDOS boots to shell prompt in QEMU"
```

**Phase 1 Milestone:** weezyDOS boots in QEMU, shows `A:\>` prompt, echoes typed text. All host-side tests pass.
