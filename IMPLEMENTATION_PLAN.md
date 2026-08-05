# Implementation Plan — TGE v1.0

**Estado:** Borrador
**Base:** ADR.md (26 decisiones arquitectónicas)
**Objetivo:** Plan de implementación por fases con milestones, dependencias y entregables concretos.

---

## Principios de implementación

1. **Fases estrictas**: No se avanza a la siguiente fase hasta que la anterior
   tenga tests pasando y esté funcional desde un programa de ejemplo.
2. **Cada fase produce un programa ejecutable**: No hay "código muerto" sin
   probar. Cada fase termina con un `examples/fase_N_ejemplo.c` que corre.
3. **Sin malloc en el hot path**: Toda la memoria se pre-asigna en init/resize
   (ADR-015).
4. **Tests primero para el parser**: El parser de bytes a eventos tiene muchos
   casos borde (secuencias ANSI, UTF-8 multi-byte, resize). Se escribe y prueba
   antes que cualquier otra cosa.
5. **El Makefile se actualiza en cada fase**: No hay un "gran rewrite" del
   build system.

---

## Arquitectura y filosofía (niveles de API)

TGE ocupa una capa intermedia: por debajo quedan SDL/SFML, por arriba
Godot/Unity/Raylib. No compite con engines; ofrece un **núcleo bajo nivel
estable** y módulos opcionales que suben la abstracción.

```
                 Usuario
                    |
                    |
              Aplicación
                    |
        +-----------+-----------+
        |                       |
   TGE Core                tge-extra
        |                       |
        |                 Grid
        |                 Entity
        |                 Animation
        |                 Camera
        |                 TileMap
        |                 UI
        |
 Runtime
 Canvas
 Events
 Scene
 Backend (interno)
        |
   ANSI / WinConsole
```

### Core (`libtge.a`) — "puedo hacer cualquier cosa"

Primitivas estables de ejecución:

- runtime, canvas, eventos, ciclo de vida, escenas, backend (interno).

Las **escenas** pertenecen al core por ser **infraestructura de ejecución**,
no porque el core tenga un modelo de juego: son una estructura de composición
y ciclo de vida. El core ejecuta callbacks, pero desconoce el significado de
esos estados.

Invariante: **el core no conoce gameplay, entidades concretas, reglas de
juegos, tamaños mínimos, ni tipos de aplicaciones.** Un editor, un emulador,
una UI o un multiplexer usan exactamente las mismas primitivas.

### tge-extra (`libtge-extra.a`) — "puedo hacer cosas comunes rápidamente"

Módulos opcionales de mayor nivel, cada uno independiente y usando solo API
pública del core:

- Existentes: `grid`, `entity`, `animation`, `collision`, `vec2i`,
  `direction`, `timer`, `fixedstep`, `input`, `view`, `input_buffer`,
  `grid_view`.
- Futuros: `tilemap`, `camera`, `ui`, `sprite`.

### Ejemplos — "así se usa para construir juegos"

Recetas concretas del modelo: `examples/games/*` consumen core + tge-extra.

### Evolución diferida: loop manual

El callback loop (`TGE_Run(app, init, update, draw, event)`) es cómodo para
la mayoría de los juegos terminales (modelo tipo love.update/draw). No es la
única salida posible: si aparece un caso real que necesite control total
(emuladores, servidores interactivos, terminal multiplexers, herramientas
editoriales), se diseña entonces una API de pump manual
(`TGE_Running`/`TGE_PollEvent`/`TGE_BeginFrame`/`TGE_EndFrame`). No se
inventa API sin uso.

Identidad: **"Si quieres control, baja al core. Si quieres velocidad de
desarrollo, usa extras."**

---

## Directorio final (post-Fase 4)

```
include/tge/
  tge.h           — umbrella header
  tge_runtime.h   — TGE_Runtime (contexto independiente, sin engine)
  tge_app.h       — TGE_App, TGE_Create, TGE_Run, TGE_Destroy
  tge_canvas.h    — TGE_Canvas (opaco), draw functions
  tge_events.h    — TGE_Event, scheduler API
  tge_math.h      — TGE_Vector2, TGE_Rect, TGE_Color, geometry
  tge_scene.h     — TGE_Scene, scene stack
  tge_sprite.h    — TGE_Sprite (Fase 2+)
  tge_tilemap.h   — TGE_Tilemap (Fase 2+)
src/
  tge_internal.h  — TGE_Cell, TGE_Diff, TGE_Backend, tipos internos
  runtime.c       — TGE_Runtime (solo: backend, parser, scheduler, event queue)
  app.c           — TGE_App (contiene TGE_Runtime + engine state)
  backend_ansi.c  — Backend ANSI: raw mode, colors, cursor, input
  canvas.c        — TGE_Canvas + draw functions (Engine)
  math.c          — Vector2, Rect, Color, geometry ops (Engine)
  parser.c        — State machine bytes → tokens → translator → eventos
  renderer.c      — Stateless: diff(current, previous) → TGE_Diff (Engine)
  scene.c         — Scene stack (push/pop diferidos) (Engine)
  scheduler.c     — TGE_CallLater / TGE_CallEvery (con prioridades)
  utf8.c          — UTF-8 decode + char width
tests/
  test_parser.c   — Tests del parser (bytes → eventos)
  test_canvas.c   — Tests de dibujo
  test_math.c     — Tests de math/geometry
  test_scene.c    — Tests del scene stack
  test_scheduler.c— Tests del scheduler
  test_renderer.c — Tests del diff
  test_runtime.c  — Tests de TGE_Runtime
  test_unit.c     — Test runner principal
benchmarks/
  bench_renderer.c    — diff performance (100x30, cambios parciales)
  bench_parser.c      — parser throughput (bytes/ms)
  bench_canvas_fill.c — fill_rect vs clear throughput
  bench_draw_line.c   — line drawing throughput
examples/
  min/                — ejemplos mínimos de cada API
    01_draw_text.c
    02_input_keys.c
    03_timer.c
    04_colors.c
    05_resize.c
    06_mouse.c
  games/              — juegos completos
    01_snake.c        — Fase 2: snake (timer, input, draw)
    02_pong.c         — Fase 3: pong con escenas, física, colisiones
    03_tetris.c       — Fase 3: tetris (timers, input, drawing)
    04_space_invaders.c — Fase 3: space invaders (composite)
    05_swarm.c        — Fase 4: arena top-down (consumidor real de tge-extra)
fuzz/
  fuzz_parser.c       — Fuzzing: millones de bytes aleatorios al parser
docs/
  API_STABILITY.md    — Política de compatibilidad
tge-extra/             — Fase 4 (opcional, post-1.0)
  entity.h/.c
  animation.h/.c
  collision.h/.c
  resources.h/.c
  fov.h/.c
  pathfinding.h/.c
  noise.h/.c
Makefile
README.md
.github/
  workflows/
    ci.yml            — CI desde Fase 0
```

