# Implementation Plan — TGE Beta 1 (freeze de API) + roadmap a v1.0 final

**Estado:** Fase 4 cerrada (auditoría de API + freeze C++ 1.0 para Beta 1 + validación post-freeze Minesweeper + migración de ejemplos al Game adapter + loop manual diferido). Fase 5 = Release Beta 1 (siguiente, pendiente de tag/versión).
**Base:** `ADR.md` (26 decisiones arquitectónicas), `PLAN_ENTRENAMIENTO.md` (roadmap por juegos).
**Objetivo:** Este documento **ya no es un plan de construcción desde cero**. TGE tiene hoy un núcleo validado, varios juegos consumidores y un `tge-extra` bastante desarrollado. El objetivo ahora es (a) documentar qué está validado, (b) cerrar una **API congelada para Beta 1** (la proyección C++ queda como 1.0 dentro de la beta), y (c) señalar el roadmap a v1.0 final. La regla rectora sigue siendo la de Fase 3b: *el módulo se implementa cuando un juego lo pide, no antes*.

> **Distinción importante:** en este documento, **"auditado" ≠ "cumple automáticamente"**. Una auditoría puede concluir "cumple", "cumple con salvedad (evitable pre-reservando)" o "issue de Fase 4". Cada caso se documenta con su evidencia.

---

## 1. Principios de implementación

1. **Fases estrictas**: no se avanza a la siguiente fase hasta que la anterior tenga tests pasando y un programa de ejemplo funcional.
2. **Cada fase produce un programa ejecutable**: no hay código muerto sin probar.
3. **Sin `malloc` en el hot path**: toda la memoria se pre-asigna en init/resize (ADR-015). Ver auditoría 4.3.
4. **Tests primero para el parser**: bytes→eventos tiene muchos casos borde (ANSI, UTF-8, resize).
5. **El Makefile se actualiza en cada fase**: no hay un "gran rewrite" del build.
6. **Abstracción solo si elimina un patrón repetido en ≥2 juegos** (ver §4). Esta regla resultó más valiosa que el roadmap original.

---

## 2. Arquitectura y filosofía (niveles de API)

TGE ocupa una capa intermedia: por debajo SDL/SFML, por arriba Godot/Unity/Raylib. Ofrece un **núcleo bajo nivel estable** y módulos opcionales que suben la abstracción.

```
                 Usuario
                    |
              Aplicación
                    |
        +-----------+-----------+
        |                       |
   TGE Core                tge-extra
        |                       |
   Runtime                 grid, grid_view, view,
   Canvas                  input_buffer, vec2i,
   Events                  direction, timer,
   Scene                   fixedstep, input,
   Backend (interno)       entity, animation,
        |                   collision, array,
   ANSI / WinConsole       sprite, tilemap,
                            actor, playfield, game, ui
                            |
                       FOV / Pathfinding / Noise / AI  (post-1.0)
```

### Core (`libtge.a`) — "puedo hacer cualquier cosa"

Primitivas estables de ejecución: runtime, canvas, eventos, ciclo de vida, escenas, backend (interno). Las **escenas** pertenecen al core por ser infraestructura de ejecución, no modelo de juego. Invariante: **el core no conoce gameplay, entidades concretas, reglas de juegos ni tamaños de aplicación.**

### tge-extra (`libtge-extra.a`) — "puedo hacer cosas comunes rápidamente"

Módulos opcionales de mayor nivel, cada uno independiente y usando solo API pública del core. **Existentes (19):** `actor`, `animation`, `array`, `collision`, `direction`, `entity`, `fixedstep`, `game`, `grid`, `grid_view`, `input`, `input_buffer`, `playfield`, `sprite`, `tilemap`, `timer`, `ui`, `vec2i`, `view`. **Futuros (post-1.0):** `camera`, `fov`, `pathfinding`, `noise`, `ai`.

### Ejemplos — "así se usa para construir juegos"

`examples/games/*` consumen core + tge-extra. `examples/min/*` ejercitan primitivas puntuales.

### Evolución diferida: loop manual

`TGE_Run(app, init, update, draw, event)` es cómodo para la mayoría de juegos terminales. Si aparece un caso real que necesite control total (emuladores, multiplexers, herramientas editoriales), se diseña entonces una API de pump manual. No se inventa API sin uso. Identidad: **"Si quieres control, baja al core. Si quieres velocidad de desarrollo, usa extras."**

---

## 3. Estado actual — qué está validado

### 3.1 Núcleo y runtime

- **Pipeline Runtime → Engine → Canvas → Renderer → Backend** funcionando y estable.
- **Render diferencial**: ejercitado por varios juegos (Snake, Breakout, Pac-Man, Sokoban, Minesweeper).
- **Scene stack**: validado; el lifecycle se resolvió con `tge_scene_create()`/`tge_scene_destroy()` (Revisión 3) — una sola asignación + trampoline de destroy, sin fugas.
- **Game adapter** (`tge-extra/game`, Revisión 5, ADR-027): capa opcional sobre `TGE_Scene` que mata el cast `scene->userdata` y el global `g_app` dentro de los callbacks.

### 3.2 tge-extra y patrones emergidos

- **Grid / View / GridView**: validados y refinados en Revisiones 2 y 4 (espacio local del juego, `tge_view_translate`/`contains`, `tge_grid_view_*_local`).
- **Playfield**: el patrón View + GridView + Layout apareció en Snake → Breakout → Pac-Man, y se encapsuló (Revisión 6). Ya pasó la barrera de "abstracción especulativa".
- **TileMap**: consumidor fuerte y **mutable** — Sokoban empuja cajas cambiando el rol de la celda.
- **Actor**: consumidor distinto de Snake — los 4 fantasmas de Pac-Man lo usan.
- **InputBuffer**: validado donde la intención de dirección importa (Snake, Pac-Man).
- **Mouse**: Minesweeper demuestra que `TGE_EVENT_MOUSE*` no es una API teórica (mapeo mouse→grid, izquierdo=reveal / derecho=flag, con fallback de teclado).
- **Vec2i / Direction / Timer / FixedStep / Input**: validados por el refactor de juegos existentes.

### 3.3 Matriz de validación por juego

| Juego | Lo que valida |
|-------|---------------|
| Snake | loop, input, timer, Grid, View |
| Breakout | Playfield, movimiento, colisiones |
| Pong | `dt` / física continua |
| Tetris | grid + timers + estado |
| Invaders | múltiples actores + colisiones |
| Pac-Man | TileMap + Actor + Playfield + IA (chase/scatter por distancia²) |
| Sokoban | TileMap mutable + input event-driven + undo del juego |
| Minesweeper | mouse + TileMap + flood fill BFS + RNG determinista |

Conclusión: **TGE no está diseñado alrededor de un único juego**. Esa matriz es una demostración más convincente que "tener 15 juegos".

---

## 4. Metodología: de juego a abstracción

La regla de Fase 3b (abstracción solo si elimina un patrón repetido en ≥2 juegos) se confirma como la metodología de diseño del proyecto:

```
juego → patrón repetido → abstracción → segundo consumidor → validación
```

Ejemplos reales en el repositorio:

```
Snake
   └─ View, InputBuffer, Grid

Snake + Breakout
   └─ Playfield

Snake + Breakout + Pac-Man
   └─ Actor (+ Playfield consolidado)

Pac-Man
   └─ TileMap

Minesweeper
   └─ validación del mouse / event system
```

Esto es lo opuesto a "voy a necesitar A* algún día → implementemos A* ahora". Por eso se cerró Fase 3b en Minesweeper: los siguientes ejemplos (Roguelike, etc.) exigen módulos nuevos y deben pasar primero por una etapa de **diseño** dirigida por el problema general, no por el juego.

---

## 5. Modelo de fases (historial y estado)

