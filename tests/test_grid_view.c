#include "tge-extra/grid_view.h"

#include "tge/tge_canvas.h"
#include "tge/tge_unicode.h"
#include "tge_internal.h"
#include "tge_test.h"

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static bool cell_is(const TGE_Canvas *c, int x, int y, uint32_t ch)
{
    const TGE_Cell *cell = cell_at(c, x, y);
    return cell->ch == ch && cell->fg.data.index == TGE_COLOR_GREEN.data.index;
}TGE_TEST(init_1x1)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&v, c);
    TGE_ASSERT(v.grid.cell_w == 1 && v.grid.cell_h == 1, "1x1 cell size");
    TGE_ASSERT(v.grid.ox == 0 && v.grid.oy == 0, "origin 0,0");
    TGE_ASSERT(v.grid.theme == &TGE_GRID_THEME_BLOCKS, "theme set");
    TGE_ASSERT(v.grid.canvas == c, "canvas attached");
    TGE_ASSERT(tge_grid_view_width(&v) == 40, "40 cols at 1x1");
    TGE_ASSERT(tge_grid_view_height(&v) == 20, "20 rows at 1x1");
    tge_canvas_destroy(c);
}

TGE_TEST(init_2x1)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_ASSERT(v.grid.cell_w == 2 && v.grid.cell_h == 1, "2x1 cell size");
    TGE_ASSERT(tge_grid_view_width(&v) == 20, "40/2 cols");
    TGE_ASSERT(tge_grid_view_height(&v) == 20, "20/1 rows");
    tge_canvas_destroy(c);
}

TGE_TEST(attach_repoints_canvas)
{
    /* init() is the persistent configuration; attach() only re-points the
     * canvas (the app swaps its double buffers every frame). */
    TGE_Canvas *c1 = tge_canvas_create(40, 20);
    TGE_Canvas *c2 = tge_canvas_create(80, 24);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c1);
    TGE_ASSERT(v.grid.canvas == c1, "first attach");
    TGE_ASSERT(tge_grid_view_width(&v) == 20, "40x20 -> 20 cols");
    tge_grid_view_attach(&v, c2);
    TGE_ASSERT(v.grid.canvas == c2, "re-attach moves the canvas");
    TGE_ASSERT(tge_grid_view_width(&v) == 40, "80x24 -> 40 cols");
    TGE_ASSERT(v.grid.cell_w == 2 && v.grid.cell_h == 1, "config persists");
    TGE_ASSERT(v.grid.theme == &TGE_GRID_THEME_BLOCKS, "theme persists");
    tge_canvas_destroy(c1);
    tge_canvas_destroy(c2);
}

TGE_TEST(null_theme_falls_back_to_blocks)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView v;
    tge_grid_view_init(&v, NULL, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&v, c);
    TGE_ASSERT(v.grid.theme == &TGE_GRID_THEME_BLOCKS, "NULL theme -> BLOCKS");
    tge_canvas_destroy(c);
}

TGE_TEST(width_height_adapts_to_resize)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_ASSERT(tge_grid_view_width(&v) == 20, "40x20 -> 20 cols");
    tge_canvas_resize(c, 80, 38);
    TGE_ASSERT(tge_grid_view_width(&v) == 40, "resized -> 40 cols");
    TGE_ASSERT(tge_grid_view_height(&v) == 38, "resized -> 38 rows");
    tge_canvas_destroy(c);
}

TGE_TEST(origin_shrinks_view)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    tge_grid_set_origin(&v.grid, 10, 5);
    TGE_ASSERT(tge_grid_view_width(&v) == 15, "(40-10)/2");
    TGE_ASSERT(tge_grid_view_height(&v) == 15, "(20-5)/1");
    tge_canvas_destroy(c);
}

TGE_TEST(size_for_matches_canvas_dims)
{
    TGE_Canvas *c = tge_canvas_create(80, 24);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    int gw, gh;
    tge_grid_view_size_for(&v, 80, 24, &gw, &gh);
    TGE_ASSERT(gw == tge_grid_view_width(&v) &&
                   gh == tge_grid_view_height(&v),
               "raw dims match canvas-backed size");
    tge_grid_view_size_for(&v, 40, 12, &gw, &gh);
    TGE_ASSERT(gw == 20 && gh == 12, "half canvas at 2x1");
    tge_canvas_destroy(c);
}

TGE_TEST(size_for_with_origin_and_no_canvas)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&v.grid, 0, 1);
    int gw, gh;
    tge_grid_view_size_for(&v, 80, 24, &gw, &gh);
    TGE_ASSERT(gw == 40 && gh == 23, "2x1 cells, HUD row on top");
    tge_grid_view_size_for(&v, 0, 0, &gw, &gh);
    TGE_ASSERT(gw == 0 && gh == 0, "degenerate size");
}

