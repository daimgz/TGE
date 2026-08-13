# ADR — Architecture Decision Record

**Estado:** Aprobado
**Fecha:** 2026-07-30
**Contexto:** Diseño de TGE (Terminal Graphics Engine), un motor de gráficos
de terminal en C con API inspirada en Raylib, Love2D y SDL.

---

## Principio rector

> Cada capa debe existir porque elimina una dependencia concreta, no porque
> haga el diagrama más elegante. Si una capa no reduce acoplamiento, no mejora
> la extensibilidad o no habilita un backend nuevo, probablemente sobre.

---

## ADR-001: El motor controla el game loop

**Decisión:** El usuario escribe `main()`, crea una aplicación con
`TGE_Create()`, registra callbacks (`init`, `update`, `draw`, `event`) y llama
a `TGE_Run()`. El motor controla el loop, no el usuario.

```c
void init(TGE_App *app) { ... }
void update(TGE_App *app, float dt) { ... }
void draw(TGE_App *app, TGE_Canvas *canvas) { ... }
void event(TGE_App *app, TGE_Event *ev) { ... }

int main(void) {
    TGE_App *app = TGE_Create(80, 24, "Juego");
    TGE_Run(app, init, update, draw, event);
    TGE_Destroy(app);
    return 0;
}
```

**Racional:** El motor necesita controlar timing, render diferencial y el
parser. Si el usuario controla el loop, termina conociendo detalles internos
(flush del framebuffer, encolado de eventos, medición de tiempo) que deberían
ser responsabilidad del motor.

**Consecuencias:**
- El render diferencial, scheduler y event loop pertenecen al motor.
- `TGE_Run()` no devuelve control hasta que la aplicación termina.
- El usuario registra callbacks, no escribe loops.
- Bindings a Python: `TGE_Run()` es una función bloqueante, los callbacks se
  registran desde Python.

---

## ADR-002: Tres capas — Runtime, Engine, Game

**Decisión:** El código se organiza en tres capas con dependencia
unidireccional:

```
Application (Game)
       │
       ▼
     Engine
       │
       ▼
     Runtime
       │
       ▼
     Backend
```

**Racional:** El runtime no sabe que existen escenas. El engine no sabe qué
es ANSI. Cada capa es testeable y reemplazable de forma independiente. El
runtime puede vivir como librería independiente (editor de terminal, dashboard,
TUI ligera).

**Runtime** — solo terminal:
- Backend I/F, terminal I/O
- Parser de bytes → tokens → eventos
- Event queue
- Scheduler, timing
- UTF-8 / Unicode
- `TGE_Diff` (tipo interno, no público)

**Engine** — ya habla de videojuegos:
- App (game loop, orquestación)
- Canvas (framebuffer + draw API)
- Renderer (comparación de canvas → `TGE_Diff`)
- Scene stack
- Math + Geometry
- Sprite (opcional), Tilemap (opcional), Camera (opcional)

**Game** — código del usuario, usa solo API pública de Engine + Runtime.

**Runtime como `TGE_Runtime`:** El runtime expone un contexto público
(`TGE_Runtime`) con API independiente del engine. Esto permite usar el runtime
sin scenes ni game loop (dashboards, editores, viewers). `TGE_App` en la capa
Engine contiene un `TGE_Runtime` internamente.

**Consecuencias:**
- Runtime no incluye headers del engine ni sabe de escenas.
- Engine no incluye headers del backend ni sabe de ANSI.
- Backend ANSI y WinConsole implementan `TGE_Backend` sin conocer al engine.
- Usuarios del runtime (sin engine) usan `tge_runtime_create()` / `tge_runtime_destroy()`.

---

## ADR-003: Todo es un Evento (no existe "Input")

**Decisión:** No hay subsistema de Input. Hay una Event Queue unificada estilo
SDL. No existe `TGE_Input`, `TGE_Keyboard` ni `TGE_Mouse`. Existe
`TGE_PollEvent()`.

```
TGE_Event event;
while (TGE_PollEvent(app, &event)) {
    switch (event.type) {
        case TGE_EVENT_KEYDOWN: ...
        case TGE_EVENT_TIMER:   ...
        case TGE_EVENT_USER:    ...
    }
}
```

**Tipos de eventos:**
- `KEYDOWN`, `KEYUP`
- `MOUSEDOWN`, `MOUSEUP`, `MOUSEMOVE`
- `RESIZE`, `QUIT`
- `TIMER` (generado por el scheduler)
- `USER` (definido por el usuario)

**Comportamiento de RESIZE:**
Cuando la terminal cambia de tamaño, el runtime:
1. Realloc el canvas actual y el canvas anterior al nuevo tamaño.
2. Limpia el canvas anterior (forzando un diff completo en el próximo frame).
3. Encola un `TGE_EVENT_RESIZE` con el nuevo ancho y alto.
4. El próximo frame dibuja el canvas completo.

**Consecuencias:**
- `TGE_PollEvent()` es la única API de entrada.
- El parser es state machine (bytes → tokens → eventos).
- El scheduler inyecta `TGE_EVENT_TIMER` en la cola (sin callbacks).
- El usuario puede inyectar `TGE_EVENT_USER`.
- **La event queue tiene capacidad máxima fija** (definida en init). Si la cola
  se llena, los eventos más viejos se descartan (drop oldest), preservando el
  estado más reciente (por ejemplo, la última posición del mouse). En debug
  builds se puede agregar un assert para detectar overflow durante desarrollo.
- **El scheduler es un subsistema separado de la event queue.** El scheduler
  mantiene su propia lista de timers y, al expirar, produce `TGE_EVENT_TIMER`
  que se encolan en la event queue como cualquier otro evento (ADR-008).

---

## ADR-004: Scene Stack, no Scene Tree

**Decisión:** Las escenas se organizan en una pila. No hay árbol, no hay
jerarquía padre-hijo.

```
MainMenu → PushScene(Gameplay) → PushScene(Pause) → PopScene()
```

**Racional:** Un árbol de escenas (Godot-style) agrega event bubbling,
transform tree y render tree que en terminal no aportan beneficio claro. Una
pila es suficiente para los juegos que este motor apunta a crear.

**Nota de diseño:** Cada escena puede indicar si es opaca o transparente. Una
escena opaca (ej: loading screen) evita dibujar las escenas debajo. Una
escena transparente (ej: pausa) se dibuja encima sin ocultar el fondo. Esto
es un detalle interno de `TGE_Scene`. **El tipo es opaco** para permitir
agregar campos en el futuro (transiciones, z-order, flags) sin romper la ABI.
El usuario configura la escena mediante setters o parámetros de creación.

