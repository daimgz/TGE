#define _POSIX_C_SOURCE 200112L
#include "tge/tge.h"
#include "tge_internal.h"
#include <stdio.h>
#include <time.h>

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static unsigned rng_state = 12345u;
static unsigned next_rand(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static void bench_diff(int w, int h, double pct, int iters)
{
    TGE_Canvas *a = tge_canvas_create(w, h);
    TGE_Canvas *b = tge_canvas_create(w, h);
    if (!a || !b) {
        printf("canvas alloc failed\n");
        return;
    }
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    int cells = w * h;
    int marked = (int)((double)cells * pct / 100.0);
    for (int i = 0; i < marked; i++) {
        int x = (int)(next_rand() % (unsigned)w);
        int y = (int)(next_rand() % (unsigned)h);
        tge_set_cell(a, x, y, '#', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    }

    TGE_Diff diff;
    tge_diff_init(&diff, cells);
    int cap = diff.capacity;

    double t0 = now_ms();
    for (int i = 0; i < iters; i++)
        tge_renderer_diff(a, b, &diff);
    double elapsed = now_ms() - t0;

    printf("diff %dx%d %3.0f%%: %8.2f us/op  (%5d spans, cap %d%s)\n",
           w, h, pct, elapsed * 1000.0 / (double)iters, diff.count,
           diff.capacity, diff.capacity == cap ? "" : "  REALLOC");

    tge_diff_free(&diff);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

int main(void)
{
    printf("=== renderer diff ===\n");
    bench_diff(100, 30, 10.0, 10000);
    bench_diff(100, 30, 50.0, 10000);
    bench_diff(100, 30, 100.0, 10000);
    return 0;
}