| Fase | Alcance | Estado |
|------|---------|--------|
| F0 | Build system / infraestructura / CI | ✅ DONE |
| F1 | Runtime / Canvas / Renderer / Parser / Backend | ✅ DONE |
| F2 | Engine / Scenes / Math / Snake | ✅ DONE |
| F3 | Core gameplay validation (Pong, Tetris, Invaders) | ✅ DONE |
| F3b | tge-extra validation (Snake, Breakout, Pac-Man, Sokoban, Minesweeper) | ✅ DONE (cerrada en #10) |
| **F4** | **Consolidación / API freeze para Beta 1** | **✅ DONE** |
| **F5** | **Release Beta 1** | **← SIGUIENTE** |
| F6 | Feedback / estabilización (Beta 2) | futuro |
| F7 | Release Candidate | futuro |
| F8 | v1.0 final | futuro |
| Post-v1.0 | FOV, Pathfinding, Noise, AI, Camera, Palette/Theme, Python bindings | por diseñar |

La contradicción anterior ("Fase 4: tge-extra (módulos opcionales)" vs. checklist lleno de `[x]`) queda resuelta: esos módulos **ya están implementados**; Fase 4 es ahora consolidación y auditoría, no implementación.

---

## 6. Fase 4 — Consolidación / API audit (ACTUAL)

Cuatro objetivos. Los tres primeros ya tienen una auditoría read-only ejecutada (2026-08-13); el cuarto es una decisión de cierre.

### 6.1 API pública — **auditado (parcialmente)**

- **Headers autocontenidos — ✅ CUMPLE.** Los 9 headers de `include/tge/` y los 19 de `tge-extra/` compilan de forma aislada (`-fsyntax-only`, `-Iinclude -I.`), sin incluir nada privado. (Ya marcado en el checklist de pre-1.0.)
- **Restantes chequeos de Fase 4 (pendientes de una pasada final antes del freeze):** `const`-correctness, nombres consistentes, ownership/lifecycle documentado, valores de retorno (códigos de error vs `void`), documentación mínima en headers, y confirmar que los símbolos realmente públicos no dependen de `static` internos. Parcialmente cubierto hoy por `docs/API_STABILITY.md` y los comentarios en headers. Se listan en el checklist de pre-1.0 como pendientes.

### 6.2 Dependencias — **auditado ✅ CUMPLE**

- Ningún `tge-extra/*.c` incluye `src/` ni `tge_internal.h`; solo headers públicos (`tge/` o `tge-extra/`) y headers de sistema (`stdlib`, `math`, `stdbool`, …).
- Ningún `examples/` incluye `src/` ni headers internos; solo `tge/` y `tge-extra/`.
- La flecha **core → tge-extra → examples** es limpia y unidireccional.

### 6.3 Hot path sin `malloc` — **auditado ✅ CUMPLE (con salvedad evitable)**

La regla "sin `malloc` en render path" ya estaba verificada (strace/valgrind). Clasificación de los `malloc`/`calloc`/`realloc` en `tge-extra` (2026-08-13):

| Archivo | Conteo | Clasificación |
|---------|--------|---------------|
| `animation.c` | 2 | init (`tge_anim_create`) / teardown (`tge_anim_destroy`) |
| `array.c` | 1 | crecimiento on-demand en `push` (runtime **potencial**, amortizado; evitable pre-reservando) |
| `collision.c` | 16 | `world` create/destroy (init/teardown) + crecimiento en `add` (runtime **potencial**, amortizado) |
| `entity.c` | 20 | `pool` create/destroy (init/teardown) + `reserve`/grow (runtime **potencial**, amortizado) |

**Conclusión:** ninguna asignación ocurre en el query/consulta ni en `draw`/`update` por frame. El crecimiento on-demand de `array`/`collision-world`/`entity-pool` *puede* asignar en runtime, pero es amortizado y se evita si se pre-reserva la capacidad. Cumple la regla ADR-015; se documenta la salvedad para no fingir "cero allocs en任何 momento".

### 6.4 Freeze de la proyección C++ para Beta 1 — **hecho (Fase 4)**

Declarar "esta es la API de Beta 1" cambia la economía: a partir de ahí importa más qué **no** agregar. Las propuestas post-Beta-1 (`TGE_CreateConfig`, `TGE_SurfaceObserver`, `TGE_View` fuera del mundo, `RingBuffer`, `PointDeque`, `ColorTheme`) se reconsideran solo si cierran una inconsistencia real, no por adelantarse al uso.

**Superficie C++ 1.0 (Beta 1) — validada y congelada.** La `tge::` se considera validada por tres consumidores reales (Snake, Sokoban y Pac-Man), con **78 tests pasando** (24 + 27 + 27), y **durante esta beta no se agregarán wrappers adicionales sin un nuevo consumidor que justifique la necesidad**. Esta regla evita que `tge::` crezca por intuición: toda promoción es *consumer-driven*, no *coverage-driven*.

Superficie congelada:

```text
tge::App / Canvas
tge::Playfield
tge::Vec2i / Direction / Rect
tge::TileMap        (init/get/set/count/draw + width()/height() + load_ascii(lambda))
tge::TileSet
tge::Actor          (position/sprite/set_fg/set_bg/draw(Playfield&))
tge::FixedStep / InputBuffer / Event
tge::Sprite / Color / GridTheme
```

Escape hatch C deliberado (se mantiene crudo, no se envuelve):

```text
UTF-8 decode + raw grid drawing (MAZE_VISUAL box-art en Pac-Man)
```

Fuera de la superficie de Beta 1 por falta de evidencia de consumidor: `tge::Scene` / `tge::Game`
(los probes los evitan vía `TGE_Run` + callbacks; cualquier promoción requiere un consumidor
real, p.ej. un probe con apilado de escenas).

Criterio de cambios post-freeze: una vez declarada Beta 1, los cambios son decisión de 1.1 / Beta 2 y
solo si hay un problema real — no "porque se vea más C++". Mejoras puramente cosméticas en
los probes (p.ej. simplificar `world_resize`, eliminar estado redundante) no alteran el
contrato de Beta 1 y no son necesarias para cerrar. Minesweeper deja de ser "el siguiente juego a
hacer" y pasa a ser una **sonda opcional** para una futura extensión — particularmente para
*mouse*, superficie que aún no tiene consumidor C++. Ver §10 y §12.

### 6.5 Sonda `tge::` (C++ ergonomics probe) — **entregable (Fase 4)**

Como herramienta de diseño **antes** del freeze de Beta 1, se construye una proyección
C++ **mínima, real y deliberadamente incompleta** (`bindings/cpp/`) que consume la
API C estable y la envuelve en `tge::`. No es un binding completo. Cubre 7 piezas
centrales (`Vec2i`, `Direction`, `Input`→`Event`, `Actor`*, `TileMap`, `Playfield`,
`App`) + extensiones que el juego real necesitó (`Color`, `Sprite`, `Canvas`,
`GridTheme`, `FixedStep`, `InputBuffer`). Un ejemplo real — el **clone fiel de
`06_snake_grid.c`** (misma jugabilidad: cuerpo de 3, comida, crecimiento, `FixedStep`
0.10s, `InputBuffer` 4, muerte por muro/auto-colisión, pausa, score, overlay; lógica
en `examples/snake_game.hpp`) — tiene su batería de pruebas headless en
`bindings/cpp/tests/test_snake.cpp` (mock backend de `tests/mock_backend.h`, lee el
framebuffer vía `app->previous->cells`), para validar sin terminal.
Un segundo ejemplo — **clone fiel de `12_sokoban.c`** (misma jugabilidad: empujar
cajas a goals, undo, niveles, event-driven sin `FixedStep`/`InputBuffer`; lógica en
`examples/sokoban_game.hpp`) — en `bindings/cpp/tests/test_sokoban.cpp`. Es el
**primer consumidor real de `tge::TileMap`** (carga/consulta/mutación/dibujo de un
nivel mutable) y reusa `tge::Playfield`/`tge::GridTheme`/`tge::Sprite`/`tge::Event`,
confirmando que `Playfield` generaliza más allá de Snake.

Un tercer ejemplo — **clone fiel (traducción 1:1) de `11_pacman.c`** (misma
jugabilidad **y misma estructura**: maze `TGE_TileMap`, Pac-Man + 4 fantasmas como
`tge::Actor`, IA scatter/chase/frightened por distancia², tunnel wrap, power pellets,
vidas, pausa; lógica en `examples/pacman_game.hpp`) — en
`bindings/cpp/tests/test_pacman.cpp`. Es el **primer consumidor real de juego de
`tge::Actor`** (4 fantasmas + Pac-Man), que era la laguna abierta de la auditoría
Sokoban (§6.6): `Actor` estaba validado aislado pero sin consumidor de juego. La
traducción fiel confirma que `Actor` es la forma correcta de modelar a los actores,
pero **superficia 3 GAPs** del wrapper (ver abajo): `Actor` no expone `fg`/`bg`,
`Actor::draw` recibe `TGE_View`/`TGE_GridView` crudos, y `tge::TileMap` no expone
`width`/`height`. También re-ejercita `tge::TileMap` (mutable + markers de entidad) y
`tge::FixedStep`/`tge::InputBuffer` en un juego de referencia distinto a Snake.

*`Actor` estaba validado por la sonda independiente pero SIN consumidor de juego; el
clone de Pac-Man lo consume de verdad (4 fantasmas + Pac-Man vía `Actor::draw`), así
que la laguna de §6.6 queda cerrada en cuanto a *forma*: `tge::Actor` es la
proyección correcta. Pero la traducción fiel reveló que aún le faltan proyecciones
(`fg`/`bg`, firma de `draw`), ahora registradas como GAPs.

Regla de diseño de la sonda: **el C++ es una traducción fiel del C**, cambiando
sólo la sintaxis necesaria para consumir los wrappers `tge::` que ya existen y las
desviaciones de alcance explícitamente declaradas. Donde el wrapper **no** cubre la
API C, se conserva la llamada C cruda y se **registra como GAP** (no se reimplementa
la primitiva, no se minimiza el C a ciegas). La sonda cumple su objetivo sólo si revela
fricciones; por eso **no** se agregan wrappers `tge::` nuevos para "acomodar" el
ejemplo — las lagunas se registran como hallazgos para decidirlas tras el freeze de Beta 1.

Hallazgos registrados (clasificación C-vs-C++):
- **extern "C" en umbrella** (`include/tge/tge.h`): el umbrella no incluía el
  bloqueo `extern "C"`, por lo que los headers subyacentes necesitaban
  `extern "C"`). **Fix-C aplicado** (C) antes del freeze. ✅