**Consecuencias:**
- `TGE_PushScene(app, scene)`, `TGE_PopScene(app)`.
- Solo la escena del tope recibe eventos y update.
- Las escenas se dibujan desde el fondo hacia el tope, respetando opacidad.
- No hay event bubbling. No hay transform tree. Posiciones absolutas.
- **Ownership de escenas:** el usuario crea y destruye las escenas. El engine
  no asume ownership. `TGE_PushScene()` recibe un puntero a una escena que
  el usuario mantiene viva. `TGE_PopScene()` no destruye la escena. Esto
  permite stack allocation, allocation dinámica, o reuso de escenas.
- **Las operaciones sobre la scene stack son diferidas.** `PushScene()`,
  `PopScene()` y `ReplaceScene()` no se ejecutan inmediatamente sino que se
  encolan y aplican al final del frame, después de draw. Esto evita
  invalidaciones del stack durante el update y simplifica el ownership. Si
  hay múltiples operaciones encoladas, se ejecutan en orden FIFO.

---

## ADR-005: Canvas es el framebuffer

**Decisión:** `TGE_Canvas` ES el framebuffer. No existe un tipo separado
`TGE_Framebuffer`. El renderer compara dos canvas: current y previous.

```c
typedef struct TGE_Canvas TGE_Canvas;  // tipo opaco

int tge_canvas_width(const TGE_Canvas *canvas);
int tge_canvas_height(const TGE_Canvas *canvas);
```

`TGE_Canvas` pertenece al Engine (no al Runtime). El Runtime no conoce
`TGE_Canvas` — solo recibe un buffer plano de celdas (`TGE_Cell *`) en
`tge_runtime_present()`. Esto permite que el Runtime sea reutilizable para
dashboards o editores que tengan su propio buffer.

`TGE_Cell` es público y pertenece al header `tge_canvas.h`. Aunque el canvas
es opaco, la celda es la unidad atómica del framebuffer y necesita ser visible
para que funciones como `tge_runtime_present()` puedan recibir buffers de
celdas sin filtrar tipos internos.

```c
typedef struct {
    uint32_t ch;    // Unicode codepoint
    TGE_Color fg;   // foreground color
    TGE_Color bg;   // background color
    uint8_t  attr;  // bold=1, dim=2, italic=4, underline=8, blink=16, reverse=32
} TGE_Cell;
```

El canvas es opaco para evitar que el usuario acceda directamente al
buffer de celdas. La API pública de dibujo (`tge_draw_*`) es la única forma
soportada de escribir sobre el canvas. Esto permite cambiar la representación
interna en el futuro sin romper código de usuario.

El ancho y alto se consultan con getters. Si en el futuro se necesitan render
targets, se agregará un tipo separado (`TGE_RenderTexture`).

El pipeline completo:

```
Scene → Canvas API (escribe en cells[]) → Renderer (diff current vs previous) → Backend
```

El canvas se pasa como parámetro a `draw()`:

```c
void draw(TGE_App *app, TGE_Canvas *canvas) {
    tge_draw_text(canvas, "Hola", 10, 5, TGE_COLOR_WHITE);
    tge_draw_rect(canvas, 8, 3, 14, 5, TGE_COLOR_YELLOW);
}
```

El renderer recibe dos canvas (current y previous), los compara celda por
celda, y envía solo las diferencias al backend. El backend convierte esas
diferencias a ANSI o Win32.

No hay Draw Commands. No hay Framebuffer como tipo separado. Un canvas es
el buffer de celdas y el otro es el buffer anterior para el diff.

**Racional:** En una GPU los draw commands tienen sentido (reordenar por z,
batch por shader). En una terminal, el canvas ya es un buffer de celdas —
agregar otra capa entre la escena y el canvas es overhead sin beneficio.

**Consecuencias:**
- `tge_draw_text(canvas, ...)`, `tge_draw_rect(canvas, ...)`, etc.
- Canvas se pasa como parámetro, no se pide con getter.
- No existe `TGE_DrawCommand`.
- Backends ANSI y WinConsole comparten la misma API de canvas.
- **El único canvas público pertenece a `TGE_App`.** El usuario no puede crear un
  `TGE_Canvas` por su cuenta. El canvas se recibe como parámetro en `draw()` y
  es de solo lectura en los demás callbacks. Si en el futuro se necesitan render
  targets, se agregará un tipo separado (`TGE_RenderTexture`) sin romper la API.

---

## ADR-006: Entity no existe en el núcleo

**Decisión:** No hay `TGE_Entity` en el engine. El usuario guarda sus datos en
`void*` dentro de la escena o donde prefiera. Si necesita una convención, usa
`tge-extra/entity`.

```c
// El núcleo NO define TGE_Entity.
// El usuario escribe:
typedef struct {
    int x, y;
    bool vivo;
    // lo que necesite
} MiEntidad;

MiEntidad jugador = {0};
```

**Racional:** Si la entidad es solo `void *userdata`, no está aportando nada
que un puntero no haga. El engine no necesita saber qué es una entidad —
necesita que la escena tenga datos. Cómo organice el usuario esos datos es su
problema.

**Consecuencias:**
- El núcleo no impone estructura de datos al usuario.
- `tge-extra/entity` puede definir `TGE_Entity { void *userdata; }` si se
  necesita una convención compartida entre proyectos.
- Sprite, animation, etc. son componentes independientes que el usuario
  combina como quiera.

---

## ADR-007: Structs públicos, no getters/setters

**Decisión:** Los structs son públicos y se modifican directamente (estilo
Raylib).

```c
pos.x = 5;                          // bien
tge_vector2_set_x(&pos, 5);         // mal
```

**Racional:** Menos código, más legible, sin overhead.

**Consecuencias:**
- `TGE_Vector2`, `TGE_Rect`, `TGE_Color`, `TGE_Cell` son structs públicos.
- No hay encapsulación. El usuario es responsable del uso correcto.
- **Excepciones:** Tipos con representación interna que debe ocultarse
  (`TGE_Canvas`, `TGE_App`, `TGE_Runtime`, `TGE_Scene`) son opacos y tienen
  getters o funciones de acceso.
- Binding a Python expone structs como objetos mutables.

---

## ADR-008: Scheduler genera eventos, no callbacks

**Decisión:** `TGE_CallLater()` y `TGE_CallEvery()` encolan
`TGE_EVENT_TIMER` en la event queue. No ejecutan callbacks.

```c
int id = tge_call_later(app, 2.0, ID_EVENT_EXPLOSION);

// en event():
if (ev.type == TGE_EVENT_TIMER && ev.data.timer.id == ID_EVENT_EXPLOSION) {
    explotar();
}
```

**Racional:** Si todo es un evento (ADR-003), el scheduler no debería tener un
camino especial. Consistencia: la event queue es la única vía de entrada.

