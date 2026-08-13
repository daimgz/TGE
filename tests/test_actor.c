#include "tge-extra/actor.h"

#include "tge/tge_canvas.h"
#include "tge/tge_utf8.h"
#include "tge_internal.h"
#include "tge_test.h"

#include <string.h>

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static bool cell_is(const TGE_Canvas *c, int x, int y, const char *glyph)
{
    const TGE_Cell *cell = cell_at(c, x, y);
    const unsigned char *p = (const unsigned char *)glyph;
    uint32_t cp;
    int n = tge_utf8_decode((const char *)p, (int)strlen(glyph), &cp);
    return n > 0 && cell->ch == cp;
}

static const TGE_Sprite SPRITE_PAC = TGE_SPRITE(1, 1, "\xE2\x97\x8F", "O");

static void make_view(TGE_View *view)
{
    tge_view_init(view, 4, 4);
    tge_view_update(view, 10, 10); /* margin 1 -> area (1,1,8,8) */
}

TGE_TEST(draw_places_actor_at_translated_position)
{
    TGE_Canvas *c = tge_canvas_create(20, 10);
    TGE_GridView gv;
    tge_grid_view_init(&gv, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&gv, c);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    TGE_View view;
    make_view(&view);

    TGE_Actor actor;
    actor.position = tge_vec2i(2, 3);
    actor.sprite = &SPRITE_PAC;
    actor.fg = tge_color_indexed(3); /* yellow */
    actor.bg = tge_color_indexed(0);

    tge_actor_draw(&gv, &view, &actor);

    /* local (2,3) + area origin (1,1) -> grid (3,4). */
    TGE_ASSERT(cell_is(c, 3, 4, "\xE2\x97\x8F"),
               "actor drawn at translated position");
    TGE_ASSERT(cell_at(c, 3, 4)->fg.data.index == 3, "actor fg applied");
    TGE_ASSERT(cell_is(c, 2, 4, " ") && cell_is(c, 3, 3, " "),
               "neighbours untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(actor_inside_second_row_of_area)
{
    TGE_Canvas *c = tge_canvas_create(20, 10);
    TGE_GridView gv;
    tge_grid_view_init(&gv, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&gv, c);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    TGE_View view;
    make_view(&view);

    TGE_Actor actor = {
        .position = tge_vec2i(0, 0),
        .sprite = &SPRITE_PAC,
        .fg = tge_color_indexed(2),
        .bg = tge_color_indexed(0),
    };
    tge_actor_draw(&gv, &view, &actor);
    TGE_ASSERT(cell_is(c, 1, 1, "\xE2\x97\x8F"), "local (0,0) -> grid (1,1)");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_null_safety)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_GridView gv;
    tge_grid_view_init(&gv, &TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_1X1);
    tge_grid_view_attach(&gv, c);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    TGE_View view;
    make_view(&view);

    TGE_Actor actor = {
        .position = tge_vec2i(0, 0),
        .sprite = &SPRITE_PAC,
        .fg = tge_color_indexed(2),
        .bg = tge_color_indexed(0),
    };
    TGE_Actor no_sprite = actor;
    no_sprite.sprite = NULL;

    tge_actor_draw(NULL, &view, &actor);
    tge_actor_draw(&gv, NULL, &actor);
    tge_actor_draw(&gv, &view, NULL);
    tge_actor_draw(&gv, &view, &no_sprite);
    TGE_ASSERT(cell_is(c, 1, 1, " "), "nothing drawn on NULL inputs");
    tge_canvas_destroy(c);
}

int main(void)
{
    test_draw_places_actor_at_translated_position();
    test_actor_inside_second_row_of_area();
    test_draw_null_safety();
    return tge_test_report();
}
