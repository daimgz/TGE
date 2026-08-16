# Tetris C++ — Diseño y contrato interno (Beta 2)

Objetivo: convertir `examples/games/03_tetris.c` en un **Tetris completo de
referencia construido exclusivamente sobre TGE**, y usarlo como *benchmark de
ergonomía* del motor a escala (no ~200 líneas, sino un juego con motor propio).

Regla rectora (consumer-driven, igual que F4/F6): el juego puede volverse
sofisticado sin pedirle a TGE que conozca T-spins, 7-bag, hold, etc. Si algo
del motor lo necesita, se evalúa y solo pasa a TGE si es **generalizable**, no
por anticipación.

## Frontera TGE — lo que el juego CONSUME, no lo que define

- `tge::App` (`run` + bridges `update_bridge`/`draw_bridge`/`event_bridge`) — loop /
  scene glue. No hay `tge::Game`: el estado `Title`/`Playing`/`Paused`/`GameOver`
  reemplaza el scene stack del C.
- `tge::Canvas` (`tge_draw_region` vía `operator TGE_Canvas*`) — HUD / paneles.
- `tge::Grid` — board 10x20 + preview 5x5. Grilla de tamaño **FIJO** (no se expande).
- `tge::Playfield` — **solo** como gate de ventana "too small" (`update_view` /
  `valid`). Su grilla se expande al tamaño de la ventana (`tge_view_update` pone
  `area = interior de la ventana`), por eso NO sirve para el board: de ahí la
  necesidad de `tge::Grid` (generalizable — snake/sokoban/minesweeper/pacman ya
  usaban `tge_grid` en crudo).
- `tge::Event` — input (base para DAS/ARR en Fase 2).
- `tge::FixedStep` — gravedad (acumulador interno) y cooldown de rotación. Reemplaza
  al scheduler / `TGE_Timer` del C.
- `tge::Vec2i`, `tge::Color`, `tge::Sprite`, `tge::CellAttr` (DIM para ghost).

TGE **no** define: 7-bag, SRS, kicks, hold, scoring, T-spin, combo, B2B,
perfect clear, replay, config. Todas son responsabilidad del juego.

## Contrato interno (game-only, congelado antes de portar)

Cada responsabilidad es una clase/módulo C++ del juego; no toca la API de TGE.

- `TetrisGame` — orquestador. Posee el mundo y implementa `update`/`draw`/`event`
  del adapter. Único punto que toca `tge::App` (vía los bridges).
- `Board` — matriz de celdas + `lock(Piece)`, `clearLines()`, `isGameOver()`.
  Usa `tge::Grid` **solo para dibujar**.
- `Piece` — forma (celdas), tipo, color, posición, rotación actual.
- `RotationSystem` — SRS + kick table (game logic). `rotate(Piece, dir, Board)
  -> bool` (con wall kicks).
- `Bag` — 7-bag (`bag_refill` / `bag_next`).
- `PieceQueue` — cola: actual + NEXT (y HOLD en fase 2).
- `Scoring` — score, lines, level, combo, B2B, T-spin, perfect clear.
- `Renderer` — dibuja board/grid/ghost/HUD vía `tge::Canvas` + `tge::ui`; reusa
  el ghost atenuado (DIM). Mantiene el ghost piece agregado en C.
- `Replay` (fase 3) — graba la secuencia de inputs.
- `Config` (fase 3, solo si hay necesidad real) — opciones; evaluar si merece
  módulo propio o simples flags.

## Fases

- **Fase 0 — Baseline:** `bindings/cpp/examples/tetris/` con el adapter + un
  port fiel de `03_tetris.c` (board, piece, 7-bag, ghost, HUD mínimo). Congela
  el contrato. Es la primera sonda a escala.
- **Fase 1 — Separar:** extraer `Board` / `Piece` / `Bag` / `PieceQueue` /
  `Renderer` / `Scoring` en sus archivos **cuando aparezca la responsabilidad
  real** (no todos desde el commit 0).
- **Fase 2 — Reglas modernas:** `RotationSystem` (SRS + kicks), Hold, Lock delay,
  DAS/ARR, T-spin, Combo, B2B, Perfect Clear.
- **Fase 3 — Producto:** Modes, Statistics, Replay, Training, Configuration —
  cada uno solo si surge necesidad real y justificada.

## Preguntas-sonda (qué revela este Tetris sobre TGE)

- ¿`tge::Event` / `InputBuffer` alcanza para DAS/ARR? (¿hace falta acceso al
  tiempo de repetición de tecla?)
- ¿`tge::Timer` alcanza para lock delay? (timer por celda vs por estado)
- ¿`tge::Grid` alcanza para board + previews + hold simultáneos?
- ¿`tge::Game` / `Scene` aguanta un juego grande sin fugas de estado?
- ¿`tge::Canvas` / `tge::ui` alcanza para HUD complejo (ghost, stats, queue)?
- ¿hace falta un módulo de configuración en TGE? (no, salvo que sea general)
- ¿hace falta replay en TGE? (no, es responsabilidad del juego)

Toda respuesta "no alcanza" se registra como candidato 1.1; no se implementa
especulativamente.

## Referencias (especificaciones vivientes, por responsabilidad)

- SRS, T-spins, kicks, perfect clears, combos: `knewjade/solution-finder`
  (y su C++ `sfinder-cpp`).
- Pieza / rotación / spin como protocolo: Tetris Bot Protocol (gist de
  MinusKelvin).
- DAS/ARR y UX/handling: `oatrice/Tetris-Battle`.
- Comportamiento clásico / Game Boy (histórico): `osnr/tetris`.
- Baseline de comportamiento actual: `examples/games/03_tetris.c` (ghost piece +
  7-bag ya implementados en C; se portan fielmente).
