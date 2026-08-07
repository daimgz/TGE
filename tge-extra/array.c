#include "tge-extra/array.h"

#include <stdlib.h>

bool tge_array_resize(void **ptr, int *capacity, int new_capacity,
                      size_t element_size)
{
    if (!ptr || !capacity)
        return false;
    if (new_capacity < 1)
        new_capacity = 1;
    if (new_capacity == *capacity)
        return true;
    void *resized = realloc(*ptr, (size_t)new_capacity * element_size);
    if (!resized)
        return false;
    *ptr = resized;
    *capacity = new_capacity;
    return true;
}
