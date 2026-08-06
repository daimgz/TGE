#ifndef TGE_UNICODE_H_
#define TGE_UNICODE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How the engine decides whether the output supports Unicode glyphs.
 *
 *   TGE_UNICODE_AUTO  default: resolve once from the process locale
 *                     (setlocale(LC_CTYPE) + nl_langinfo(CODESET)).
 *   TGE_UNICODE_ON    force Unicode (override detection).
 *   TGE_UNICODE_OFF   force ASCII (override detection).
 *
 * TGE_UNICODE_OFF makes the grid draw each sprite's ASCII fallback and the
 * frame helpers emit + - | instead of box-drawing glyphs, so a game keeps
 * its geometry and readability on terminals that cannot render UTF-8. */
typedef enum {
    TGE_UNICODE_AUTO = 0,
    TGE_UNICODE_ON = 1,
    TGE_UNICODE_OFF = 2,
} TGE_UnicodeMode;

/* Force a mode; TGE_UNICODE_AUTO re-enables locale-based detection. */
void tge_unicode_set_mode(TGE_UnicodeMode mode);

/* The current mode: AUTO returns the mode, ON/OFF as set. Use this to show
 * how Unicode support was decided (e.g. for debug output). */
TGE_UnicodeMode tge_unicode_mode(void);

/* True when the output can use Unicode glyphs. In AUTO mode this is the
 * detected value; in ON/OFF it follows the override. Grid sprites and frame
 * helpers use this to pick the primary or the ASCII fallback. */
bool tge_unicode_supported(void);

#ifdef __cplusplus
}
#endif

#endif
