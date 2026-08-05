#ifndef TGE_EXTRA_INPUT_BUFFER_H_
#define TGE_EXTRA_INPUT_BUFFER_H_

#include <stdbool.h>

#include "tge-extra/direction.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TGE_INPUT_BUFFER_MAX 16 /* storage capacity of the fixed array */

/* Fixed-capacity FIFO of buffered directions (queued turns), so fast input
 * between simulation steps is not lost. Typical use is a snake/tetris turn
 * queue: events push, the fixed-step simulation pops one per step.
 *
 *   TGE_InputBuffer input;
 *   tge_input_buffer_init(&input, 4);
 *   // event: tge_input_buffer_push(&input, dir);
 *   // per step:
 *   TGE_Direction d;
 *   while (tge_input_buffer_pop(&input, &d))
 *       apply_turn(d);
 *
 * Push semantics are drop-new: when the buffer is full the newest input is
 * rejected and the older queued ones are kept. In a game an old input usually
 * carries more intent than a brand new one (dropping the first of up/left/down
 * can lose a valid corner). No allocation: the array is embedded. */
typedef struct {
    TGE_Direction items[TGE_INPUT_BUFFER_MAX];
    int capacity; /* runtime cap set in init, clamped to [0, MAX] */
    int head;     /* index of the oldest queued item */
    int count;    /* number of queued items */
} TGE_InputBuffer;

/* Initialize empty with the given capacity (clamped to [0, TGE_INPUT_BUFFER_MAX]).
 * A capacity of 0 disables the buffer: push always fails. */
void tge_input_buffer_init(TGE_InputBuffer *buffer, int capacity);

/* Queue `d`. False when the buffer is full (the new input is dropped). */
bool tge_input_buffer_push(TGE_InputBuffer *buffer, TGE_Direction d);

/* Dequeue the oldest item into `*out`. False when the buffer is empty. */
bool tge_input_buffer_pop(TGE_InputBuffer *buffer, TGE_Direction *out);

/* Empty the buffer, keeping its capacity. */
void tge_input_buffer_clear(TGE_InputBuffer *buffer);

/* Number of queued items. */
int tge_input_buffer_count(const TGE_InputBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
