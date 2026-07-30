#include "tge/tge_utf8.h"
#include "tge/tge_events.h"
#include "tge_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PARSER_BUF_SIZE   32
#define EVENT_QUEUE_SIZE  64

typedef enum {
    PARSER_GROUND,
    PARSER_ESC,
    PARSER_CSI,
    PARSER_CSI_PARAM,
    PARSER_CSI_INTERMEDIATE,
    PARSER_SS3,
} ParserState;

typedef enum {
    TOKEN_CHAR,
    TOKEN_CSI,
    TOKEN_SS3,
    TOKEN_ESC,
} ParserTokenType;

typedef struct {
    ParserTokenType type;
    int params[16];
    int param_count;
    unsigned char intermediates[4];
    int intermediate_count;
    unsigned char final;
    uint32_t codepoint;
} ParserToken;

struct TGE_Parser {
    ParserState state;
    int esc_buf_len;
    int utf8_remaining;
    uint32_t utf8_codepoint;
    int params[16];
    int param_count;
    unsigned char intermediates[4];
    int intermediate_count;
    TGE_Event queue[EVENT_QUEUE_SIZE];
    int head;
    int tail;
};

TGE_Parser *tge_parser_create(void)
{
    TGE_Parser *p = (TGE_Parser *)calloc(1, sizeof(TGE_Parser));
    return p;
}

void tge_parser_destroy(TGE_Parser *p)
{
    free(p);
}

static void queue_event(TGE_Parser *p, TGE_Event *ev)
{
    int next = (p->head + 1) % EVENT_QUEUE_SIZE;
    if (next == p->tail)
        return;
    p->queue[p->head] = *ev;
    p->head = next;
}

static void queue_key(TGE_Parser *p, int keycode, int mod)
{
    TGE_Event ev;
    ev.type = TGE_EVENT_KEYDOWN;
    ev.data.key.keycode = keycode;
    ev.data.key.mod = mod;
    queue_event(p, &ev);
}

static void queue_text(TGE_Parser *p, uint32_t codepoint)
{
    TGE_Event ev;
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = codepoint;
    queue_event(p, &ev);
}

static int csi_modifier(int param)
{
    switch (param) {
        case 2:  return TGE_MOD_SHIFT;
        case 3:  return TGE_MOD_ALT;
        case 4:  return TGE_MOD_ALT | TGE_MOD_SHIFT;
        case 5:  return TGE_MOD_CTRL;
        case 6:  return TGE_MOD_CTRL | TGE_MOD_SHIFT;
        case 7:  return TGE_MOD_CTRL | TGE_MOD_ALT;
        case 8:  return TGE_MOD_CTRL | TGE_MOD_ALT | TGE_MOD_SHIFT;
        default: return TGE_MOD_NONE;
    }
}

static int csi_default_param(const int *params, int count, int idx, int def)
{
    if (count > idx && params[idx] >= 0)
        return params[idx];
    return def;
}

