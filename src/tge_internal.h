#ifndef TGE_INTERNAL_H_
#define TGE_INTERNAL_H_

#include <stdint.h>
#include <stdbool.h>

#include "tge/tge_canvas.h"
#include "tge/tge_events.h"

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
} TGE_Backend;

struct TGE_Runtime {
    TGE_Backend *backend;
    int width;
    int height;
};

struct TGE_Canvas {
    int width;
    int height;
    TGE_Cell *cells;
};

TGE_Backend *tge_backend_ansi_create(void);

typedef struct TGE_Parser TGE_Parser;
typedef struct TGE_Scheduler TGE_Scheduler;

TGE_Parser *tge_parser_new(void);
void tge_parser_free(TGE_Parser *p);
void tge_parser_feed(TGE_Parser *p, const char *bytes, int len);
bool tge_parser_poll(TGE_Parser *p, TGE_Event *ev);

TGE_Scheduler *tge_scheduler_new(void);
void tge_scheduler_free(TGE_Scheduler *s);
int  tge_scheduler_call_later(TGE_Scheduler *s, double delay_sec,
                              int event_id, int priority);
int  tge_scheduler_call_every(TGE_Scheduler *s, double interval,
                              int event_id, int priority);
void tge_scheduler_cancel(TGE_Scheduler *s, int timer_id);
void tge_scheduler_poll(TGE_Scheduler *s, double now,
                        TGE_Event *ev_out, int *count);

#endif
