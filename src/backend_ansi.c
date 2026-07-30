#include "tge_internal.h"
#include <stdlib.h>

typedef struct {
    int w, h;
} ANSIState;

static bool ansi_init(void *data, int w, int h)
{
    ANSIState *s = (ANSIState *)data;
    s->w = w; s->h = h;
    return true;
}

static void ansi_term(void *data)
{
    (void)data;
}

static int ansi_width(void *data)
{
    return ((ANSIState *)data)->w;
}

static int ansi_height(void *data)
{
    return ((ANSIState *)data)->h;
}

static void ansi_present(void *data, struct TGE_Diff *diff,
                         const TGE_Cell *cells, int stride)
{
    (void)data; (void)diff; (void)cells; (void)stride;
}

static int ansi_read_input(void *data, char *buf, int bufsize)
{
    (void)data; (void)buf; (void)bufsize;
    return 0;
}

static uint64_t ansi_ticks(void *data)
{
    (void)data;
    return 0;
}

TGE_Backend *tge_backend_ansi_create(void)
{
    TGE_Backend *b = calloc(1, sizeof(TGE_Backend));
    if (!b) return NULL;
    b->data = calloc(1, sizeof(ANSIState));
    if (!b->data) { free(b); return NULL; }
    b->init = ansi_init;
    b->term = ansi_term;
    b->width = ansi_width;
    b->height = ansi_height;
    b->present = ansi_present;
    b->read_input = ansi_read_input;
    b->ticks = ansi_ticks;
    return b;
}
