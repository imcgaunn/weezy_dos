#include "test.h"
#include "../kernel/shell.h"

static const char *test_input_ptr;
static char test_output[512];
static int test_output_pos;

static char test_getchar(void) {
    if (*test_input_ptr == '\0') return '\n';
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
    ASSERT_EQ(test_output[0], 'a');
    ASSERT_EQ(test_output[1], 'b');
}

TEST(shell_readline_truncates_long_input) {
    setup("abcde\n");
    char buf[4];
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
