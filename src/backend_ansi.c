#define _POSIX_C_SOURCE 200112L
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
    bool rgb;
    uint8_t index;
    uint8_t rgbv[3];
} ANSIStyle;

typedef struct {
    FILE *out;
    int w, h;
    struct termios orig_termios;
    bool raw_active;
    char *obuf;
    int obuf_len;
    int obuf_cap;
    ANSIStyle last_fg;
    ANSIStyle last_bg;
    int last_attr;
    int cur_x;
    int cur_y;
} ANSIState;

static void flush_obuf(ANSIState *s)
{
    if (s->obuf && s->obuf_len > 0) {
        fwrite(s->obuf, 1, (size_t)s->obuf_len, s->out);
        s->obuf_len = 0;
    }
}

static void obuf_write(ANSIState *s, const char *data, int len)
{
    if (!s->obuf) {
        fwrite(data, 1, (size_t)len, s->out);
        return;
    }
    if (s->obuf_len + len > s->obuf_cap) {
        flush_obuf(s);
        if (len >= s->obuf_cap) {
            fwrite(data, 1, (size_t)len, s->out);
            return;
        }
    }
    memcpy(s->obuf + s->obuf_len, data, (size_t)len);
    s->obuf_len += len;
}

static void obuf_puts(ANSIState *s, const char *str)
{
    obuf_write(s, str, (int)strlen(str));
}

static void style_from_color(const TGE_Color *c, ANSIStyle *out)
{
    if (c->mode == TGE_COLOR_MODE_RGB) {
        out->rgb = true;
        out->rgbv[0] = c->data.rgb.r;
        out->rgbv[1] = c->data.rgb.g;
        out->rgbv[2] = c->data.rgb.b;
    } else {
        out->rgb = false;
        out->index = c->data.index;
    }
}

static bool style_equal(const ANSIStyle *a, const ANSIStyle *b)
{
    if (a->rgb != b->rgb)
        return false;
    if (a->rgb)
        return a->rgbv[0] == b->rgbv[0] && a->rgbv[1] == b->rgbv[1] &&
               a->rgbv[2] == b->rgbv[2];
    return a->index == b->index;
}

static void emit_style(ANSIState *s, TGE_Color fg, TGE_Color bg, uint8_t attr)
{
    ANSIStyle f, b;
    style_from_color(&fg, &f);
    style_from_color(&bg, &b);

    bool same_fg = style_equal(&f, &s->last_fg);
    bool same_bg = style_equal(&b, &s->last_bg);
    bool same_attr = (attr == (uint8_t)s->last_attr);
    if (same_fg && same_bg && same_attr)
        return;

    char buf[96];
    int len = 0;
    buf[len++] = 0x1B;
    buf[len++] = '[';
    buf[len++] = '0';
    if (attr & 1)  { buf[len++] = ';'; buf[len++] = '1'; }
    if (attr & 2)  { buf[len++] = ';'; buf[len++] = '2'; }
    if (attr & 4)  { buf[len++] = ';'; buf[len++] = '3'; }
    if (attr & 8)  { buf[len++] = ';'; buf[len++] = '4'; }
    if (attr & 16) { buf[len++] = ';'; buf[len++] = '5'; }
    if (attr & 32) { buf[len++] = ';'; buf[len++] = '7'; }
    if (!same_fg) {
        if (f.rgb) {
            int n = snprintf(buf + len, (size_t)(96 - len), ";38;2;%d;%d;%d",
                             f.rgbv[0], f.rgbv[1], f.rgbv[2]);
            len += n;
        } else {
            buf[len++] = ';'; buf[len++] = '3';
            buf[len++] = (char)('0' + (f.index & 7));
        }
    }
    if (!same_bg) {
        if (b.rgb) {
            int n = snprintf(buf + len, (size_t)(96 - len), ";48;2;%d;%d;%d",
                             b.rgbv[0], b.rgbv[1], b.rgbv[2]);
            len += n;
        } else {
            buf[len++] = ';'; buf[len++] = '4';
            buf[len++] = (char)('0' + (b.index & 7));
        }
    }
    buf[len++] = 'm';
    obuf_write(s, buf, len);

    s->last_fg = f;
    s->last_bg = b;
    s->last_attr = attr;
}

