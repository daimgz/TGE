#ifndef TGE_TEST_MOCK_BACKEND_H_
#define TGE_TEST_MOCK_BACKEND_H_

#include "tge/tge_canvas.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *input;
    int input_len;
    int input_pos;
    uint64_t now_ms;
    int presented;
    int presented_full;
    TGE_Cell captured[128];
    int captured_count;
} MockData;

static bool mock_init(void *d, int w, int h)
{
    (void)d;
    (void)w;
    (void)h;
    return true;
}

static void mock_term(void *d)
{
    (void)d;
}

static int mock_width(void *d)
{
    (void)d;
    return 0;
}

static int mock_height(void *d)
{
    (void)d;
    return 0;
}

static void mock_present(void *d, TGE_Diff *diff, const TGE_Cell *cells,
                         int stride)
{
    MockData *m = (MockData *)d;
    m->presented++;
    for (int i = 0; i < diff->count; i++) {
        for (int x = diff->spans[i].x_start;
             x < diff->spans[i].x_end && m->captured_count < 128; x++) {
            m->captured[m->captured_count++] =
                cells[(size_t)diff->spans[i].y * (size_t)stride + (size_t)x];
        }
    }
}

static int mock_read_input(void *d, char *buf, int bufsize)
{
    MockData *m = (MockData *)d;
    int n = 0;
    while (m->input_pos < m->input_len && n < bufsize)
        buf[n++] = m->input[m->input_pos++];
    return n;
}

static uint64_t mock_ticks(void *d)
{
    return ((MockData *)d)->now_ms;
}

static void mock_set_input(MockData *m, const char *bytes, int len)
{
    m->input = bytes;
    m->input_len = len;
    m->input_pos = 0;
}

static TGE_Backend *mock_backend_create(MockData **out)
{
    MockData *m = (MockData *)calloc(1, sizeof(MockData));
    if (!m)
        return NULL;
    TGE_Backend *b = (TGE_Backend *)calloc(1, sizeof(TGE_Backend));
    if (!b) {
        free(m);
        return NULL;
    }
    b->data = m;
    b->init = mock_init;
    b->term = mock_term;
    b->width = mock_width;
    b->height = mock_height;
    b->present = mock_present;
    b->read_input = mock_read_input;
    b->ticks = mock_ticks;
    b->set_title = NULL;
    if (out)
        *out = m;
    return b;
}

#endif