---

## Fase 0: Build system + infraestructura + CI

**Objetivo:** Poder compilar y correr un "hola mundo" que no use TGE,
verificando que el toolchain, Makefile, CI y políticas de compatibilidad
están en su lugar desde el día 1.

### Entregables
- `Makefile` con targets: `all`, `clean`, `test`, `examples`, `bench`, `fuzz`
- Compila con `-std=c99 -Wall -Wextra -pedantic -g -O0`
- Static library `libtge.a`
- `.github/workflows/ci.yml`: `make test` en push + PR, Ubuntu + macOS
- `docs/API_STABILITY.md`: política de compatibilidad
- `tests/test_unit.c`: test runner vacío

### Makefile design

```makefile
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -O0
INCLUDES = -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = libtge.a

.PHONY: all clean test examples bench fuzz

all: $(TARGET)

$(TARGET): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/tge/tge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET) tests/test_unit
	./tests/test_unit

tests/test_unit: tests/test_unit.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_unit.c -L. -ltge -o $@

examples: $(TARGET)
	# cada fase agrega sus propios ejemplos aquí

bench: $(TARGET)
	# benchmarks, se agregan en Fase 1

fuzz: $(TARGET) fuzz/fuzz_parser
	./fuzz/fuzz_parser

fuzz/fuzz_parser: fuzz/fuzz_parser.c $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) fuzz/fuzz_parser.c -L. -ltge -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f tests/test_unit
	rm -f fuzz/fuzz_parser
	rm -f examples/min/*.o examples/games/*.o
```

### CI (`./github/workflows/ci.yml`)

```yaml
name: CI
on: [push, pull_request]
jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - run: make test
      - run: make examples
```

### API Stability (`docs/API_STABILITY.md`)

```
# API Stability Policy — TGE

Versiones 0.x (pre-1.0):
  - Breaking changes permitidos en cualquier release.
  - Se anuncian en el changelog.
  - Se evitan dentro de lo posible, pero no se garantizan.

Versiones 1.x:
  - Solo additive changes.
  - Nada se elimina de la API pública.
  - Nada cambia de semántica (bugs exceptuados).
  - Deprecación: una función marcada como deprecated se mantiene
    al menos 2 releases antes de eliminarse.

Versiones 2.x:
  - Breaking changes permitidos nuevamente.
  - Cada breaking change se documenta con guía de migración.
```

### Tests
- `tests/test_unit.c`: test runner vacío que reporta "0 tests, 0 failures"
- Verifica que el linker funciona: incluye `tge.h` desde un `.c` de prueba

### Dependencias
- Ninguna

---

## Fase 1: Runtime base (sub-fases incrementales)

**Objetivo:** Construir el Runtime pieza por pieza, validando cada componente
con tests antes de integrarlo. Cada sub-fase produce un archivo `.c` y `.h`
compilable y testeable de forma independiente.

### 1.1 — UTF-8

**Archivos:** `src/utf8.c`, tests en `tests/test_parser.c`

Implementar decode de UTF-8 y cálculo de ancho de caracteres.

```c
int tge_utf8_decode(const char *s, int *len);  // codepoint o -1
int tge_utf8_char_width(uint32_t codepoint);    // ancho en columnas
```

**Tests:**
- Caracteres ASCII (1-byte)
- 2-byte, 3-byte, 4-byte sequences
- Caracteres inválidos (secuencias truncadas, overlong)
- Char width: latín (1), CJK (2), emoji (2), tab (1)
- Bounds: U+0000, U+10FFFF, U+110000 (inválido)

**Duración:** 1 día

---

### 1.2 — Parser (two-stage)

**Archivos:** `src/parser.c`, `include/tge/tge_events.h`, `src/tge_internal.h`

State machine que reconoce secuencias de bytes y produce tokens internos,
luego un translator convierte tokens a eventos.

**Etapas:**
1. State machine: bytes → tokens (tipos internos: `TOKEN_CHAR`, `TOKEN_CSI`,
   `TOKEN_SS3`, `TOKEN_ESC`)
2. Translator: tokens → `TGE_Event` (`TOKEN_CSI_A` → `KEY_UP`)
3. API pública: `tge_parser_feed()`, `tge_parser_poll()`

**Eventos soportados:**
- KEYDOWN: flechas, WASD, ESC, Enter, Tab, Ctrl+letter, Shift+arrow
- KEYUP (cuando la terminal lo soporte)
- MOUSE: click, motion (modo SGR)
- RESIZE
- QUIT (Ctrl+C, Ctrl+D)

**`include/tge/tge_events.h`:**
```c
typedef enum {
    TGE_EVENT_NONE = 0,
    TGE_EVENT_KEYDOWN,
    TGE_EVENT_KEYUP,
    TGE_EVENT_MOUSEDOWN,
    TGE_EVENT_MOUSEUP,
    TGE_EVENT_MOUSEMOVE,
    TGE_EVENT_RESIZE,
    TGE_EVENT_QUIT,
    TGE_EVENT_TIMER,
    TGE_EVENT_USER,
} TGE_EventType;

typedef enum {
    TGE_TIMER_HIGH   = 0,
    TGE_TIMER_NORMAL = 1,
    TGE_TIMER_LOW    = 2,
} TGE_TimerPriority;

typedef struct {
    TGE_EventType type;
    union {
        struct { int keycode; int mod; } key;
        struct { int x, y; int button; } mouse;
        struct { int w, h; } resize;
        struct { int id; int priority; } timer;
        struct { int code; void *data; } user;
    } data;
} TGE_Event;
```

**Tests:**
- Cada tecla produce el evento correcto
- Secuencias CSI completas vs truncadas
- UTF-8 multi-byte + caracteres directos
- Mouse events (SGR mode)
- Resize sequence
- Overflow de buffer interno
- Bytes aleatorios no crashean (fuzz seed)

