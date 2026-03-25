#include "test.h"
#include "../kernel/keyboard.h"

#define SC_A     0x1E
#define SC_B     0x1F
#define SC_ENTER 0x1C
#define SC_SPACE 0x39
#define SC_1     0x02
#define SC_SHIFT 0x2A
#define SC_A_RELEASE 0x9E

static void setup(void) {
    keyboard_init(NULL);
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
    keyboard_handle_scancode(SC_A);
    keyboard_handle_scancode(SC_A_RELEASE);
    ASSERT_EQ(keyboard_getchar(), 'a');
    ASSERT(!keyboard_has_key());
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
