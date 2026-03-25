#ifndef WEEZYDOS_TEST_H
#define WEEZYDOS_TEST_H

#include <stdio.h>
#include <string.h>

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

#define TEST(name) \
    void test_##name(void); \
    void test_##name(void)

#define ASSERT(expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("  FAIL: %s:%d: %s is NULL\n", __FILE__, __LINE__, #ptr); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define RUN_TEST(name) do { \
    int _failed_before = tests_failed; \
    tests_run++; \
    printf("  %s... ", #name); \
    test_##name(); \
    if (tests_failed == _failed_before) { \
        tests_passed++; \
        printf("OK\n"); \
    } \
} while (0)

#define RUN_TEST_FILTERED(name, filter) do { \
    if (filter == NULL || strcmp(filter, #name) == 0) { \
        RUN_TEST(name); \
    } \
} while (0)

#endif
