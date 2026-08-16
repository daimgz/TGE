# Tetris C++ — Equivalencias con `03_tetris.c`

Mapeo del baseline C (`examples/games/03_tetris.c`) al Tetris C++ (`bindings/cpp/examples/tetris/`).
El objetivo del port es **comportamiento baseline preservado**, no fidelidad estructural 1:1 con el
ejemplo C: el C fue escrito para demostrar `Grid + Timer + View + scheduler + Game`; el C++ es un
Tetris serio que usa solo la superficie envuelta `tge::`.

| Baseline C (`03_tetris.c`)          | Tetris C++                                            |
| ----------------------------------- | ----------------------------------------------------- |
| gravedad por `tge_runtime_call_every` (scheduler) | `FixedStep gravity` interno (acumulador de paso fijo en `update`) |
| `TGE_Timer` debounce de rotación    | `FixedStep rot` interno + `rot.reset()` (cooldown)   |
| `TGE_View` (gate "too small")       | `tge::Playfield::update_view` / `valid()` (solo View) |
| `TGE_Grid` board 10x20              | `tge::Grid board_grid` (fijo, no se expande)         |
| `TGE_Grid` preview 5x5              | `tge::Grid preview_grid`                              |
| Scene stack (`TGE_PushScene`/`tge_game_create`) | `enum class State { Title, Playing, Paused, GameOver }` |
| `TGE_GameContext`                   | `TetrisGame` (orquestador)                            |
| `fits` / `fits_shape`               | `Board::fits`                                         |
| `lock` / `clear_lines`              | `Board::lock` / `Board::clearLines`                   |
| `bag_refill` / `bag_next` (7-bag)   | `Bag::refill` / `Bag::next`                           |
| `rotate_cw` + kicks horizontales    | `RotationSystem::rotateCW` / `tryRotate`              |
| `tge_grid_put_attr(..., DIM)` ghost | `Grid::put_attr(..., TGE_CELL_ATTR_DIM)`              |
| `tge_draw_region` HUD/NEXT          | `tge_draw_region` vía `Canvas` (mismo C subyacente)   |

## Sondas de ergonomía reveladas

- **`tge::Playfield` NO sirve para un board fijo**: expande la grilla al tamaño de la ventana
  (`tge_view_update` pone `area = interior de la ventana`). Por eso se añadió `tge::Grid`
  (grilla de tamaño fijo, generalizable — la usaban snake/sokoban/minesweeper/pacman en crudo).
- **Gravedad / lock delay / ARE**: un acumulador de paso fijo interno alcanza (sin `tge::Runtime`).
- **Rotación / DAS / ARR**: cooldown interno alcanza para el baseline; DAS/ARR reales en Fase 2.
- **Pausa**: `Event` + `State` alcanza, sin nueva API.
- **Ghost / 7-bag / SRS / Hold / Replay**: estado de gameplay, nada nuevo en TGE.

## Comportamiento preservado (checklist Fase 0)

- [x] Board 10x20, pieza activa, NEXT preview
- [x] Movimiento L/R, rotación CW con kicks, soft/hard drop
- [x] Gravedad por nivel, line clear, score/level/lines
- [x] Pause / Game Over / retry / back to title
- [x] Ghost piece (proyección de aterrizaje atenuada)
- [x] 7-bag
- [x] Resize ("too small" gate)
- [x] Tests headless (`bindings/cpp/tests/test_tetris.cpp`)