**Prioridades:** Los timers tienen prioridad (HIGH, NORMAL, LOW). Cuando el
scheduler poll events, los timers de mayor prioridad se encolan primero. Esto
permite que eventos de input (HIGH) se procesen antes que timers de juego
(NORMAL) o carga de assets (LOW) en el futuro.

**Consecuencias:**
- `TGE_CallLater(app, delay, event_id, priority)` encola un `TGE_EVENT_TIMER`.
- `TGE_CallEvery(app, interval, event_id, priority)` versión repetitiva.
- `TGE_CancelScheduled(app, timer_id)` cancela.
- No hay `TGE_TimerCallback`.
- El scheduler es un subsistema independiente de la event queue. Mantiene su
  propia lista de timers y produce eventos que se encolan en la event queue.

---

## ADR-009: Math + UTF-8 desde el día 1; Geometry sí, Collision no

**Decisión:** Math y UTF-8 son núcleo. Geometry (intersecciones de rect,
círculo, distancia) también. Collision como sistema (broadphase, response) NO.

**Racional:** Sin vectores no se hace nada. Sin UTF-8 no hay sprites. Las
intersecciones son operaciones matemáticas, no un sistema. El usuario arma sus
colisiones encima.

**Diseño de TGE_Color:** Se define como struct desde el día 1 para permitir
expansión a 256 colores, true color (24-bit) y modos extendidos sin romper
la ABI. La primera implementación usa solo indexed (8 colores base), pero el
tipo soporta RGB nativamente.

```c
// tge_canvas.h
typedef enum {
    TGE_COLOR_MODE_INDEXED = 0,
    TGE_COLOR_MODE_RGB     = 1,
} TGE_ColorMode;

typedef struct {
    uint8_t mode;  // TGE_ColorMode
    union {
        uint8_t index;              // 0-255 (8 colores base en Fase 1)
        struct { uint8_t r, g, b; } rgb;  // 24-bit true color
    };
} TGE_Color;

// Constantes para los 8 colores base (modo indexed)
#define TGE_COLOR_BLACK   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 0 })
#define TGE_COLOR_RED     ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 1 })
#define TGE_COLOR_GREEN   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 2 })
#define TGE_COLOR_YELLOW  ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 3 })
#define TGE_COLOR_BLUE    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 4 })
#define TGE_COLOR_MAGENTA ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 5 })
#define TGE_COLOR_CYAN    ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 6 })
#define TGE_COLOR_WHITE   ((TGE_Color){ .mode = TGE_COLOR_MODE_INDEXED, .index = 7 })
```

**Consecuencias:**
- `TGE_Vector2`, `TGE_Rect` en `tge_math.h`.
- `TGE_Color`, `TGE_Cell` en `tge_canvas.h`.
- `tge_rect_intersects()`, `tge_circle_intersects()`, `tge_distance()` en
  `tge_geometry.h`.
- No hay `TGE_CollisionSystem`.
- Collision como sistema → `tge-extra/collision`.

---

## ADR-010: Camera es opcional (después de Tilemap)

**Decisión:** Camera no está en el núcleo 1.0. Se implementa después si hay
necesidad.

**Racional:** Pong, Snake, Tetris, Space Invaders — cero necesitan cámara.

**Consecuencias:**
- Camera no bloquea Fase 1 ni Fase 2.
- Cuando se implemente: struct con offset y viewport.

---

## ADR-011: Backend intercambiable vtable

**Decisión:** El backend es un struct de function pointers. Recibe un
`TGE_Diff*` con las celdas modificadas, no el canvas completo.

```c
// Tipos internos (no públicos, definidos en src/tge_internal.h)

// Representación de una celda en el framebuffer.
typedef struct {
    uint32_t ch;    // Unicode codepoint
    uint16_t fg;    // foreground color
    uint16_t bg;    // background color
    uint8_t  attr;  // bold=1, dim=2, italic=4, underline=8, blink=16, reverse=32
} TGE_Cell;

// Span de celdas modificadas consecutivas en una misma fila.
typedef struct {
    int y;
    int x_start;  // inclusive
    int x_end;    // exclusive (half-open)
} TGE_DirtySpan;

typedef struct {
    TGE_DirtySpan *spans;
    int count;
    int capacity;
} TGE_Diff;

// Cada backend tiene su propio state (buffer ANSI, último color, etc.)
// guardado en `data`. Todas las funciones reciben el data pointer.
typedef struct {
    void *data;  // backend-specific state
    bool (*init)(void *data, int w, int h);
    void (*term)(void *data);
    int  (*width)(void *data);
    int  (*height)(void *data);
    void (*present)(void *data, TGE_Diff *diff, const TGE_Cell *cells, int stride);
    int  (*read_input)(void *data, char *buf, int bufsize);
    uint64_t (*ticks)(void *data);
} TGE_Backend;
```

**Racional:** El renderer (Engine) compara current vs previous canvas y
produce un `TGE_Diff` con spans por fila. El Engine llama a
`tge_runtime_present(runtime, &diff, cells, width)` con un buffer plano
de celdas para que el backend serialice. El backend lee las celdas del
buffer usando las coordenadas del span; al ser row-oriented, puede emitir
ANSI con mínimos cursor movements. Cada backend tiene su propio `data`
para estado (último color, buffer ANSI, etc.) sin usar variables globales.

**Consecuencias:**
- El backend no necesita hacer su propio diff ni almacenar estado anterior.
- El Runtime no conoce `TGE_Canvas` — solo recibe un buffer de celdas.
- El renderer (Engine) es el responsable exclusivo de calcular diferencias.
- `TGE_Diff`, `TGE_DirtySpan` y `TGE_Cell` son tipos internos.
- Cada backend tiene su propio `data` pointer, eliminando globales.
- ANSI backend por defecto en Linux/macOS.
- `TGE_Create()` / `tge_runtime_create()` detectan plataforma.
- Backend mockeable para tests.

---

## ADR-012: Render pipeline

**Decisión:** El pipeline completo:

```
   Engine
┌─────────────────────────────────────┐
│ Canvas API (tge_draw_*, escribe     │
│           en cells[])               │
│           ↓                         │
│ Renderer (compara current vs        │
│           previous canvas)          │
│           ↓                         │
│      TGE_Diff (spans por fila)      │
└─────────────────────────────────────┘
           ↓
   Runtime
┌─────────────────────────────────────┐
│ present(diff, cells, width)         │
│           ↓                         │
│ Backend (ANSI / Win32)              │
│           ↓                         │
│      stdout / terminal              │
└─────────────────────────────────────┘
```

No hay "render commands", no hay "draw commands". El pipeline es un flujo
Engine → Runtime sin retroalimentación:

1. **Renderer** (Engine) — función pura. Recibe dos `TGE_Canvas*` (current y
   previous), los compara celda por celda recorriendo por filas, y produce
   un `TGE_Diff` con spans. No modifica los canvas ni intercambia punteros.
   Los spans agrupan celdas consecutivas modificadas en una misma fila
   para que el backend pueda serializar con mínimo cursor movement.

