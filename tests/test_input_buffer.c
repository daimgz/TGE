#include "tge-extra/input_buffer.h"

#include "tge_test.h"

TGE_TEST(fifo_order)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 4);
    tge_input_buffer_push(&b, TGE_DIR_UP);
    tge_input_buffer_push(&b, TGE_DIR_LEFT);
    tge_input_buffer_push(&b, TGE_DIR_DOWN);
    TGE_Direction d;
    TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_UP, "first out is UP");
    TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_LEFT, "then LEFT");
    TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_DOWN, "then DOWN");
    TGE_ASSERT(!tge_input_buffer_pop(&b, &d), "empty after draining");
}

TGE_TEST(wraps_around)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 4);
    for (int i = 0; i < 3; i++)
        tge_input_buffer_push(&b, TGE_DIR_RIGHT);
    TGE_Direction d;
    for (int i = 0; i < 3; i++)
        TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_RIGHT, "wraps");
    for (int i = 0; i < 3; i++)
        tge_input_buffer_push(&b, TGE_DIR_UP);
    TGE_ASSERT(tge_input_buffer_count(&b) == 3, "head wrapped, count correct");
}

TGE_TEST(full_drops_new)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 2);
    TGE_ASSERT(tge_input_buffer_push(&b, TGE_DIR_UP), "first fits");
    TGE_ASSERT(tge_input_buffer_push(&b, TGE_DIR_LEFT), "second fits");
    TGE_ASSERT(!tge_input_buffer_push(&b, TGE_DIR_DOWN), "full: new dropped");
    TGE_ASSERT(tge_input_buffer_count(&b) == 2, "count unchanged after drop");
    TGE_Direction d;
    TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_UP, "old kept: UP");
    TGE_ASSERT(tge_input_buffer_pop(&b, &d) && d == TGE_DIR_LEFT, "old kept: LEFT");
}

TGE_TEST(empty_pop_fails)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 4);
    TGE_Direction d = TGE_DIR_NONE;
    TGE_ASSERT(!tge_input_buffer_pop(&b, &d), "empty pop fails");
}

TGE_TEST(clear_empties)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 4);
    tge_input_buffer_push(&b, TGE_DIR_UP);
    tge_input_buffer_push(&b, TGE_DIR_DOWN);
    tge_input_buffer_clear(&b);
    TGE_ASSERT(tge_input_buffer_count(&b) == 0, "clear empties");
    TGE_Direction d;
    TGE_ASSERT(!tge_input_buffer_pop(&b, &d), "nothing after clear");
    TGE_ASSERT(tge_input_buffer_push(&b, TGE_DIR_LEFT), "reusable after clear");
}

TGE_TEST(capacity_clamped)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, -5);
    TGE_ASSERT(tge_input_buffer_count(&b) == 0, "negative capacity to zero");
    TGE_ASSERT(!tge_input_buffer_push(&b, TGE_DIR_UP), "zero capacity drops all");
    tge_input_buffer_init(&b, 100);
    for (int i = 0; i < TGE_INPUT_BUFFER_MAX; i++)
        TGE_ASSERT(tge_input_buffer_push(&b, TGE_DIR_RIGHT), "fills up to MAX");
    TGE_ASSERT(!tge_input_buffer_push(&b, TGE_DIR_UP), "beyond MAX dropped");
    TGE_ASSERT(tge_input_buffer_count(&b) == TGE_INPUT_BUFFER_MAX, "count capped");
}

TGE_TEST(reuses_freed_slot)
{
    TGE_InputBuffer b;
    tge_input_buffer_init(&b, 2);
    tge_input_buffer_push(&b, TGE_DIR_UP);
    tge_input_buffer_push(&b, TGE_DIR_LEFT);
    TGE_Direction d;
    tge_input_buffer_pop(&b, &d);
    TGE_ASSERT(tge_input_buffer_push(&b, TGE_DIR_DOWN), "slot freed by pop");
    TGE_ASSERT(tge_input_buffer_count(&b) == 2, "count back to full");
    tge_input_buffer_pop(&b, &d);
    TGE_ASSERT(d == TGE_DIR_LEFT, "FIFO order across wrap");
    tge_input_buffer_pop(&b, &d);
    TGE_ASSERT(d == TGE_DIR_DOWN, "new item popped last");
}

TGE_TEST(null_safety)
{
    tge_input_buffer_init(NULL, 4);
    tge_input_buffer_clear(NULL);
    TGE_Direction d;
    TGE_ASSERT(!tge_input_buffer_push(NULL, TGE_DIR_UP), "NULL push false");
    TGE_ASSERT(!tge_input_buffer_pop(NULL, &d), "NULL pop false");
    TGE_ASSERT(!tge_input_buffer_pop(NULL, NULL), "NULL out false");
    TGE_ASSERT(tge_input_buffer_count(NULL) == 0, "NULL count 0");
}

int main(void)
{
    test_fifo_order();
    test_wraps_around();
    test_full_drops_new();
    test_empty_pop_fails();
    test_clear_empties();
    test_capacity_clamped();
    test_reuses_freed_slot();
    test_null_safety();
    return tge_test_report();
}
