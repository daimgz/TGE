#include "tge/tge_canvas.h"
#include "tge/tge_events.h"
#include "tge_internal.h"
#include "mock_backend.h"
#include <stdio.h>
#include <stdlib.h>

static long n_malloc = 0;
static long n_calloc = 0;
static long n_realloc = 0;

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);

void *__wrap_malloc(size_t size)
{
    n_malloc++;
    return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
    n_calloc++;
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *ptr, size_t size)
{
    n_realloc++;
    return __real_realloc(ptr, size);
}

int main(void)
{
    MockData *m;
    TGE_Backend *b = mock_backend_create(&m);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, 60, 20);
    TGE_App *app = tge_app_create_with_runtime(rt, 60, 20);
    if (!app)
        return 1;
    app->fps = 0;

    n_malloc = 0;
    n_calloc = 0;
    n_realloc = 0;
    for (int i = 0; i < 60; i++)
        tge_app_frame(app);

    printf("steady-state render path (60 frames): malloc=%ld calloc=%ld "
           "realloc=%ld\n", n_malloc, n_calloc, n_realloc);
    int failed = (n_malloc + n_calloc + n_realloc) != 0;

    tge_app_destroy(app);
    if (failed) {
        printf("FAIL: allocations in render path (ADR-015 violated)\n");
        return 1;
    }
    return 0;
}
