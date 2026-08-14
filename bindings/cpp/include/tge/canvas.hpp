#pragma once

#include "tge/tge_canvas.h"
#include "tge-extra/ui.h"
#include "tge/color.hpp"

#include <cstdint>
#include <cstdarg>

namespace tge {

/* Non-owning handle to the canvas drawn into each frame. The canvas lives in
 * the TGE_App and is re-attached on every present, so a tge::Canvas is only
 * valid inside a draw callback / TGE_Step. Game code never names TGE_Canvas*
 * nor the raw tge_clear/tge_printf/tge_draw_* helpers. */
struct Canvas {
    TGE_Canvas *raw;

    explicit Canvas(TGE_Canvas *c) : raw{c} {}
    operator TGE_Canvas *() const { return raw; }

    int width()  const { return tge_canvas_width(raw); }
    int height() const { return tge_canvas_height(raw); }

    void clear(uint32_t ch, Color fg, Color bg) { tge_clear(raw, ch, fg, bg); }
    void fill_rect(int x, int y, int w, int h, uint32_t ch, Color fg, Color bg) {
        tge_fill_rect(raw, x, y, w, h, ch, fg, bg);
    }
    void draw_text(int x, int y, const char *text, Color fg, Color bg) {
        tge_draw_text(raw, x, y, text, fg, bg);
    }
    void draw_centered_text(int y, const char *text, Color fg, Color bg) {
        tge_draw_centered_text(raw, y, text, fg, bg);
    }
    void draw_modal(const char *title, const char *subtitle, Color title_fg) {
        tge_draw_modal(raw, title, subtitle, title_fg);
    }
    void print(int x, int y, Color fg, Color bg, const char *fmt, ...) {
        va_list ap; va_start(ap, fmt);
        tge_vprintf(raw, x, y, fg, bg, fmt, ap);
        va_end(ap);
    }

    static constexpr uint8_t attr_bold     = TGE_CELL_ATTR_BOLD;
    static constexpr uint8_t attr_dim      = TGE_CELL_ATTR_DIM;
    static constexpr uint8_t attr_italic   = TGE_CELL_ATTR_ITALIC;
    static constexpr uint8_t attr_underline= TGE_CELL_ATTR_UNDERLINE;
};

} // namespace tge
