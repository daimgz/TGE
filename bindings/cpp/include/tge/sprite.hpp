#pragma once

#include "tge/tge_canvas.h"
#include <cstddef>

namespace tge {

/* Value type for a glyph sprite. Replaces the C-only TGE_SPRITE compound-literal
 * macro; sprites are plain data and are stored by value where a palette/theme
 * owns them. */
struct Sprite {
    TGE_Sprite raw;

    Sprite() { raw = TGE_SPRITE(1, 1, " ", NULL); }
    Sprite(int w, int h, const char *utf8, const char *ascii)
        : raw() { raw = TGE_SPRITE(w, h, utf8, ascii); }

    const TGE_Sprite *ptr() const { return &raw; }
    operator const TGE_Sprite &() const { return raw; }
};

} // namespace tge
