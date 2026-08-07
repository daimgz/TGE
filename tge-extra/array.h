#ifndef TGE_EXTRA_ARRAY_H_
#define TGE_EXTRA_ARRAY_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Grow/shrink a heap array to hold exactly `new_capacity` elements of
 * `element_size` bytes, updating the capacity counter on success. Games that
 * size a backing buffer from their playfield (snake body, breakout brick
 * map) keep one (ptr, capacity) pair and hand it to this helper instead of
 * reimplementing realloc + bookkeeping:
 *
 *   tge_array_resize(&world->body, &world->body_capacity,
 *                    world->view.area.w * world->view.area.h,
 *                    sizeof(TGE_Vec2i));
 *
 * The length in use is the caller's concern: this only resizes the backing
 * store, it never reads or writes the array contents.
 *
 * new_capacity <= 0 is treated as 1, and a new_capacity equal to *capacity
 * is a no-op. On realloc failure the function returns false leaving *ptr and
 * *capacity untouched (the old buffer stays valid), so callers keep a usable
 * buffer and only the size stays stale. */
bool tge_array_resize(void **ptr, int *capacity, int new_capacity,
                      size_t element_size);

#ifdef __cplusplus
}
#endif

#endif
