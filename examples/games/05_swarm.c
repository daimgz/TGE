/* 05_swarm - top-down arena shooter built on the tge-extra modules.
 *
 * This game exists to battle-test tge-extra against a real consumer; the
 * other games in this directory showcase the core only. Every extra module
 * is exercised for real:
 *
 *   - TGE_EntityPool owns every actor (player, enemies, bullets) through
 *     stable opaque handles; per-actor state lives in userdata bodies.
 *   - TGE_Anim drives enemy motion twice: an ease-out drop-in tween whose
 *     value() is the vertical spawn position, then a looping tween used as
 *     a clock for horizontal sway, sampled through progress().
 *   - TGE_CollisionWorld keeps one 1x1 rect per entity; rects are move()d
 *     every frame and query() resolves bullet-enemy, player-enemy and
 *     bottom-breach contact without allocation.
 */
#include "tge/tge.h"

#include "tge-extra/animation.h"
#include "tge-extra/collision.h"
#include "tge-extra/entity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 52
#define WIN_H 22
#define BULLET_SPEED 7.0f
#define MAX_BULLETS 6
#define MAX_ENEMIES 24
#define MAX_ENTITIES 48
#define CELL 8

typedef enum { K_PLAYER = 0, K_ENEMY, K_BULLET } Kind;
typedef enum { S_PLAYING = 0, S_WAVE, S_OVER } SwarmState;

typedef struct {
    Kind kind;
    TGE_Anim *anim;
    int x, y;
    int base_x;
    float fy;
    float sway;
    float fall_speed;
    bool dropping;
    bool dead;
} Body;

typedef struct {
    TGE_EntityPool *pool;
    TGE_CollisionWorld *world;
    TGE_EntityId player;
    int px, py;
    int score;
    int lives;
    int level;
    int spawned;
    int live;
    int wave_enemies;
    float spawn_acc;
    float spawn_interval;
    float wave_timer;
    float hit_flash;
    int bullets;
    TGE_EntityId to_kill[MAX_ENTITIES];
    int to_kill_count;
    SwarmState state;
} Game;

typedef struct {
    Game *game;
    float dt;
} UpdateCtx;

typedef struct {
    Game *game;
} CollideCtx;

typedef struct {
    Game *game;
    bool *hit;
} BreachCtx;

typedef struct {
    TGE_Canvas *canvas;
} DrawCtx;

static TGE_App *g_app = NULL;
static TGE_Scene *g_title = NULL;
static TGE_Scene *g_game = NULL;

static void kill_entity(Game *g, TGE_EntityId id)
{
    tge_collision_remove(g->world, id);
    TGE_Entity *e = tge_entity_get(g->pool, id);
    Body *b = e ? (Body *)e->userdata : NULL;
    if (b) {
        if (b->kind == K_BULLET)
            g->bullets--;
        if (b->anim)
            tge_anim_destroy(b->anim);
        free(b);
    }
    tge_entity_release(g->pool, id);
}

static void defer_kill(Game *g, TGE_EntityId id)
{
    if (g->to_kill_count < MAX_ENTITIES)
        g->to_kill[g->to_kill_count++] = id;
}

static void flush_kills(Game *g)
{
    for (int i = 0; i < g->to_kill_count; i++)
        kill_entity(g, g->to_kill[i]);
    g->to_kill_count = 0;
}

static void hit_player(Game *g)
{
    if (g->state == S_OVER)
        return;
    g->lives--;
    g->hit_flash = 0.9f;
    if (g->lives <= 0)
        g->state = S_OVER;
}

static bool spawn_bullet(Game *g, int x, int y)
{
    Body *b = (Body *)calloc(1, sizeof(*b));
    if (!b)
        return false;
    b->kind = K_BULLET;
    b->x = x;
    b->y = y;
    b->fy = (float)y;
    TGE_EntityId id = tge_entity_alloc(g->pool, b);
    if (id == TGE_ENTITY_NONE) {
        free(b);
        return false;
    }
    if (!tge_collision_insert(g->world, id, tge_rect(x, y, 1, 1))) {
        tge_entity_release(g->pool, id);
        free(b);
        return false;
    }
    return true;
}

