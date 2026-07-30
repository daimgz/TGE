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
    04_invaders.c     — Fase 3: space invaders (composite)
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

### Reglas de tge-extra
1. Cada módulo es independiente y se compila solo si se necesita.
2. Pueden usar la API pública de TGE (include/tge/*.h) pero no los internals.
3. Pueden tener malloc en init/free (no están en el hot path).
4. No hay dependencias entre módulos de tge-extra.

### Dependencias
- Fase 2 (engine API)
- No dependen entre sí

### Criterio de éxito
```c
// Cada módulo tiene un test:
#include "tge-extra/entity.h"
// compila, linkea, corre
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
- [ ] Fase 2: Math + geometry (Vector2, Rect)
- [ ] Fase 2: Scene stack con ops diferidas
- [ ] Fase 2: TGE_App con game loop (orden ADR-025) + Snake jugable
- [ ] Fase 3: Pong jugable (física continua, dt, colisiones con ángulo)
- [ ] Fase 3: Tetris jugable (rotación, grid, gravedad)
- [ ] Fase 3: Space Invaders jugable (múltiples entidades, colisiones)
- [ ] Todos los headers públicos autocontenidos
- [ ] API documentation (comentarios en headers)
- [ ] docs/API_STABILITY.md publicado