2. **Engine** — llama al renderer para producir el diff, luego llama a
   `tge_runtime_present(runtime, &diff, canvas->cells, canvas->width)`
   para que el Runtime serialice el diff. Después, intercambia los punteros
   de canvas (current → previous) para el próximo frame.

3. **Backend** (Runtime) — recibe el `TGE_Diff*` y un buffer plano de
   celdas (`const TGE_Cell *cells, int width`). Serializa los spans a ANSI
   o Win32 leyendo las celdas del buffer. No conoce `TGE_Canvas`.

**Racional:** El canvas ES el framebuffer (ADR-005). El renderer no conoce
ANSI; el backend no conoce el canvas completo. Separar el swap de canvas del
renderer mantiene cada componente con una sola responsabilidad. Los spans
por fila optimizan la salida ANSI sin complejidad en el backend.

**Consecuencias:**
- El renderer es una función pura: `renderer_diff(current, previous, &diff)`.
- El runtime (o `TGE_App`) intercambia los canvas después de `present()`.
- El backend recibe `TGE_Diff*` con spans row-oriented (ADR-011).
- Pipeline plano, tres tipos: canvas, diff (span-based), backend.

---

## ADR-013: Game loop determinista

**Decisión:** El game loop usa `delta_time`. La API pública no expone
funciones de suspensión de hilo.

**Racional:** El timing debe ser consistente entre frames. Internamente el
runtime puede usar `clock_nanosleep()`, `select(timeout)` o
`WaitForSingleObject()` para limitar FPS sin quemar CPU. El usuario nunca ve
eso.

**Consecuencias:**
- `update(dt)` recibe delta_time en segundos.
- El runtime decide cómo esperar entre frames.
- FPS limit es configuración del runtime, no del código de juego.
- Sin `sleep()` en la API pública.

---

## ADR-014: Parser de bytes, no parser ANSI

**Decisión:** El parser es una state machine que consume bytes crudos y
produce eventos. No conoce "ANSI" como concepto.

```
Bytes crudos (stdin)
    ↓
State machine (patrones de bytes)
    ↓
Tokens
    ↓
Eventos (TGE_Event)
```

**Racional:** ANSI es un protocolo entre muchos. El parser solo reconoce
sintaxis (secuencias de bytes). La traducción a eventos es responsabilidad de
un translator separado. Esto permite agregar nuevos protocolos (Kitty keyboard,
WezTerm, SGR mouse) sin modificar el translator.

**Arquitectura interna:**

```
Bytes crudos (stdin)
    ↓
State machine (reconoce sintaxis: CSI, OSC, caracteres directos)
    ↓
Tokens internos (p.ej., TOKEN_CSI_CURSOR_UP, TOKEN_CHAR)
    ↓
Translator (asigna semántica: TOKEN_CSI_CURSOR_UP → KEY_UP)
    ↓
Eventos (TGE_Event)
```

El parser y el translator son módulos separados dentro del runtime, pero
la API pública los unifica: `tge_parser_feed()` → bytes, `tge_parser_poll()`
→ eventos. Los tokens son un tipo interno.

**Consecuencias:**
- `tge_parser_feed(parser, bytes, len)` consume bytes y produce tokens internos.
- `tge_parser_poll(parser, &event)` traduce tokens → eventos. Si no hay tokens
  completos, retorna false.
- El translator es un paso separado: el parser solo reconoce sintaxis, el
  translator asigna semántica.
- Nuevos protocolos = nuevos patrones en la state machine que producen los
  mismos tokens. El translator no cambia.
- El generador ANSI (dirección inversa) está en el backend, separado.

---

## ADR-015: Sin allocaciones durante el renderizado

**Decisión:** Toda la memoria necesaria para renderizar se reserva al crear la
aplicación o al redimensionar la terminal. El render path no llama a `malloc`.

**Racional:** `malloc` por frame causa fragmentación, jitter y rendimiento
impredecible.

**Consecuencias:**
- Dos canvas: `current` y `previous`. Se alocan en init/resize.
- El parser tiene buffer fijo para secuencias.
- Event queue con capacidad máxima fija.
- Si se necesita más memoria, se redimensiona fuera del hot path.
- Resize es la única operación que puede alocar memoria fuera del init.
  Pertenece al cold path: se ejecuta en respuesta a un evento de terminal, no
  durante el renderizado.

---

## ADR-016: Estabilidad de API

**Decisión:** API pública = `include/tge/*.h`. Internals pueden cambiar entre
versiones menores. ABI no garantizada hasta 1.0.

**Racional:** El proyecto va a cambiar mucho antes de 1.0. Mantener libertad
para refactorizar es crucial.

**Consecuencias:**
- `include/tge/*.h` = semver estricto.
- `src/*` = implementación, cambia en minors.
- Backend vtable es interna.
- Headers públicos autocontenidos.

---

## ADR-017: Application como dueño de todos los sistemas

**Decisión:** `TGE_App` es el único dueño de todos los subsistemas. No hay
variables globales. Todo vive dentro de `TGE_App`:

```
TGE_App
├── Runtime
│   ├── Backend
│   ├── Canvas actual
│   ├── Canvas anterior (para diff)
│   ├── Parser
│   ├── Event Queue
│   ├── Scheduler
│   └── Timing
│
└── Engine
    ├── Scene Stack
    └── Configuración
```

**Racional:** Ownership explícito evita variables globales, facilita tests
(múltiples instancias), y deja claro qué se destruye y en qué orden.

**Consecuencias:**
- El usuario nunca accede al runtime ni al engine directamente.
- `TGE_Create()` aloca todo. `TGE_Destroy()` libera todo.
- No hay `tge_init()` / `tge_quit()` globales.
- Se pueden tener múltiples `TGE_App` para tests o herramientas.

---

## ADR-018: Módulos opcionales (tge-extra)

**Decisión:** Animation, ResourceManager, FOV, Pathfinding, Noise, Collision
System, Entity — todos como `tge-extra/*`, no en el núcleo.

**Racional:** La pregunta para cada feature es: "¿Es necesaria para que un
desarrollador cree un juego sencillo desde el día 1?" Si la respuesta es no,
va a tge-extra.

**Núcleo 1.0:**
- Runtime: backend, framebuffer, renderer, parser, event queue, scheduler,
  utf-8, timing.
- Engine: app, scene stack, canvas, math, geometry.
- Opcionales en engine: sprite, tilemap, camera.

**tge-extra:**
- `entity` (convención opcional)
- `animation`
- `resources`
- `fov`
- `pathfinding`
- `noise`
- `collision`

---