- **Loop manual no es superficie pública de primera clase**: la sonda lo detectó
  al quedar la pantalla negra con `TGE_Step`+draw (el canvas se presenta dentro
  del frame). Corregido usando `TGE_Run` con callbacks. Registrado como
  **candidato de freeze sin fix automático** (ver §8): falta un segundo
  consumidor que justifique un loop manual + `TGE_IsRunning` en Beta 1.
- **Sin doble conversión de grilla**: el clone pasa el tamaño **físico** del canvas
  (`cv.width()/cv.height()`) a `playfield.sync`; `tge_playfield_sync`/`tge_grid_layout_sync`
  derivan la grilla internamente. Verificado por test: canvas 40×16 / 2×1 → grilla
  20×15 (el `set_origin(0,1)` reserva la fila HUD, fiel a `06`) → interior 18×13.
  Sugerencia de doc: aclarar en el header que `surface_w/h` es tamaño físico.
- **Lagunas C++ — lo que ESCAPA a C (lo único que un usuario C++ necesita tocar
  del tipo C para una operación normal del motor).** La pregunta no es "¿hay un tipo
  C en el código?", sino "¿un usuario C++ necesita conocer ese tipo C para realizar
  una operación normal?". Si la respuesta es no, está cubierto por un wrapper
  idiomático (boundary/bridge cuenta como C aceptable). Registrado, sin wrapper nuevo:
   - Superficie CUBIERTA por wrappers (no GAP): `tge::Sprite` (value type),
     `tge::Canvas`, `tge::GridTheme` (owning; ver ownership abajo), `tge::Playfield`
     (incluye `width()`/`height()`/`origin_*()` que esconden `TGE_View`/`TGE_Rect`),
     `tge::TileMap` (consumido por el clone de Sokoban: init/get/set/count/load_ascii/
     draw), `tge::Color`, `tge::FixedStep`, `tge::InputBuffer`, `tge::ViewUpdate`
     (enum espejo de `TGE_ViewUpdate`, devuelto por `Playfield::update_view`),
     `tge::GridScale` (enum espejo de `TGE_GridScale`, tomado por `Playfield::init`),
     `tge::TileSet` (wrapper *owning* sobre `TGE_TileSet`, análogo a `tge::GridTheme`;
     consumidor: Sokoban).
   - **`TileMap::load_ascii` (loader C) — resuelto como fachada idiomática C++ (revisado
      tras Pac-Man, ver §6.7):** el loader `tge_tilemap_load_ascii` exige C crudo:
      `TGE_TileLegend` (tabla glyph→rol, POD trivial) + un callback `TGE_TileMarkerFn`
      (raw function pointer). Sokoban lo ejercita vía `SOKO_LEGEND` + `&Game::level_marker`;
      Pac-Man repite el **mismo patrón** (`PACMAN_LEGEND` + `&Game::level_marker`, con
      markers `P/1-4/H/A-D` que reescriben roles). Dos consumidores reales confirmaron que
      era un punto de **diseño de API** (no solo "exponer un tipo C"): la fricción es el
      legend/marker callback, no la estructura del tilemap. Resuelto en la tercera pasada
      como **fachada de ergonomía** sobre el contrato C (overload C++ de `load_ascii` que
      toma `std::initializer_list<TileLegend>` + un callable lambda; el overload C original
      se mantiene para interop). La paleta (`TGE_TileSet`) ya fue promovida a `tge::TileSet`;
      `TGE_View`/`TGE_VIEW_*`/`TGE_GRID_SCALE_*` también fueron promovidos
      (`tge::Playfield` accessors / `tge::ViewUpdate` / `tge::GridScale`). Ver §6.7.
    - **GAPs surfaced por la traducción fiel de Pac-Man (el wrapper no cubría la
       API C → se usó C crudo y se registró; resueltos en la segunda pasada, §6.7):**
       * `tge::TileMap` no exponía `width()`/`height()` → **resuelto**: agregados
         `TileMap::width()`/`height()`. El clone ahora usa `map.width()`/`map.height()`.
       * `tge::Actor` no exponía `fg`/`bg` → **resuelto**: agregados `set_fg`/`set_bg`/
         `fg`/`bg`. El clone ahora usa `actor.set_fg(...)`/`set_bg(...)`.
       * `tge::Actor::draw` recibía `TGE_GridView*`/`const TGE_View*` crudos →
         **resuelto**: `Actor::draw(Playfield&)` consume el wrapper (acopla
         `actor.hpp`→`playfield.hpp`). El clone ahora usa `actor.draw(playfield)`.
       * `tge::Vec2i` no exponía `dist2` → **resuelto**: agregado `Vec2i::dist2`.
         El clone ahora usa `next.dist2(target)`.
       * No existía `tge::Rect` → **resuelto**: nuevo `tge::Rect` con `contains`.
         El clone ahora usa `GHOST_HOUSE.contains(x, y)`.
       * `tge_utf8_decode` + `tge_grid_put` (box-art `MAZE_VISUAL`) → **escape hatch
         C deliberado** (§6.7): se mantiene crudo en el boundary de render; no se envuelve.
       Criterio consumer-driven respetado: promovidos solo los que un programa C++
       normal necesitaría, con consumidor real. Ver §6.6 / §6.7.
- **Ownership de `GridTheme` (hallazgo de la sonda, corregido en C++):** `GridTheme`
  es *owning* (posee 4 `Sprite`s y construye un `TGE_GridTheme` que solo **toma
  prestados** punteros). Una copia/movimiento *member-wise* dejaría `raw.tiles[i].sprite`
  apuntando a los `sprites[]` del objeto origen (colgante tras destruirlo). Corregido
  en el wrapper con `bind()` que reenlaza los punteros al `sprites[]` propio tras
  copy/move. `Sprite` es value type (POD sobre literales estáticos), copia superficial
   segura. Test: `probe_gridtheme_sprite_ownership`.

### 6.6 Auditoría de wrappers/GAPs (Snake + Sokoban + Pac-Man) — promociones aplicadas

Tras los tres probes (24 + 27 + 27 tests en verde) se auditó cada GAP con el
criterio: *"¿un programa C++ normal debería tocar C para hacer esto?"*.

- **Promovidos (evidencia de dos consumidores):** `tge::ViewUpdate` (enum espejo de
  `TGE_ViewUpdate`), `tge::GridScale` (enum espejo de `TGE_GridScale`),
  `tge::Playfield::width()/height()/origin_*()` (eliminan `TGE_View`/`TGE_Rect` del
  gameplay), y `tge::TileSet` (wrapper *owning* sobre `TGE_TileSet`, análogo a
  `tge::GridTheme`; Sokoban es consumidor real). Snake y Sokoban fueron refactorizados
  para consumir estos wrappers; los 24 + 27 tests siguen en verde tras el refactor.
