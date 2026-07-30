#ifndef TGE_APP_H_
#define TGE_APP_H_

#include <stdbool.h>
#include "tge_events.h"
#include "tge_canvas.h"
#include "tge_runtime.h"

typedef struct TGE_App TGE_App;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tge_init_fn)(TGE_App *app);
typedef void (*tge_update_fn)(TGE_App *app, float dt);
typedef void (*tge_draw_fn)(TGE_App *app, TGE_Canvas *canvas);
typedef void (*tge_event_fn)(TGE_App *app, TGE_Event *ev);

TGE_App *TGE_Create(int width, int height, const char *title);
void     TGE_Destroy(TGE_App *app);
void     TGE_Run(TGE_App *app, tge_init_fn init, tge_update_fn update,
                 tge_draw_fn draw, tge_event_fn event);
void     TGE_Quit(TGE_App *app);

bool TGE_PollEvent(TGE_App *app, TGE_Event *ev);
TGE_Canvas *TGE_GetCanvas(TGE_App *app);
TGE_Runtime *TGE_GetRuntime(TGE_App *app);

#ifdef __cplusplus
}
#endif

#endif