## ADR-019: API orientada a C moderno (estilo Raylib)

**Decisión:** Todas las funciones públicas siguen las convenciones de Raylib:

- Prefijo `tge_` para todo lo público.
- Nombres consistentes: `tge_dibujar_texto`, no `TGE_DrawText_EX`.
- Parámetros simples: punteros planos, sin handles opacos.
- Ownership explícito: `TGE_Create()` / `TGE_Destroy()` por cada tipo.
- Sin macros mágicas: nada de `#define TGE_INIT(...)`.
- Sin callbacks ocultos: si hay un callback, se pasa como parámetro.
- Sin código generado: nada de metaprogramación ni codegen.
- Sin variables globales: todo vive dentro de `TGE_App` (ADR-017).

**Racional:** El estilo de la API define cómo se siente el motor al usarlo.
Raylib demostró que una API consistente y predecible es más importante que
una API potente pero irregular. TGE debe sentirse como Raylib, no como un
framework empresarial.

**Consecuencias:**
- `TGE_Color`, `TGE_Vector2`, `TGE_Rect` como structs públicos.
- `tge_create_ventana()`, no `TGE_Window::Create()`.
- `TGE_App` como único contexto (no hay `tge_get_default_app()`).
- Las funciones de dibujo reciben el canvas como primer parámetro.

---

## ADR-020: Política de ownership

**Decisión:** Quien crea un objeto es responsable de destruirlo, salvo que la
documentación indique explícitamente transferencia de ownership.

**Aplicación:**
- `TGE_App`: el usuario lo crea con `TGE_Create()` y lo destruye con
  `TGE_Destroy()`. `TGE_Run()` no asume ownership.
- `TGE_Scene`: el usuario las crea y destruye. `TGE_PushScene()` no
  transfiere ownership.
- `TGE_Canvas`: pertenece a `TGE_App`. El usuario no lo crea ni destruye.
- `TGE_Backend`: interno del runtime. El usuario no lo toca.
- Recursos (sprites, tilemaps): creados por el usuario → el usuario los
  destruye. Gestionados por `tge-extra/resources` → el gestor los administra.

**Racional:** Ownership explícito elimina ambigüedad. El usuario sabe
exactamente qué debe liberar y qué no. Especialmente importante para bindings
a otros lenguajes.

**Consecuencias:**
- `TGE_Create()` / `TGE_Destroy()` por cada tipo público que el usuario crea.
- Funciones que transfieren ownership se documentan con `[takes ownership]`.
- Funciones que retienen ownership se documentan con `[borrowed]`.

---

## ADR-021: Manejo de errores

**Decisión:** Las funciones que pueden fallar retornan `bool` (éxito/fracaso).
Las funciones que retornan un puntero retornan `NULL` en caso de error. No
hay `TGE_Result`, no hay códigos de error enum. Para diagnóstico hay una
función `tge_last_error()` que retorna un string legible.

```c
bool tge_guardar_archivo(const char *path, ...);
TGE_Sprite *tge_cargar_sprite(const char *path, ...);  // NULL si falla
void tge_dibujar_texto(...);  // no falla

const char *tge_last_error(void);  // último error como string, solo debugging
```

**Racional:** Raylib demostró que `bool` + `NULL` cubre el 99% de los casos
en un motor de juegos. Los errores no son "expected" en un juego — si un
sprite no carga, la pantalla se pone negra o se cierra la aplicación. No
tiene sentido propagar códigos de error complejos. Sin embargo, para
diagnóstico es útil saber _por qué_ falló: `tge_last_error()` es una
concesión mínima que permite mostrar "No such file" sin agregar un sistema
de errores estructurado.

**Reglas de `tge_last_error()`:**
- NO se usa para control de flujo. El programa nunca debe depender del
  contenido del string para decidir su comportamiento.
- El string se sobrescribe en cada nueva operación que falla.
- El string puede ser `NULL` si no hubo error previo.
- En release builds, `tge_last_error()` puede retornar siempre `NULL` si
  se define una flag de compilación (TGE_STRIP_ERRORS).

**Excepciones:**
- Funciones que pueden fallar por entrada del usuario: retornan `bool`.
- Funciones de inicialización: retornan `bool`.
- Funciones que retornan recursos: retornan `NULL` si no existen.
- Funciones de dibujo: `void` (no fallan, escriben en memoria).
- Funciones del parser: `void` (el parser ignora bytes inválidos).

**Consecuencias:**
- Sin `TGE_Result`, sin `TGE_ErrorCode`.
- Sin `TGE_GetErrorString()` con número mágico.
- `tge_last_error()` existe solo para debugging humano.
- El motor puede imprimir advertencias a stderr en debug builds.
- Bindings a Python pueden convertir `NULL` a `None` y `bool` a `True/False`,
  y exponer `last_error()` como propiedad de solo lectura.

---

## ADR-022: const-correctness

**Decisión:** Los parámetros de solo lectura usan `const`. Los parámetros
modificables no llevan `const`.

```c
bool tge_cargar_sprite(const char *path, TGE_Sprite *out);
void tge_dibujar_texto(TGE_Canvas *canvas, const char *texto, ...);
int  tge_ancho_del_texto(const TGE_Canvas *canvas, ...);
```

- `const TGE_Canvas *` cuando solo se lee.
- `TGE_Canvas *` cuando se escribe.
- `const char *` para cadenas de entrada.
- `const TGE_Sprite *` para sprites de solo lectura.

**Racional:** Consistencia. El compilador puede optimizar mejor con `const`.
La intención queda documentada en la firma. Los bindings a Python pueden
mapear `const` como objetos de solo lectura.

**Consecuencias:**
- Todas las funciones de dibujo reciben `TGE_Canvas *` (no const, porque
  escriben).
- Todas las funciones de consulta reciben `const TGE_Canvas *`.
- Los strings de entrada son `const char *`.
- Los strings de salida son `char *` con tamaño explícito. 

---

## ADR-023: Thread Safety

**Decisión:** TGE no es thread-safe. Todas las funciones públicas deben
llamarse desde el thread del game loop. Los módulos internos pueden usar
threads (por ejemplo, un asset loader en tge-extra) pero la API pública no
sincroniza acceso.

```c
// Válido: todo desde el mismo thread
TGE_App *app = TGE_Create(80, 24, "Juego");
TGE_Run(app, init, update, draw, event);
TGE_Destroy(app);

// Invalido: llamar a TGE_PollEvent desde otro thread
// Invalido: dibujar sobre el canvas desde otro thread
```

**Racional:** La sincronización agrega complejidad, overhead y superficie de
bugs. Un motor de terminal no necesita manejar entrada desde múltiples hilos
ni renderizar en paralelo — el canvas no es lo suficientemente grande para
que valga la pena. Si un módulo externo necesita threads (carga de recursos,
pathfinding), es responsabilidad de ese módulo sincronizar su acceso a la API
pública de TGE.

