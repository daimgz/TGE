#include "tge-extra/input_buffer.h"

void tge_input_buffer_init(TGE_InputBuffer *buffer, int capacity)
{
    if (!buffer)
        return;
    if (capacity < 0)
        capacity = 0;
    if (capacity > TGE_INPUT_BUFFER_MAX)
        capacity = TGE_INPUT_BUFFER_MAX;
    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->count = 0;
}

bool tge_input_buffer_push(TGE_InputBuffer *buffer, TGE_Direction d)
{
    if (!buffer)
        return false;
    if (buffer->count >= buffer->capacity)
        return false;
    buffer->items[(buffer->head + buffer->count) % buffer->capacity] = d;
    buffer->count++;
    return true;
}

bool tge_input_buffer_pop(TGE_InputBuffer *buffer, TGE_Direction *out)
{
    if (!buffer || !out)
        return false;
    if (buffer->count == 0)
        return false;
    *out = buffer->items[buffer->head];
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count--;
    return true;
}

void tge_input_buffer_clear(TGE_InputBuffer *buffer)
{
    if (!buffer)
        return;
    buffer->head = 0;
    buffer->count = 0;
}

int tge_input_buffer_count(const TGE_InputBuffer *buffer)
{
    if (!buffer)
        return 0;
    return buffer->count;
}
