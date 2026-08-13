#pragma once

#include "tge/tge_canvas.h"

namespace tge {

/* Value type mirroring TGE_Color. Wraps the C color (indexed / rgb / default)
 * and exposes named constructors so a C++ caller never writes the C compound
 * literals by hand. */
struct Color {
    TGE_Color raw;

    Color() {
        raw.mode = TGE_COLOR_MODE_DEFAULT;
        raw.data.index = 0;
    }
    Color(TGE_Color c) : raw{c} {}

    static Color indexed(uint8_t i) { return Color(tge_color_indexed(i)); }
    static Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return Color(tge_color_rgb(r, g, b));
    }

    static Color black()   { return indexed(0); }
    static Color red()     { return indexed(1); }
    static Color green()   { return indexed(2); }
    static Color yellow()  { return indexed(3); }
    static Color blue()    { return indexed(4); }
    static Color magenta() { return indexed(5); }
    static Color cyan()    { return indexed(6); }
    static Color white()   { return indexed(7); }
    static Color DEFAULT() {
        TGE_Color c;
        c.mode = TGE_COLOR_MODE_DEFAULT;
        c.data.index = 0;
        return Color(c);
    }

    operator TGE_Color() const { return raw; }
};

} // namespace tge
