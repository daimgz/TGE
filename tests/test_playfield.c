#include "tge-extra/playfield.h"

#include "tge/tge_canvas.h"
#include "tge_internal.h"
#include "tge_test.h"

static int g_fires;
static TGE_ViewUpdate g_update;
static int g_last_w;
static int g_last_h;

static void resize_cb(void *userdata, int grid_w, int grid_h)
{
    TGE_Playfield *pf = (TGE_Playfield *)userdata;
    g_fires++;
    g_last_w = grid_w;
    g_last_h = grid_h;
    g_update = tge_view_update(&pf->view, grid_w, grid_h);
}

TGE_TEST(init_wires_components)
{
    TGE_Playfield pf;
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1, 3, 4);
    TGE_ASSERT(pf.view.min_w == 3 && pf.view.min_h == 4, "view minimum set");
    TGE_ASSERT(pf.view.margin == 1, "default view margin");
    TGE_ASSERT(pf.grid_view.grid.theme == &TGE_GRID_THEME_BLOCKS,
               "grid theme wired");
    TGE_ASSERT(pf.grid_view.grid.cell_w == 2 && pf.grid_view.grid.cell_h == 1,
               "2x1 cell scale");
    TGE_ASSERT(pf.layout.view == &pf.grid_view, "layout points at grid view");
    TGE_ASSERT(pf.layout.cached_width == 0 && pf.layout.cached_height == 0,
               "layout cache starts empty");
}

TGE_TEST(sync_first_valid_then_resized)
{
    TGE_Playfield pf;
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1, 3, 3);

    g_fires = 0;
    g_update = TGE_VIEW_INVALID;
    /* scale 1x1, origin 0 -> logical grid == surface. First sync always fires. */
    TGE_ASSERT(tge_playfield_sync(&pf, 10, 10, resize_cb, &pf),
               "first sync reports a change");
    TGE_ASSERT(g_fires == 1, "callback fired once");
    TGE_ASSERT(g_last_w == 10 && g_last_h == 10, "grid size delivered");
    TGE_ASSERT(g_update == TGE_VIEW_FIRST_VALID, "first valid layout");
    TGE_ASSERT(pf.view.valid, "view valid");
    TGE_ASSERT(pf.view.area.x == 1 && pf.view.area.y == 1, "margin applied");
    TGE_ASSERT(pf.view.area.w == 8 && pf.view.area.h == 8, "area size");

    TGE_ASSERT(tge_playfield_sync(&pf, 20, 10, resize_cb, &pf),
               "second sync reports a change");
    TGE_ASSERT(g_update == TGE_VIEW_RESIZED, "later layout is a resize");
    TGE_ASSERT(pf.view.area.w == 18, "area grew with the surface");
    TGE_ASSERT(pf.view.first == false, "first consumed after first valid");
    TGE_ASSERT(!tge_playfield_sync(&pf, 20, 10, resize_cb, &pf),
               "no change -> no callback, false");
    TGE_ASSERT(g_fires == 2, "callback not fired when unchanged");
    TGE_ASSERT(!tge_playfield_sync(&pf, 20, 10, NULL, NULL),
               "no change even without callback");
    TGE_ASSERT(g_fires == 2, "callback pointer not used on no change");
}

TGE_TEST(sync_invalid_when_too_small)
{
    TGE_Playfield pf;
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1, 5, 5);

    g_fires = 0;
    g_update = TGE_VIEW_RESIZED;
    tge_playfield_sync(&pf, 6, 6, resize_cb, &pf);
    TGE_ASSERT(g_update == TGE_VIEW_INVALID, "too small is invalid");
    TGE_ASSERT(!pf.view.valid, "view invalid");
    TGE_ASSERT(pf.view.first, "first stays pending until valid");

    g_update = TGE_VIEW_INVALID;
    tge_playfield_sync(&pf, 10, 10, resize_cb, &pf);
    TGE_ASSERT(g_update == TGE_VIEW_FIRST_VALID, "growth to valid is FIRST_VALID");
    TGE_ASSERT(pf.view.valid && !pf.view.first, "view valid, first consumed");
}

TGE_TEST(attach_and_border)
{
    TGE_Canvas *c = tge_canvas_create(20, 10);
    TGE_Playfield pf;
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1, 3, 3);
    tge_playfield_attach(&pf, c);
    TGE_ASSERT(pf.grid_view.grid.canvas == c, "grid attached to canvas");
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_playfield_draw_border(&pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    const TGE_Cell *top = &c->cells[0];
    TGE_ASSERT(top->ch != ' ', "border drawn at top edge");
    TGE_ASSERT(pf.grid_view.grid.canvas != NULL, "border did not detach");
    tge_canvas_destroy(c);
}

TGE_TEST(null_safety)
{
    TGE_Playfield pf;
    TGE_Canvas *c = tge_canvas_create(8, 4);
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1, 2, 2);
    tge_playfield_init(NULL, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1, 2, 2);
    tge_playfield_attach(NULL, c);
    tge_playfield_attach(&pf, NULL);
    TGE_ASSERT(!tge_playfield_sync(NULL, 10, 10, resize_cb, &pf),
               "sync NULL safe");
    tge_playfield_draw_border(NULL, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_canvas_destroy(c);
}

int main(void)
{
    test_init_wires_components();
    test_sync_first_valid_then_resized();
    test_sync_invalid_when_too_small();
    test_attach_and_border();
    test_null_safety();
    return tge_test_report();
}
