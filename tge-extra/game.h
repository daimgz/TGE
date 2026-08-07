#ifndef TGE_EXTRA_GAME_H_
#define TGE_EXTRA_GAME_H_

#include "tge/tge_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A stateful interactive application ("game", editor, demo, ...) running on a
 * TGE_Scene. The scene is the engine's primitive abstraction; this module is
 * an optional ergonomic layer on top that removes the scene plumbing from the
 * application code. It never leaks into core: the engine keeps talking about
 * scenes, the application talks about TGE_GameContext, and nothing mixes
 * responsibilities. The raw scene API stays available as the escape hatch for
 * scenes that are not games (menus, overlays, transitions).
 *
 * Contract:
 *   TGE_GameContext must be the FIRST member of the instance struct:
 *
 *     typedef struct {
 *         TGE_GameContext ctx;    offset 0: scene->userdata == &game->ctx
 *         SnakeWorld world;
 *         SnakeRenderer renderer;
 *     } SnakeGame;
 *
 *   Breaking the offset-0 rule fails silently, so it is the one rule that must
 *   not bend. Because ctx sits at offset 0, `ctx == instance` in memory;
 *   `instance` exists only as a convenience reference so the callbacks never
 *   cast from the context to the container. */

typedef struct TGE_GameContext TGE_GameContext;

typedef void (*tge_game_update_fn)(TGE_GameContext *ctx, float dt);
typedef void (*tge_game_draw_fn)(TGE_GameContext *ctx, TGE_Canvas *canvas);
typedef void (*tge_game_event_fn)(TGE_GameContext *ctx, TGE_Event *ev);
typedef void (*tge_game_destroy_fn)(TGE_GameContext *ctx);

/* The application's interface: fill in the callbacks you need; unused ones
 * may be NULL. The struct must outlive the scene (keep it static). */
typedef struct {
    tge_game_update_fn update;
    tge_game_draw_fn draw;
    tge_game_event_fn event;
    tge_game_destroy_fn destroy;
} TGE_GameCallbacks;

/* The context embedded at offset 0 of the instance. Filled in by
 * tge_game_scene_create(). */
struct TGE_GameContext {
    TGE_App *app;                        /* the app this game runs under */
    void *instance;                      /* == the containing object (offset 0) */
    const TGE_GameCallbacks *callbacks;  /* the struct passed to create */
};

/* The instance the callbacks belong to. Keeps the adapter's internals out of
 * the game code: if ctx->instance ever changes shape, no game is touched. */
static inline void *tge_game_instance(TGE_GameContext *ctx)
{
    return ctx->instance;
}

/* Convenience constructor: allocates the scene and a zeroed instance of
 * `game_size` bytes in one call (delegating to tge_scene_create), wires the
 * trampolines, fills ctx.app/instance/callbacks and returns the instance.
 * `callbacks` is mandatory (use tge_scene_create for empty scenes) and
 * `game_size` must be at least sizeof(TGE_GameContext). */
void *tge_game_scene_create(TGE_App *app, TGE_Scene **out, size_t game_size,
                            const TGE_GameCallbacks *callbacks);

/* Convenience wrapper for the common title-screen flow: creates the game
 * scene exactly like tge_game_scene_create and pushes it on the app in one
 * call, returning the context at offset 0 of the instance (the caller casts
 * it to its game struct: `MyGame *game = (MyGame *)tge_game_create(...)`).
 * Same rejection rules as tge_game_scene_create; nothing is pushed on
 * failure. */
TGE_GameContext *tge_game_create(TGE_App *app, size_t game_size,
                                 const TGE_GameCallbacks *callbacks);

#ifdef __cplusplus
}
#endif

#endif
