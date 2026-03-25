#include "test.h"
#include "../kernel/string.h"

TEST(string_length_empty) {
    ASSERT_EQ(wdos_strlen(""), 0);
}

TEST(string_length_hello) {
    ASSERT_EQ(wdos_strlen("hello"), 5);
}

TEST(string_length_one_char) {
    ASSERT_EQ(wdos_strlen("x"), 1);
}

void run_string_tests(const char *filter) {
    RUN_TEST_FILTERED(string_length_empty, filter);
    RUN_TEST_FILTERED(string_length_hello, filter);
    RUN_TEST_FILTERED(string_length_one_char, filter);
}
