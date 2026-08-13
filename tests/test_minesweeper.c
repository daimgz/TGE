#define TGE_MINESWEEPER_TEST
#include "../examples/games/13_minesweeper.c"

#include "tge-extra/tilemap.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/playfield.h"
#include "tge_test.h"

#include <string.h>

static void setup_custom(MineWorld *w)
{
    world_init(w);          /* sets w/h/mines and lays 40 random mines */
    w->mines = 0;
    w->flags = 0;
    w->revealed_count = 0;
    w->first_click = 1; /* disable safe-first-click for determinism */
    w->state = MS_PLAYING;
    for (int y = 0; y < w->h; y++)
        for (int x = 0; x < w->w; x++) {
            w->mine[y][x] = false;
            w->adj[y][x] = 0;
            tge_tilemap_set(&w->map, x, y, CELL_HIDDEN);
        }
}

static int count_mines(const MineWorld *w)
{
    int n = 0;
    for (int y = 0; y < w->h; y++)
        for (int x = 0; x < w->w; x++)
            if (w->mine[y][x])
                n++;
    return n;
}

TGE_TEST(default_layout_has_exactly_40_mines)
{
    MineWorld w;
    world_init(&w);
    TGE_ASSERT(w.map.width == MS_W && w.map.height == MS_H,
               "tilemap sized MS_W x MS_H");
    TGE_ASSERT(count_mines(&w) == MS_MINES, "exactly MS_MINES mines placed");
    TGE_ASSERT(w.revealed_count == 0, "nothing revealed yet");
    TGE_ASSERT(w.state == MS_PLAYING, "starts in PLAYING");
}

TGE_TEST(rng_is_deterministic_per_seed)
{
    uint32_t s1 = 12345;
    uint32_t a = mine_rng_next(&s1);
    uint32_t s2 = 12345;
    uint32_t b = mine_rng_next(&s2);
    TGE_ASSERT(a == b, "same seed -> same first value");
    uint32_t s3 = 999;
    uint32_t c = mine_rng_next(&s3);
    TGE_ASSERT(a != c, "different seed -> different value");

    uint32_t s4 = 12345;
    uint32_t first = mine_rng_next(&s4);
    uint32_t second = mine_rng_next(&s4);
    TGE_ASSERT(first != second, "rng advances between draws");
}

TGE_TEST(adjacency_counts_eight_neighbors)
{
    MineWorld w;
    setup_custom(&w);
    w.mine[0][0] = true;
    compute_adj(&w);
    TGE_ASSERT(w.adj[0][1] == 1, "right neighbor counts the corner mine");
    TGE_ASSERT(w.adj[1][0] == 1, "down neighbor counts the corner mine");
    TGE_ASSERT(w.adj[1][1] == 1, "diagonal neighbor counts the corner mine");
    TGE_ASSERT(w.adj[2][2] == 0, "distant cell has no adjacent mine");

    setup_custom(&w);
    for (int y = 0; y < w.h; y++)
        for (int x = 0; x < w.w; x++)
            if (x != 5 || y != 5)
                w.mine[y][x] = true;
    compute_adj(&w);
    TGE_ASSERT(w.adj[5][5] == 8, "isolated cell surrounded by mines counts 8");
}

TGE_TEST(flag_toggle_roundtrips)
{
    MineWorld w;
    setup_custom(&w);
    toggle_flag(&w, 5, 5);
    TGE_ASSERT(tge_tilemap_get(&w.map, 5, 5) == CELL_FLAG, "hidden -> flag");
    TGE_ASSERT(w.flags == 1, "flag counter incremented");
    toggle_flag(&w, 5, 5);
    TGE_ASSERT(tge_tilemap_get(&w.map, 5, 5) == CELL_HIDDEN, "flag -> hidden");
    TGE_ASSERT(w.flags == 0, "flag counter decremented");

    /* a revealed (numbered) cell cannot be flagged */
    tge_tilemap_set(&w.map, 7, 7, CELL_3);
    int before = w.flags;
    toggle_flag(&w, 7, 7);
    TGE_ASSERT(tge_tilemap_get(&w.map, 7, 7) == CELL_3, "revealed cell stays");
    TGE_ASSERT(w.flags == before, "flagging a revealed cell is a no-op");
}

TGE_TEST(safe_first_click_moves_the_mine)
{
    MineWorld w;
    world_init(&w); /* random layout, first_click == 0 */
    int mx = -1, my = -1;
    for (int y = 0; y < w.h && mx < 0; y++)
        for (int x = 0; x < w.w; x++)
            if (w.mine[y][x]) { mx = x; my = y; break; }
    TGE_ASSERT(mx >= 0, "a mine exists to click");

    reveal(&w, mx, my);
    TGE_ASSERT(!w.mine[my][mx], "first-clicked cell is no longer a mine");
    TGE_ASSERT(count_mines(&w) == MS_MINES, "mine count preserved (moved, not removed)");
    TGE_ASSERT(w.state == MS_PLAYING, "safe first click never loses");
    TGE_ASSERT(w.first_click == 1, "first_click latched on");
}

