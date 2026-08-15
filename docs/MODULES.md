# Módulos de `tge-extra`

`tge-extra/` es una colección de módulos opcionales compilados en
`libtge-extra.a`. Cada módulo es independiente, usa solo la API pública de TGE y
**nunca** incluye internos del core. Los juegos que no los usan linkean solo
`-ltge`.

> **Estado en Beta 1:** todos los módulos de esta tabla son **experimentales**
> (pueden romper compatibilidad entre *minor versions*). La columna
> *Validated-by* lista ejemplos de `examples/` que ya los consumen (los
> archivos `tests/_*.c` son pruebas unitarias del propio módulo, no consumidores
> externos).

| Módulo | Propósito | Validated-by | Estado |
|--------|-----------|--------------|--------|
| `actor` | Entidad visual simple (posición + sprite) para grilla. | `11_pacman`, sondas C++ | experimental (Beta 1) |
| `animation` | Animación por frames (create/destroy). | `05_swarm`, `07_extra_demo` | experimental (Beta 1) |
| `array` | Array dinámico genérico (crecimiento on-demand). | `06_snake_grid`, `07_breakout` | experimental (Beta 1) |
| `collision` | Mundo de colisiones (create/destroy + grow). | `05_swarm`, `07_extra_demo` | experimental (Beta 1) |
| `direction` | Direcciones cardinales + vectores. | `01_snake`, `06_snake_grid`, `08_dino`, `09_dungeon`, `10_map_editor`, `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `entity` | Pool de entidades con generación/id. | `05_swarm`, `07_extra_demo` | experimental (Beta 1) |
| `fixedstep` | Simulación de paso fijo (acumula `dt`). | `01_snake`, `06_snake_grid`, `11_pacman` | experimental (Beta 1) |
| `game` | Adapter de ciclo de vida sobre `TGE_Scene` (`TGE_GameContext`). | `03_tetris`, `06_snake_grid`, `07_breakout`, `08_dino`, `09_dungeon`, `10_map_editor`, `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `grid` | Capa de dibujo por grilla (tema, cell size, origen). | `03_tetris`, `06_snake_grid`, `07_breakout`, `08_grid_canvas`, `09_dungeon`, `10_map_editor` | experimental (Beta 1) |
| `grid_view` | Wrapper de configuración sobre `TGE_Grid` (init/attach). | `06_snake_grid`, `07_breakout` | experimental (Beta 1) |
| `input` | Interpretación de eventos a intención. | `01_snake`, `06_snake_grid`, `07_breakout`, `08_dino`, `09_dungeon`, `10_map_editor`, `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `input_buffer` | FIFO de direcciones (drop-new). | `01_snake`, `06_snake_grid`, `11_pacman` | experimental (Beta 1) |
| `playfield` | Composición `view` + `grid_view` + `layout`. | `11_pacman`, `12_sokoban`, `13_minesweeper`, sondas C++ | experimental (Beta 1) |
| `sprite` | Representación visual + máscara de colisión (`TGE_MaskSprite`). | `08_dino` | experimental (Beta 1) |
| `tilemap` | Mapa mutable de roles sobre `TGE_Grid`. | `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `timer` | Temporización (`call_later` / `call_every`). | `03_tetris` | experimental (Beta 1) |
| `ui` | Helpers de UI (paneles/HUD). | `06_snake_grid`, `07_breakout`, `08_dino`, `09_dungeon`, `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `vec2i` | Matemática 2D de enteros (celdas). | `01_snake`, `03_tetris`, `05_swarm`, `06_snake_grid`, `07_breakout`, `08_dino`, `09_dungeon`, `10_map_editor`, `11_pacman`, `12_sokoban`, `13_minesweeper` | experimental (Beta 1) |
| `view` | Área lógica adaptativa (mínimo, validez, layout). | `01_snake`, `03_tetris`, `06_snake_grid`, `07_breakout`, `08_dino`, `11_pacman` | experimental (Beta 1) |

## Regla de agregado

Un módulo nuevo solo se agrega si elimina un patrón que apareció en al menos dos
juegos (ver `docs/API_STABILITY.md` y `ADR.md`). Si un header de esta lista no
tuviera una responsabilidad clara, es señal de que no debería existir todavía.
