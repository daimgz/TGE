#ifndef TGE_EVENTS_H_
#define TGE_EVENTS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TGE_EVENT_NONE = 0,
    TGE_EVENT_KEYDOWN,
    TGE_EVENT_KEYUP,
    TGE_EVENT_MOUSEDOWN,
    TGE_EVENT_MOUSEUP,
    TGE_EVENT_MOUSEMOVE,
    TGE_EVENT_RESIZE,
    TGE_EVENT_QUIT,
    TGE_EVENT_TIMER,
    TGE_EVENT_USER,
} TGE_EventType;

typedef enum {
    TGE_TIMER_HIGH   = 0,
    TGE_TIMER_NORMAL = 1,
    TGE_TIMER_LOW    = 2,
} TGE_TimerPriority;

typedef struct {
    TGE_EventType type;
    union {
        struct { int keycode; int mod; } key;
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