- **Validado en *forma* (Pac-Man):** `tge::Actor`. Estaba validado aislado pero sin
  consumidor de juego; el clone de Pac-Man lo consume de verdad (4 fantasmas + Pac-Man
  vía `Actor::draw`). La *forma* es correcta (el wrapper oculta `TGE_Actor` y mueve el
  `draw` a un método del objeto), y la traducción fiel superfició 3 GAPs que la
  validación aislada no había revelado. **Resueltos en la segunda pasada (§6.7):** `Actor`
  ahora expone `set_fg`/`set_bg`/`fg`/`bg`, y `Actor::draw(Playfield&)` consume el
  wrapper (no punteros C crudos); `Vec2i::dist2` también agregado.
  * `tge::Actor` no exponía `fg`/`bg` → ahora `set_fg`/`set_bg`/`fg`/`bg` (C++ 1.0).
  * `tge::Actor::draw(TGE_GridView*, const TGE_View*)` recibía tipos C crudos → ahora
    `draw(Playfield&)` (C++ 1.0, acopla `actor.hpp`→`playfield.hpp`).
  * Consecuencia: `tge::Vec2i` no exponía `dist2` → ahora `Vec2i::dist2` (C++ 1.0).
- **`TileMap::load_ascii` — resuelto como fachada idiomática C++ (§6.7):** no era una
  simple representación C faltante sino una decisión de diseño de API (Pac-Man repite el
  **mismo patrón** que Sokoban: legend + marker callback que reescribe roles). Resuelto
  con un overload de `tge::TileMap::load_ascii` que toma `std::initializer_list<TileLegend>`
  + un callable lambda, conservando el overload C original (`TGE_TileLegend` + `TGE_TileMarkerFn`
  + `void*`) para interoperabilidad. No cambia la semántica de `tge_tilemap_load_ascii`.
  Ver §6.7.
- **Sin evidencia suficiente aún:** `tge::Scene`, `tge::Game`. Los probes los evitan a
  propósito (disciplina gameplay-only): usan `TGE_Run` + callbacks y `app.quit()` en
  lugar de `TGE_PushScene`/`TGE_PopScene`/`tge_game_create`. No se promueven hasta
  tener un consumidor real que justifique la proyección del lifecycle de escenas/juego.


> **Orden de trabajo (comportamiento primero, wrappers después):** compilar →
> tilemap smoke → view sizing → wall death → growth → self collision → render →
> ownership, y **luego** documentar. Esto evita diseñar la API C++ para que el clone
> pase, en vez de dejar que el clone revele dónde la API C ya tiene buena semántica y
> dónde realmente falta una proyección. No se agregan wrappers `tge::` hasta que un
> consumidor real lo justifique (regla consumer-driven, no coverage-driven).

### 6.7 Decisión post-probes (asignación de GAPs a destino)

Esta sección es el puente entre la observación de GAPs (§6.5/§6.6) y el diseño
concreto de APIs (segunda pasada). Regla que rige toda la Fase 4: **promoción
consumer-driven, no por anticipación**. Un GAP pasa a `tge::` solo si un programa
C++ normal necesitaría tocar C para hacerlo, y la evidencia la aporta un consumidor
real de los probes — no cobertura especulativa.

Categorías congeladas (ninguna implica cambio de implementación en esta sección):

**C++ 1.0 — cerrar con wrapper (5 promociones):**
- `Vec2i::dist2` — evidencia: Pac-Man `ghost_choose_direction` (targeting por
  distancia²). Helper puro de vector 2D; sin valor de escape-hatch.
- `TileMap::width()`/`height()` — evidencia: Pac-Man `step_position`,
  `cell_in_ghost_house`, loops de `renderer_draw_walls` (4+ usos de `map.raw`).
  Dimensiones fundamentales de cualquier consumidor de tilemap; leer `.raw` es la
  fuga que se cierra.
- `Rect` (`TGE_Rect` + `tge_rect_contains`) — evidencia: Pac-Man `cell_in_ghost_house`.
  Primitiva geométrica núcleo; `GHOST_HOUSE` pasa a `Rect{10,12,8,3}`.
- `Actor` `fg`/`bg` (accessors) — evidencia: Pac-Man `pac.actor.raw.fg`,
  `g.actor.raw.fg`, ojos `copy.raw.fg`. El color es intrínseco al render del actor.
- `Actor::draw(Playfield&)` (firma rediseñada, **no** envoltura literal) — evidencia:
  Pac-Man `pac.actor.draw(&playfield.grid_view(), &playfield.view())`. Acopla al
  wrapper `Playfield` en vez de exponer punteros `TGE_GridView*`/`TGE_View*` crudos.

**Resuelto como fachada idiomática C++ (no como wrapper mecánico):**
- `TileMap::load_ascii` + `TGE_TileLegend` + `TGE_TileMarkerFn` — evidencia:
  Sokoban (`SOKO_LEGEND`+`sokoban_marker`) **y** Pac-Man (`PACMAN_LEGEND`+
  `level_marker`). Dos consumidores con semánticas de marker *distintas*
  (cajas/player vs spawns/targets) → se decidió la API con esa evidencia. La
  solución es una **fachada de ergonomía** sobre el contrato C existente
  (`tge_tilemap_load_ascii`), no un reemplazo: `tge::TileMap::load_ascii` gana un
  overload que toma `std::initializer_list<tge::TileLegend>` (la leyenda inline) y
  un callable `void(char glyph, int x, int y)` (lambda que captura el contexto del
  nivel y decide el rol final de la celda vía `map.set`). Elimina `LevelLoadCtx`,
  el método estático `level_marker` y el `void*` cast en ambos probes. El overload
  C original (leyenda C + `TGE_TileMarkerFn` + `void*`) se mantiene para
  interoperabilidad. No cambia la semántica de `tge_tilemap_load_ascii`.

**Escape hatch C deliberado (no envolver):**
- `tge_utf8_decode` + `tge_grid_put` — evidencia: Pac-Man `renderer_draw_walls`
  (box-art `MAZE_VISUAL`). Muy específico de render de paredes; baja reusabilidad.
  Se documenta como patrón raw en el boundary de render; reconsiderar solo si otro
  probe lo necesita.

**Fuera de decisión (sin evidencia de consumidor):**
- `Scene` / `Game` — evidencia negativa: Snake + Pac-Man evitan vía `app.quit()` /
  callbacks `TGE_Run` (sin `TGE_PushScene`/`tge_game_create`). Cero consumidor
  positivo; fuera de 1.0.

| GAP                                       | Destino                                | Consumidor que lo justifica     |
| ----------------------------------------- | -------------------------------------- | ------------------------------- |
| `Vec2i::dist2`                            | C++ 1.0                                | Pac-Man                         |
| `TileMap` width/height                    | C++ 1.0                                | Pac-Man                         |
| `Rect` / `rect_contains`                  | C++ 1.0                                | Pac-Man                         |
| `Actor` fg/bg                             | C++ 1.0                                | Pac-Man                         |
| `Actor::draw(Playfield&)`                 | C++ 1.0 (rediseñada)                   | Pac-Man                         |
| `load_ascii` (fachada C++ sobre C)        | Resuelto (fachada idiomática)          | Sokoban + Pac-Man               |
| `tge_utf8_decode` + `tge_grid_put`        | Escape hatch C deliberado               | Pac-Man                         |
| `Scene` / `Game`                          | Fuera de alcance                       | — (evitado en Snake/Pac-Man)    |

**Segunda pasada (hecha):** las 5 promociones 1.0 están implementadas y el probe de
Pac-Man reconectado a ellas; los 3 suites siguen en verde (Snake 24 / Sokoban 27 /
Pac-Man 27). Detalle:
- `Vec2i::dist2` → `bindings/cpp/include/tge/vec2i.hpp` (`int dist2(Vec2i) const`).
- `TileMap::width()`/`height()` → `bindings/cpp/include/tge/tilemap.hpp`.
- `tge::Rect` (nuevo) → `bindings/cpp/include/tge/rect.hpp` (`struct Rect` + `contains`).
- `Actor::set_fg`/`set_bg`/`fg`/`bg` → `bindings/cpp/include/tge/actor.hpp`.
- `Actor::draw(Playfield&)` → `bindings/cpp/include/tge/actor.hpp` (acopla
  `actor.hpp`→`playfield.hpp`; consume `pf.raw.grid_view`/`pf.raw.view`).

