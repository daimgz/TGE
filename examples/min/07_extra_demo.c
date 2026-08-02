/* 07_extra_demo - the three tge-extra modules used together.
 *
 * - TGE_EntityPool holds 100 entities, each animated vertically by a
 *   TGE_Anim (loop, staggered by duration/ease).
 * - Every entity is registered in a TGE_CollisionWorld; each frame the
 *   anim is advanced, the entity's rect is moved, and a probe rect moving
 *   horizontally queries the world.
 *
 * Demonstrates that the extra modules compose without depending on each
 * other: only Entity knows the ids, only Collision knows the rects, and
 * Animation drives the motion.
 */
#include "tge/tge.h"

#include "tge-extra/animation.h"
#include "tge-extra/collision.h"
#include "tge-extra/entity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define N_ENTITIES 100
#define QUERY_MAX 64

typedef struct {
    TGE_Anim *anim;
    int x;
    int y;
} Body;

typedef struct {
    float dt;
} UpdateCtx;

static TGE_EntityPool *g_pool;
static TGE_CollisionWorld *g_world;
static TGE_Rect g_probe;
static float g_t;
static uint32_t g_hits[QUERY_MAX];
static int g_hit_count;
static int g_w, g_h;

static void update_body(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                        void *ctx)
{
    (void)pool;
    (void)id;
    Body *b = (Body *)userdata;
    UpdateCtx *u = (UpdateCtx *)ctx;
    tge_anim_update(b->anim, u->dt);
    int y = (int)tge_anim_value(b->anim);
    if (y != b->y) {
        b->y = y;
        tge_collision_move(g_world, id, tge_rect(b->x, b->y, 1, 1));
    }
}

static void draw_body(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                      void *ctx)
{
    (void)pool;
    Body *b = (Body *)userdata;
    TGE_Canvas *canvas = (TGE_Canvas *)ctx;
    bool hit = false;
    for (int i = 0; i < g_hit_count; i++)
        if (g_hits[i] == id)
            hit = true;
    TGE_Color c = hit ? TGE_COLOR_RED : TGE_COLOR_GREEN;
    if (b->x >= 0 && b->x < g_w && b->y >= 0 && b->y < g_h)
        tge_set_cell(canvas, b->x, b->y, hit ? 'X' : '.', c, TGE_COLOR_BLACK);
}

static void init(TGE_App *app)
{
    g_w = tge_runtime_width(TGE_GetRuntime(app));
    g_h = tge_runtime_height(TGE_GetRuntime(app));
    g_pool = tge_entity_pool_create(N_ENTITIES);
    g_world = tge_collision_world_create(4);
    for (int i = 0; i < N_ENTITIES; i++) {
        Body *b = (Body *)calloc(1, sizeof(*b));
        int y0 = 2 + (i % 3) * 2;
        int y1 = g_h - 3 - (i % 4);
        float dur = 1.0f + (float)(i % 5) * 0.6f;
        TGE_Ease ease = (TGE_Ease)(i % 3);
        b->anim = tge_anim_create((float)y0, (float)y1, dur, ease);
        tge_anim_set_loop(b->anim, true);
        tge_anim_play(b->anim);
        b->x = (i % 20) * 3 + 1;
        b->y = y0;
        TGE_EntityId id = tge_entity_alloc(g_pool, b);
        tge_collision_insert(g_world, id, tge_rect(b->x, b->y, 1, 1));
    }
    g_probe = tge_rect(1, g_h / 2, 1, 1);
}

static void update(TGE_App *app, float dt)
{
    (void)app;
    g_t += dt;
    int cx = g_w / 2;
    int px = cx + (int)(sinf(g_t * 0.7f) * (cx - 4));
    g_probe.x = px;
    UpdateCtx ctx = { .dt = dt };
    tge_entity_for_each(g_pool, update_body, &ctx);
    g_hit_count = tge_collision_query(g_world, g_probe, g_hits, QUERY_MAX);
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_entity_for_each(g_pool, draw_body, canvas);
    if (g_probe.x >= 0 && g_probe.x < g_w && g_probe.y >= 0 && g_probe.y < g_h)
        tge_set_cell(canvas, g_probe.x, g_probe.y, 'P', TGE_COLOR_YELLOW,
                     TGE_COLOR_BLACK);
    char line[64];
    snprintf(line, sizeof(line), "hits: %d / %d", g_hit_count, N_ENTITIES);
    tge_draw_text(canvas, 1, 1, line, TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 1, g_h - 1, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_BLACK);
}

static void free_bodies(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                        void *ctx)
{
    (void)pool;
    (void)id;
    (void)ctx;
    Body *b = (Body *)userdata;
    tge_anim_destroy(b->anim);
    free(b);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_KEYDOWN &&
        ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(app);
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 24, "TGE tge-extra demo");
    if (!app)
        return 1;
    TGE_Run(app, init, update, draw, on_event);
    tge_entity_for_each(g_pool, free_bodies, NULL);
    tge_entity_pool_destroy(g_pool);
    tge_collision_world_destroy(g_world);
    TGE_Destroy(app);
    return 0;
}
