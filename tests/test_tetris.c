/* Headless regression test for the Game-adapter migration of
 * examples/games/03_tetris.c.
 *
 * The example's callbacks and state live in 03_tetris.c as `static` symbols, so
 * we pull the whole translation unit into this test (renaming its `main`) to
 * reach `tetris_callbacks` / `TetrisGame` directly. We then drive the real game
 * through the public game-adapter API (tge_game_create) under the mock backend:
 * create the scene, run a frame, and confirm (a) the adapter wiring is correct
 * (ctx.instance == game) and (b) a keypress actually mutates game state (a left
 * arrow shifts the spawned piece). This locks the migration as a regression
 * fixture without touching the example's gameplay code. */

#define main tetris_real_main
#include "../examples/games/03_tetris.c"
#undef main

#include "tge/tge_app.h"
#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <stdlib.h>

static TGE_App *make_app(MockData **md, int w, int h) {
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, w, h);
    TGE_App *app = tge_app_create_with_runtime(rt, w, h);
    app->fps = 0;
    return app;
}

TGE_TEST(adapter_wiring) {
    MockData *m;
    TGE_App *app = make_app(&m, 80, 24);
    TetrisGame *g = (TetrisGame *)tge_game_create(
        app, sizeof(TetrisGame), &tetris_callbacks);
    tge_app_process_scene_ops(app);
    TGE_ASSERT(g != NULL, "game instance created");
    TGE_ASSERT(g->ctx.instance == g, "ctx.instance == game (offset 0)");
    TGE_ASSERT(g->ctx.app == app, "ctx.app filled by adapter");
    TGE_ASSERT(g->ctx.callbacks == &tetris_callbacks, "callbacks stored");
    TGE_ASSERT(g->world.state == STATE_PLAYING, "fresh game is Playing");
    TGE_Destroy(app);
}

TGE_TEST(input_moves_piece) {
    MockData *m;
    TGE_App *app = make_app(&m, 80, 24);
    TetrisGame *g = (TetrisGame *)tge_game_create(
        app, sizeof(TetrisGame), &tetris_callbacks);
    tge_app_process_scene_ops(app);

    int x0 = g->world.pos.x;
    TGE_Event ev;
    ev.type = TGE_EVENT_KEYDOWN;
    ev.data.key.keycode = TGE_KEY_LEFT;
    TGE_PushEvent(app, &ev);
    tge_app_frame(app); /* update + draw run through the adapter trampolines */

    TGE_ASSERT(g->world.state == STATE_PLAYING, "still playing after a frame");
    TGE_ASSERT(g->world.pos.x == x0 - 1, "left arrow shifted the piece");
    TGE_Destroy(app);
}

int main() {
    test_adapter_wiring();
    test_input_moves_piece();
    return tge_test_report();
}
