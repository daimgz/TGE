# Getting Started

Un recorrido progresivo: de clonar el repo a un pequeño juego con `Playfield`.
Los fragmentos usan la API pública de Beta 1; puedes copiarlos en un archivo
`.c` y compilar con `gcc -std=c99 tujuego.c -L. -ltge -lm`.

## 1. Build

```sh
make            # libtge.a + libtge-extra.a
make test       # 34 suites, 0 fallos
```

## 2. Run Snake

```sh
make games
./examples/games/01_snake
```

## 3. Crear la App

`TGE_Create` reserva todo; `TGE_Run` controla el loop y llama tus callbacks
(`init`, `update`, `draw`, `event`) cada frame; `TGE_Destroy` libera todo.

```c
#include "tge/tge.h"

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_clear(canvas, ' ', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 1, 1, "Hola", TGE_COLOR_GREEN, TGE_COLOR_BLACK);
}

int main(void)
{
    TGE_App *app = TGE_Create(80, 24, "Mi juego");
    if (!app) return 1;
    TGE_Run(app, NULL, NULL, draw, NULL);
    TGE_Destroy(app);
    return 0;
}
```

## 4. Crear un Playfield

Un `Playfield` junta una `View` (área lógica que se adapta a la terminal), una
`GridView` (superficie de dibujo) y un `GridLayout` (cache del tamaño aplicado).
Se configura una vez y se "engancha" al canvas en cada draw.

```c
#include "tge/tge.h"
#include "tge-extra/playfield.h"

static TGE_Playfield pf;

static void init(TGE_App *app)
{
    (void)app;
    tge_playfield_init(&pf, &TGE_GRID_THEME_BLOCKS,
                       TGE_GRID_SCALE_2X1, 10, 6);
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_playfield_attach(&pf, canvas);
    tge_playfield_draw_border(&pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    /* dibuja una celda en coordenadas locales del playfield */
    tge_grid_view_set_cell(&pf.grid_view, 3, 3,
                           TGE_COLOR_GREEN, TGE_COLOR_BLACK);
}
```

## 5. Procesar eventos

`TGE_PollEvent` extrae de la cola; solo es válido el miembro de `data` que
corresponde a `type`.

```c
#include "tge/tge.h"

static void event(TGE_App *app, TGE_Event *ev)
{
    (void)app;
    if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
            case TGE_KEY_LEFT:  /* ... */ break;
            case TGE_KEY_RIGHT: /* ... */ break;
            case TGE_KEY_ESC:   TGE_Quit(app); break;
        }
    }
}
```

## 6. Update con paso fijo

Para lógica determinista usa `TGE_FixedStep`: acumula `dt` y te da un número
entero de pasos de simulación.

```c
#include "tge-extra/fixedstep.h"

static TGE_FixedStep fs;

static void init(TGE_App *app) { (void)app; tge_fixedstep_init(&fs, 0.10f); }

static void update(TGE_App *app, float dt)
{
    (void)app;
    tge_fixedstep_update(&fs, dt);
    while (tge_fixedstep_next(&fs)) {
        /* un paso de simulación: mover, colisionar, spawn */
    }
}
```

## 7. Draw

Dibuja en coordenadas locales del `Playfield` (la `View` traduce al canvas).
Para *sprites* usa `tge_grid_view_put_local`; para texto/HUD usa
`tge_draw_text` (que no escala por celda).

```c
TGE_Sprite head = TGE_SPRITE(1, 1, (const char *[]){"@"}, (const char *[]){"@"});
tge_grid_view_put_local(&pf.grid_view, &pf.view,
                        tge_vec2i(px, py), &head,
                        TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
```

## 8. Resize

El terminal puede cambiar de tamaño. `tge_playfield_sync` recalcula el grid
lógico y, si cambió, llama a tu callback `on_resize` (donde adaptas el mundo).
Nunca muta estado de juego por sí solo: el callback es el único efecto.

```c
static void on_resize(void *userdata, int grid_w, int grid_h)
{
    World *world = userdata;
    world_resize(world, grid_w, grid_h); /* re-coloca dentro del nuevo tamaño */
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_playfield_attach(&pf, canvas);
    if (tge_playfield_sync(&pf, w, h, on_resize, &world)) {
        /* el layout cambió: on_resize ya adaptó el mundo */
    }
    tge_playfield_draw_border(&pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
}
```

## 9. Un juego mínimo

Juntando lo anterior: `init` configura el playfield y el *fixed step*; `update`
avanza la serpiente un paso por tick; `draw` sincroniza y pinta; `event`
cambia la dirección. El esqueleto completo está en
`examples/games/01_snake.c` (y su versión sobre `TGE_GameContext` en
`06_snake_grid.c`).

El flujo canónico es siempre el mismo:

```
TGE_Create  ->  TGE_Run(init, update, draw, event)  ->  TGE_Destroy
```
