#ifndef TGE_CANVAS_H_
#define TGE_CANVAS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How a TGE_Color payload is interpreted. */
typedef enum {
    TGE_COLOR_MODE_INDEXED = 0,
    TGE_COLOR_MODE_RGB     = 1,
} TGE_ColorMode;

/* A terminal color. Indexed mode selects one of the 8 ANSI palette slots;
 * RGB mode uses 24-bit color (when the terminal supports it). */
typedef struct {
    uint8_t mode;
    union {
        uint8_t index;
        struct { uint8_t r, g, b; } rgb;
    } data;
} TGE_Color;

/* Predefined indexed palette colors. These are compound literals, which are
 * not valid in static/const initializers; build runtime values through
 * tge_color_indexed() instead. */
#define TGE_COLOR_BLACK   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 0 })
#define TGE_COLOR_RED     ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 1 })
#define TGE_COLOR_GREEN   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 2 })
#define TGE_COLOR_YELLOW  ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 3 })
#define TGE_COLOR_BLUE    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 4 })
#define TGE_COLOR_MAGENTA ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 5 })
#define TGE_COLOR_CYAN    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 6 })
#define TGE_COLOR_WHITE   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 7 })

/* Color constructors. */
TGE_Color tge_color_indexed(uint8_t index); /* Palette entry [0..7]; see TGE_COLOR_*. */
TGE_Color tge_color_rgb(uint8_t r, uint8_t g, uint8_t b); /* 24-bit RGB (subject to terminal support). */

/* A single screen cell: a Unicode codepoint plus foreground/background
 * color and attribute flags. */
typedef struct TGE_Cell {
    uint32_t ch;
    TGE_Color fg;
    TGE_Color bg;
    uint8_t  attr;
} TGE_Cell;

/* Cell attribute flags (SGR modes), OR-able into the attr field. */
#define TGE_CELL_ATTR_BOLD       (1u << 0)
#define TGE_CELL_ATTR_DIM        (1u << 1)
#define TGE_CELL_ATTR_ITALIC     (1u << 2)
#define TGE_CELL_ATTR_UNDERLINE  (1u << 3)
#define TGE_CELL_ATTR_BLINK      (1u << 4)
#define TGE_CELL_ATTR_REVERSE    (1u << 5)

typedef struct TGE_Canvas TGE_Canvas;

int  tge_canvas_width(const TGE_Canvas *canvas);
int  tge_canvas_height(const TGE_Canvas *canvas);

void tge_clear(TGE_Canvas *canvas, uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_set_cell(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                  TGE_Color fg, TGE_Color bg);
/* Same as tge_set_cell plus cell attributes (TGE_CELL_ATTR_*, OR-able). */
void tge_set_cell_attr(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                       TGE_Color fg, TGE_Color bg, uint8_t attr);
void tge_draw_text(TGE_Canvas *canvas, int x, int y, const char *text,
                   TGE_Color fg, TGE_Color bg);

#define TGE_PRINTF_BUF 256 /* fixed stack buffer for tge_printf */

/* Formatted text: printf-style shorthand for snprintf into a fixed stack
 * buffer (no allocation, safe for the render path) followed by
 * tge_draw_text. Truncates like snprintf when the result does not fit.
 *   tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK,
 *              " SCORE: %d ", score); */
void tge_printf(TGE_Canvas *canvas, int x, int y, TGE_Color fg, TGE_Color bg,
                const char *fmt, ...);

/* Text centered horizontally on the canvas at row `y`. Width is measured in
 * display columns (wide UTF-8 chars count double), so centering survives
 * full-width glyphs. No-ops on NULL canvas/text. */
void tge_draw_centered_text(TGE_Canvas *canvas, int y, const char *text,
                            TGE_Color fg, TGE_Color bg);

void tge_draw_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   TGE_Color fg, TGE_Color bg);
void tge_draw_frame(TGE_Canvas *canvas, int x, int y, int w, int h,
                    TGE_Color fg, TGE_Color bg);
void tge_fill_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_draw_line(TGE_Canvas *canvas, int x1, int y1, int x2, int y2,
                   uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_draw_circle(TGE_Canvas *canvas, int cx, int cy, int r,
                     uint32_t ch, TGE_Color fg, TGE_Color bg);

/* Copies the visible region of src into dst starting at (dx, dy), clipping
 * to both canvases. All draw calls clip implicitly. */
void tge_blit(TGE_Canvas *dst, int dx, int dy, const TGE_Canvas *src);

#ifdef __cplusplus
}
#endif

#endif