**Fuzz:** `fuzz/fuzz_parser.c` alimenta `tge_parser_feed()` con bloques de
bytes aleatorios (con seed fijo para reproducibilidad). Verifica que no haya
crash, infinite loop ni leak. Se ejecuta con `make fuzz`.

**Duración:** 1 semana (incluye fuzz + casos borde)

---

### 1.3 — Backend ANSI

**Archivos:** `src/backend_ansi.c`, `src/tge_internal.h` (TGE_Backend vtable)

Backend ANSI con estado interno (último color, cursor, buffer de salida).

```c
// Interno (tge_internal.h)
typedef struct {
    void *data;
    bool (*init)(void *data, int w, int h);
    void (*term)(void *data);
    int  (*width)(void *data);
    int  (*height)(void *data);
    void (*present)(void *data, TGE_Diff *diff, const TGE_Cell *cells, int stride);
    int  (*read_input)(void *data, char *buf, int bufsize);
    uint64_t (*ticks)(void *data);
} TGE_Backend;

TGE_Backend *tge_backend_ansi_create(void);
```

**Responsabilidades:**
- init: raw mode, guardar terminal attributes, request cursor hide
- term: restaurar terminal, show cursor
- read_input: read() no bloqueante de stdin
- present: serializar TGE_Diff a ANSI escape codes, usando el buffer de
  celdas para leer los valores actuales
- ticks: clock_gettime(CLOCK_MONOTONIC)

**Tests (en terminal real o con mock):**
- init/term restaura correctamente la terminal
- present: escribe secuencias ANSI correctas a stdout
- read_input: lee bytes de stdin sin bloquear
- ticks: monotónico, no decrece

**Duración:** 3 días

---

### 1.4 — Scheduler

**Archivos:** `src/scheduler.c`

Timer event scheduler independiente de la event queue.

```c
typedef struct TGE_Scheduler TGE_Scheduler;
TGE_Scheduler *tge_scheduler_new(void);
void tge_scheduler_free(TGE_Scheduler *s);
int  tge_scheduler_call_later(TGE_Scheduler *s, double delay_sec,
                              int event_id, int priority);
int  tge_scheduler_call_every(TGE_Scheduler *s, double interval,
                              int event_id, int priority);
void tge_scheduler_cancel(TGE_Scheduler *s, int timer_id);
void tge_scheduler_poll(TGE_Scheduler *s, double now,
                        TGE_Event *ev_out, int *count);
```

**Comportamiento:**
- `scheduler_poll` retorna timers expirados ordenados por prioridad (HIGH
  primero, luego NORMAL, luego LOW). Misma prioridad: FIFO.
- Timers repetitivos se re-encolan después de expirar.
- Cancelar un timer que ya expiró es no-op.

**Duración:** 2 días

---

### 1.5 — TGE_Runtime (integración del Runtime)

**Archivos:** `src/runtime.c`, `include/tge/tge_runtime.h`

Ensambla backend + parser + scheduler + event queue en un solo contexto.

**API pública:**
```c
typedef struct TGE_Runtime TGE_Runtime;

TGE_Runtime *tge_runtime_create(int width, int height);
void         tge_runtime_destroy(TGE_Runtime *rt);

bool tge_runtime_poll_event(TGE_Runtime *rt, TGE_Event *ev);

// Present recibe un diff + buffer de celdas (producido por Engine)
void tge_runtime_present(TGE_Runtime *rt, TGE_Diff *diff,
                         const TGE_Cell *cells, int stride);

// Scheduler
int  tge_runtime_call_later(TGE_Runtime *rt, double delay, int event_id,
                            int priority);
int  tge_runtime_call_every(TGE_Runtime *rt, double interval, int event_id,
                            int priority);
void tge_runtime_cancel_scheduled(TGE_Runtime *rt, int timer_id);

// Timing
uint64_t tge_runtime_ticks(TGE_Runtime *rt);
double   tge_runtime_now(TGE_Runtime *rt);

// Terminal info
int tge_runtime_width(TGE_Runtime *rt);
int tge_runtime_height(TGE_Runtime *rt);
```

**Nota:** TGE_Runtime NO conoce TGE_Canvas. El buffer de celdas se recibe
como `const TGE_Cell *cells` en `present()`. Esto permite que el Runtime
sea usado por cualquier programa con su propio buffer (dashboard, editor,
viewer) sin dependencia del Engine.

**Ejemplo de uso (Runtime-only):**
```c
TGE_Runtime *rt = tge_runtime_create(80, 24);
TGE_Cell *my_buffer = calloc(80 * 24, sizeof(TGE_Cell));
TGE_Diff diff;

while (running) {
    TGE_Event ev;
    while (tge_runtime_poll_event(rt, &ev)) {
        if (ev.type == TGE_EVENT_QUIT) running = false;
    }
    // dibujar en my_buffer...
    tge_renderer_diff(my_current, my_previous, &diff);  // o nuestro propio diff
    tge_runtime_present(rt, &diff, my_buffer, 80);
}
```

**Duración:** 1 día

---

### 1.6 — Canvas (Engine) — primitivas de dibujo

**Archivos:** `src/canvas.c`, `include/tge/tge_canvas.h`

Canvas opaco + draw functions. Pertenecen al Engine.

**Tipos públicos en `tge_canvas.h`:**
```c
// TGE_Color — diseñado para expansión desde el día 1
typedef enum {
    TGE_COLOR_MODE_INDEXED = 0,
    TGE_COLOR_MODE_RGB     = 1,
} TGE_ColorMode;

typedef struct {
    uint8_t mode;  // TGE_ColorMode
    union {
        uint8_t index;
        struct { uint8_t r, g, b; } rgb;
    };
} TGE_Color;

#define TGE_COLOR_BLACK   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 0 })
#define TGE_COLOR_RED     ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 1 })
#define TGE_COLOR_GREEN   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 2 })
#define TGE_COLOR_YELLOW  ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 3 })
#define TGE_COLOR_BLUE    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 4 })
#define TGE_COLOR_MAGENTA ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 5 })
#define TGE_COLOR_CYAN    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 6 })
#define TGE_COLOR_WHITE   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 7 })

// TGE_Cell — unidad atómica del framebuffer (público)
typedef struct {
    uint32_t ch;    // Unicode codepoint
    TGE_Color fg;
    TGE_Color bg;
    uint8_t  attr;  // bold=1, dim=2, italic=4, underline=8, blink=16, reverse=32
} TGE_Cell;

// TGE_Canvas — opaco
typedef struct TGE_Canvas TGE_Canvas;
int  tge_canvas_width(const TGE_Canvas *canvas);
int  tge_canvas_height(const TGE_Canvas *canvas);
```

