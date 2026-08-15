# TGE — Terminal Game Engine

TGE es un motor de juegos para terminal escrita en C: un *runtime* ANSI, un
*renderer* con render diferencial y un sistema de escenas sobre el cual vivien
los juegos. No es una librería para dibujar texto en una terminal — es un motor
donde los juegos son el punto, y la terminal es solo el *renderer*.

El núcleo (`libtge.a`) expone una API C pequeña y estable. Los módulos opcionales
(`tge-extra`, `libtge-extra.a`) añaden utilidades de grilla, *sprites*, física
ligera y un *adapter* de ciclo de vida, cada uno independiente y usando solo la
API pública. No impone ECS, cámara, *fixed-step* ni un framework de UI: el juego
decide.

## Features

- **Runtime ANSI**: backend de terminal con *raw mode*, colores, resize y parser de input.
- **Render diferencial**: solo se emiten las celdas que cambiaron.
- **Unicode / UTF-8**: ancho real de caracteres (los *wide chars* cuentan doble).
- **Playfield / Grid / TileMap**: capas para juegos de celda lógica (snake, sokoban, roguelikes).
- **Sistema de input/eventos**: teclado, ratón, resize, timers y eventos de usuario.
- **Escenas y adapter de juego**: stack de escenas, o `TGE_GameContext` para juegos con estado.
- **Sin malloc en el render path** (ver `docs/ARCHITECTURE.md`).

## Quick Start

```sh
git clone https://github.com/daimgz/TGE.git
cd TGE
make            # construye libtge.a y libtge-extra.a
make test       # corre las suites de prueba
make games      # construye los juegos
./examples/games/01_snake
```

Requiere un compilador C99 (`gcc` por defecto) y una terminal POSIX.

## Minimal example

```c
#include "tge/tge.h"

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_draw_text(canvas, 0, 0, "Hello, TGE", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
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

## Architecture

Una sola dirección: `libtge` (core) → `tge-extra` (módulos) → `examples`
(juegos). Ningún módulo incluye internos del core; los ejemplos solo usan
headers públicos. Ver `docs/ARCHITECTURE.md`.

## Modules

`tge-extra` agrupa módulos opcionales (grilla, *sprites*, *view*, *playfield*,
*fixedstep*, etc.). La lista completa con propósito y estado está en
`docs/MODULES.md`.

## Examples / Games

`01_snake`, `02_pong`, `03_tetris`, `04_space_invaders`, `05_swarm`,
`06_snake_grid`, `07_breakout`, `08_dino`, `09_dungeon`, `10_map_editor`,
`11_pacman`, `12_sokoban`, `13_minesweeper`. `06_snake_grid` es el ejemplo de
referencia de la arquitectura (split *world* / *renderer* sobre `TGE_GameContext`);
`01_snake` es su contraparte mínima, escrita directo sobre la API de escenas.

## Beta 1 status

**Beta 1** congela la API pública C (`include/tge/*.h`): durante la beta solo se
permiten cambios *additive* y justificados por un consumidor real; la
compatibilidad ABI/API definitiva se garantiza en RC/1.0. `tge-extra` sigue
**experimental** (puede romper entre *minor versions*). La proyección C++
(`bindings/cpp/`) es un laboratorio de ergonomía **excluido** de la política de
estabilidad. Ver `docs/API_STABILITY.md`.

## Run a game

```sh
make games
./examples/games/01_snake
```

Un frame típico de Snake se ve así:

```text
+------------------------------------------+
|                                          |
|    ####                                  |
|      #        .                           |
|      #                                    |
|      #######                             |
|            #                             |
|            #                             |
|                                          |
+------------------------------------------+
```

(El comando exacto y el frame real dependen de tu terminal; lo de arriba es
representativo.)

## Documentation

- `README.md` — qué es TGE y cómo empezar.
- `GETTING_STARTED.md` — aprender haciendo, con ejemplos progresivos.
- `docs/MODULES.md` — qué existe en `tge-extra`.
- `docs/ARCHITECTURE.md` — cómo está diseñado (capas, modelo de memoria, bindings).
- `docs/API_STABILITY.md` — qué significa "estable" en TGE.
- `CHANGELOG.md` — qué cambió en Beta 1.
- `IMPLEMENTATION_PLAN.md` — historia y decisiones internas del proyecto.
- `ADR.md` — decisiones arquitectónicas (Architecture Decision Records).

## Roadmap

`Beta 1` (API congelada) → `Beta 2` (feedback / estabilización, extensiones 1.1
según consumidores reales) → `RC` (compatibilidad definitiva) → `1.0`.

## License

MIT — ver [LICENSE](LICENSE).