TGE_TEST(reveal_mine_loses)
{
    MineWorld w;
    setup_custom(&w);
    w.mine[3][4] = true;     /* y=3, x=4 */
    w.mines = 1;
    compute_adj(&w);
    reveal(&w, 4, 3);
    TGE_ASSERT(w.state == MS_LOST, "revealing a mine loses");
    TGE_ASSERT(tge_tilemap_get(&w.map, 4, 3) == CELL_MINE, "boom cell shown");
    TGE_ASSERT(count_mines(&w) == 1, "all mines exposed on loss");
}

TGE_TEST(flood_fill_wins_with_single_mine)
{
    MineWorld w;
    setup_custom(&w);
    w.mine[15][15] = true;   /* only mine, in the far corner */
    w.mines = 1;
    compute_adj(&w);
    reveal(&w, 0, 0);
    TGE_ASSERT(w.state == MS_WON, "revealing everything but the mine wins");
    TGE_ASSERT(w.revealed_count == w.w * w.h - 1, "all safe cells revealed");
    TGE_ASSERT(tge_tilemap_get(&w.map, 15, 15) == CELL_HIDDEN,
               "the mine is never auto-revealed");
}

TGE_TEST(flood_fill_stops_at_numbered_cell)
{
    MineWorld w;
    setup_custom(&w);
    w.mine[1][1] = true;     /* makes (0,0) a number, not a zero */
    w.mines = 1;
    compute_adj(&w);
    reveal(&w, 0, 0);
    TGE_ASSERT(w.state == MS_PLAYING, "not a win with a mine left");
    TGE_ASSERT(w.revealed_count == 1, "only the clicked numbered cell revealed");
    TGE_ASSERT(tge_tilemap_get(&w.map, 0, 0) == CELL_1, "shows adjacency count");
}

TGE_TEST(reveal_blocked_on_flagged_cell)
{
    MineWorld w;
    setup_custom(&w);
    toggle_flag(&w, 2, 2);
    reveal(&w, 2, 2);
    TGE_ASSERT(tge_tilemap_get(&w.map, 2, 2) == CELL_FLAG, "flag protects the cell");
    TGE_ASSERT(w.revealed_count == 0, "flagged cell is not revealed");
}

TGE_TEST(mouse_click_maps_to_clicked_cell)
{
    TGE_Playfield pf;
    tge_playfield_init(&pf, &MINESWEEPER_THEME, TGE_GRID_SCALE_1X1, MS_W, MS_H);
    /* emulate the running game: 1-cell margin and a grid origin of (0,1)
     * so the status line sits above the board. */
    pf.view.area = (TGE_Rect){ 1, 1, MS_W, MS_H };
    pf.view.valid = true;
    tge_grid_set_origin(&pf.grid_view.grid, 0, 1);

    TGE_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = TGE_EVENT_MOUSEDOWN;
    ev.data.mouse.button = MS_BTN_LEFT;

    /* terminal coords are 1-based; the cell drawn at (cx,cy) lives at canvas
     * (area.x + cx, area.y + oy + cy), so terminal = canvas + 1. */
    int cx = 7, cy = 9;
    ev.data.mouse.x = cx + 2;
    ev.data.mouse.y = cy + 3;
    int gx, gy;
    TGE_ASSERT(mouse_to_cell(&pf, &ev, &gx, &gy), "click inside the grid");
    TGE_ASSERT(gx == cx && gy == cy, "cell matches the clicked position");

    /* the header/status row is above the grid origin: must not map to a cell */
    ev.data.mouse.x = cx + 2;
    ev.data.mouse.y = 1 + 1; /* canvas row 1 -> local 0 -> cell y -1 */
    TGE_ASSERT(!mouse_to_cell(&pf, &ev, &gx, &gy), "header row maps to no cell");
}

int main(void)
{
    test_mouse_click_maps_to_clicked_cell();
    test_default_layout_has_exactly_40_mines();
    test_rng_is_deterministic_per_seed();
    test_adjacency_counts_eight_neighbors();
    test_flag_toggle_roundtrips();
    test_safe_first_click_moves_the_mine();
    test_reveal_mine_loses();
    test_flood_fill_wins_with_single_mine();
    test_flood_fill_stops_at_numbered_cell();
    test_reveal_blocked_on_flagged_cell();
    return tge_test_report();
}