Empezó por `Rect`/`TileMap`/`Vec2i` (mecánicos) y terminó en `Actor::draw` (la única
que implica acoplamiento entre wrappers). Las superficies restantes (`load_ascii`
diferido; `tge_utf8_decode`+`tge_grid_put` escape hatch; `Scene`/`Game` fuera de
alcance) **no** se tocaron.

### 6.8 Auditoría ergonómica post-freeze

Revisión de **ergonomía**, no de arquitectura, hecha *después* de declarar el freeze
1.0 en §6.4. No busca nuevos wrappers: usa los tres probes (Snake 24 / Sokoban 27 /
Pac-Man 27 = 78 tests) como corpus y clasifica cada API de `tge::` que consumen. El
objetivo es comprobar que **no hay ninguna mala abstracción que obligue a reabrir el
freeze**, y dejar registrado el veredicto. Criterio por API: ¿expresa naturalmente la
intención? ¿el C++ resultante es claramente mejor que C? ¿responsabilidad obvia?
¿cambia semántica o solo ergonomía?

Escala:
- **A** — idiomática C++.
- **B** — aceptable, pero algo C-like.
- **C** — incómoda / filtración de implementación.
- **D** — directamente una mala abstracción.

Resultado (por superficie usada en los probes):

| Superficie | C equivalente | Clasif. | Nota |
| --- | --- | --- | --- |
| `Vec2i` / `Direction` (`+`, `to_vec`, `dist2`, `from_event`) | `tge_vec2i_add` / `tge_direction_vec` / `tge_vec2i_dist2` | **A** | claramente mejor que C |
| `TileMap` `get/set/count/init/draw` | `tge_tilemap_*` | **A** | — |
| `TileMap::load_ascii(rows,w,h,{...},lambda)` | `tge_tilemap_load_ascii` + `TGE_TileLegend` + `TGE_TileMarkerFn` + `void*` | **A/B** | enormemente mejor; `w/h` redundante con `rows` (ver B) |
| `Actor` `set/get position/sprite/fg/bg` + `draw(Playfield&)` | `tge_actor_*` | **A** | convención `set_x`/`x` uniforme |
| `Playfield` `sync/update_view/width/height/origin_*/attach/draw_border/contains/random_point/valid/put_local` | `tge_playfield_*` / `tge_view_*` | **A** | — |
| `Playfield::grid_view()` (devuelve `TGE_GridView&`) | — | **B/C** | véase C-1 |
| `Rect` `contains(x,y)` / `contains(pos)` | `tge_rect_contains` | **A** | mínimo, consistente con `Vec2i` |
| `Color` `yellow()/DEFAULT()/indexed()/blue()/...` | `tge_color_*` | **A/B** | modelo coherente; `Color::DEFAULT()` repetido 18× (ruido, no defecto) |
| `Event` `from_event` / `tge::Event(*ev)` | `TGE_Event*` | **A** | sin `e.raw` en la lógica de los probes |
| `FixedStep`/`InputBuffer`/`Sprite`/`TileSet`/`Canvas`/`GridTheme`/`App` (ctores valor + `push/pop/update/next/draw_modal`) | — | **A** | — |
| `playfield.init(...)` / `map.init(...)` | `tge_playfield_init` / `tge_tilemap_init` | **B** | justificado por re-init/ownership (dos fases) |

Conteo: **A** → mayoría abrumadora; **B** → `load_ascii` `w/h` redundante,
`Color::DEFAULT()` repetido, `init()` vs ctor (justificado), `grid_view()` expone tipo
C; **C** → únicamente `playfield.grid_view().grid` en la integración TileMap↔Playfield;
**D** → ninguno.

**C-1 (`playfield.grid_view().grid`) — única filtración real, candidato 1.1 / Beta 2 (NO
implementado ahora).** Aparece 4×: 2 en `map.draw(&playfield.grid_view().grid, ox, oy,
...)` y 2 en el escape hatch `tge_grid_put(&playfield.grid_view().grid, ...)`. La causa
 no es `grid_view()` en sí, sino que **no existe una forma de "dibujar este tilemap en
 este playfield"** sin bajar al `TGE_Grid` crudo: `TileMap::draw` toma `TGE_Grid*`, así
 que el probe debe hacer `&playfield.grid_view().grid`. Mejora 1.1 (sin romper el
 contrato): `Playfield::draw_tilemap(const TileMap&, const TileSet&)` que internamente
 llame `tge_tilemap_draw(&raw.grid_view.grid, ...)`, acoplando `Playfield`→`TileMap`
 (misma dirección que `Actor::draw(Playfield&)`). El escape hatch seguiría necesitando
 `grid_view().grid`, así que la filtración no se elimina del todo, solo se reduce al
 hatch. **Reformulación (ver §6.9, Minesweeper):** el gap de fondo es más general que
 "dibujar un TileMap" — es *mapeo de coordenadas del Playfield* (Canvas ↔ celda). La
 API 1.1 debe diseñar esa mini-API de transformación primero y hacer que `draw_tilemap`
 sea un consumidor de ella, no solo un parche de render.

**Nota 1.1 (no defecto 1.0):** `load_ascii(rows, w, h, ...)` podría derivar `w/h` de
`rows` (`const char* const*`) en una futura revisión; hoy es explícito y aceptable.

Veredicto: el proceso *consumer-driven* ya eliminó las malas abstracciones de raíz. La
C++ 1.0 **sigue congelada como contrato de Beta 1**; C-1 es candidato a 1.1 / Beta 2, no a un cambio
inmediato. No se tocó más el binding C++ tras esta auditoría.

### 6.9 Validación post-freeze: Minesweeper (13_minesweeper.c)

Cuarto juego sobre la superficie 1.0, pero **post-freeze**: no diseña wrappers, los
*consume* para confirmar que la API congelada resiste una superficie que los tres
probes anteriores no ejercitaban — **mouse** — sin necesidad de abrirla. Regla
estricta durante el probe: **no se modifica la 1.0 para acomodar hallazgos**; cualquier
necesidad genuina se registra como candidato 1.1, igual que `Playfield::draw_tilemap`.

Implementación: `bindings/cpp/examples/minesweeper_game.hpp` (clon 1:1 de
`13_minesweeper.c` sobre `tge::`), `examples/minesweeper.cpp`, y
`tests/test_minesweeper.cpp` (**47 tests, 0 failures**, headless vía mock backend).

Superficie nueva ejercitada (y veredicto sobre la Beta 1):
- `TGE_EVENT_MOUSEDOWN` + `e.raw.data.mouse.{x,y,button}` — **M-1 (gap 1.1):** `tge::Event`
  expone `EventType::MouseDown/Up/Move` pero **no** accesores de mouse. La única forma de
  leer cursor/botón hoy es `e.raw.data.mouse.*`. La traducción 1:1 funciona, pero es ruido
  C en código de juego. Candidato 1.1: `Event::mouse_x()/mouse_y()/mouse_button()` (o un
  `MouseData`).
- `TileMap` + `TileSet` como capa de render puro (el estado lógico vive en arreglos del
  mundo, como en el C) — otra consumidora real de `TileMap`; no reveló gaps.
- `Playfield::grid_view().grid.ox/oy` en `mouse_to_cell` — **re-confirma C-1**, pero
  Minesweeper lo **reformula**: ya no es solo "dibujar un TileMap", sino un gap de
  *mapeo de coordenadas del Playfield* (Canvas ↔ celda, y por extensión dibujar un
  TileMap). `mouse_to_cell` es una operación genuina de interacción, no box-art
  exótico como en Pac-Man, así que la evidencia es más fuerte. El probe la absorbe vía
  el escape hatch ya documentado. `Playfield::view()`/`contains`/`width()`/`height()`/
  `origin_*()` cubren el resto de la conversión sin tocar crudo, pero la capa de
  transformación de coordenadas sigue filtrando `grid_view().grid`.
- RNG: el C usa su propio xorshift32 determinista (no la RNG de TGE), así que no hay gap de
  RNG que revelar; queda como decisión de juego, no de API.

