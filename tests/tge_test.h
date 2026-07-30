#ifndef TGE_TEST_H_
#define TGE_TEST_H_

#include <stdio.h>
#include <stdlib.h>

static int tge_test_count = 0;
static int tge_test_fail  = 0;

#define TGE_TEST(name) \
    static void test_##name(void)

#define TGE_ASSERT(cond, msg) do { \
    tge_test_count++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
        tge_test_fail++; \
    } \
} while (0)

static int tge_test_report(void)
{
    fprintf(stderr, "%d tests, %d failures\n", tge_test_count, tge_test_fail);
    return tge_test_fail > 0 ? 1 : 0;
}

#endif
