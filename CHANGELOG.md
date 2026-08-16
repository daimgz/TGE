# Changelog

Todas las fechas en formato ISO (YYYY-MM-DD). Las versiones siguen la política de
`docs/API_STABILITY.md`: la API pública C se congela en Beta 1 (solo cambios
*additive* y justificados por un consumidor real); `tge-extra` es experimental;
la compatibilidad ABI/API definitiva se garantiza en RC/1.0.

## [Unreleased]

### tge-extra (experimental)
- `ui`: añade `tge_draw_region(TGE_Canvas*, TGE_Rect, title, fg)` — panel
  rectangular delimitado (marco + título opcional en el borde superior).
  Extraído de la operación repetida *frame + título* que `03_tetris`,
  `09_dungeon` y `10_map_editor` implementaban de forma independiente. No es un
  status-bar ni un sistema de layout; el área de contenido la dibuja el
  consumidor (p.ej. `tge_rect_inset`).

## [1.0.0-beta.1] - 2026-08-15

Primer release de congelación de la API pública C.

### API pública C (estable en Beta 1)
- `include/tge/*.h` congelada para la beta: no se elimina ni se cambia la
  semántica de lo ya público durante Beta 1.
- Validado por `make test`: **34 suites de prueba, 0 fallos** (incluye
  autocontención de headers y la verificación de "sin malloc en render path").

### Build / ejemplos
- `make games` construye los 13 juegos de `examples/games/`.
- `make examples` construye los 9 ejemplos mínimos de `examples/min/`.

### tge-extra (experimental)
- Módulos opcionales: actor, animation, array, collision, direction, entity,
  fixedstep, game, grid, grid_view, input, input_buffer, playfield, sprite,
  tilemap, timer, ui, vec2i, view. Ver `docs/MODULES.md` para propósito y
  módulos validados por cada ejemplo.

### Laboratorio C++ (fuera de la política de estabilidad)
- `bindings/cpp/` es una proyección ergonómica `tge::` sobre la API C, congelada
  en "C++ 1.0" dentro de Beta 1. 4 sondas (snake, sokoban, pacman, minesweeper),
  **125 tests, 0 fallos**. No es una dependencia del engine ni del C API.

### Candidatos 1.1 (diferidos a Beta 2 / RC)
- `Event` sin accesores de ratón (M-1).
- Mapeo de coordenadas de `Playfield` / `draw_tilemap` (C-1).
- Loop manual (`TGE_Step` + `TGE_IsRunning`).
- `TGE_CreateConfig`, `TGE_SurfaceObserver`.