static bool spawn_enemy(Game *g)
{
    Body *b = (Body *)calloc(1, sizeof(*b));
    if (!b)
        return false;
    int col = 3 + rand() % (WIN_W - 6);
    int target_y = 2 + rand() % 2;
    b->kind = K_ENEMY;
    b->base_x = col;
    b->x = col;
    b->y = -1;
    b->fy = -1.0f;
    b->fall_speed = 1.3f + 0.07f * (float)g->level;
    if (b->fall_speed > 2.4f)
        b->fall_speed = 2.4f;
    b->sway = 1.0f + (float)(rand() % 3);
    b->dropping = true;
    b->anim = tge_anim_create(-1.0f, (float)target_y, 0.45f, TGE_EASE_OUT);
    if (!b->anim) {
        free(b);
        return false;
    }
    tge_anim_play(b->anim);
    TGE_EntityId id = tge_entity_alloc(g->pool, b);
    if (id == TGE_ENTITY_NONE) {
        tge_anim_destroy(b->anim);
        free(b);
        return false;
    }
    if (!tge_collision_insert(g->world, id, tge_rect(col, -1, 1, 1))) {
        tge_entity_release(g->pool, id);
        tge_anim_destroy(b->anim);
        free(b);
        return false;
    }
    g->live++;
    return true;
}

static void free_body(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                      void *ctx)
{
    (void)pool;
    (void)id;
    (void)ctx;
    Body *b = (Body *)userdata;
    if (b) {
        if (b->anim)
            tge_anim_destroy(b->anim);
        free(b);
    }
}

static void game_free(Game *g)
{
    if (!g->pool)
        return;
    tge_entity_for_each(g->pool, free_body, NULL);
    tge_entity_pool_destroy(g->pool);
    tge_collision_world_destroy(g->world);
    g->pool = NULL;
    g->world = NULL;
}

static void game_reset(Game *g)
{
    game_free(g);
    memset(g, 0, sizeof(*g));
    g->pool = tge_entity_pool_create(32);
    g->world = tge_collision_world_create(CELL);
    g->px = WIN_W / 2;
    g->py = WIN_H - 2;
    g->lives = 3;
    g->level = 1;
    g->spawn_interval = 0.6f;
    g->wave_enemies = 8;
    g->state = S_PLAYING;

    Body *b = (Body *)calloc(1, sizeof(*b));
    if (b) {
        b->kind = K_PLAYER;
        b->x = g->px;
        b->y = g->py;
        g->player = tge_entity_alloc(g->pool, b);
        if (g->player != TGE_ENTITY_NONE)
            tge_collision_insert(g->world, g->player,
                                 tge_rect(g->px, g->py, 1, 1));
    }
}

static void start_wave(Game *g)
{
    g->level++;
    g->spawned = 0;
    g->spawn_acc = 0.0f;
    g->spawn_interval = 0.6f - 0.04f * (float)g->level;
    if (g->spawn_interval < 0.25f)
        g->spawn_interval = 0.25f;
    g->wave_enemies = 6 + g->level * 2;
    if (g->wave_enemies > MAX_ENEMIES)
        g->wave_enemies = MAX_ENEMIES;
    g->state = S_PLAYING;
}

static void update_body(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                        void *ctx)
{
    (void)pool;
    (void)id;
    Body *b = (Body *)userdata;
    Game *g = ((UpdateCtx *)ctx)->game;
    float dt = ((UpdateCtx *)ctx)->dt;

    if (b->kind == K_ENEMY) {
        tge_anim_update(b->anim, dt);
        if (b->dropping) {
            b->fy = tge_anim_value(b->anim);
            b->y = (int)roundf(b->fy);
            b->x = b->base_x;
            if (tge_anim_finished(b->anim)) {
                b->dropping = false;
                tge_anim_set_loop(b->anim, true);
                tge_anim_play(b->anim);
            }
        } else {
            b->fy += b->fall_speed * dt;
            b->y = (int)roundf(b->fy);
            float p = tge_anim_progress(b->anim);
            b->x = b->base_x + (int)roundf(sinf(p * 6.2831853f) * b->sway);
        }
        if (b->y >= WIN_H - 1)
            b->dead = true;
    } else if (b->kind == K_BULLET) {
        b->fy -= BULLET_SPEED * dt;
        b->y = (int)roundf(b->fy);
        if (b->y < 1)
            b->dead = true;
    }

    if (!b->dead)
        tge_collision_move(g->world, id, tge_rect(b->x, b->y, 1, 1));
}

