#define _POSIX_C_SOURCE 200112L
#include "tge/tge_app.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

static void ensure_diff_capacity(TGE_App *app, int w, int h)
{
    int need = (w * h + h) / 2 + 1;
    if (need > app->diff.capacity) {
        tge_diff_free(&app->diff);
        tge_diff_init(&app->diff, need);
    }
}

static void dispatch_event(TGE_App *app, TGE_Event *ev)
{
    if (app->scene_count > 0) {
        TGE_Scene *top = app->scenes[app->scene_count - 1];
        if (top->event)
            top->event(top, ev);
    } else if (app->event_cb) {
        app->event_cb(app, ev);
    }
}

static void handle_events(TGE_App *app, double now)
{
    TGE_Runtime *rt = app->runtime;
    TGE_Event ev;

    for (int i = 0; i < app->app_event_count; i++) {
        if (app->app_events[i].type == TGE_EVENT_QUIT) {
            app->quit = true;
            app->app_event_count = 0;
            return;
        }
        dispatch_event(app, &app->app_events[i]);
    }
    app->app_event_count = 0;

    tge_runtime_pump_input(rt);
    while (tge_runtime_poll_queued(rt, &ev)) {
        if (ev.type == TGE_EVENT_RESIZE) {
            int w = ev.data.resize.w;
            int h = ev.data.resize.h;
            tge_canvas_resize(app->current, w, h);
            tge_canvas_resize(app->previous, w, h);
            tge_clear(app->previous, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
            ensure_diff_capacity(app, w, h);
        }
        if (ev.type == TGE_EVENT_QUIT) {
            app->quit = true;
            return;
        }
        dispatch_event(app, &ev);
    }

    tge_runtime_pump_timers(rt, now);
    while (tge_runtime_poll_queued(rt, &ev)) {
        if (ev.type == TGE_EVENT_QUIT) {
            app->quit = true;
            return;
        }
        dispatch_event(app, &ev);
    }
}

static void update_scene(TGE_App *app, float dt)
{
    if (app->scene_count > 0) {
        TGE_Scene *top = app->scenes[app->scene_count - 1];
        if (top->update)
            top->update(top, dt);
    } else if (app->update_cb) {
        app->update_cb(app, dt);
    }
}

static void draw_scene(TGE_App *app)
{
    tge_clear(app->current, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    if (app->scene_count > 0) {
        for (int i = 0; i < app->scene_count; i++) {
            TGE_Scene *sc = app->scenes[i];
            if (sc->draw)
                sc->draw(sc, app->current);
            if (sc->opaque)
                break;
        }
    } else if (app->draw_cb) {
        app->draw_cb(app, app->current);
    }
}

void tge_app_frame(TGE_App *app)
{
    if (!app)
        return;
    TGE_Runtime *rt = app->runtime;
    uint64_t ms = tge_runtime_ticks(rt);
    double now = (double)ms / 1000.0;
    float dt = (float)(now - app->last_time);
    app->last_time = now;

    handle_events(app, now);
    if (app->quit)
        return;

    update_scene(app, dt);

    draw_scene(app);

    tge_renderer_diff(app->current, app->previous, &app->diff);
    tge_runtime_present(rt, &app->diff, app->current->cells, app->current->width);
    {
        TGE_Canvas *tmp = app->previous;
        app->previous = app->current;
        app->current = tmp;
    }

    tge_app_process_scene_ops(app);

    if (app->fps > 0.0f) {
        double frame_ms = 1000.0 / (double)app->fps;
        double elapsed = (double)(tge_runtime_ticks(rt) - ms);
        double rem = frame_ms - elapsed;
        if (rem > 0.5) {
            struct timeval tv;
            tv.tv_sec = (time_t)(rem / 1000.0);
            tv.tv_usec = (suseconds_t)((rem - (double)tv.tv_sec * 1000.0) * 1000.0);
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(STDIN_FILENO, &rfds);
            select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        }
    }
}

TGE_App *TGE_Create(int width, int height, const char *title)
{
    TGE_Runtime *rt = tge_runtime_create(width, height);
    if (!rt)
        return NULL;
    int real_w = tge_runtime_width(rt);
    int real_h = tge_runtime_height(rt);
    TGE_App *app = tge_app_create_with_runtime(rt, real_w, real_h);
    if (!app) {
        tge_runtime_destroy(rt);
        return NULL;
    }
    if (title)
        tge_runtime_set_title(rt, title);
    return app;
}

TGE_App *tge_app_create_with_runtime(TGE_Runtime *rt, int width, int height)
{
    if (!rt || width <= 0 || height <= 0)
        return NULL;
    TGE_App *app = (TGE_App *)calloc(1, sizeof(TGE_App));
    if (!app)
        return NULL;
    app->runtime = rt;
    app->current = tge_canvas_create(width, height);
    app->previous = tge_canvas_create(width, height);
    if (!app->current || !app->previous) {
        tge_app_destroy(app);
        return NULL;
    }
    tge_diff_init(&app->diff, (width * height + height) / 2 + 1);
    if (!app->diff.spans) {
        tge_app_destroy(app);
        return NULL;
    }
    app->fps = 60.0f;
    app->last_time = (double)tge_runtime_ticks(rt) / 1000.0;
    return app;
}

void TGE_Destroy(TGE_App *app)
{
    tge_app_destroy(app);
}

void tge_app_destroy(TGE_App *app)
{
    if (!app)
        return;
    /* Destroy any heap-managed scenes still on the stack: the app owns them
     * while they are pushed, so at shutdown it must release them. Done before
     * freeing the runtime so destroy() callbacks may still use the app. */
    for (int i = app->scene_count - 1; i >= 0; --i) {
        TGE_Scene *scene = app->scenes[i];
        if (scene->destroy)
            scene->destroy(scene);
    }
    app->scene_count = 0;
    tge_diff_free(&app->diff);
    if (app->current)
        tge_canvas_destroy(app->current);
    if (app->previous)
        tge_canvas_destroy(app->previous);
    if (app->runtime)
        tge_runtime_destroy(app->runtime);
    free(app);
}

void TGE_Run(TGE_App *app, tge_init_fn init,
             tge_update_fn update, tge_draw_fn draw,
             tge_event_fn event)
{
    if (!app)
        return;
    app->init_cb = init;
    app->update_cb = update;
    app->draw_cb = draw;
    app->event_cb = event;
    app->quit = false;
    if (init)
        init(app);
    while (!app->quit)
        tge_app_frame(app);
}

void TGE_Quit(TGE_App *app)
{
    if (app)
        app->quit = true;
}

bool TGE_PollEvent(TGE_App *app, TGE_Event *ev)
{
    if (!app || !ev)
        return false;
    tge_runtime_pump_input(app->runtime);
    return tge_runtime_poll_queued(app->runtime, ev);
}

void TGE_PushEvent(TGE_App *app, const TGE_Event *ev)
{
    if (!app || !ev || app->app_event_count >= TGE_APP_EVENT_PENDING)
        return;
    app->app_events[app->app_event_count++] = *ev;
}

void TGE_SetFPS(TGE_App *app, float fps)
{
    if (app)
        app->fps = fps;
}

void TGE_SetTitle(TGE_App *app, const char *title)
{
    if (app && app->runtime && title)
        tge_runtime_set_title(app->runtime, title);
}

TGE_Canvas *TGE_GetCanvas(TGE_App *app)
{
    return app ? app->current : NULL;
}

TGE_Runtime *TGE_GetRuntime(TGE_App *app)
{
    return app ? app->runtime : NULL;
}
