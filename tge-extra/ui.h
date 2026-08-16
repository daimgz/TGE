#ifndef TGE_EXTRA_UI_H_
#define TGE_EXTRA_UI_H_

#include "tge/tge_canvas.h"
#include "tge/tge_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Small reusable canvas-level UI primitives (overlays, panels, menus,
 * status bars). These draw straight to a TGE_Canvas and know nothing about
 * games, scenes or grids; anything more than centered lines and boxes is
 * out of scope.
 *
 * Typical use is the pause / game-over overlay shared by every game:
 *
 *   if (world->paused)
 *       tge_draw_modal(canvas, " PAUSED ", " [P] resume ",
 *                      TGE_COLOR_YELLOW);
 *   else if (world->state == OVER)
 *       tge_draw_modal(canvas, " GAME OVER ",
 *                      " [ENTER] restart  [ESC] menu  [Q] quit ",
 *                      TGE_COLOR_RED);
 */

/* Draw a centered modal overlay: a full-width black bar three rows high at
 * the vertical center of the canvas with `title` centered in the first row
 * and `subtitle` centered in the third. The background is black and the
 * subtitle is white by convention; only the title color is the caller's. */
void tge_draw_modal(TGE_Canvas *canvas, const char *title,
                    const char *subtitle, TGE_Color title_fg);

/* Draw a bounded rectangular panel: a box-drawing frame (corner glyphs) around
 * `rect` plus an optional `title` drawn on the top border, inset one column
 * from the left. Pass NULL for `title` to draw a plain border with no title.
 *
 * This is a UI *pattern* built on the core primitives tge_draw_frame() +
 * tge_draw_text(): it resolves delimited rectangular panels and is NOT a
 * status-bar or a layout system. The frame and the title share the same `fg`;
 * the content area inside the border is intentionally NOT drawn here. The
 * caller lays out its own content, typically via tge_rect_inset(rect, 1) for a
 * one-cell margin. It exists because several games (Tetris, Dungeon, Map
 * Editor) repeated this exact frame+title operation independently.
 *
 *   tge_draw_region(canvas, tge_rect(x, y, w, h), " TITLE ", TGE_COLOR_YELLOW);
 *   TGE_Rect inner = tge_rect_inset(rect, 1);
 *   tge_printf(canvas, inner.x, inner.y, ...); */
void tge_draw_region(TGE_Canvas *canvas, TGE_Rect rect, const char *title,
                     TGE_Color fg);

#ifdef __cplusplus
}
#endif

#endif