TGE_TEST(size_for_null_safety)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    int gw = -1, gh = -1;
    tge_grid_view_size_for(NULL, 80, 24, &gw, &gh);
    TGE_ASSERT(gw == -1 && gh == -1, "NULL view leaves outputs");
    tge_grid_view_size_for(&v, 80, 24, NULL, &gh);
    TGE_ASSERT(gh == -1, "NULL gw leaves gh");
    tge_grid_view_size_for(&v, 80, 24, &gw, NULL);
    TGE_ASSERT(gw == -1, "NULL gh leaves gw");
}

typedef struct {
    int calls;
    int last_w;
    int last_h;
    void *userdata_seen;
} ResizeRecorder;

static void record_resize(void *userdata, int gw, int gh)
{
    ResizeRecorder *r = (ResizeRecorder *)userdata;
    r->calls++;
    r->last_w = gw;
    r->last_h = gh;
    r->userdata_seen = userdata;
}

TGE_TEST(grid_layout_init_binds_view)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    TGE_GridLayout layout;
    tge_grid_layout_init(&layout, &v);
    TGE_ASSERT(layout.view == &v, "view bound");
    TGE_ASSERT(layout.cached_width == 0 && layout.cached_height == 0,
               "cache starts empty so the first sync fires");
}

TGE_TEST(grid_layout_sync_fires_on_change_only)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    TGE_GridLayout layout;
    tge_grid_layout_init(&layout, &v);
    ResizeRecorder r = {0, 0, 0, NULL};

    TGE_ASSERT(tge_grid_layout_sync(&layout, 40, 20, record_resize, &r),
               "first sync reports a resize");
    TGE_ASSERT(r.calls == 1 && r.last_w == 20 && r.last_h == 20,
               "40x20 -> 20x20 grid cells at 2x1");
    TGE_ASSERT(r.userdata_seen == &r, "userdata forwarded");

    TGE_ASSERT(!tge_grid_layout_sync(&layout, 40, 20, record_resize, &r),
               "unchanged surface is a no-op");
    TGE_ASSERT(r.calls == 1, "no callback on unchanged");

    TGE_ASSERT(tge_grid_layout_sync(&layout, 80, 20, record_resize, &r),
               "grow reports a resize");
    TGE_ASSERT(r.calls == 2 && r.last_w == 40 && r.last_h == 20,
               "80x20 -> 40x20 grid cells");
}

TGE_TEST(grid_layout_sync_honors_origin_and_cache)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&v.grid, 0, 1); /* HUD row on top */
    TGE_GridLayout layout;
    tge_grid_layout_init(&layout, &v);
    ResizeRecorder r = {0, 0, 0, NULL};

    tge_grid_layout_sync(&layout, 80, 24, record_resize, &r);
    TGE_ASSERT(r.last_w == 40 && r.last_h == 23, "origin folds into the size");

    tge_grid_layout_sync(&layout, 80, 24, record_resize, &r);
    TGE_ASSERT(r.calls == 1, "cache persists across syncs");
}

TGE_TEST(grid_layout_null_safety)
{
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    TGE_GridLayout layout;
    tge_grid_layout_init(&layout, &v);
    ResizeRecorder r = {0, 0, 0, NULL};
    TGE_ASSERT(!tge_grid_layout_sync(NULL, 40, 20, record_resize, &r),
               "NULL layout no-ops");
    TGE_ASSERT(r.calls == 0, "no callback from NULL layout");

    layout.view = NULL;
    TGE_ASSERT(!tge_grid_layout_sync(&layout, 40, 20, record_resize, &r),
               "NULL view no-ops");
    TGE_ASSERT(r.calls == 0, "no callback from NULL view");
}

TGE_TEST(draw_border_fills_perimeter)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&v, c);
    tge_grid_view_draw_border(&v, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int x = 0; x < 8; x++)
        TGE_ASSERT(cell_is(c, x, 0, 0x2588), "top border");
    for (int x = 0; x < 8; x++)
        TGE_ASSERT(cell_is(c, x, 3, 0x2588), "bottom border");
    for (int y = 0; y < 4; y++)
        TGE_ASSERT(cell_is(c, 0, y, 0x2588), "left border");
    for (int y = 0; y < 4; y++)
        TGE_ASSERT(cell_is(c, 7, y, 0x2588), "right border");
    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0, "interior untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_border_square_cells)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    tge_grid_view_draw_border(&v, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, 0x2588) && cell_is(c, 1, 0, 0x2588),
               "2x1 top-left cell");
    TGE_ASSERT(cell_is(c, 6, 0, 0x2588) && cell_is(c, 7, 0, 0x2588),
               "2x1 top-right cell");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0, "interior untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_writes_default_sprite)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    tge_grid_view_set_cell(&v, 1, 2, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 2, 0x2588), "default sprite left col");
    TGE_ASSERT(cell_is(c, 3, 2, 0x2588), "default sprite right col");
    tge_canvas_destroy(c);
}

TGE_TEST(put_draws_sprite)
{
    static const TGE_Sprite cross = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&v, c);
    tge_grid_view_put(&v, 3, 1, &cross, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 3, 1)->ch == '<', "sprite left glyph");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == '>', "sprite right glyph");
    TGE_ASSERT(cell_at(c, 5, 1)->ch == 0, "neighbor untouched");
    tge_canvas_destroy(c);
}