static int utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static bool ansi_init(void *data, int w, int h)
{
    ANSIState *s = (ANSIState *)data;
    s->w = w;
    s->h = h;
    s->out = s->out ? s->out : stdout;
    s->last_fg.index = 0;
    s->last_bg.index = 0;
    s->last_attr = -1;
    s->cur_x = -1;
    s->cur_y = -1;

    int cap = w * h * 32 + 256;
    if (cap < 4096)
        cap = 4096;
    s->obuf = (char *)malloc((size_t)cap);
    if (!s->obuf)
        return false;
    s->obuf_cap = cap;

    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &s->orig_termios) == 0) {
            struct termios raw = s->orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
            raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
            raw.c_cflag |= CS8;
            raw.c_oflag &= ~(OPOST);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0)
                s->raw_active = true;
        }
    }

    obuf_puts(s, "\x1b[?1049h");
    obuf_puts(s, "\x1b[?25l");
    obuf_puts(s, "\x1b[2J\x1b[H");
    obuf_puts(s, "\x1b[?1000h\x1b[?1006h");
    flush_obuf(s);
    return true;
}

static void ansi_term(void *data)
{
    ANSIState *s = (ANSIState *)data;
    obuf_puts(s, "\x1b[?1006l\x1b[?1000l");
    obuf_puts(s, "\x1b[?25h");
    obuf_puts(s, "\x1b[?1049l");
    flush_obuf(s);
    if (s->raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s->orig_termios);
        s->raw_active = false;
    }
    free(s->obuf);
    s->obuf = NULL;
    s->obuf_cap = 0;
    s->obuf_len = 0;
}

static int ansi_width(void *data)
{
    return ((ANSIState *)data)->w;
}

static int ansi_height(void *data)
{
    return ((ANSIState *)data)->h;
}

static void ansi_present(void *data, struct TGE_Diff *diff,
                         const TGE_Cell *cells, int stride)
{
    ANSIState *s = (ANSIState *)data;
    for (int i = 0; i < diff->count; i++) {
        TGE_DirtySpan *span = &diff->spans[i];
        if (span->x_end <= span->x_start)
            continue;
        if (s->cur_x != span->x_start || s->cur_y != span->y) {
            char mv[32];
            int mlen = snprintf(mv, sizeof(mv), "\x1b[%d;%dH",
                                span->y + 1, span->x_start + 1);
            obuf_write(s, mv, mlen);
        }
        int written = 0;
        for (int x = span->x_start; x < span->x_end; x++) {
            const TGE_Cell *cell = &cells[(size_t)span->y * (size_t)stride + (size_t)x];
            if (cell->ch == 0) {
                continue;
            }
            emit_style(s, cell->fg, cell->bg, cell->attr);
            char tmp[8];
            int n = utf8_encode(cell->ch, tmp);
            obuf_write(s, tmp, n);
            written++;
        }
        s->cur_x = span->x_start + written;
        s->cur_y = span->y;
    }
    flush_obuf(s);
}

static int ansi_read_input(void *data, char *buf, int bufsize)
{
    (void)data;
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    int r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (r <= 0)
        return 0;
    int n = (int)read(STDIN_FILENO, buf, (size_t)bufsize);
    return n < 0 ? 0 : n;
}

static uint64_t ansi_ticks(void *data)
{
    (void)data;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

static void ansi_set_title(void *data, const char *title)
{
    ANSIState *s = (ANSIState *)data;
    obuf_puts(s, "\x1b]0;");
    obuf_puts(s, title);
    obuf_puts(s, "\x07");
    flush_obuf(s);
}

static TGE_Backend *backend_new(FILE *out)
{
    TGE_Backend *b = (TGE_Backend *)calloc(1, sizeof(TGE_Backend));
    if (!b)
        return NULL;
    ANSIState *s = (ANSIState *)calloc(1, sizeof(ANSIState));
    if (!s) {
        free(b);
        return NULL;
    }
    s->out = out;
    b->data = s;
    b->init = ansi_init;
    b->term = ansi_term;
    b->width = ansi_width;
    b->height = ansi_height;
    b->present = ansi_present;
    b->read_input = ansi_read_input;
    b->ticks = ansi_ticks;
    b->set_title = ansi_set_title;
    return b;
}

TGE_Backend *tge_backend_ansi_create(void)
{
    return backend_new(stdout);
}

TGE_Backend *tge_backend_ansi_create_with_file(FILE *out)
{
    return backend_new(out);
}