static void translate_csi(TGE_Parser *p, ParserToken *tok)
{
    int has_intermediate = (tok->intermediate_count > 0);
    unsigned char inter = has_intermediate ? tok->intermediates[0] : 0;
    unsigned char fin = tok->final;
    int p0 = csi_default_param(tok->params, tok->param_count, 0, -1);
    int p1 = csi_default_param(tok->params, tok->param_count, 1, -1);
    int mod = (p1 >= 2 && p1 <= 8) ? csi_modifier(p1) : TGE_MOD_NONE;

    if (inter == '<') {
        if (fin == 'M' || fin == 'm') {
            int btn = p0 & 0x3F;
            int motion = (p0 & 0x20) ? 1 : 0;
            int x = tok->params[1];
            int y = tok->params[2];
            TGE_EventType etype;
            if (fin == 'm')
                etype = TGE_EVENT_MOUSEUP;
            else if (motion)
                etype = TGE_EVENT_MOUSEMOVE;
            else
                etype = TGE_EVENT_MOUSEDOWN;
            TGE_Event ev;
            ev.type = etype;
            ev.data.mouse.x = x;
            ev.data.mouse.y = y;
            ev.data.mouse.button = btn;
            queue_event(p, &ev);
        }
        return;
    }

    if (fin == 'R')
        return;

    if (fin == 't') {
        if (p0 == 8) {
            TGE_Event ev;
            ev.type = TGE_EVENT_RESIZE;
            ev.data.resize.w = p1;
            ev.data.resize.h = tok->params[2];
            queue_event(p, &ev);
        }
        return;
    }

    switch (fin) {
        case 'A': queue_key(p, TGE_KEY_UP, mod); return;
        case 'B': queue_key(p, TGE_KEY_DOWN, mod); return;
        case 'C': queue_key(p, TGE_KEY_RIGHT, mod); return;
        case 'D': queue_key(p, TGE_KEY_LEFT, mod); return;
        case 'H': queue_key(p, TGE_KEY_HOME, mod); return;
        case 'F': queue_key(p, TGE_KEY_END, mod); return;
        case 'Z': queue_key(p, TGE_KEY_TAB, TGE_MOD_SHIFT); return;
        default: break;
    }

    if (fin == '~') {
        switch (p0) {
            case 1:  queue_key(p, TGE_KEY_HOME, mod); return;
            case 2:  queue_key(p, TGE_KEY_INSERT, mod); return;
            case 3:  queue_key(p, TGE_KEY_DELETE, mod); return;
            case 4:  queue_key(p, TGE_KEY_END, mod); return;
            case 5:  queue_key(p, TGE_KEY_PAGEUP, mod); return;
            case 6:  queue_key(p, TGE_KEY_PAGEDOWN, mod); return;
            case 11: queue_key(p, TGE_KEY_F1, mod); return;
            case 12: queue_key(p, TGE_KEY_F2, mod); return;
            case 13: queue_key(p, TGE_KEY_F3, mod); return;
            case 14: queue_key(p, TGE_KEY_F4, mod); return;
            case 15: queue_key(p, TGE_KEY_F5, mod); return;
            case 17: queue_key(p, TGE_KEY_F6, mod); return;
            case 18: queue_key(p, TGE_KEY_F7, mod); return;
            case 19: queue_key(p, TGE_KEY_F8, mod); return;
            case 20: queue_key(p, TGE_KEY_F9, mod); return;
            case 21: queue_key(p, TGE_KEY_F10, mod); return;
            case 23: queue_key(p, TGE_KEY_F11, mod); return;
            case 24: queue_key(p, TGE_KEY_F12, mod); return;
        }
    }
}

static void translate_ss3(TGE_Parser *p, ParserToken *tok)
{
    switch (tok->final) {
        case 'P': queue_key(p, TGE_KEY_F1, TGE_MOD_NONE); return;
        case 'Q': queue_key(p, TGE_KEY_F2, TGE_MOD_NONE); return;
        case 'R': queue_key(p, TGE_KEY_F3, TGE_MOD_NONE); return;
        case 'S': queue_key(p, TGE_KEY_F4, TGE_MOD_NONE); return;
        case 'A': queue_key(p, TGE_KEY_UP, TGE_MOD_NONE); return;
        case 'B': queue_key(p, TGE_KEY_DOWN, TGE_MOD_NONE); return;
        case 'C': queue_key(p, TGE_KEY_RIGHT, TGE_MOD_NONE); return;
        case 'D': queue_key(p, TGE_KEY_LEFT, TGE_MOD_NONE); return;
        default: break;
    }
}

static void process_token(TGE_Parser *p, ParserToken *tok)
{
    switch (tok->type) {
        case TOKEN_CHAR:
            queue_text(p, tok->codepoint);
            break;
        case TOKEN_CSI:
            translate_csi(p, tok);
            break;
        case TOKEN_SS3:
            translate_ss3(p, tok);
            break;
        case TOKEN_ESC:
            queue_key(p, TGE_KEY_ESC, TGE_MOD_NONE);
            break;
    }
}

