#ifndef TGE_CANVAS_H_
#define TGE_CANVAS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TGE_COLOR_MODE_INDEXED = 0,
    TGE_COLOR_MODE_RGB     = 1,
} TGE_ColorMode;

typedef struct {
    uint8_t mode;
    union {
        uint8_t index;
        struct { uint8_t r, g, b; } rgb;
    } data;
} TGE_Color;

#define TGE_COLOR_BLACK   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 0 })
#define TGE_COLOR_RED     ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 1 })
#define TGE_COLOR_GREEN   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 2 })
#define TGE_COLOR_YELLOW  ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 3 })
#define TGE_COLOR_BLUE    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 4 })
#define TGE_COLOR_MAGENTA ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 5 })
#define TGE_COLOR_CYAN    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 6 })
#define TGE_COLOR_WHITE   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .data.index = 7 })

typedef struct TGE_Cell {
    uint32_t ch;
    TGE_Color fg;
    TGE_Color bg;
    uint8_t  attr;
} TGE_Cell;

typedef struct TGE_Canvas TGE_Canvas;

int  tge_canvas_width(const TGE_Canvas *canvas);
int  tge_canvas_height(const TGE_Canvas *canvas);

void tge_clear(TGE_Canvas *canvas, uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_set_cell(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                  TGE_Color fg, TGE_Color bg);
void tge_draw_text(TGE_Canvas *canvas, int x, int y, const char *text,
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

#ifdef __cplusplus
}
#endif

#endif