**Primitivas de dibujo (core — 3 funciones):**
```c
void tge_clear(TGE_Canvas *canvas, uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_set_cell(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                  TGE_Color fg, TGE_Color bg);
void tge_draw_text(TGE_Canvas *canvas, int x, int y, const char *text,
                   TGE_Color fg, TGE_Color bg);
```

**Helpers (implementados sobre primitivas — se agregan aquí o en 1.7):**
```c
void tge_draw_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   TGE_Color fg, TGE_Color bg);
void tge_draw_frame(TGE_Canvas *canvas, int x, int y, int w, int h,
                    TGE_Color fg, TGE_Color bg);
void tge_fill_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_draw_line(TGE_Canvas *canvas, int x1, int y1, int x2, int y2,
                   uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_draw_circle(TGE_Canvas *canvas, int cx, int cy, int r,
                     uint32_t ch, TGE_Color fg, TGE_Color bg);
```

**Tests:**
- `tests/test_canvas.c`: cada primitiva con clipping, cada helper con cases representativos

**Duración:** 2 días

---

### 1.7 — Renderer (Engine) — diff + benchmarks

**Archivos:** `src/renderer.c`

Renderer stateless. Produce spans de celdas modificadas entre dos canvases.

```c
// Produce spans de celdas modificadas. No modifica canvas.
void tge_renderer_diff(const TGE_Canvas *current, const TGE_Canvas *previous,
                       TGE_Diff *diff);
```

**Tests** (`tests/test_renderer.c`):
- Sin cambios → diff vacío
- Una celda cambiada → un span de 1 celda
- Varias celdas consecutivas en una fila → un span
- Celdas no consecutivas → múltiples spans
- Varias filas con cambios → múltiples spans
- Canvas completo cambiado → un span por fila (`w * h` spans)
- Canvas con todas las celdas iguales → vacío

**Benchmarks** (`benchmarks/bench_renderer.c`):
- diff en canvas 100x30 con 10% de cambios aleatorios
- diff en canvas 100x30 con 50% de cambios
- diff en canvas 100x30 con 100% de cambios (full refresh)

**Duración:** 2 días

---

### Ejemplo integrador de Fase 1

**`examples/min/00_runtime_only.c`** — valida que Runtime funciona sin Engine:
```c
#include "tge/tge_runtime.h"
#include "tge/tge_canvas.h"  // solo por TGE_Cell
#include "tge/tge_events.h"

int main(void) {
    TGE_Runtime *rt = tge_runtime_create(80, 24);
    TGE_Cell *buf = calloc(80 * 24, sizeof(TGE_Cell));
    TGE_Diff diff;  // si Diff es público, o usamos una función helper

    bool running = true;
    while (running) {
        TGE_Event ev;
        while (tge_runtime_poll_event(rt, &ev)) {
            if (ev.type == TGE_EVENT_QUIT) running = false;
        }
        // dibujar directamente en buf...
        // computar diff contra otro buffer...
        tge_runtime_present(rt, &diff, buf, 80);
    }

    free(buf);
    tge_runtime_destroy(rt);
    return 0;
}
```
Esto demuestra que el Runtime es autónomo: no necesita Engine, ni Canvas,
ni scenes. Cualquier dashboard o editor puede usarlo con su propio buffer.

**`examples/min/01_draw_text.c`** — Canvas + Renderer + Runtime integrados:
```c
#include "tge/tge_runtime.h"
#include "tge/tge_canvas.h"
#include "tge/tge_events.h"

// Crea Runtime, crea dos canvases, dibuja texto con renderer,
// presenta con runtime. Termina con ESC.
```

### Dependencias
- Fase 0 (Makefile + toolchain + CI)
- 1.6 (Canvas) debe estar completa antes de 1.7 (Renderer)

### Criterio de éxito
```bash
make test                          # tests de 1.1 a 1.7 pasan
make bench                         # benchmarks corren sin error
make fuzz                          # fuzz parser 1M+ iteraciones sin crash
make examples/min/00_runtime_only  # runtime solo, sin engine
make examples/min/01_draw_text     # canvas + renderer + runtime integrados
```

---

## Fase 2: Engine — base (App + scenes + math + Snake)

**Objetivo:** Construir el Engine sobre el Runtime: `TGE_App`, scene stack,
math completo, y el pipeline scene → canvas → renderer → runtime → backend.
Validar con Snake (timer + input + draw sin física continua).

### API pública nueva

**`include/tge/tge_app.h`** — `TGE_App` contiene un `TGE_Runtime` más el
estado del Engine (canvas, scene stack). Delega eventos al Runtime.

```c
typedef struct TGE_App TGE_App;

typedef void (*tge_init_fn)(TGE_App *app);
typedef void (*tge_update_fn)(TGE_App *app, float dt);
typedef void (*tge_draw_fn)(TGE_App *app, TGE_Canvas *canvas);
typedef void (*tge_event_fn)(TGE_App *app, TGE_Event *ev);

TGE_App *TGE_Create(int width, int height, const char *title);
```
El tamaño pedido es **mínimo/fallback**: al iniciar, el runtime consulta el
tamaño real de la terminal al backend (`query_size`, TIOCGWINSZ en el backend
ANSI). Si la consulta tiene éxito, los canvases se crean con ese tamaño; si no
(no-tty), se usa el pedido. Los juegos deben tratar `width`/`height` como el
mínimo soportado y ser adaptativos al canvas que reciben en `draw`.
void     TGE_Destroy(TGE_App *app);
void     TGE_Run(TGE_App *app, tge_init_fn init, tge_update_fn update,
                 tge_draw_fn draw, tge_event_fn event);
void     TGE_Quit(TGE_App *app);

// Event polling (conveniencia, delega a TGE_Runtime)
bool TGE_PollEvent(TGE_App *app, TGE_Event *ev);

// Canvas (recibido en draw(), disponible también desde update)
TGE_Canvas *TGE_GetCanvas(TGE_App *app);

