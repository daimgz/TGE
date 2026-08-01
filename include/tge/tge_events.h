#ifndef TGE_EVENTS_H_
#define TGE_EVENTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TGE_EVENT_NONE = 0,
    TGE_EVENT_KEYDOWN,
    TGE_EVENT_KEYUP,
    TGE_EVENT_TEXT,
    TGE_EVENT_MOUSEDOWN,
    TGE_EVENT_MOUSEUP,
    TGE_EVENT_MOUSEMOVE,
    TGE_EVENT_RESIZE,
    TGE_EVENT_QUIT,
    TGE_EVENT_TIMER,
    TGE_EVENT_USER,
} TGE_EventType;

#define TGE_MOD_NONE    0
#define TGE_MOD_SHIFT  (1<<0)
#define TGE_MOD_ALT    (1<<1)
#define TGE_MOD_CTRL   (1<<2)

typedef enum {
    TGE_TIMER_HIGH   = 0,
    TGE_TIMER_NORMAL = 1,
    TGE_TIMER_LOW    = 2,
} TGE_TimerPriority;

#define TGE_KEY_UNKNOWN     0
#define TGE_KEY_ESC         27
#define TGE_KEY_ENTER       13
#define TGE_KEY_TAB          9
#define TGE_KEY_BACKSPACE  127
#define TGE_KEY_SPACE       32
#define TGE_KEY_UP         256
#define TGE_KEY_DOWN       257
#define TGE_KEY_LEFT       258
#define TGE_KEY_RIGHT      259
#define TGE_KEY_HOME       260
#define TGE_KEY_END        261
#define TGE_KEY_PAGEUP     262
#define TGE_KEY_PAGEDOWN   263
#define TGE_KEY_INSERT     264
#define TGE_KEY_DELETE     265
#define TGE_KEY_F1         266
#define TGE_KEY_F2         267
#define TGE_KEY_F3         268
#define TGE_KEY_F4         269
#define TGE_KEY_F5         270
#define TGE_KEY_F6         271
#define TGE_KEY_F7         272
#define TGE_KEY_F8         273
#define TGE_KEY_F9         274
#define TGE_KEY_F10        275
#define TGE_KEY_F11        276
#define TGE_KEY_F12        277

typedef struct {
    TGE_EventType type;
    union {
        struct { int keycode; int mod; } key;
        struct { uint32_t codepoint; } text;
        struct { int x, y; int button; } mouse;
        struct { int w, h; } resize;
        struct { int id; int priority; } timer;
        struct { int code; void *data; } user;
    } data;
} TGE_Event;

#ifdef __cplusplus
}
#endif

#endif
