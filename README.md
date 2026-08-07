# TGE — Terminal Game Engine

TGE is a terminal game engine in C: a runtime, a canvas renderer and a
scene system that games live on top of. It is not a library for drawing text
to a terminal — it is an engine where the games are the point, and the
terminal is just the renderer.

## Layout

```
src/            the engine: runtime, canvas, renderer, scenes, scheduler
include/tge/    the public API (libtge.a)
tge-extra/      optional extension modules (libtge-extra.a)
examples/min/   minimal API examples (draw text, input, timer, grid)
examples/games/ playable games: snake, pong, tetris, space invaders, ...
tests/          unit tests + sanity checks
docs/           ADR, API stability policy
```

## Build

```sh
make            # builds libtge.a and libtge-extra.a
make test       # runs the test suites
make games      # builds all games
make examples   # builds all min examples
```

Requires a C99 compiler (`gcc` by default) and a POSIX terminal.

## Run a game

```sh
make games
./examples/games/01_snake
```

Games: `01_snake`, `02_pong`, `03_tetris`, `04_space_invaders`, `05_swarm`,
`06_snake_grid`, `07_breakout`, `08_dino`. `06_snake_grid` is the reference example of
the game architecture (world / renderer split over `TGE_Game`); `01_snake` is
its minimal counterpart, written directly on the scene API.

## Minimal program

```c
#include "tge/tge.h"

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_draw_text(canvas, 0, 0, "Hello", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
}

int main(void)
{
    TGE_App *app = TGE_Create(80, 24, "TGE");
    if (!app)
        return 1;
    TGE_Run(app, NULL, NULL, draw, NULL);
    TGE_Destroy(app);
    return 0;
}
```

## License

MIT — see [LICENSE](LICENSE).