static void bullet_collide(TGE_EntityPool *pool, TGE_EntityId id,
                           void *userdata, void *ctx)
{
    (void)pool;
    Body *b = (Body *)userdata;
    Game *g = ((CollideCtx *)ctx)->game;
    if (b->kind != K_BULLET || b->dead)
        return;
    uint32_t hits[MAX_ENTITIES];
    int n = tge_collision_query(g->world, tge_rect(b->x, b->y, 1, 1), hits,
                                MAX_ENTITIES);
    for (int i = 0; i < n; i++) {
        if (hits[i] == id)
            continue;
        TGE_Entity *e = tge_entity_get(g->pool, hits[i]);
        Body *ob = e ? (Body *)e->userdata : NULL;
        if (ob && ob->kind == K_ENEMY && !ob->dead) {
            b->dead = true;
            ob->dead = true;
            g->score += 10 * g->level;
            g->live--;
            defer_kill(g, id);
            defer_kill(g, hits[i]);
            break;
        }
    }
}

static void breach_check(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                         void *ctx)
{
    (void)pool;
    Body *b = (Body *)userdata;
    BreachCtx *c = (BreachCtx *)ctx;
    if (b->kind != K_ENEMY || b->dead)
        return;
    if (c->game->state != S_PLAYING)
        return;
    if (b->y >= WIN_H - 2) {
        b->dead = true;
        c->game->live--;
        defer_kill(c->game, id);
        *c->hit = true;
    }
}

static void collide_pass(Game *g)
{
    CollideCtx c = { g };
    tge_entity_for_each(g->pool, bullet_collide, &c);
    flush_kills(g);

    bool hit = false;
    if (g->state == S_PLAYING) {
        uint32_t hits[MAX_ENEMIES];
        int n = tge_collision_query(g->world, tge_rect(g->px, g->py, 1, 1),
                                    hits, MAX_ENEMIES);
        for (int i = 0; i < n; i++) {
            TGE_Entity *e = tge_entity_get(g->pool, hits[i]);
            Body *ob = e ? (Body *)e->userdata : NULL;
            if (ob && ob->kind == K_ENEMY && !ob->dead) {
                ob->dead = true;
                g->live--;
                defer_kill(g, hits[i]);
                hit = true;
            }
        }
        flush_kills(g);
        if (hit)
            hit_player(g);
    }

    bool breached = false;
    BreachCtx bc = { g, &breached };
    tge_entity_for_each(g->pool, breach_check, &bc);
    flush_kills(g);
    if (breached)
        hit_player(g);
}

static void draw_body(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                      void *ctx)
{
    (void)pool;
    (void)id;
    Body *b = (Body *)userdata;
    TGE_Canvas *canvas = ((DrawCtx *)ctx)->canvas;
    if (b->dead)
        return;
    if (b->kind == K_ENEMY) {
        if (b->x >= 0 && b->x < WIN_W && b->y >= 0 && b->y < WIN_H)
            tge_set_cell(canvas, b->x, b->y, 'M', TGE_COLOR_RED,
                         TGE_COLOR_BLACK);
    } else if (b->kind == K_BULLET) {
        if (b->y >= 0 && b->x >= 0 && b->x < WIN_W)
            tge_set_cell(canvas, b->x, b->y, '|', TGE_COLOR_YELLOW,
                         TGE_COLOR_BLACK);
    }
}