Resultado de la validación: la 1.0 **sí** da abasto con mouse + TileMap + interacción
directa. El único defecto de ergonomía es M-1 (falta de accesores de mouse en `Event`),
que **no** obliga a reabrir el freeze — es un candidato 1.1 limpio. No se tocó el binding.

Candidatos 1.1 confirmados tras la validación (no implementados; la C++ 1.0 queda cerrada dentro de Beta 1):
- **`Event::mouse_x() / mouse_y() / mouse_button()`** (M-1). La API de Beta 1 distingue el tipo
  de evento mouse pero no expone cursor/botón; hoy solo vía `e.raw.data.mouse.*`.
- **API de mapeo de coordenadas de `Playfield`** (C-1, reformulado). Subsume el candidato
  `Playfield::draw_tilemap(...)` de §6.8: el problema de fondo es que varias operaciones
  normales de interacción terminan conociendo detalles internos de `GridView`/`Grid`
  (`view().area.*` y `grid_view().grid.ox/oy`). Para 1.1 conviene diseñar primero una
  pequeña API de transformación y que `draw_tilemap` sea un consumidor de ella, p.ej.
  conceptualmente `Vec2i Playfield::cell_at(int canvas_x, int canvas_y) const;` y
  `bool Playfield::contains_canvas_point(int x, int y) const;` (las firmas finales se
  deciden al implementar 1.1, no ahora). La rareza de mezclar `area.*` y `grid.ox/oy` en
  una sola función es justamente lo que vuelve frágil el código ante cambios de layout y
  refuerza este candidato.

El probe **no se modifica**: sus 47 tests verdes documentan exactamente el estado actual de
la API (incluido el escape hatch), que es el valor que queríamos preservar.

---

## 7. Historial de construcción (resumen)

Registro condensado de cómo se llegó al estado actual. El detalle de especificación ya implementado no se re-produce.

- **F0 — Build/infra:** Makefile, CI, `docs/API_STABILITY.md`, test runner. ✅
- **F1 — Runtime base:** UTF-8 + `wcwidth` (1.1); parser two-stage bytes→eventos con fuzz (1.2); backend ANSI con estado interno raw mode/colores/cursor/input (1.3); scheduler con prioridad (1.4); `TGE_Runtime` sin dependencia de Canvas (1.5); Canvas opaco + primitivas de dibujo + Renderer stateless row-oriented + draw helpers + clipping implícito + benchmarks (1.6). Decisiones: render diferencial, sin `malloc` en render path (ADR-015). ✅
- **F2 — Engine:** Math/Rect, Scene stack con ops diferidas, `TGE_App` con game loop (orden ADR-025), Snake jugable. ✅
- **F3 — Core gameplay:** Pong (dt/física continua), Tetris (rotación/grid/gravedad), Space Invaders (múltiples entidades/colisiones). ✅
- **F3b — tge-extra:** módulos testeados (entity/animation/collision; vec2i/direction/timer/fixedstep/input; view/input_buffer/grid_view; grid; tilemap; actor; playfield; game). Revisiones 2–6 migraron los juegos existentes a la API de espacio local, `tge_printf`, `tge_scene_create/destroy`, `tge_grid_view_*_local`, adapter de juego (ADR-027). Cierre en #10 Minesweeper (mouse + flood fill + RNG). ✅

---

## 8. Checklist de pre-1.0

Estado granular (conservado del histórico; los ítems de construcción ya están ✅):

- [x] Fase 0: Makefile, CI, API_STABILITY.md, test runner.
- [x] Fase 1: UTF-8, parser, backend ANSI, scheduler, runtime, canvas, renderer (primitivas + helpers + clipping + benchmarks).
- [x] Fase 1.6: sin `malloc` en render path (verificado strace/valgrind; ver auditoría 4.3).
- [x] Fase 2: Math/Rect, Scene stack, `TGE_App` + Snake.
- [x] Fase 3: Pong, Tetris, Invaders.
- [x] Fase 3b: `tilemap`, `actor`, `playfield`, `vec2i_dist2`, `11_pacman`, `12_sokoban`, `13_minesweeper` (todos con tests).
- [x] Revisiones 2–6: API de espacio local, `tge_printf`, `tge_scene_create/destroy`, adapter `tge-extra/game`, migración de 01/05/06.
- [x] Todos los headers públicos autocontenidos (ver auditoría 4.1).
- [x] `docs/API_STABILITY.md` publicado.
- [x] **Migración a Game adapter (Revisión 5 / F4):** `07_breakout` ya usaba `tge_game_create` (Revisión 5); `03_tetris` migrado en F4 (ahora `TetrisGame` embebe `TGE_GameContext` en offset 0, callbacks vía `tge_game_instance`, `title_event` usa `tge_game_create`). `tests/test_tetris.c` añadido como fixture de regresión headless (crea la escena vía adapter y verifica que una flecha izquierda mueve la pieza). Todos los `examples/games/*` compilan; 34 suites de tests, 0 fallos.
- [x] **Fase 4 (consolidación / API freeze para Beta 1):** superficie C++ validada por tres consumidores reales (Snake/Sokoban/Pac-Man, 78 tests); declarado freeze de la proyección C++ para Beta 1 en §6.4 (regla consumer-driven, sin wrappers sin consumidor durante la beta). Ver §6.4 / §6.5 / §6.6 / §6.7.
- [x] **Validación post-freeze (Minesweeper, 13):** cuarto consumidor sobre la Beta 1 congelada; confirma que mouse + TileMap + interacción directa dan abasto sin abrir la API. Hallazgo M-1 (`Event` sin accesores de mouse) registrado como candidato 1.1; no se modificó la C++ 1.0. 47 tests (total 125/0). Ver §6.9.
- [ ] **Loop manual (decidido diferir — fuera de Beta 1):** ¿`TGE_Step` + `TGE_IsRunning` como API de primera clase (setter de draw/update/event callback con `TGE_Step` presentando el canvas del llamador), o basta `TGE_Run` como único loop público? La sonda lo halló al usar `TGE_Step`+draw (pantalla negra) y se corrigió con `TGE_Run`. **Decisión F4:** se difiere — solo hubo un consumidor (la sonda) y la regla es *consumer-driven*, no *coverage-driven*; `TGE_Run` queda como único loop público de Beta 1. Se reabre solo si aparece un segundo consumidor real. Ver §6.5 / PLAN_ENTRENAMIENTO.md (hallazgos de la sonda).
- [ ] **F5 — Release Beta 1 (siguiente):** pasada final sobre `API_STABILITY.md`, `README`, `IMPLEMENTATION_PLAN.md`, `ADR.md`, `Makefile`/CI; correr todos los tests + compilar todos los ejemplos; etiquetar la versión (Beta 1). `bindings/cpp` queda como laboratorio de ergonomía congelado en C++ 1.0 (4 probes / 125 tests); los candidatos 1.1 (`Event` mouse accessors, Playfield coordinate mapping) no entran en Beta 1 (quedan para Beta 2 / RC).
- [ ] **F6 — Feedback / estabilización (Beta 2):** uso real (otros lenguajes/proyectos consumiendo la API C + proyección C++), issues de consumidores, posibles extensiones 1.1 (`Event` mouse accessors, Playfield coordinate mapping, `draw_tilemap`, loop manual, `TGE_CreateConfig`, `TGE_SurfaceObserver`) según evidencia real. No se promete compatibilidad final todavía.
- [ ] **F7 — Release Candidate:** congelación definitiva de compatibilidad (ABI/API) tras estabilizar las extensiones de Beta 2.
- [ ] **F8 — v1.0 final:** release con contrato de compatibilidad estable.

---

## 9. Lo que NO está en el plan / post-1.0

- **Bindings / proyecciones (Python, Rust, Zig)**: post-1.0; FFI directo sobre la API C (no vía C++). Ver §12. C++ ya tiene una sonda de ergonomía en Fase 4 (§6.5).
- **Camera** (`TGE_Camera`): post-1.0, tras TileMap/Playfield (ya existentes).
- **FOV / Pathfinding / Noise / AI** (`tge-extra/`): post-1.0, por diseño primero (ver §10). No se implementan hasta que un juego los pida.
- **Sprite + Tilemap como built-ins**: ya en `tge-extra` (`sprite.h`, `tilemap.h`). ✅ No pendiente.
- **Win32 backend**: stub hasta que alguien lo implemente. Fase 1 solo ANSI.
- **True color (24-bit)**: post-1.0 sin romper API (`TGE_Color` se queda como enum; se agrega `TGE_ColorRGB`).