// Acceso al Runtime subyacente
// (para scheduler, timing, o uso avanzado)
TGE_Runtime *TGE_GetRuntime(TGE_App *app);
```

**`include/tge/tge_scene.h`**
```c
typedef struct TGE_Scene TGE_Scene;

typedef void (*tge_scene_init_fn)(TGE_Scene *scene);
typedef void (*tge_scene_update_fn)(TGE_Scene *scene, float dt);
typedef void (*tge_scene_draw_fn)(TGE_Scene *scene, TGE_Canvas *canvas);
typedef void (*tge_scene_event_fn)(TGE_Scene *scene, TGE_Event *ev);
typedef void (*tge_scene_destroy_fn)(TGE_Scene *scene);

struct TGE_Scene {
    bool opaque;
    void *userdata;
    tge_scene_init_fn    init;
    tge_scene_update_fn  update;
    tge_scene_draw_fn    draw;
    tge_scene_event_fn   event;
    tge_scene_destroy_fn destroy;
};

void TGE_PushScene(TGE_App *app, TGE_Scene *scene);
void TGE_PopScene(TGE_App *app);
void TGE_ReplaceScene(TGE_App *app, TGE_Scene *scene);
// Nota: todas las operaciones son diferidas (fin del frame, después de draw)
```

**`include/tge/tge_math.h`** (completo)
```c
typedef struct { float x, y; } TGE_Vector2;
typedef struct { int x, y, w, h; } TGE_Rect;