static void game_update(TGE_Scene *scene, float dt)
{
    Game *g = (Game *)scene->userdata;

    if (g->hit_flash > 0.0f) {
        g->hit_flash -= dt;
        if (g->hit_flash < 0.0f)
            g->hit_flash = 0.0f;
    }

    if (g->state == S_WAVE) {
        g->wave_timer -= dt;
        if (g->wave_timer <= 0.0f)
            start_wave(g);
        return;
    }
    if (g->state != S_PLAYING)
        return;

    g->spawn_acc += dt;
    while (g->spawn_acc >= g->spawn_interval && g->spawned < g->wave_enemies &&
           g->live < MAX_ENEMIES) {
        g->spawn_acc -= g->spawn_interval;
        if (spawn_enemy(g))
            g->spawned++;
    }

    UpdateCtx uc = { g, dt };
    tge_entity_for_each(g->pool, update_body, &uc);

    collide_pass(g);

    if (g->state == S_PLAYING && g->spawned >= g->wave_enemies &&
        g->live == 0) {
        g->state = S_WAVE;
        g->wave_timer = 1.2f;
    }
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    Game *g = (Game *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    DrawCtx d = { canvas };
    tge_entity_for_each(g->pool, draw_body, &d);

    if (g->hit_flash <= 0.0f && g->state != S_OVER)
        tge_set_cell(canvas, g->px, g->py, '@', TGE_COLOR_GREEN,
                     TGE_COLOR_BLACK);

    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK,
               " SCORE %06d ", g->score);
    tge_printf(canvas, 17, 0, TGE_COLOR_CYAN, TGE_COLOR_BLACK, " LV %d ",
               g->level);
    tge_printf(canvas, 25, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK, " LIVES %d ",
               g->lives);
    tge_printf(canvas, 37, 0, TGE_COLOR_WHITE, TGE_COLOR_BLACK, " LEFT %02d ",
               g->live);

    if (g->state == S_WAVE) {
        char buf[24];
        snprintf(buf, sizeof(buf), " WAVE %d ", g->level + 1);
        tge_draw_centered_text(canvas, h / 2, buf, TGE_COLOR_YELLOW,
                               TGE_COLOR_BLACK);
    }

    if (g->state == S_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] retry  [ESC] menu  [Q] quit ";
        tge_draw_centered_text(canvas, h / 2 - 2, msg, TGE_COLOR_RED,
                               TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, h / 2, again, TGE_COLOR_WHITE,
                               TGE_COLOR_BLACK);
    }
}

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    Game *g = (Game *)scene->userdata;

    if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_LEFT:
            if (g->state == S_PLAYING && g->px > 2) {
                g->px--;
                tge_collision_move(g->world, g->player,
                                   tge_rect(g->px, g->py, 1, 1));
            }
            break;
        case TGE_KEY_RIGHT:
            if (g->state == S_PLAYING && g->px < WIN_W - 3) {
                g->px++;
                tge_collision_move(g->world, g->player,
                                   tge_rect(g->px, g->py, 1, 1));
            }
            break;
        case TGE_KEY_UP:
            if (g->state == S_PLAYING && g->py > 1) {
                g->py--;
                tge_collision_move(g->world, g->player,
                                   tge_rect(g->px, g->py, 1, 1));
            }
            break;
        case TGE_KEY_DOWN:
            if (g->state == S_PLAYING && g->py < WIN_H - 2) {
                g->py++;
                tge_collision_move(g->world, g->player,
                                   tge_rect(g->px, g->py, 1, 1));
            }
            break;
        case TGE_KEY_SPACE:
            if (g->state == S_PLAYING && g->bullets < MAX_BULLETS) {
                if (spawn_bullet(g, g->px, g->py - 1))
                    g->bullets++;
            }
            break;
        case TGE_KEY_ENTER:
            if (g->state == S_OVER)
                game_reset(g);
            break;
        case TGE_KEY_ESC:
            g_game = NULL;
            TGE_PopScene(g_app);
            break;
        default:
            break;
        }
    } else if (ev->type == TGE_EVENT_TEXT) {
        switch (ev->data.text.codepoint) {
        case ' ':
            if (g->state == S_PLAYING && g->bullets < MAX_BULLETS) {
                if (spawn_bullet(g, g->px, g->py - 1))
                    g->bullets++;
            }
            break;
        case 'q': case 'Q':
            if (g->state == S_OVER)
                TGE_Quit(g_app);
            break;
        case 13:
            if (g->state == S_OVER)
                game_reset(g);
            break;
        default:
            break;
        }
    }
}

static void game_destroy(TGE_Scene *scene)
{
    Game *g = (Game *)scene->userdata;
    game_free(g);
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " SWARM ";
    const char *controls = " Arrows move   Space shoot ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 - 2, title, TGE_COLOR_GREEN,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 + 2, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_BLACK);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    bool enter = false;
    if (ev->type == TGE_EVENT_TEXT) {
        if (ev->data.text.codepoint == 13) {
            enter = true;
        } else if (ev->data.text.codepoint == 'q' ||
                   ev->data.text.codepoint == 'Q') {
            TGE_Quit(g_app);
            return;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN &&
               ev->data.key.keycode == TGE_KEY_ENTER) {
        enter = true;
    }
    if (ev->type == TGE_EVENT_KEYDOWN &&
        ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(g_app);
        return;
    }
    if (enter) {
        TGE_Scene *game = NULL;
        Game *g = (Game *)tge_scene_create(&game, sizeof(Game), game_update,
                                           game_draw, game_event, game_destroy);
        game_reset(g);
        g_game = game;
        TGE_PushScene(g_app, game);
    }
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    g_title = title;
    TGE_PushScene(app, title);
}

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Swarm");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    tge_scene_destroy(g_game);
    tge_scene_destroy(g_title);
    TGE_Destroy(app);
    return 0;
}