**Excepciones:**
- El backend `read_input()` puede ser llamado desde un hilo de lectura si el
  runtime decide usar uno para no bloquear el game loop. En ese caso, la
  implementación interna del runtime garantiza que la event queue es la única
  estructura compartida, y usa un lock simple o lock-free queue.
- `tge_last_error()` usa thread-local storage para que cada hilo vea su
  propio último error.

**Consecuencias:**
- `TGE_App` no tiene locks. No hay `TGE_Mutex` en la API pública.
- El usuario no puede compartir `TGE_App` entre hilos sin sincronización
  externa.
- Bindings a Python: el GIL de CPython protege el acceso; no hay problema.

---

## ADR-024: Sistema de coordenadas

**Decisión:** El origen (0, 0) está en la esquina superior izquierda del
canvas/terminal. +X hacia la derecha, +Y hacia abajo. Los rectángulos usan
semiarriba cerrada (half-open): `x <= px < x + w`.

```c
typedef struct {
    int x, y;       // posición (esquina superior izquierda)
    int w, h;       // ancho y alto
} TGE_Rect;

// Semántica: el rect cubre desde (x, y) hasta (x + w - 1, y + h - 1)
// Inclusión: x <= px < x + w, y <= py < y + h
```

**Coordenadas de celdas:**
- `(0, 0)` = celda en la esquina superior izquierda.
- `(width - 1, height - 1)` = celda en la esquina inferior derecha.
- Coordenadas fuera del canvas: las funciones de dibujo las ignoran
  silenciosamente (clipping, ver ADR-026).

**Rectángulos:**
- `TGE_Rect` con `w=0` o `h=0` se considera vacío.
- `tge_rect_intersects(a, b)` usa la semántica half-open.
- `tge_rect_contains(rect, x, y)` retorna true si `x ∈ [rect.x, rect.x + rect.w)`
  e `y ∈ [rect.y, rect.y + rect.h)`.

**Racional:** Las coordenadas de terminal (top-left origin, +y down) son el
estándar en terminals, curses, y la mayoría de los motores de terminal.
Half-open es la convención más usada en geometría computacional y evita bugs
de off-by-one (un rect de 80x24 cubre exactamente las columnas 0..79 y las
filas 0..23).

**Consecuencias:**
- `tge_draw_text(canvas, text, x, y, color)` dibuja desde `(x, y)` hacia la
  derecha.
- `tge_draw_rect(canvas, x, y, w, h, color)` dibuja desde `(x, y)` hasta
  `(x + w - 1, y + h - 1)`.
- El sistema de coordenadas es el mismo para canvas, escenas y sprites.
- No existe el concepto de "coordenadas de mundo" vs "coordenadas de
  pantalla". Todo es absoluto (ADR-004: no hay transform tree).
- `tge-extra/view.h` ofrece un espacio local por juego: el playfield piensa en
  coordenadas 0-origen (0..w-1, 0..h-1) y `tge_view_translate()` las mapea a
  la superficie sumando el origen del `area` (un desplazamiento puro, no un
  árbol de transformaciones). El core sigue siendo absoluto.

---

## ADR-025: Frame event order

**Decisión:** El orden de operaciones dentro de un frame está estrictamente
definido. No hay paths alternativos.

```
1. Leer input de terminal
   read_input() → parser_feed() → parser_poll() → queue eventos

2. Procesar eventos de input (KEYDOWN, MOUSE, RESIZE, QUIT)
   event_callback(app, ev) para cada evento en la cola
   (RESIZE: realloc canvases + clear previous ANTES del draw)

3. Poll scheduler → TGE_EVENT_TIMER → queue eventos
   event_callback(app, ev) para cada timer expirado

4. Update
   scene_stack.top.update(app, dt)

5. Draw (de abajo hacia arriba, respetando opacidad)
   clear canvas → for each scene: scene.draw(canvas); if opaque: break

6. Render
   renderer_diff(current, previous, &diff)
   backend.present(&diff)
   swap pointers (current ↔ previous)

7. Procesar operaciones diferidas de scene stack
   PushScene, PopScene, ReplaceScene encolados durante el frame

8. Frame rate limiting
   sleep hasta completar el target frame time
```

**Racional:** El orden importa para consistencia. Input se procesa primero
para minimizar latencia. Los timers se procesan después para que eventos de
input tengan prioridad (consistente con ADR-008). Update ocurre después de
eventos para que el estado del juego refleje toda la entrada. Draw ocurre
después para que el canvas refleje el estado actualizado. Scene ops son
diferidas (ADR-004) para evitar cambios durante la iteración.

**Consecuencias:**
- Un `TGE_EVENT_TIMER` nunca se procesa antes que un `TGE_EVENT_KEYDOWN`
  del mismo frame.
- RESIZE realloc los canvas antes de draw, asegurando que el canvas tenga
  el tamaño correcto al dibujar.
- Scene ops no afectan el frame actual (se aplican al final).
- El usuario no necesita conocer este orden para usar TGE, pero es útil
  para debugging y para entender por qué ciertos eventos llegan juntos.

---

## ADR-026: Política de clipping

**Decisión:** Todas las funciones de dibujo hacen clipping implícito. Nunca
fallan. Nunca escriben fuera del canvas. Coordenadas parcialmente fuera del
canvas dibujan solo la porción visible.

```c
// Siempre seguro, aunque x,y,w,h estén parcial o totalmente fuera:
tge_draw_rect(canvas, -5, -5, 100, 100, TGE_COLOR_RED);
// Dibuja solo la porción visible dentro de [0, 0, canvas.w, canvas.h)
```

**Racional:** En un motor de juegos, las entidades pueden estar parcialmente
fuera de la pantalla (bordes, transiciones, scrolling). Forzar al usuario a
calcular clipping manualmente agrega bugs y código repetitivo. El clipping
implícito es la política de Raylib, SDL (en su renderer) y la mayoría de los
motores modernos.

**Reglas:**
- Todas las funciones `tge_draw_*` recortan su salida al rectángulo
  `[0, 0, canvas.w, canvas.h)`.
- Ninguna función de dibujo retorna error por coordenadas inválidas.
- `tge_set_cell(canvas, x, y, ...)` con coordenadas fuera del canvas es un
  no-op (no es error).
- Si un span de `tge_draw_text` o `tge_draw_rect` está parcialmente fuera,
  se dibuja la porción visible sin modificar el canvas fuera del área
  visible.
- `tge_clear(canvas, ...)` siempre llena el canvas completo — no aplica
  clipping (es la única excepción).

**Consecuencias:**
- El usuario no necesita verificar límites antes de dibujar.
- El código de juego es más simple: las entidades pueden estar en coordenadas
  negativas sin causar crashes.