static TGE_View layout_for(int margin, int w, int h)
{
    TGE_View layout;
    tge_view_init(&layout, 10, 6);
    layout.margin = margin;
    tge_view_update(&layout, w, h);
    return layout;
}

TGE_TEST(set_cell_local_maps_through_layout)
{
    TGE_Canvas *c = tge_canvas_create(24, 8); /* 12x8 grid cells at 2x1 */
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_View layout = layout_for(1, 12, 8);
    tge_grid_view_set_cell_local(&v, &layout, tge_vec2i(0, 0),
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 1, 0x2588), "local (0,0) -> physical (2,1)");
    TGE_ASSERT(cell_is(c, 3, 1, 0x2588), "2x1 right col");
    TGE_ASSERT(cell_at(c, 0, 1)->ch == 0, "left margin untouched");
    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0, "grid border column untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_local_uses_custom_margin)
{
    TGE_Canvas *c = tge_canvas_create(32, 12); /* 16x12 grid cells at 2x1 */
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_View layout = layout_for(3, 16, 12);
    tge_grid_view_set_cell_local(&v, &layout, tge_vec2i(0, 0),
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 6, 3, 0x2588), "margin 3 -> physical (6,3)");
    tge_canvas_destroy(c);
}

TGE_TEST(put_local_maps_through_layout)
{
    static const TGE_Sprite cross = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_Canvas *c = tge_canvas_create(24, 8); /* 12x8 grid cells at 2x1 */
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_View layout = layout_for(1, 12, 8);
    tge_grid_view_put_local(&v, &layout, tge_vec2i(1, 2), &cross,
                            TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 4, 3)->ch == '<', "local (1,2) -> physical (4,3)");
    TGE_ASSERT(cell_at(c, 5, 3)->ch == '>', "sprite right col");
    tge_canvas_destroy(c);
}

TGE_TEST(put_attr_local_sets_attr)
{
    static const TGE_Sprite cross = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_Canvas *c = tge_canvas_create(24, 8); /* 12x8 grid cells at 2x1 */
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    TGE_View layout = layout_for(1, 12, 8);
    tge_grid_view_put_attr_local(&v, &layout, tge_vec2i(1, 2), &cross,
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK,
                                 TGE_CELL_ATTR_BOLD);
    TGE_ASSERT(cell_at(c, 4, 3)->attr & TGE_CELL_ATTR_BOLD,
               "attr forwarded to the cell");
    tge_canvas_destroy(c);
}

TGE_TEST(local_ops_null_safety)
{
    static const TGE_Sprite cross = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_View layout = layout_for(1, 12, 8);
    tge_grid_view_set_cell_local(NULL, &layout, tge_vec2i(0, 0),
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put_local(NULL, &layout, tge_vec2i(0, 0), &cross,
                            TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put_attr_local(NULL, &layout, tge_vec2i(0, 0), &cross,
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK,
                                 TGE_CELL_ATTR_BOLD);
    TGE_Canvas *c = tge_canvas_create(24, 8); /* 12x8 grid cells at 2x1 */
    TGE_GridView v;
    tge_grid_view_init(&v, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&v, c);
    tge_grid_view_set_cell_local(&v, NULL, tge_vec2i(0, 0),
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put_local(&v, NULL, tge_vec2i(0, 0), &cross,
                            TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put_attr_local(&v, NULL, tge_vec2i(0, 0), &cross,
                                 TGE_COLOR_GREEN, TGE_COLOR_BLACK,
                                 TGE_CELL_ATTR_BOLD);
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0, "NULL layout draws nothing");
    tge_canvas_destroy(c);
}

TGE_TEST(null_safety)
{
    tge_grid_view_init(NULL, NULL, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(NULL, NULL);
    TGE_ASSERT(tge_grid_view_width(NULL) == 0, "NULL width 0");
    TGE_ASSERT(tge_grid_view_height(NULL) == 0, "NULL height 0");
    tge_grid_view_draw_border(NULL, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_set_cell(NULL, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put(NULL, 0, 0, NULL, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
}

int main(void)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    test_init_1x1();
    test_init_2x1();
    test_attach_repoints_canvas();
    test_null_theme_falls_back_to_blocks();
    test_width_height_adapts_to_resize();
    test_origin_shrinks_view();
    test_size_for_matches_canvas_dims();
    test_size_for_with_origin_and_no_canvas();
    test_size_for_null_safety();
    test_grid_layout_init_binds_view();
    test_grid_layout_sync_fires_on_change_only();
    test_grid_layout_sync_honors_origin_and_cache();
    test_grid_layout_null_safety();
    test_draw_border_fills_perimeter();
    test_draw_border_square_cells();
    test_set_cell_writes_default_sprite();
    test_put_draws_sprite();
    test_set_cell_local_maps_through_layout();
    test_set_cell_local_uses_custom_margin();
    test_put_local_maps_through_layout();
    test_put_attr_local_sets_attr();
    test_local_ops_null_safety();
    test_null_safety();
    return tge_test_report();
}
