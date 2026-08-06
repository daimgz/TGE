#ifndef TGE_EXTRA_INPUT_H_
#define TGE_EXTRA_INPUT_H_

#include <stdbool.h>

#include "tge/tge_events.h"

#include "tge-extra/direction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maps key events to game-agnostic directions and actions, so games accept
 * arrows + WASD (and ENTER/ESC/Q) without duplicating the switch. */

/* Direction for arrow keys (KEYDOWN) and WASD (TEXT); TGE_DIR_NONE for any
 * other event. */
TGE_Direction tge_input_direction(const TGE_Event *ev);

/* True for ENTER, whether it arrives as KEYDOWN or TEXT. */
bool tge_input_confirm(const TGE_Event *ev);
/* True for ESC. */
bool tge_input_cancel(const TGE_Event *ev);
/* True for Q/q (TEXT). */
bool tge_input_quit(const TGE_Event *ev);
/* True for P/p (TEXT). */
bool tge_input_pause(const TGE_Event *ev);

#ifdef __cplusplus
}
#endif

#endif