- El renderer puede recibir un canvas con cambios fuera del área visible
  (es responsabilidad del renderer no incluir esos cambios en el diff).

## ADR-027: TGE_Game como capa adaptadora opcional

**Decisión:** TGE_Scene sigue siendo la abstracción primitiva de escena.
TGE_Game (tge-extra/game) es una capa adaptadora opcional para aplicaciones
interactivas con estado ("juego", editor, demo, visualizador). El adapter no
debe filtrarse al core.

```c
typedef struct {
    TGE_GameContext ctx;   /* primer miembro: scene->userdata == &game->ctx */
    SnakeWorld world;
    SnakeRenderer renderer;
} SnakeGame;

static void game_update(TGE_GameContext *ctx, float dt)
{
    SnakeGame *game = (SnakeGame *)tge_game_instance(ctx);
    world_update(&game->world, dt);
}
```

**Racional:** El pegamento entre los callbacks de escena y el estado del juego
se repetía en todos los juegos de ejemplo: el cast `scene->userdata` y el
global `g_app` (necesario para `TGE_Quit`/`TGE_PopScene`). Esa capa existe de
facto, solo que implícita y repetida. Hacerla explícita cambia la unidad de
abstracción, no solo el código: el engine habla de escenas, el juego habla de
`TGE_GameContext`, y nada mezcla responsabilidades.

**Reglas:**
- `TGE_GameContext` es el primer miembro del struct del juego (offset 0), para
  que `scene->userdata == &game->ctx`. Romper esa regla falla silenciosamente:
  es la única regla que no se negocia.
- El adapter es opt-in: las escenas que no son juegos (título, overlays,
  menús) y los juegos que necesitan control de escena usan `tge_scene_create`
  directamente.
- Los callbacks del juego reciben `TGE_GameContext *` y nunca ven la scene.
- El adapter no administra memoria propia: todo pasa por `tge_scene_create`.

**Consecuencias:**
- `g_app` se reduce a los sitios de creación de escenas (init_app / título);
  los callbacks del juego lo pierden.
- Un juego puede correr desde otro sistema (test runner, replay, networking)
  sin depender del concepto Scene.
- Si el módulo no convence, borrar `tge-extra/game` no afecta al resto del
  motor.

*Fin del ADR. Decisiones vinculantes hasta que un nuevo ADR las modifique.
Ninguna decisión es inmutable, pero cambiar un ADR requiere justificación
explícita.*

## ADR-028: Colores default de la terminal

**Decisión:** TGE tiene un modo de color `TGE_COLOR_MODE_DEFAULT` (macros
`TGE_COLOR_DEFAULT` para fg y bg). Una celda en modo default no especifica
color: el backend ANSI emite SGR `39` (foreground) y `49` (background) y
restaura los defaults de la terminal, en vez de forzar un color de paleta o
RGB. El canvas inicial (current y previous) se limpia a `TGE_COLOR_DEFAULT`
en lugar de negro.