TGE_Vector2 tge_vec2(float x, float y);
TGE_Vector2 tge_vec2_add(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_vec2_sub(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_vec2_scale(TGE_Vector2 v, float s);
float       tge_vec2_length(TGE_Vector2 v);
float       tge_vec2_distance(TGE_Vector2 a, TGE_Vector2 b);

TGE_Rect tge_rect(int x, int y, int w, int h);
bool     tge_rect_contains(TGE_Rect r, int x, int y);
bool     tge_rect_intersects(TGE_Rect a, TGE_Rect b);
bool     tge_circle_intersects(TGE_Vector2 center, float r, TGE_Rect rect);
```

### Archivos nuevos

| Archivo | Dependencias | Propósito |
|---------|-------------|-----------|
| `include/tge/tge_app.h` | tge_runtime.h, tge_canvas.h | TGE_App + game loop |
| `include/tge/tge_scene.h` | tge_canvas.h, tge_events.h | TGE_Scene + scene stack |
| `include/tge/tge.h` | todos los headers | Umbrella header |
| `src/app.c` | runtime + engine internals | TGE_App implementation |
| `src/scene.c` | tge_internal.h, tge_scene.h | Scene stack |
| `src/math.c` | tge_math.h | Vector2 + Rect + geometry |
| `examples/games/01_snake.c` | — | Snake jugable |

### Game loop internos (`app.c`)

Sigue el orden de ADR-025. La diferencia clave respecto al prototipo: el
Engine es dueño de los canvas y el renderer. El Runtime solo recibe el
diff + buffer de celdas en `present()`.

```
TGE_Run():
  init(app)
  while not quit:
    now = tge_runtime_ticks(runtime)
    dt = now - last; last = now

    // 1. Leer input → parser → event queue
    tge_runtime_read_input(runtime, buf, sizeof(buf))
    parser_feed(buf)
    while parser_poll(&ev):
      event_queue_push(&ev)

    // 2. Procesar eventos (input, resize, quit)
    while event_queue_pop(&ev):
      if ev.type == TGE_EVENT_RESIZE:
        canvas_resize(current, ev.data.resize.w, ev.data.resize.h)
        canvas_resize(previous, ev.data.resize.w, ev.data.resize.h)
        canvas_clear(previous)  // fuerza diff completo
      else if ev.type == TGE_EVENT_QUIT:
        quit = true; break
      event_callback(app, &ev)

    // 3. Poll scheduler → timer events
    tge_runtime_poll_timers(runtime, now, timer_events, &count)
    for each timer_ev: event_callback(app, &timer_ev)

    // 4. Update (solo escena del tope)
    if scene_stack.count > 0:
      scene_stack.top.update(scene_stack.top, dt)

    // 5. Draw (stack bottom→top, respetando opacidad)
    tge_clear(current, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK)
    for each scene in stack (bottom to top):
      if scene.draw: scene.draw(scene, current)
      if scene.opaque: break

    // 6. Render (Engine produce diff, Runtime serializa)
    tge_renderer_diff(current, previous, &diff)
    tge_runtime_present(runtime, &diff, current->cells, current->width)
    // swap
    TGE_Canvas *tmp = previous; previous = current; current = tmp

    // 7. Scene ops diferidas
    process_scene_queue(app)

    // 8. Frame rate limiting
    sleep_remaining(now + target_frame_time)
```

### Tests

| Archivo | Lo que prueba |
|---------|--------------|
| `tests/test_math.c` | Vector2 ops, Rect contains/intersects, circle collision |
| `tests/test_scene.c` | Push, Pop, Replace, opacidad, operaciones diferidas, escena nula |

### Ejemplo de Fase 2 (`examples/games/01_snake.c`)

Snake completo para validar el Engine temprano:
- Grid-based movement (20x20)
- Timer event para game tick (no frame-rate dependent)
- Input: arrow keys (bufferizado para no perder dirección rápida)
- Score + Game Over screen
- Sin físicas continuas, sin floats, sin dt complejo

**Por qué Snake antes que Pong:** Snake solo necesita timer + input +
draw. No necesita física continua (floats, dt, velocidad, rebotes).
Valida el pipeline completo (scene → canvas → renderer → runtime →
backend) con mínima complejidad de juego.

### Dependencias
- Fase 1 (Runtime completo + Canvas + Renderer)
- No depende de Fase 3

### Criterio de éxito
```bash
make test              # tests de Fase 1 + Fase 2 pasan
make examples/games/01_snake  # snake corre y es jugable
```

---

## Fase 3: Pong + Tetris + Invaders

**Objetivo:** Validar el Engine completo con juegos de complejidad creciente.
Pong introduce física continua (floats, dt, velocidad, rebotes). Tetris
introduce rotación, grid y gravedad. Invaders introduce múltiples entidades
y colisiones.

No hay API nueva en esta fase (salvo bugs o cambios menores descubiertos
durante la implementación).

### `examples/games/02_pong.c`

- Dos escenas: `MenuScene` (transparente) y `GameScene` (opaca)
- Paddles controlados por teclado (W/S y flechas arriba/abajo)
- Pelota con física continua: velocidad, rebotes en ángulo
- Score (player 1 / player 2)
- Timer event para delays (countdown 3-2-1 antes de empezar)

**Qué valida:** dt, física continua, colisiones con ángulo, escenas
múltiples.

### `examples/games/03_tetris.c`

- 7 tetrominoes con rotación
- Timer event para gravedad (incrementando velocidad con nivel)
- Input: flechas + rotación (W/Up), hard drop (Space)
- Line clearing
- Score + next piece preview
- Game Over detection

**Qué valida:** Matrices de rotación, colisiones grid-based, timers
con prioridad, drawing con offset.

### `examples/games/04_invaders.c`

- Player, enemies, bullets, barriers
- Colisiones bala-enemigo, bala-barrera
- Enemies con movimiento sincronizado (shuffle)
- Timers para disparos enemies
- Escenas: Menu, Game, GameOver

**Qué valida:** Muchas entidades sin entity system, sprites multi-cell
con `draw_text`, colisiones simples.

### Dependencias
- Fase 2 (engine: app, scenes, canvas, math, runtime)

### Criterio de éxito
```bash
make examples/games/02_pong      # pong corre y es jugable
make examples/games/03_tetris    # tetris corre y es jugable
make examples/games/04_invaders  # space invaders corre y es jugable
```

---

## Fase 4: tge-extra (módulos opcionales)

**Objetivo:** Proporcionar módulos reutilizables para juegos más complejos,
sin inflar el núcleo.

| Módulo | Archivos | Propósito |
|--------|----------|-----------|
| Entity | `tge-extra/entity.h/.c` | `TGE_Entity { void *userdata; }`, ID pool opcional |
| Animation | `tge-extra/animation.h/.c` | Keyframes por celda, frame timer, easing |
| Resources | `tge-extra/resources.h/.c` | Carga y cacheo de sprites/tilemaps desde archivos |
| Collision | `tge-extra/collision.h/.c` | Broadphase spatial hash + narrowphase AABB |
| FOV | `tge-extra/fov.h/.c` | Raycasting, shadowcasting (roguelike) |
| Pathfinding | `tge-extra/pathfinding.h/.c` | A* sobre grid |
| Noise | `tge-extra/noise.h/.c` | Perlin/Simplex noise para generación procedural |
| Vec2i | `tge-extra/vec2i.h/.c` | `TGE_Vec2i { int x, y; }` + add/sub/scale/eq/zero |
| Direction | `tge-extra/direction.h/.c` | Enum NONE/UP/DOWN/LEFT/RIGHT + opposite/dx/dy/vec |
| Timer | `tge-extra/timer.h/.c` | Acumulador de intervalos (update + drain de ticks) |
| FixedStep | `tge-extra/fixedstep.h/.c` | Lazo fixed-timestep con clamp a `max_step` pasos |
| Input | `tge-extra/input.h/.c` | Helpers: direction (flechas + WASD), confirm, cancel, quit |
| Grid | `tge-extra/grid.h/.c` | Capa de dibujo para juegos por grilla: cada celda lógica = bloque `cell_w × cell_h` (corrige el aspecto 1:2); tema visual `TGE_GridTheme` (empty/default_sprite/border/selection) + tiles `TGE_GridTile` (`put_tile`/`fill`), sprites arbitrarios (`put`), y helpers (`set_cell`/`draw_border`/`clear`/`erase`/`draw_frame`/`line`/`circle`/`text`) |
| TileMap | `tge-extra/tilemap.h/.c` | Mapa de celdas lógicas a sprites sobre un `TGE_Grid`: `tge_tilemap_init(&map, 64, 64)`, `tge_tilemap_set(&map, 10, 5, &wall)`, `tge_tilemap_draw(&grid, &map)` |

### Grid (aspecto de celda)

Las celdas de terminal no son cuadradas (aprox. 1:2 ancho:alto). `grid.h`
resuelve el problema con una grilla lógica: el juego dibuja en coordenadas
lógicas y cada celda se renderiza como un bloque `cell_w × cell_h` de
caracteres. Con `tge_grid_square_pixels()` (macro de `set_cell_size(2, 1)`)
la celda se ve cuadrada: círculos dejan de ser elipses y el movimiento
horizontal y vertical se perciben iguales. La misma lógica de juego corre a
cualquier resolución física solo cambiando el tamaño de celda (grilla 20×19 →
20×19 / 40×19 / 80×38).

El Grid es una capa de dibujo para juegos por grilla, no un mini motor 2D
(sin tilemaps, cámaras, capas, entidades, animaciones ni z-order: eso va en
módulos separados). El Grid tiene un origen, un tamaño de celda, un tema y
helpers de dibujo. Cada celda lógica se dibuja con un *tile* semántico
(`TGE_GridTile`: empty/default/border/selection) que se resuelve a través del
tema (`TGE_GridTheme`, un sprite por rol: `empty`, `default_sprite`, `border`,
`selection`). `set_cell`/`fill`/`line`/`circle` usan el tile `default`
(`default_sprite`: la celda normal del mapa),
`draw_border` usa `border`, `clear`/`erase` usan `empty`, y `put_tile`
dibuja cualquier tile explícito. `put` dibuja un `TGE_Sprite` arbitrario a
tamaño natural sin escalar (p.ej. un personaje 5×5). Hay tres temas
listos: `TGE_GRID_THEME_BLOCKS` (bloques Unicode, para juegos),
`TGE_GRID_THEME_ASCII` (para terminales sin Unicode) y `TGE_GRID_THEME_DOTS`
(demuestra que un sprite no tiene por qué ser un bloque). `draw_text` y
`draw_frame` operan a nivel grilla sin escalar (HUD y paneles mantienen su
tamaño natural). Validado en `examples/min/08_grid_canvas.c` (swap de
temas). El ejemplo didáctico de juego de grilla es
`examples/games/06_snake_grid.c`: mismo Snake que 01 pero con la lógica de
`tge-extra` (Grid 2×1 + tema `SNAKE_THEME`; pared con `draw_border`, cuerpo
con `set_cell`, cabeza y comida con `put`), demostrando el valor de la
grilla frente al ejemplo core-only.

### TileMap (próximo módulo extra)

Dado que el Grid ya resuelve la representación de celdas (tema + tiles), el
siguiente módulo extra natural es `TileMap`: una matriz de celdas lógicas →
sprites renderizada sobre un `TGE_Grid`, pensada para niveles estilo
Zelda/Pokémon/Pac-Man:
`tge_tilemap_init(&map, 64, 64)`; `tge_tilemap_set(&map, 10, 5, &wall)`;
`tge_tilemap_draw(&grid, &map)`. Sin malloc (capacidad fija) como el resto
de tge-extra. Se implementará cuando se pida un juego que lo necesite.

### Módulos utilitarios (Batch 2)

Se añadieron pensando en simplificar los juegos core. Se validan
refactorizando `examples/games/01_snake.c` con Vec2i, Direction, FixedStep e
Input: el snake deja de manejar `x`/`y` sueltos, `dx/dy` o keycodes a mano.

### Reglas de tge-extra
1. Cada módulo es independiente y se compila solo si se necesita.
2. Pueden usar la API pública de TGE (include/tge/*.h) pero no los internals.
3. Pueden tener malloc en init/free (no están en el hot path).
4. No hay dependencias entre módulos de tge-extra.
5. **Solo se agrega una abstracción si elimina un patrón que apareció en al
   menos dos juegos** (regla del usuario, revisión 3). Mantiene tge-extra
   limpio y evita que se convierta en un framework gigante. Vale también
   para el core: `tge_printf` (HUDs), `tge_draw_centered_text` (menús y game
   over) y `tge_scene_create` (setup de escenas) se justifican porque el
   patrón estaba en todos los juegos.

### Dependencias
- Fase 2 (engine API)
- No dependen entre sí

### `examples/games/05_swarm.c` (consumidor real de tge-extra)

Los demás juegos del directorio son showcases del core a propósito; este
existe para "dogfooding": es un arena shooter top-down que usa los tres
módulos de tge-extra en un caso natural, no sintético.

- `TGE_EntityPool` — posee todos los actores (player, enemies, bullets)
  mediante handles opacos; el estado vive en `userdata` (`Body`).
- `TGE_Anim` — dos usos por enemigo: tween de entrada (drop-in, `value()`
  = posición vertical de spawn, ease OUT) y tween en loop usado como reloj
  para el sway horizontal (muestreado con `progress()`).
- `TGE_CollisionWorld` — un rect 1x1 por entidad, `move()` cada frame y
  `query()` para bala-enemigo, player-enemigo y breach inferior.

Cada frame: `for_each` actualiza anims + rects → `query()`s de colisión →
remoción diferida (no se libera durante la iteración, se acumula en
`to_kill[]` y se `flush_kills()` después).

**Qué valida:** ergonomía real de tge-extra (composición de los tres
módulos), restricción "no alloc/release durante `for_each`", y que la
remoción diferida es un patrón natural.

### Criterio de éxito
```c
// Cada módulo tiene un test:
#include "tge-extra/entity.h"
// compila, linkea, corre
```
```bash
make examples/games/05_swarm  # swarm corre, es jugable y no pierde memoria
```

---

## Resumen de dependencias y timeline estimado

```
Fase 0: Build + CI + API Stability          (1 día)

Fase 1: Runtime (sub-fases)
 ├─ 1.1 UTF-8                                (1 día)
 ├─ 1.2 Parser + fuzz                        (1 semana)
 ├─ 1.3 Backend ANSI                         (3 días)
 ├─ 1.4 Scheduler                            (2 días)
 ├─ 1.5 TGE_Runtime (integración)            (1 día)
 └─ 1.6 Canvas + Renderer + benchmarks       (3 días)
                                              ≈ 2.5 semanas

Fase 2: Engine base
 ├─ Math + geometry                          (2 días)
 ├─ Scene stack                              (2 días)
 ├─ TGE_App + game loop                      (4 días)
 └─ Snake (validación)                       (3 días)
                                              ≈ 1.5 semanas

Fase 3: Juegos completos
 ├─ Pong (física continua, dt)               (4 días)
 ├─ Tetris (rotación, grid, gravedad)        (5 días)
 └─ Space Invaders (entidades, colisiones)   (4 días)
                                              ≈ 2 semanas

Fase 4: tge-extra (opcional)                 (1-2 semanas)
```

**Total** estimado: ~7 semanas para núcleo (Fases 0-3).
**Puntos de validación clave:**
- Fin de 1.2: parser con fuzz, sin crashes
- Fin de 1.6: renderer con benchmarks, diff correcto
- Fin de Fase 2: Snake jugable (valida pipeline completo)
- Fin de 3.2: Pong jugable (valida física continua)
- Fin de 3.4: Invaders jugable (valida múltiples entidades)

---

## Primitivas vs Helpers

Las funciones de dibujo se dividen en dos categorías:

**Primitivas** (core, 3 funciones):
- `tge_clear()` — llena todo el canvas
- `tge_set_cell()` — escribe una celda individual
- `tge_draw_text()` — escribe una cadena UTF-8

**Helpers** (implementados sobre primitivas):
- `tge_draw_rect()` — borde de rectángulo (usa set_cell)
- `tge_draw_frame()` — borde con esquinas (usa set_cell)
- `tge_fill_rect()` — rectángulo relleno (usa set_cell en loop)
- `tge_draw_line()` — línea Bresenham (usa set_cell)
- `tge_draw_circle()` — círculo (usa set_cell)

Esta distinción es importante para tests: las primitivas tienen tests
exhaustivos de clipping y cobertura. Los helpers se testean con cases
representativos, asumiendo que las primitivas ya están validadas.

---

## Lo que NO está en el plan

- **Bindings a Python**: después de 1.0
- **Camera**: después de tilemap, no antes de 1.0
- **Sprite + Tilemap como built-ins**: opcionales en Fase 2, pero pueden
  moverse a tge-extra si no están listos
- **Win32 backend**: stub hasta que alguien lo implemente. Fase 1 solo ANSI.
- **True color (24-bit)**: Fase 1 solo 8 colores. 24-bit se agrega como
  extensión post-1.0 sin romper API (TGE_Color se queda como enum, se agrega
  `TGE_ColorRGB`).

---

## Checklist de pre-1.0

- [ ] Fase 0: Makefile, CI, API_STABILITY.md, test runner vacío
- [ ] 1.1: UTF-8 decode + char width
- [ ] 1.2: Parser two-stage (bytes → tokens → eventos) + fuzz sin crashes
- [ ] 1.3: Backend ANSI con estado interno (raw mode, colors, cursor, input)
- [ ] 1.4: Scheduler con prioridad (HIGH/NORMAL/LOW)
- [ ] 1.5: TGE_Runtime contexto (create/destroy/poll/present) sin dependencia de Canvas
- [ ] 1.6: Canvas opaco + draw primitives (clear, set_cell, text)
- [ ] 1.6: Renderer stateless: produce spans row-oriented correctos
- [ ] 1.6: Draw helpers (line, circle, frame, fill_rect) sobre primitivas
- [ ] 1.6: Clipping implícito en todas las draw functions
- [ ] 1.6: Benchmarks: renderer (10/50/100% diff), canvas fill, draw line
- [ ] 1.6: Sin malloc en render path (verificado con strace/valgrind)
- [x] Fase 2: Math + geometry (Vector2, Rect)
- [x] Fase 2: Scene stack con ops diferidas
- [x] Fase 2: TGE_App con game loop (orden ADR-025) + Snake jugable
- [x] Fase 3: Pong jugable (física continua, dt, colisiones con ángulo)
- [x] Fase 3: Tetris jugable (rotación, grid, gravedad)
- [x] Fase 3: Space Invaders jugable (múltiples entidades, colisiones)
- [x] Fase 4: juego consumidor real de tge-extra (05_swarm: entity + animation + collision)
- [x] Fase 4: módulos Batch 1 testeados (entity, animation, collision: test_entity, test_animation, test_collision)
- [x] Fase 4: módulos Batch 2 testeados (vec2i, direction, timer, fixedstep, input: test_vec2i, test_direction, test_timer, test_fixedstep, test_input)
- [x] Fase 4: Snake refactorizado con tge-extra (Vec2i, Direction, FixedStep, Input) y verificado
- [x] Fase 4: Grid (grid.h) con tests (test_grid) y demo (examples/min/08_grid_canvas)
- [x] Fase 4: tema visual (TGE_Grid + TGE_GridTheme + TGE_GridTile) y Snake con píxeles cuadrados (examples/games/06_snake_grid.c)
- [x] Core: `TGE_Create` usa el tamaño real de la terminal (TIOCGWINSZ) como tamaño inicial, con el tamaño pedido como fallback (`backend->query_size`)
- [x] Snakes adaptativos: `01_snake` y `06_snake_grid` se adaptan al tamaño de la terminal (layout canvas → HUD → playfield, evento `TGE_EVENT_RESIZE`, mensaje si es muy pequeño)
- [x] Cuerpo dinámico: `body` crece con `realloc` (capacidad = área del playfield) y se re-tunea al resize; corrige el límite fijo `MAX_LEN` que era menor que el área máxima del campo
- [x] Todos los headers públicos autocontenidos
- [x] API documentation (comentarios en headers)
- [x] docs/API_STABILITY.md publicado
- [x] Fase 4: módulos de layout/entrada (view: TGE_View, input_buffer: TGE_InputBuffer, grid_view: TGE_GridView) con tests (test_view, test_input_buffer, test_grid_view)
- [x] Fase 4: `01_snake` refactorizado con TGE_View + TGE_InputBuffer (elimina field/too_small/dir_queue manuales)
- [x] Fase 4: `06_snake_grid` refactorizado como ejemplo de referencia de la arquitectura world/renderer (SnakeWorld puro + SnakeRenderer con TGE_GridView; escenas solo pegan world + renderer)
- [x] Revisión 2: `tge_view_update()` devuelve `TGE_ViewUpdate` (INVALID/RESIZED/FIRST_VALID); se elimina `laid_out` de los snakes (01 y 06 reaccionan con un switch)
- [x] Revisión 2: `tge_grid_size_for` / `tge_grid_view_size_for` (tamaño lógico para una superficie arbitraria, sin canvas) — reemplaza el `renderer_playfield` del Snake
- [x] Revisión 2: `renderer_playfield` → `tge_grid_view_size_for`; `renderer_setup` → `renderer_bind` (sincroniza con el canvas actual, que el app intercambia cada frame); `GameState` → `SnakeGame`
- [x] Revisión 2: `tge_printf()` en el core con tests y HUDs de todos los juegos migrados
- [x] Revisión 3: `tge_draw_centered_text()` en el core — texto centrado midiendo ancho real (wide chars cuentan doble), con tests; reemplaza `(w - strlen)/2` en todos los juegos (queda solo el puntaje derecho de Pong, que es alineación derecha)
- [x] Revisión 3: `tge_scene_create()`/`tge_scene_destroy()` en el core — escena + userdata en un solo alloc con trampoline de destroy; elimina `calloc`/wiring/`cleanup_scene` en 01/05/06 (resuelve el punto 1 de Pendiente/Diferido)
- [x] Revisión 3: helpers de punto en tge-extra (`vec2i`): `tge_vec2i_clamp_rect` (clamp post-resize de los snakes), `tge_rect_random_point` (spawn de comida/oleadas) y `tge_rect_translate_point` (mapeo local→global al dibujar), con tests

---

## Pendiente / Diferido (decisión del usuario, pendiente de retomar)

El usuario pidió **anotar** esto antes de pasar a algo más urgente. No está
bloqueado, solo diferido.

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

### 11. `TGE_Actor` (anotado, esperar evidencia)

Candidato fuerte para el siguiente módulo "de patrones" de tge-extra:

```c
typedef struct {
    TGE_Vec2i position;
    TGE_Sprite *sprite;
    TGE_Color fg, bg;
} TGE_Actor;
```

Con `tge_grid_draw_actor(...)` para el renderer. No es ECS ni específico de
Snake (sirve para roguelikes, Tetris, Pac-Man, Bomberman, Sokoban, tower
defense). **Regla:** no se implementa hasta que 2-3 juegos demuestren que el
diseño sirve de verdad.

### 12. `TGE_Playfield` (anotado, esperar evidencia)

El Snake muestra una arquitectura emergente:

```
View + InputBuffer + FixedStep + GridView
```

Si Snake, Breakout y Pac-Man usan el mismo patrón, recién entonces se
encapsula. No se implementa todavía.
