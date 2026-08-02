#include "collision.h"

#include <stdlib.h>

#define TGE_COLLISION_BUCKETS 256

struct CellBucket {
    int cx, cy;
    int *indices; /* slot indices */
    int count;
    int cap;
    struct CellBucket *next;
};

struct TGE_CollisionWorld {
    int cell;
    uint32_t *ids;
    TGE_Rect *rects;
    uint8_t  *used;
    uint32_t *stamp;   /* dedup stamp; must match query_counter to be returned once */
    int      *free_list;
    int       free_count;
    int       slot_count; /* number of slots ever handed out (>= live_count) */
    int       slot_cap;
    int       live_count;
    struct CellBucket *buckets[TGE_COLLISION_BUCKETS];
    uint32_t query_counter;
};

static uint32_t bucket_hash(int cx, int cy)
{
    uint32_t h = ((uint32_t)cx * 73856093u) ^ ((uint32_t)cy * 19349663u);
    return h & (TGE_COLLISION_BUCKETS - 1);
}

static int floor_div(int a, int b)
{
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((a < 0) != (b < 0)))
        q--;
    return q;
}

static void cell_range(TGE_Rect r, int cell, int *c0x, int *c1x, int *c0y, int *c1y)
{
    *c0x = floor_div(r.x, cell);
    *c1x = floor_div(r.x + r.w - 1, cell);
    *c0y = floor_div(r.y, cell);
    *c1y = floor_div(r.y + r.h - 1, cell);
}

