#ifndef TGE_APP_H_
#define TGE_APP_H_

#include <stdbool.h>
#include "tge_events.h"
#include "tge_canvas.h"
#include "tge_runtime.h"

/* Application-level API on top of TGE_Runtime: window, game loop and
 * scene stack. */
typedef struct TGE_App TGE_App;

#ifdef __cplusplus
extern "C" {
#endif

/* Callbacks passed to TGE_Run. dt is the frame delta in seconds. */
typedef void (*tge_init_fn)(TGE_App *app);
typedef void (*tge_update_fn)(TGE_App *app, float dt);
typedef void (*tge_draw_fn)(TGE_App *app, TGE_Canvas *canvas);
typedef void (*tge_event_fn)(TGE_App *app, TGE_Event *ev);

/* Creates an application.
 *
 * width and height are FALLBACK dimensions, not a guaranteed canvas size.
 * If the backend can query the real output size (for example a terminal),
 * that size is used instead. Applications should adapt their rendering to
 * the canvas size they receive in draw() and resize events. */
TGE_App *TGE_Create(int width, int height, const char *title);
/* Frees the application. Destroys any heap-managed scenes still present on
 * the scene stack before releasing the runtime (see tge_scene_create for the
 * ownership contract); scenes built manually are the caller's responsibility. */
void     TGE_Destroy(TGE_App *app);
/* Runs the main loop, calling the given callbacks each frame, until
 * TGE_Quit is called or the terminal sends QUIT. */
void     TGE_Run(TGE_App *app, tge_init_fn init, tge_update_fn update,
                 tge_draw_fn draw, tge_event_fn event);
/* Requests the main loop to exit after the current frame. */
void     TGE_Quit(TGE_App *app);

/* Dequeues one pending event; returns false when the queue is empty. */
bool TGE_PollEvent(TGE_App *app, TGE_Event *ev);
/* Injects a synthetic event; dropped when the (fixed-size) queue is full. */
void TGE_PushEvent(TGE_App *app, const TGE_Event *ev);
TGE_Canvas *TGE_GetCanvas(TGE_App *app);
TGE_Runtime *TGE_GetRuntime(TGE_App *app);

/* Caps the frame rate to the given FPS (0 disables the cap). */
void TGE_SetFPS(TGE_App *app, float fps);
/* Sets the terminal window title; no-op when the backend has no title
 * support. Passing NULL leaves the current title unchanged. */
void TGE_SetTitle(TGE_App *app, const char *title);

#ifdef __cplusplus
}
#endif

#endif