**Motivo:** el engine no impone estética. La terminal ya tiene un tema del
usuario (paleta, fondo transparente, wallpaper); TGE debe dibujar encima de
ese fondo y respetarlo por defecto, no taparlo con negro. Con la paleta
indexed o RGB, TGE tenía una opinión fuerte ("el fondo de una app TGE es
negro") que chocaba con la configuración del usuario.

`TGE_COLOR_DEFAULT` **no significa transparente ni alpha blending**: no deja
ver lo que había dibujado debajo (el canvas sigue siendo una composición
propia). Solo significa "no emitir un color explícito y restaurar el default
de la terminal".

**Consecuencias:**

- `TGE_ColorMode` gana un tercer valor; la comparación de celdas del
  renderer trata el modo default explícitamente (un default es igual a otro
  default, sin depender del contenido del union).
- La emisión ANSI usa `39`/`49`; el estado de estilo inicial se asume en
  default (coincide con el terminal tras el reset inicial).
- El clear inicial de `app.c` usa default: las aplicaciones heredan el tema
  del usuario por defecto.
- Los ejemplos/juegos mantienen sus colores explícitos donde el negro (u
  otro bg) es parte de la intención visual; migrarlos a default es una
  decisión de cada renderer, no de esta pasada.
- Aditivo en API pública pre-1.0 (ver `docs/API_STABILITY.md`).

*Fin del ADR. Decisiones vinculantes hasta que un nuevo ADR las modifique.
Ninguna decisión es inmutable, pero cambiar un ADR requiere justificación
explícita.*

---

## ADR-029: `TGE_Step` como unidad de ejecución pública

**Decisión:** Se expone `void TGE_Step(TGE_App *app)`, que ejecuta
exactamente una iteración del pipeline normal de frame (ADR-025: eventos →
update → draw → render/present → scene ops → frame limiting, en ese orden y
sin omitir el limiter). `TGE_Run` delega en `TGE_Step`:
`TGE_Run(app, ...)` ≡ `while (!quit) TGE_Step(app)`.

**Racional:** es el primer punto de integración para alojar TGE desde otro
runtime (C++, Python, loop propio, varias apps). La iteración ya existía
internamente (`tge_app_frame`); `Step` la hace pública sin crear un segundo
modelo de ejecución: el pipeline es único y `Run` sigue siendo la API
principal. Evita el antipatrón de dos loops con semánticas que divergen con
el tiempo.

**Consecuencias:**
- El estado de quit vive en la app y se consulta por la API existente; `Step`
  no lo devuelve como retorno.
- Un host loop (`while (!done) { TGE_Step(app); host_work(); }`) es
  observacionalmente equivalente a `TGE_Run`: mismo orden de eventos, scene
  ops al final del frame, frame limiter en la misma posición.
- Test de equivalencia en `tests/test_app.c` (`run_equivalent_to_step_loop`,
  `step_runs_full_pipeline`).

*Ver `docs/HOSTING_API.md` §3.1.*

---

## ADR-030: `userdata` como slot del host en `TGE_App`

**Decisión:** `TGE_App` gana un slot opaco `void *userdata`, accesible por
`TGE_SetUserData(app, ptr)` / `void *TGE_GetUserData(app)`. TGE no lo
interpreta ni lo posee; los callbacks de app existentes
(`init`/`update`/`draw`/`event`) pueden leerlo para reencontrar el objeto de
alto nivel del host.

**Racional:** los callbacks de `TGE_Run` reciben solo `TGE_App *`, un tipo
opaco; sin un slot, un binding debe mantener un registro global
`TGE_App * → wrapper`. El slot elimina ese registro sin cambiar la familia de
callbacks ni la ABI.

**Consecuencias:**
- No hay una segunda familia de callbacks ni `TGE_RunUser`; la API existente
  permanece intacta.
- Leíble/escribible desde el thread del game loop (ADR-023).
- La identidad y el lifetime del objeto del host quedan del lado del host
  (ADR-032).
- Test en `tests/test_app.c` (`userdata_slot_roundtrip`,
  `userdata_visible_from_callback`).

*Ver `docs/HOSTING_API.md` §3.2.*

---

## ADR-031: `tge_vprintf` como API de compatibilidad C para bindings

**Decisión:** Se expone `void tge_vprintf(TGE_Canvas *, int x, int y,
TGE_Color fg, TGE_Color bg, const char *fmt, va_list ap)`, con el mismo
comportamiento que `tge_printf` (buffer de pila fijo `TGE_PRINTF_BUF`, sin
alloc, truncamiento estilo `snprintf`). `tge_printf` delega en
`tge_vprintf`. El header `tge_canvas.h` incluye `<stdarg.h>` (autocontenido).

**Racional:** los bindings (cffi/ctypes) no cruzan varargs de forma portable
ni fabrican un `va_list`. `tge_vprintf` es una API de compatibilidad C: deja
que código C intermedio o trampolines del binding deleguen en el mismo
formateador interno sin atravesar una frontera C varargs. El binding de alto
nivel mantiene su propia adaptación de formato (strings ya formateados o una
API propia que baje a la API C no-varargs); no es una API de formateo de
Python.

**Consecuencias:**
- `tge_printf` y `tge_vprintf` producen salida idéntica para los mismos args.
- `check_no_malloc` sigue pasando: `tge_vprintf` no aloca.
- `make check_headers` verifica que `tge_canvas.h` siga autocontenido.
- Tests en `tests/test_canvas.c` (`vprintf_formats_and_draws`,
  `printf_null_safety` ampliado).

*Ver `docs/HOSTING_API.md` §3.3.*

---

## ADR-032: Frontera de hosting — ownership del engine, identidad del host

**Decisión:** Se adopta formalmente el patrón de instancia host-construida y
la regla de dos owners. El engine administra el ciclo de vida de sus
estructuras C (`TGE_App`, `TGE_Canvas`, escenas, recursos) y nunca construye
tipos del host. El host administra la identidad y el ciclo de vida de su
objeto de alto nivel. TGE no necesita saber qué hay detrás de un `void *`.
El offset-0 de `TGE_GameContext` es una peculiaridad de representación
C/C++ (herencia pública + `reinterpret_cast`), no un requisito de
integración para Python; el binding mantiene la identidad
`TGE_GameContext * → objeto` por su cuenta.

**Racional:** el core sigue siendo C con structs, callbacks y ownership
explícito (ADR-020). Meter C++/Python en el core para facilitar un binding
sería prematuro: lo que hace falta es una frontera de hosting estable, no una
API de bindings. La separación de identidad/lifetime evita registros globales
y destructores cruzados.

**Consecuencias:**
- El ownership de `TGE_Scene` se documenta por comportamiento observable:
  Pop/Replace encolan; la destrucción ocurre al final del frame en
  `tge_app_process_scene_ops`; `TGE_Destroy` destruye las escenas del stack y
  descarta ops pendientes; escenas manuales (`destroy == NULL`) nunca se
  liberan.
- `TGE_Backend` público, header C++, eventos tipados, errores estructurados y
  globals de Unicode quedan diferidos con gatillos explícitos.
- El contrato completo vive en `docs/HOSTING_API.md` (análisis, reglas de
  ownership, patrones C++/Python, verificación).

*Ver `docs/HOSTING_API.md` §3.4 y §4.*

---

## ADR-033: Módulos de composición — TileMap, Actor, Playfield

**Decisión:** Se incorporan a `tge-extra` tres módulos de composición para
juegos por grilla, gatillados por su consumidor real (Pac-Man) tras cumplir
las reglas de evidencia del plan:

- `TGE_TileMap` (`tge-extra/tilemap.h/.c`): matriz fija sin malloc de celdas
  lógicas (max 32×32) que guarda un byte de **rol** por celda, más un
  `TGE_TileSet` (rol → `TGE_Tile { sprite, fg, bg }`) que define cómo se
  representa. Es deliberadamente tonto: no conoce paredes, colisiones ni
  reglas; el juego define su enum de roles, rellena la paleta y lee roles
  con `tge_tilemap_get` para su lógica (`role == MI_PARED` es sólido).
- `TGE_Actor` (`tge-extra/actor.h/.c`): `{ TGE_Vec2i position; const
  TGE_Sprite *sprite; TGE_Color fg, bg; }` más `tge_actor_draw(&grid_view,
  &view, &actor)` (= `tge_grid_view_put_local`). No es ECS: es data plana que
  los juegos embeben y extienden con sus propios campos (dirección, modo,
  objetivos); la animación es swapear `sprite`.
- `TGE_Playfield` (`tge-extra/playfield.h/.c`): encapsula la composición que
  Snake y Breakout repetían a mano — `TGE_View` + `TGE_GridView` +
  `TGE_GridLayout` — con `init`/`attach`/`sync`/`draw_border`. Es solo
  infraestructura: no cámara, no entidades, no reglas; el callback de resize
  del juego sigue decidiendo cómo reaccionar a un `TGE_ViewUpdate`.

**Motivo:** la regla "el módulo se implementa cuando un juego lo pide, no
antes" se cumplió en cadena. Snake y Breakout mostraron el patrón
View+GridView+Layout; Pac-Man fue el tercer juego en repetirlo (la regla de
los 3 juegos del plan), así que se encapsuló como `TGE_Playfield`. Pac-Man
también exigió el TileMap (laberinto data-driven 28×31 con roles
pared/puerta/pellet/power/túnel) y el Actor (sus 4 fantasmas son actores con
IA chase/scatter por distancia²). Se mantuvo el alcance mínimo: nada de
pathfinding, animación, cámaras ni reglas de maze en los módulos.

**Consecuencias:**
- `tge-extra/vec2i` gana `tge_vec2i_dist2` (distancia²), el único helper nuevo
  que la IA de Pac-Man necesitó (la raíz cuadrada es un paso monótono que
  nunca se necesita para comparar vecinos).
- Pac-Man (`examples/games/11_pacman.c`) es el consumidor real y el
  siguiente hito de validación del hosting: funciona primero en TGE nativo,
  y luego se aloja externamente para validar `TGE_Step` + `userdata`
  (ADR-029/030) en una app de verdad.
- El laberinto se valida en carga (cada fila debe tener MAZE_W caracteres) y
  por tests (test_pacman: roles, wrap del túnel, bloqueo por puerta, IA,
  frightened, vidas, win).

*Fin del ADR. Decisiones vinculantes hasta que un nuevo ADR las modifique.
Ninguna decisión es inmutable, pero cambiar un ADR requiere justificación
explícita.*
