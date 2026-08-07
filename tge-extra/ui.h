#ifndef TGE_EXTRA_UI_H_
#define TGE_EXTRA_UI_H_

#include "tge/tge_canvas.h"

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

#ifdef __cplusplus
}
#endif

#endif
