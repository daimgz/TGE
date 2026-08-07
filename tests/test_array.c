#include "tge-extra/array.h"

#include "tge_test.h"

#include <stdlib.h>

TGE_TEST(allocates_from_null)
{
    void *ptr = NULL;
    int capacity = 0;
    TGE_ASSERT(tge_array_resize(&ptr, &capacity, 10, sizeof(int)),
               "allocates");
    TGE_ASSERT(ptr != NULL, "pointer set");
    TGE_ASSERT(capacity == 10, "capacity updated");
    free(ptr);
}

TGE_TEST(noop_when_capacity_unchanged)
{
    void *ptr = malloc(5 * sizeof(int));
    int capacity = 5;
    void *before = ptr;
    TGE_ASSERT(tge_array_resize(&ptr, &capacity, 5, sizeof(int)),
               "same size is a no-op");
    TGE_ASSERT(ptr == before, "pointer untouched");
    TGE_ASSERT(capacity == 5, "capacity untouched");
    free(ptr);
}

TGE_TEST(grows_and_shrinks)
{
    void *ptr = NULL;
    int capacity = 0;
    TGE_ASSERT(tge_array_resize(&ptr, &capacity, 8, sizeof(int)), "grown");
    TGE_ASSERT(capacity == 8, "capacity grown");
    TGE_ASSERT(tge_array_resize(&ptr, &capacity, 3, sizeof(int)), "shrunk");
    TGE_ASSERT(capacity == 3, "capacity shrunk");
    free(ptr);
}

TGE_TEST(tiny_capacity_clamped_to_one)
{
    void *ptr = NULL;
    int capacity = 0;
    tge_array_resize(&ptr, &capacity, 0, sizeof(int));
    TGE_ASSERT(capacity == 1, "zero clamped to 1");
    free(ptr);

    ptr = NULL;
    capacity = 0;
    tge_array_resize(&ptr, &capacity, -4, sizeof(int));
    TGE_ASSERT(capacity == 1, "negative clamped to 1");
    free(ptr);
}

TGE_TEST(element_size_respected)
{
    void *ptr = NULL;
    int capacity = 0;
    tge_array_resize(&ptr, &capacity, 4, sizeof(char));
    char *bytes = (char *)ptr;
    for (int i = 0; i < 4; i++)
        bytes[i] = (char)('a' + i);
    TGE_ASSERT(bytes[3] == 'd', "element_size governs the buffer stride");
    free(ptr);
}

TGE_TEST(null_arguments_rejected)
{
    void *ptr = NULL;
    int capacity = 0;
    TGE_ASSERT(!tge_array_resize(NULL, &capacity, 10, sizeof(int)),
               "NULL ptr rejected");
    TGE_ASSERT(!tge_array_resize(&ptr, NULL, 10, sizeof(int)),
               "NULL capacity rejected");
    TGE_ASSERT(ptr == NULL, "nothing allocated on rejection");
}

int main(void)
{
    test_allocates_from_null();
    test_noop_when_capacity_unchanged();
    test_grows_and_shrinks();
    test_tiny_capacity_clamped_to_one();
    test_element_size_respected();
    test_null_arguments_rejected();
    return tge_test_report();
}
