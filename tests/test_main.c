#include "test.h"

/* Define the shared test counters (declared extern in test.h) */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

/* Declare test functions from test files */
void test_string_length_empty(void);
void test_string_length_hello(void);
void test_string_length_one_char(void);

/* Forward declare test runner functions */
void run_string_tests(const char *filter);
void run_console_tests(const char *filter);

int main(int argc, char *argv[]) {
    const char *filter = (argc > 1) ? argv[1] : NULL;

    printf("weezyDOS test runner\n");
    printf("====================\n\n");

    printf("[string]\n");
    run_string_tests(filter);

    printf("\n[console]\n");
    run_console_tests(filter);

    printf("\n====================\n");
    printf("Results: %d passed, %d failed, %d total\n",
           tests_passed, tests_failed, tests_run);

    return tests_failed > 0 ? 1 : 0;
}
