# Implementation Plan — Cierre TGE v1.0 + roadmap post-1.0

**Estado:** En consolidación (Fase 4 — auditoría/cierre de API). Fase 3b cerrada en #10 (Minesweeper). Fase 5 = release v1.0 (siguiente).
**Base:** `ADR.md` (26 decisiones arquitectónicas), `PLAN_ENTRENAMIENTO.md` (roadmap por juegos).
**Objetivo:** Este documento **ya no es un plan de construcción desde cero**. TGE tiene hoy un núcleo validado, varios juegos consumidores y un `tge-extra` bastante desarrollado. El objetivo ahora es (a) documentar qué está validado, (b) cerrar una **API 1.0 deliberadamente congelada y documentada**, y (c) señalar el roadmap post-1.0. La regla rectora sigue siendo la de Fase 3b: *el módulo se implementa cuando un juego lo pide, no antes*.

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
| **F4** | **Consolidación / API audit** | **← ACTUAL** |
| **F5** | **Release v1.0** | **← SIGUIENTE** |
| Post-1.0 | FOV, Pathfinding, Noise, AI, Camera, Palette/Theme, Python bindings | por diseñar |

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

### 6.4 Freeze de API 1.0 — **decisión / acción de cierre (no hecho)**

Declarar "esta es la API 1.0" cambia la economía: a partir de ahí importa más qué **no** agregar. Las propuestas post-1.0 (`TGE_CreateConfig`, `TGE_SurfaceObserver`, `TGE_View` fuera del mundo, `RingBuffer`, `PointDeque`, `ColorTheme`) se reconsideran solo si cierran una inconsistencia real, no por adelantarse al uso. Ver §10 y §12.

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
- [ ] **Abierto:** migrar `07_breakout` y `03_tetris` al adapter de juego (Revisión 5, Fase 2 de validación) — si obliga a tocar la API, ajustar antes.
- [ ] **Fase 4 (cierre 1.0):** pasada final de `const`/nombres/ownership/lifecycle/retornos/docs en headers públicos; confirmar símbolos realmente públicos; declarar freeze 1.0 (§6.4).

---

## 9. Lo que NO está en el plan / post-1.0

- **Bindings a Python**: post-1.0.
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
tests/             → validación (test_unit, test_*, test_pacman, test_sokoban,
                     test_minesweeper, …)
```

Regla de capas: `src/` es privado y no debe ser incluido por `tge-extra/` ni `examples/` (verificado en auditoría 4.2).

---

## 12. Pendiente / Diferido (decisión del usuario, pendiente de retomar)

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
