#include "test.h"
#include "../kernel/console.h"

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
    test_vga_buffer[0] = 0x0741;
    console_clear();
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
    console_set_cursor(0, 0);
    console_putchar('A');
    console_set_cursor(24, 79);
    console_putchar('Z');
    console_putchar('!');
    ASSERT_EQ(cell_char(0, 0), ' ');
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
