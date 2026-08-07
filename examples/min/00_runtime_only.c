#include "tge/tge_runtime.h"
#include "tge/tge_canvas.h"
#include "tge/tge_events.h"
#include <stdlib.h>
#include <stdbool.h>

#define W 60
#define H 16

int main(void)
{
    TGE_Runtime *rt = tge_runtime_create(W, H);
    if (!rt)
        return 1;

    TGE_Cell *buf = (TGE_Cell *)calloc((size_t)W * (size_t)H, sizeof(TGE_Cell));
    if (!buf) {
        tge_runtime_destroy(rt);
        return 1;
    }

    bool running = true;
    while (running) {
        TGE_Event ev;
        while (tge_runtime_poll_event(rt, &ev)) {
            if (ev.type == TGE_EVENT_QUIT ||
                (ev.type == TGE_EVENT_KEYDOWN &&
                 ev.data.key.keycode == TGE_KEY_ESC)) {
                running = false;
            }
        }

        for (int i = 0; i < W * H; i++) {
            buf[i].ch = ' ';
            buf[i].fg = TGE_COLOR_BLACK;
            buf[i].bg = TGE_COLOR_DEFAULT;
            buf[i].attr = 0;
        }
        for (int x = 0; x < W; x++) {
            buf[x].ch = 0x2500;
            buf[x].fg = TGE_COLOR_CYAN;
            buf[(size_t)(H - 1) * (size_t)W + (size_t)x].ch = 0x2500;
            buf[(size_t)(H - 1) * (size_t)W + (size_t)x].fg = TGE_COLOR_CYAN;
        }
        for (int y = 0; y < H; y++) {
            buf[(size_t)y * (size_t)W].ch = 0x2502;
            buf[(size_t)y * (size_t)W].fg = TGE_COLOR_CYAN;
            buf[(size_t)y * (size_t)W + (size_t)(W - 1)].ch = 0x2502;
            buf[(size_t)y * (size_t)W + (size_t)(W - 1)].fg = TGE_COLOR_CYAN;
        }
        buf[0].ch = 0x250C;
        buf[W - 1].ch = 0x2510;
        buf[(size_t)(H - 1) * (size_t)W].ch = 0x2514;
        buf[(size_t)(H - 1) * (size_t)W + (size_t)(W - 1)].ch = 0x2518;

        static const char *msg = "Runtime-only: no Engine, no Canvas";
        for (int i = 0; msg[i]; i++) {
            buf[(size_t)2 * (size_t)W + (size_t)(4 + i)].ch =
                (uint32_t)(unsigned char)msg[i];
            buf[(size_t)2 * (size_t)W + (size_t)(4 + i)].fg = TGE_COLOR_GREEN;
        }

        if (tge_runtime_width(rt) == W && tge_runtime_height(rt) == H)
            tge_runtime_present_full(rt, buf, W);
    }

    free(buf);
    tge_runtime_destroy(rt);
    return 0;
}
