#ifndef TGE_INTERNAL_H_
#define TGE_INTERNAL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "tge/tge_canvas.h"
#include "tge/tge_events.h"
#include "tge/tge_app.h"
#include "tge/tge_scene.h"

#define TGE_RUNTIME_QUEUE_CAP 64
#define TGE_SCHED_MAX_TIMERS 256
#define TGE_SCENE_MAX 16
#define TGE_SCENE_OPS_MAX 32
#define TGE_APP_EVENT_PENDING 16

typedef struct {
    int y;
    int x_start;
    int x_end;
} TGE_DirtySpan;

struct TGE_Diff {
    TGE_DirtySpan *spans;
    int count;
    int capacity;
};

typedef struct {
    void *data;
    bool (*init)(void *data, int w, int h);
    void (*term)(void *data);
    int  (*width)(void *data);
    int  (*height)(void *data);
    void (*present)(void *data, struct TGE_Diff *diff, const TGE_Cell *cells, int stride);
    int  (*read_input)(void *data, char *buf, int bufsize);
    uint64_t (*ticks)(void *data);
    void (*set_title)(void *data, const char *title);
} TGE_Backend;

typedef struct TGE_Parser TGE_Parser;
typedef struct TGE_Scheduler TGE_Scheduler;

struct TGE_Runtime {
    TGE_Backend *backend;
    int width;
    int height;
    TGE_Parser *parser;
    TGE_Scheduler *scheduler;
    TGE_Event queue[TGE_RUNTIME_QUEUE_CAP];
    int q_head;
    int q_tail;
    TGE_DirtySpan *full_spans;
    int full_spans_cap;
};

struct TGE_Canvas {
    int width;
    int height;
    TGE_Cell *cells;
};

typedef enum {
    TGE_SCENE_OP_PUSH = 1,
    TGE_SCENE_OP_POP,
    TGE_SCENE_OP_REPLACE,
} TGE_SceneOpType;

typedef struct {
    TGE_SceneOpType type;
    TGE_Scene *scene;
} TGE_SceneOp;

struct TGE_App {
    TGE_Runtime *runtime;
    TGE_Canvas *current;
    TGE_Canvas *previous;
    TGE_Diff diff;
    TGE_Scene *scenes[TGE_SCENE_MAX];
    int scene_count;
    TGE_SceneOp op_queue[TGE_SCENE_OPS_MAX];
    int op_count;
    bool quit;
    float fps;
    double last_time;
    tge_init_fn init_cb;
    tge_update_fn update_cb;
    tge_draw_fn draw_cb;
    tge_event_fn event_cb;
    TGE_Event app_events[TGE_APP_EVENT_PENDING];
    int app_event_count;
};

/* ── Backend ─────────────────────────────────────────── */

TGE_Backend *tge_backend_ansi_create(void);
TGE_Backend *tge_backend_ansi_create_with_file(FILE *out);

/* ── Parser (Fase 1.2) ───────────────────────────────── */

TGE_Parser *tge_parser_create(void);
void tge_parser_destroy(TGE_Parser *p);
void tge_parser_feed(TGE_Parser *p, const char *bytes, int len);
void tge_parser_flush(TGE_Parser *p);
bool tge_parser_poll(TGE_Parser *p, TGE_Event *ev);

/* ── Scheduler (Fase 1.4) ────────────────────────────── */

TGE_Scheduler *tge_scheduler_new(void);
void tge_scheduler_free(TGE_Scheduler *s);
int  tge_scheduler_call_later(TGE_Scheduler *s, double delay_sec,
                              int event_id, int priority);
int  tge_scheduler_call_every(TGE_Scheduler *s, double interval,
                              int event_id, int priority);
void tge_scheduler_cancel(TGE_Scheduler *s, int timer_id);
void tge_scheduler_poll(TGE_Scheduler *s, double now,
                        TGE_Event *ev_out, int *count);

/* ── Runtime internals (Fase 1.5) ────────────────────── */

TGE_Runtime *tge_runtime_create_with_backend(TGE_Backend *backend,
                                             int width, int height);
void tge_runtime_pump_input(TGE_Runtime *rt);
void tge_runtime_pump_timers(TGE_Runtime *rt, double now_sec);
bool tge_runtime_poll_queued(TGE_Runtime *rt, TGE_Event *ev);
void tge_runtime_set_title(TGE_Runtime *rt, const char *title);

/* ── Canvas internals (Fase 1.6) ─────────────────────── */

TGE_Canvas *tge_canvas_create(int width, int height);
void tge_canvas_destroy(TGE_Canvas *canvas);
void tge_canvas_resize(TGE_Canvas *canvas, int width, int height);

/* ── Diff helpers (Fase 1.7) ─────────────────────────── */

void tge_diff_init(TGE_Diff *diff, int capacity);
void tge_diff_free(TGE_Diff *diff);
void tge_renderer_diff(const TGE_Canvas *current, const TGE_Canvas *previous,
                       struct TGE_Diff *diff);

/* ── App internals (Fase 2) ──────────────────────────── */

TGE_App *tge_app_create_with_runtime(TGE_Runtime *rt, int width, int height);
void tge_app_destroy(TGE_App *app);
void tge_app_frame(TGE_App *app);
void tge_app_process_scene_ops(TGE_App *app);

#endif
