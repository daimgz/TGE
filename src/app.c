#include "tge/tge_app.h"
#include "tge_internal.h"
#include <stdlib.h>

struct TGE_App {
    TGE_Runtime *runtime;
};

TGE_App *TGE_Create(int width, int height, const char *title)
{
    (void)width; (void)height; (void)title;
    return NULL;
}

void TGE_Destroy(TGE_App *app)
{
    (void)app;
}

void TGE_Run(TGE_App *app, tge_init_fn init,
             tge_update_fn update, tge_draw_fn draw,
             tge_event_fn event)
{
    (void)app; (void)init; (void)update; (void)draw; (void)event;
}

void TGE_Quit(TGE_App *app)
{
    (void)app;
}

bool TGE_PollEvent(TGE_App *app, TGE_Event *ev)
{
    (void)app; (void)ev;
    return false;
}

TGE_Canvas *TGE_GetCanvas(TGE_App *app)
{
    (void)app;
    return NULL;
}

TGE_Runtime *TGE_GetRuntime(TGE_App *app)
{
    (void)app;
    return NULL;
}