---

## 10. Roadmap post-1.0

Módulos y juegos claramente post-1.0, gated por diseño-first (no por Roguelike):

- **Módulos algorítmicos** (cada uno se define por el problema general que resuelve y la API que merece entrar en `tge-extra`): `fov` (shadowcasting), `pathfinding` (A* sobre tilemap), `noise` (Perlin/Simplex), `ai` (minimax, para Conecta 4). Roguelike es el integrador final, no el driver.
- **Juegos #11–#18 del roadmap** (Roguelike, Bomberman, Tower Defense, Conecta 4, 2048, Snake 2P, Sudoku, Laberintos): diferidos; cada uno dispara diseño de módulo antes de implementarse.
- **API complementaria post-1.0 (solo-si-cierran-inconsistencia):** `TGE_CreateConfig` (estilo SDL3), `TGE_SurfaceObserver` (patrón resize ya repetido en snakes), `TGE_View` fuera del mundo (el mundo no debería saber el tamaño de la terminal), `RingBuffer` genérico (base de DirQueue/PointDeque), `TGE_ColorTheme`/`TGE_Palette` (roles semánticos de color, análogo a `TGE_GridTheme`). Ver §12.
- **Bindings (proyecciones de lenguaje):** Python/Rust/Zig sobre C directo (FFI); C++ como laboratorio de ergonomía (sonda `tge::` ya iniciada en Fase 4, §6.5). Ver §12.

---

## 11. Directorio final (actualizado)

Arquitectura real del repositorio:

```
include/tge/       → API pública estable del core (9 headers: tge.h, tge_app,
                     tge_canvas, tge_events, tge_math, tge_runtime, tge_scene,
                     tge_unicode, tge_utf8)
tge-extra/         → API pública opcional (19 módulos, ver §2)
src/               → implementación privada del core (app, canvas, renderer,
                     runtime, scene, scheduler, parser, backend_ansi,
                     unicode, utf8, math) + tge_internal.h
examples/          → consumidores (games/, min/)
bindings/cpp/       → sonda `tge::` (C++), proyección de ergonomía sobre la API C
                     estable (no binding completo; ver §6.5 / §12)
tests/             → validación (test_unit, test_*, test_pacman, test_sokoban,
                     test_minesweeper, …)
```

Regla de capas: `src/` es privado y no debe ser incluido por `tge-extra/` ni `examples/` (verificado en auditoría 4.2).

---

## 12. Arquitectura de lenguajes / proyecciones de lenguaje

TGE se expone a otros lenguajes como **un contrato ABI en C** (`libtge.a` +
`libtge-extra.a`), no como una capa C++. La política de estabilidad de ese
contrato vive en `docs/API_STABILITY.md`.

```text
                         TGE C
                    ┌──────┴──────┐
                    │             │
                 libtge      libtge-extra
                    │             │
        ┌───────────┼─────────────┼───────────┐
        ▼           ▼             ▼           ▼
      tge::       Python        Rust        Zig
       C++          FFI           FFI         FFI
    ergonomía    semántica C  semántica C  semántica C
```

- **C = contrato.** Define la semántica y la ABI. Es la única fuente de verdad
  que enlazan los demás lenguajes.
- **C++ = laboratorio de ergonomía.** `tge::` es la *primera* proyección
  idiomática y, por tanto, la primera prueba de diseño de la API C. Puede
  influir en las APIs de alto nivel, pero **no es dependencia de nadie**: ningún
  otro binding pasa por C++.
- **Python / Rust / Zig = proyecciones de semántica C.** Beben de la API C
  directamente (ctypes/cffi/PyCapsule/extensión en Python; FFI en Rust/Zig).
  Adoptan el mismo modelo conceptual (`Vec2i`, `Direction`, `TileMap`, `Actor`,
  `App`) pero en sintaxis idiomática propia; no copian mecánicamente los nombres
  de C.

Regla de triaje para cualquier fricción descubierta por una proyección:

> Si afecta la semántica o el contrato de TGE → corregir en C.
> Si es cómo expresar esa semántica idiomáticamente → resolverlo en la proyección.

Por eso conviene hacer la sonda `tge::` *antes* del freeze de Beta 1 (§6.4/§6.5): una
firma C incómoda aún se puede corregir; tras Beta 1 ya sería parte del contrato ABI.

---

## 13. Pendiente / Diferido (decisión del usuario, pendiente de retomar)

El usuario pidió **anotar** esto antes de pasar a algo más urgente. No está
bloqueado, solo diferido.

> **Fase 3b cerrada en #10 (Sokoban + Minesweeper) — hecho (decisión del
> usuario).** La segunda tanda de juegos validó las primitivas existentes de
> `tge-extra` (`TileMap`, `Playfield`, `Actor`, mouse, event-driven); se cierra
> en #10. Los ítems #11–#18 del roadmap (Roguelike, Bomberman, Tower Defense,
> Conecta 4, 2048, Snake 2P, Sudoku, Laberintos) se **difieren**: cada uno
> exige diseñar nuevos módulos (`tge-extra/fov`, `tge-extra/pathfinding`,
> `tge-extra/noise`, `tge-extra/ai`) que hoy no existen. Pasan a una **etapa de
> diseño de módulos algorítmicos** donde cada módulo se define por el problema
> general que resuelve y por la API que merece entrar en `tge-extra`, no porque
> Roguelike lo necesite. Se mantiene la regla de Fase 3b — *el módulo se
> implementa cuando un juego lo pide, no antes* — así que no se escribe código
> de esos módulos hasta que un ejemplo concreto lo demande. Ver también
> `PLAN_ENTRENAMIENTO.md` (Estado de Fase 3b y nota de Fase 4).

### 1. Lifecycle de escenas en el core — **hecho (revisión 3)**

Se resolvió con `tge_scene_create()`/`tge_scene_destroy()` en
`include/tge/tge_scene.h` y `src/scene.c` (diagnóstico original abajo). Un
`tge_scene_create()` asigna el struct `TGE_Scene` + el bloque de `userdata`
en **una sola** llamada a `calloc`, cablea los callbacks, marca la escena
opaca y setea `scene->destroy` a un trampoline interno que llama al
`destroy` del usuario (que solo libera recursos profundos, p.ej. `body`) y
luego libera el bloque completo. Así `TGE_PopScene`/`TGE_ReplaceScene` (que
llaman a `destroy`) ya liberan todo, y `main()` termina con
`tge_scene_destroy()` (que además es seguro con `destroy == NULL`).

Migrados los tres juegos que usaban el patrón heap (01_snake, 05_swarm,
06_snake_grid); 02/03/04 usan escenas `static` + callback `init` y no se
tocan (su `destroy == NULL` y `tge_scene_destroy` los libera bien).

Diagnóstico original: `TGE_PopScene`/`TGE_ReplaceScene` llaman `destroy()`
pero **nunca liberan el struct `TGE_Scene`**; `TGE_Run` tampoco limpia la
pila al salir. Escenas `static` (02/03/04) no se tocan (destroy=NULL).

### 2. tge-extra Batch 3 (módulos de patrones, no utilidades sueltas)

Prioridad del usuario:

1. RingBuffer genérico (`TGE_RingBuffer`, capacidad fija, sin malloc).
2. Grid (`contains`/`inside`/`center`/`area`/`step`/`random_cell`) — el
   GridWalker va dentro de Grid (`tge_grid_step`).
3. Random helpers (`tge_rand_seed`, `tge_rand_int(min,max)`).
   **Parcial:** `tge_rect_random_point` (random point dentro de un rect,
   basado en `rand()`) ya existe y lo usan 01/05/06; el seed/RNG propio queda
   para cuando se necesite determinismo real.
4. InputQueue de direcciones (DirQueue) — push/pop con semántica de input
   buffering. **Hecho (parcial):** realizado como `input_buffer.h`
   (`TGE_InputBuffer`, FIFO de capacidad fija, drop-new) y consumido por
   `01_snake`/`06_snake_grid`. Si se implementa el RingBuffer genérico,
   TGE_InputBuffer puede pasar a construirse sobre él.
5. PointArray/Deque (`push_front`/`pop_back`/`get`/`len`/`contains`) para el
   body de Snake → `snake_step()` mucho más corto.