static bool rects_overlap(TGE_Rect a, TGE_Rect b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

static struct CellBucket *bucket_find(const TGE_CollisionWorld *w, int cx, int cy,
                                      bool create)
{
    uint32_t h = bucket_hash(cx, cy);
    struct CellBucket *b = w->buckets[h];
    for (; b; b = b->next)
        if (b->cx == cx && b->cy == cy)
            return b;
    if (!create)
        return NULL;
    b = (struct CellBucket *)malloc(sizeof(*b));
    if (!b)
        return NULL;
    b->cx = cx;
    b->cy = cy;
    b->indices = NULL;
    b->count = 0;
    b->cap = 0;
    b->next = w->buckets[h];
    ((TGE_CollisionWorld *)w)->buckets[h] = b;
    return b;
}

static bool bucket_add(TGE_CollisionWorld *w, int cx, int cy, int slot)
{
    struct CellBucket *b = bucket_find(w, cx, cy, true);
    if (!b)
        return false;
    if (b->count == b->cap) {
        int new_cap = b->cap ? b->cap * 2 : 8;
        int *ni = (int *)realloc(b->indices, (size_t)new_cap * sizeof(int));
        if (!ni)
            return false;
        b->indices = ni;
        b->cap = new_cap;
    }
    b->indices[b->count++] = slot;
    return true;
}

static void bucket_remove_slot(struct CellBucket *b, int slot)
{
    for (int i = 0; i < b->count; i++) {
        if (b->indices[i] == slot) {
            b->indices[i] = b->indices[--b->count];
            return;
        }
    }
}

static void slot_unlink(TGE_CollisionWorld *w, int slot)
{
    TGE_Rect r = w->rects[slot];
    int c0x, c1x, c0y, c1y;
    cell_range(r, w->cell, &c0x, &c1x, &c0y, &c1y);
    for (int cy = c0y; cy <= c1y; cy++)
        for (int cx = c0x; cx <= c1x; cx++) {
            struct CellBucket *b = bucket_find(w, cx, cy, false);
            if (b)
                bucket_remove_slot(b, slot);
        }
}

static int find_slot(const TGE_CollisionWorld *w, uint32_t id)
{
    for (int i = 0; i < w->slot_count; i++)
        if (w->used[i] && w->ids[i] == id)
            return i;
    return -1;
}

static bool world_grow(TGE_CollisionWorld *w)
{
    int new_cap = w->slot_cap ? w->slot_cap * 2 : 64;
    uint32_t *ni = (uint32_t *)realloc(w->ids, (size_t)new_cap * sizeof(uint32_t));
    if (!ni)
        return false;
    w->ids = ni;
    TGE_Rect *nr = (TGE_Rect *)realloc(w->rects, (size_t)new_cap * sizeof(TGE_Rect));
    if (!nr)
        return false;
    w->rects = nr;
    uint8_t *nu = (uint8_t *)realloc(w->used, (size_t)new_cap * sizeof(uint8_t));
    if (!nu)
        return false;
    w->used = nu;
    uint32_t *ns = (uint32_t *)realloc(w->stamp, (size_t)new_cap * sizeof(uint32_t));
    if (!ns)
        return false;
    w->stamp = ns;
    int *nf = (int *)realloc(w->free_list, (size_t)new_cap * sizeof(int));
    if (!nf)
        return false;
    w->free_list = nf;
    w->slot_cap = new_cap;
    return true;
}

TGE_CollisionWorld *tge_collision_world_create(int cell_size)
{
    if (cell_size <= 0)
        return NULL;
    TGE_CollisionWorld *w = (TGE_CollisionWorld *)calloc(1, sizeof(*w));
    if (!w)
        return NULL;
    w->cell = cell_size;
    return w;
}

void tge_collision_world_destroy(TGE_CollisionWorld *w)
{
    if (!w)
        return;
    for (int i = 0; i < TGE_COLLISION_BUCKETS; i++) {
        struct CellBucket *b = w->buckets[i];
        while (b) {
            struct CellBucket *next = b->next;
            free(b->indices);
            free(b);
            b = next;
        }
    }
    free(w->ids);
    free(w->rects);
    free(w->used);
    free(w->stamp);
    free(w->free_list);
    free(w);
}

bool tge_collision_insert(TGE_CollisionWorld *w, uint32_t id, TGE_Rect r)
{
    if (!w || r.w <= 0 || r.h <= 0 || find_slot(w, id) >= 0)
        return false;
    int slot;
    if (w->free_count > 0) {
        slot = w->free_list[--w->free_count];
    } else {
        if (w->slot_count == w->slot_cap && !world_grow(w))
            return false;
        slot = w->slot_count++;
    }
    w->ids[slot] = id;
    w->rects[slot] = r;
    w->stamp[slot] = 0;
    w->used[slot] = 1;
    w->live_count++;
    int c0x, c1x, c0y, c1y;
    cell_range(r, w->cell, &c0x, &c1x, &c0y, &c1y);
    for (int cy = c0y; cy <= c1y; cy++)
        for (int cx = c0x; cx <= c1x; cx++)
            if (!bucket_add(w, cx, cy, slot)) {
                slot_unlink(w, slot);
                w->used[slot] = 0;
                w->live_count--;
                w->free_list[w->free_count++] = slot;
                return false;
            }
    return true;
}

void tge_collision_move(TGE_CollisionWorld *w, uint32_t id, TGE_Rect r)
{
    if (!w)
        return;
    int slot = find_slot(w, id);
    if (slot < 0)
        return;
    slot_unlink(w, slot);
    w->rects[slot] = r;
    if (r.w > 0 && r.h > 0) {
        int c0x, c1x, c0y, c1y;
        cell_range(r, w->cell, &c0x, &c1x, &c0y, &c1y);
        for (int cy = c0y; cy <= c1y; cy++)
            for (int cx = c0x; cx <= c1x; cx++)
                bucket_add(w, cx, cy, slot);
    }
}

void tge_collision_remove(TGE_CollisionWorld *w, uint32_t id)
{
    if (!w)
        return;
    int slot = find_slot(w, id);
    if (slot < 0)
        return;
    slot_unlink(w, slot);
    w->used[slot] = 0;
    w->live_count--;
    w->free_list[w->free_count++] = slot;
}

int tge_collision_query(const TGE_CollisionWorld *w, TGE_Rect r,
                        uint32_t *out, int max)
{
    if (!w || !out || max <= 0 || r.w <= 0 || r.h <= 0)
        return 0;
    TGE_CollisionWorld *ww = (TGE_CollisionWorld *)w; /* dedup bookkeeping */
    ww->query_counter++;
    if (ww->query_counter == 0) { /* wrapped: reset all stamps */
        for (int i = 0; i < w->slot_count; i++)
            ww->stamp[i] = 0;
        ww->query_counter = 1;
    }
    int n = 0;
    int c0x, c1x, c0y, c1y;
    cell_range(r, w->cell, &c0x, &c1x, &c0y, &c1y);
    for (int cy = c0y; cy <= c1y; cy++) {
        for (int cx = c0x; cx <= c1x; cx++) {
            struct CellBucket *b = bucket_find(w, cx, cy, false);
            if (!b)
                continue;
            for (int i = 0; i < b->count; i++) {
                int s = b->indices[i];
                if (!w->used[s])
                    continue;
                if (ww->stamp[s] == ww->query_counter)
                    continue;
                if (rects_overlap(w->rects[s], r)) {
                    if (n >= max)
                        return n;
                    ww->stamp[s] = ww->query_counter;
                    out[n++] = w->ids[s];
                }
            }
        }
    }
    return n;
}

bool tge_collision_rect_overlap(TGE_Rect a, TGE_Rect b)
{
    return rects_overlap(a, b);
}