static void emit_token(TGE_Parser *p, ParserTokenType type,
                       const int *params, int param_count,
                       const unsigned char *intermediates, int intermediate_count,
                       unsigned char final, uint32_t codepoint)
{
    ParserToken tok;
    tok.type = type;
    tok.param_count = param_count < 16 ? param_count : 16;
    for (int i = 0; i < tok.param_count; i++)
        tok.params[i] = params[i];
    tok.intermediate_count = intermediate_count < 4 ? intermediate_count : 4;
    for (int i = 0; i < tok.intermediate_count; i++)
        tok.intermediates[i] = intermediates[i];
    tok.final = final;
    tok.codepoint = codepoint;
    process_token(p, &tok);
}

static void reset_params(TGE_Parser *p)
{
    p->param_count = 0;
    p->intermediate_count = 0;
}

void tge_parser_feed(TGE_Parser *p, const char *bytes, int len)
{
    int i = 0;
    while (i < len) {
        unsigned char b = (unsigned char)bytes[i];

        switch (p->state) {

        case PARSER_GROUND: {
            if (b == 0x1B) {
                p->state = PARSER_ESC;
                i++;
                continue;
            }
            if (b == 0x7F) {
                i++;
                continue;
            }
            if (b >= 0x20 && b <= 0x7E) {
                queue_text(p, b);
                i++;
                continue;
            }
            if (b == 0x09) {
                queue_key(p, TGE_KEY_TAB, TGE_MOD_NONE);
                i++;
                continue;
            }
            if (b == 0x0A || b == 0x0D) {
                queue_key(p, TGE_KEY_ENTER, TGE_MOD_NONE);
                i++;
                continue;
            }
            if (b == 0x08) {
                queue_key(p, TGE_KEY_BACKSPACE, TGE_MOD_NONE);
                i++;
                continue;
            }
            if (b >= 0x01 && b <= 0x1A) {
                int letter = (b == 0x1A) ? 'Z' : 'A' + b - 1;
                queue_key(p, letter, TGE_MOD_CTRL);
                i++;
                continue;
            }
            if (b == 0x1C || b == 0x1D || b == 0x1E || b == 0x1F) {
                queue_key(p, '\\' + b - 0x1C, TGE_MOD_CTRL);
                i++;
                continue;
            }
            if (b >= 0xC0 && b <= 0xDF) {
                p->utf8_remaining = 1;
                p->utf8_codepoint = b & 0x1F;
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            if (b >= 0xE0 && b <= 0xEF) {
                p->utf8_remaining = 2;
                p->utf8_codepoint = b & 0x0F;
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            if (b >= 0xF0 && b <= 0xF4) {
                p->utf8_remaining = 3;
                p->utf8_codepoint = b & 0x07;
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            if (b >= 0x80 && b <= 0xBF && p->utf8_remaining > 0) {
                p->utf8_codepoint = (p->utf8_codepoint << 6) | (b & 0x3F);
                p->utf8_remaining--;
                if (p->utf8_remaining == 0) {
                    queue_text(p, p->utf8_codepoint);
                }
                i++;
                continue;
            }
            i++;
            continue;
        }

        case PARSER_ESC: {
            if (b == 0x5B) {
                p->state = PARSER_CSI;
                reset_params(p);
                i++;
                continue;
            }
            if (b == 0x4F) {
                p->state = PARSER_SS3;
                reset_params(p);
                i++;
                continue;
            }
            queue_key(p, TGE_KEY_ESC, TGE_MOD_NONE);
            p->state = PARSER_GROUND;
            continue;
        }

        case PARSER_CSI: {
            if (b >= 0x30 && b <= 0x39) {
                int val = b - 0x30;
                if (p->param_count == 0) {
                    p->params[0] = val;
                    p->param_count = 1;
                } else if (p->params[p->param_count - 1] >= 0) {
                    p->params[p->param_count - 1] =
                        p->params[p->param_count - 1] * 10 + val;
                }
                p->state = PARSER_CSI_PARAM;
                i++;
                continue;
            }
            if (b == 0x3B) {
                if (p->param_count < 16)
                    p->params[p->param_count++] = -1;
                p->state = PARSER_CSI_PARAM;
                i++;
                continue;
            }
            if (b >= 0x3C && b <= 0x3F) {
                if (p->intermediate_count < 4)
                    p->intermediates[p->intermediate_count++] = b;
                p->state = PARSER_CSI_PARAM;
                i++;
                continue;
            }
            if (b >= 0x20 && b <= 0x2F) {
                if (p->intermediate_count < 4)
                    p->intermediates[p->intermediate_count++] = b;
                p->state = PARSER_CSI_INTERMEDIATE;
                i++;
                continue;
            }
            if (b >= 0x40 && b <= 0x7E) {
                emit_token(p, TOKEN_CSI,
                           p->params, p->param_count,
                           p->intermediates, p->intermediate_count,
                           b, 0);
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            p->state = PARSER_GROUND;
            i++;
            continue;
        }

        case PARSER_CSI_PARAM: {
            if (b >= 0x30 && b <= 0x39) {
                int val = b - 0x30;
                if (p->param_count == 0) {
                    p->params[0] = val;
                    p->param_count = 1;
                } else if (p->params[p->param_count - 1] >= 0) {
                    p->params[p->param_count - 1] =
                        p->params[p->param_count - 1] * 10 + val;
                } else {
                    p->params[p->param_count - 1] = val;
                }
                i++;
                continue;
            }
            if (b == 0x3B) {
                if (p->param_count < 16)
                    p->params[p->param_count++] = -1;
                i++;
                continue;
            }
            if (b >= 0x3C && b <= 0x3F) {
                if (p->intermediate_count < 4)
                    p->intermediates[p->intermediate_count++] = b;
                i++;
                continue;
            }
            if (b >= 0x20 && b <= 0x2F) {
                if (p->intermediate_count < 4)
                    p->intermediates[p->intermediate_count++] = b;
                p->state = PARSER_CSI_INTERMEDIATE;
                i++;
                continue;
            }
            if (b >= 0x40 && b <= 0x7E) {
                emit_token(p, TOKEN_CSI,
                           p->params, p->param_count,
                           p->intermediates, p->intermediate_count,
                           b, 0);
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            p->state = PARSER_GROUND;
            i++;
            continue;
        }

        case PARSER_CSI_INTERMEDIATE: {
            if (b >= 0x20 && b <= 0x2F) {
                if (p->intermediate_count < 4)
                    p->intermediates[p->intermediate_count++] = b;
                i++;
                continue;
            }
            if (b >= 0x40 && b <= 0x7E) {
                emit_token(p, TOKEN_CSI,
                           p->params, p->param_count,
                           p->intermediates, p->intermediate_count,
                           b, 0);
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            p->state = PARSER_GROUND;
            i++;
            continue;
        }

        case PARSER_SS3: {
            if (b >= 0x20 && b <= 0x7E) {
                emit_token(p, TOKEN_SS3,
                           p->params, p->param_count,
                           p->intermediates, p->intermediate_count,
                           b, 0);
                p->state = PARSER_GROUND;
                i++;
                continue;
            }
            p->state = PARSER_GROUND;
            i++;
            continue;
        }
        }
    }
}

void tge_parser_flush(TGE_Parser *p)
{
    if (p->state == PARSER_ESC) {
        queue_key(p, TGE_KEY_ESC, TGE_MOD_NONE);
        p->state = PARSER_GROUND;
    }
    if (p->utf8_remaining > 0) {
        p->utf8_remaining = 0;
        p->state = PARSER_GROUND;
    }
}

bool tge_parser_poll(TGE_Parser *p, TGE_Event *ev)
{
    if (p->tail == p->head)
        return false;
    *ev = p->queue[p->tail];
    p->tail = (p->tail + 1) % EVENT_QUEUE_SIZE;
    return true;
}