6. Refactor de `01_snake` con todo esto y de `05_swarm` con el nuevo
   lifecycle de escenas. **Hecho (01_snake):** `01_snake` usa ya TGE_View +
   TGE_InputBuffer; `05_swarm` queda pendiente del lifecycle de escenas.

**Nombre a resolver:** el módulo de grilla lógica *matemática* del punto 2
estaba pensado como `grid.h`, pero ese nombre ya lo ocupa el `TGE_Grid` de
renderizado (aspecto de celda, Fase 4). El módulo matemático se renombrará
(p.ej. `board.h`) cuando se implemente.

Decisión abierta: **dependencias entre módulos** — construir DirQueue y
PointDeque sobre el RingBuffer genérico (relajar la regla "sin dependencias"
a "deps unidireccionales solo hacia ring.h") vs. cada uno autocontenido.

### 3. tge-extra como biblioteca de patrones

Dirección general acordada: dejar de ser colección de utilidades y pasar a
ser una biblioteca de patrones comunes de juegos (input buffering, grillas,
colas, etc.), siempre bajo el criterio "¿hace el código del juego
significativamente más simple sin volverlo más complejo?".

### 4. `TGE_CreateConfig` (API complementaria, estilo SDL3, evolución futura)

Principio: **las APIs simples no desaparecen cuando aparece una API avanzada.**
Un usuario que quiere lo básico no debería tener que escribir un struct:

```c
TGE_Create(80, 24, "Snake");   // caso básico: sigue siendo lo normal
```

`TGE_Config` + `TGE_DEFAULT_CONFIG` se agregan como API **complementaria**
(tipo SDL_image/ttf), para opciones avanzadas que no caben en el shortcut:

```c
TGE_Config cfg = TGE_DEFAULT_CONFIG;
cfg.title     = "Snake";
cfg.width     = 80;   // fallback si el backend no puede consultar el tamaño real
cfg.height    = 24;
cfg.resizable = true; // etc.

TGE_App *app = TGE_CreateConfig(&cfg);
```

Candidatos futuros: `resizable`, `vsync`, `fps limit`, `backend hints`,
`cursor visibility`. Ventajas: parámetros con nombre, el orden deja de
importar, agregar campos no rompe ABI, campos omitidos toman valores por
defecto. Mientras no existan esas opciones, `TGE_Create(w, h, title)` es
suficiente y la semántica (width/height = fallback, tamaño real si se puede
consultar) queda documentada en `include/tge/tge_app.h`.

### 5. Regla de estilo de juegos (anotada, no implementada ahora)

El refactor de `06_snake_grid` deja el split world/renderer **dentro de un
solo archivo**. La regla de estilo futura (cuando un juego crezca lo
suficiente) es separarlo en directorio propio:

```
examples/games/snake/
  world.c        — SnakeWorld puro (lógica, sin dibujo)
  world.h
  renderer.c     — SnakeRenderer (solo dibuja el world)
  renderer.h
  main.c         — TGE_App/Scene wiring (pega world + renderer)
```

No se hace ahora: los ejemplos de un solo archivo son más legibles y 06 ya
muestra el patrón completo con secciones comentadas.

### 6. TGE_ColorTheme / TGE_Palette (anotado, no implementado)

Observación del usuario para la lista mental (post-1.0): los temas de color
por rol semántico —`TGE_ColorTheme`/`TGE_Palette`, análogos a
`TGE_GridTheme`/`TGE_GridTile`— son el paso natural tras el tema visual de
grid. La paleta se resolvería igual que los tiles: el juego dice el rol, el
tema dice los colores. No se implementa ahora: los juegos actuales pasan
colores explícitos por argumento y eso es suficiente.

### 7. `tge_printf` (core, hecho)

El patrón `char buf[N]; snprintf(...); tge_draw_text(...)` aparecía en todos
los juegos, así que se agregó `tge_printf()` al core (`tge_canvas.h`): formato
printf en buffer fijo de pila (sin malloc, seguro para el render path) +
`tge_draw_text`. Se usa en los HUDs de 01/02/03/04/05/06. Quedan con
`snprintf` solo los textos que necesitan medir su ancho antes de dibujar
(WAVE centrada, puntaje derecho de Pong).

### 8. Observador de resize (anotado, no implementado)

Patrón que ya aparece en los snakes y se repetirá:

```c
if (g->world.gw != gw || g->world.gh != gh)
    world_layout(&g->world, gw, gh);
```

Candidato futuro (cuando exista un segundo o tercer juego con el patrón):
`TGE_SurfaceObserver`/`TGE_ResizeWatcher`/`TGE_ViewObserver` que encapsule
"detectar cambio de tamaño y avisar una sola vez". No se implementa todavía:
se necesita más evidencia de uso.

### 9. `TGE_View` fuera del mundo (anotado, decisión futura)

La única crítica de arquitectura real: hoy `TGE_View` vive dentro de
`SnakeWorld`, y el mundo no debería saber del tamaño de la terminal. La forma
ideal (cuando el patrón se repita en un segundo o tercer juego):

```
SnakeGame
    SnakeWorld
    SnakeRenderer
    TGE_View
```

```c
world_step(world, view.area);
world_reset(world, view.area);
renderer_draw(renderer, world, view);
```

El mundo recibe el rect cuando lo necesita pero no lo almacena, y así el mismo
`SnakeWorld` correría en un renderer SDL, una ventana fija, una consola o una
simulación sin pantalla. No se cambia ahora (ya está probado); el switch de
`TGE_ViewUpdate` en `world_layout` ya deja cada decisión localizada, lo que
hará el traslado mecánico.

### 10. Macro/función de callbacks de escena (anotado, no implementado)

Los callbacks se siguen cableando a mano:

```c
scene->update = game_update;
scene->draw = game_draw;
scene->event = game_event;
scene->destroy = game_destroy;
```

Candidato futuro: algo tipo `TGE_GAME_SCENE(SnakeGame, update, draw, event,
destroy)` o un constructor de un solo paso. `tge_scene_create` ya absorbe el
`calloc`/wiring; queda decidir si hace falta un nivel más (macro con los
nombres de las funciones). No se implementa: los callbacks por nombre siguen
siendo legibles y el usuario prefiere esperar a que el patrón crezca.

### 11. `TGE_Actor` — **implementado (Revisión 6, Pac-Man)**

El sketch de abajo se mantuvo tal cual salvo el `TGE_Sprite *` no-const:

```c
typedef struct {
    TGE_Vec2i position;
    const TGE_Sprite *sprite;
    TGE_Color fg, bg;
} TGE_Actor;
```

Con `tge_actor_draw(&grid_view, &view, &actor)` (= `tge_grid_view_put_local`
del actor). No es ECS ni específico de Snake: es data plana (posición +
representación) y un draw helper; el juego embebe el `TGE_Actor` en sus
structs y le añade encima sus campos (dirección, modo, targets), animando
swapeando `sprite`. La regla "2-3 juegos demuestran el diseño" se cumplió con
Snake → Breakout → **Pac-Man** (sus 4 fantasmas son `PacmanGhost` con un
`TGE_Actor` embebido).

### 12. `TGE_Playfield` — **implementado (Revisión 6, Pac-Man)**

El patrón emergente del Snake (View + GridView + Layout) apareció igual en
Snake, Breakout y Pac-Man, así que se encapsuló (la regla de los 3 juegos
cumplida):

```c
typedef struct {
    TGE_View view;          /* playfield lógico que se adapta a la terminal */
    TGE_GridView grid_view; /* la superficie de dibujo */
    TGE_GridLayout layout;  /* tamaño lógico ya aplicado al view */
} TGE_Playfield;

tge_playfield_init(&pf, &theme, TGE_GRID_SCALE_2X1, min_w, min_h);
tge_playfield_attach(&pf, canvas);               // por frame
tge_playfield_sync(&pf, w, h, resize_cb, userdata); // cambia de tamaño lógico
tge_playfield_draw_border(&pf, fg, bg);
```

Es solo infraestructura: no cámara, no entidades, no reglas. El callback de
resize (que es del juego) sigue decidiendo cómo reaccionar a un
`TGE_ViewUpdate`. Pac-Man lo consume como `TGE_Playfield pf` embebido en
`PacmanGame`, con su world (TileMap + actores) escribiendo en `pf.view` /
`pf.grid_view`.
