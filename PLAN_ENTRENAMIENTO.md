# Plan de Entrenamiento: Diseño de un Motor de Videojuegos de Terminal en C

> TGE (Terminal Game Engine) — "El Raylib de la terminal."

---

## Filosofía del proyecto

TGE es una biblioteca escrita en C para crear videojuegos de terminal modernos. Su objetivo es ofrecer una experiencia similar a Raylib: una API pequeña, clara y multiplataforma, construida sobre un runtime de terminal eficiente con render diferencial, soporte Unicode y backends intercambiables. El mismo núcleo está pensado para exponer bindings de alto rendimiento a Python y otros lenguajes.

**No es:**
- Un framework TUI (como Textual o ncurses)
- Un clon de Love2D o Godot
- Una librería de widgets

**Es:**
- Un motor donde programar Pong, Snake, Tetris, Space Invaders, Pac-Man y roguelikes sea tan simple como con un motor gráfico moderno.

---

## Análisis de Proyectos

### 1. Raylib — La filosofía correcta

**¿Qué problemas resuelve?**
Hacer programación de videojuegos accesible y divertida, eliminando la complejidad de OpenGL y la gestión de ventanas. Sin dependencias externas, todo incluido.

**Organización del código:**
7 módulos principales en un solo header público (`raylib.h`):
- `rcore`: ventana, contexto gráfico, input
- `rlgl`: wrapper de OpenGL
- `rtextures`: texturas/imágenes
- `rtext`: fuentes y texto
- `rshapes`: figuras 2D
- `rmodels`: modelos 3D
- `raudio`: audio

Módulos adicionales como header-only: `raymath`, `rcamera`, `rgestures`, `raygui`.

**Decisiones que conviene adoptar:**
- API plana y consistente (sin jerarquías de objetos)
- Convención PascalCase para funciones, TitleCase para tipos
- Inicializar todas las variables
- Cero dependencias externas obligatorias
- Game loop explícito pero controlado por el motor
- Código C99, sin magia ni macros complejas
- Structs públicos (como Raylib) en lugar de setters/getters
- Filosofía: "muy fácil de aprender, muy difícil de usar mal"

**Decisiones que NO conviene copiar:**
- Single header monolítico. Mejor tener headers separables para bindings (`tge.h` incluye todo, pero cada subsistema tiene su propio header).
- Variables globales internas. Usar un contexto `TGE_App*` explícito.
- No copiar la implementación, copiar la filosofía de diseño.

**Adaptación a terminal:**
- `rcore` → `tge_core` (gestión de terminal, raw mode, resize)
- `rlgl` → `tge_backend` (renderer diferencial ANSI/Windows)
- `rtextures` → `tge_sprite` + `tge_tilemap`
- `rtext` → `tge_text` (UTF-8, ancho de caracteres)
- `rshapes` → `tge_canvas` (dibujo de líneas, rectángulos en terminal)

---

### 2. notcurses — El backend moderno

**¿Qué problemas resuelve?**
Renderizado moderno en terminal con soporte de color RGB, Unicode, multimedia, imágenes, video. Abandona la API curses legacy.

**Arquitectura del renderer:**
- Render diferencial: compara el framebuffer actual con el anterior y solo envía las diferencias
- Doble buffer: `ncplane` como lienzos virtuales, `notcurses_render()` hace el diff
- Sistema de planos apilados (z-order), cada plano con su propio framebuffer
- La detección de cambios es por celda (carácter + estilo + color)

**Manejo del cursor:**
- Cursor invisible durante el renderizado
- Posicionamiento vía secuencias CSI (`\x1b[row;colH`)
- Se mueve solo cuando es necesario (no envía secuencias redundantes)

**Colores RGB:**
- TrueColor vía `\x1b[38;2;R;G;Bm` (foreground) y `\x1b[48;2;R;G;Bm` (background)
- Cuantización automática si el terminal no soporta 24-bit
- Palette de 256 colores como fallback

**Unicode/UTF-8:**
- Basado en EGC (Extended Grapheme Cluster) de Unicode
- Soporte de quadrants, sextants, octants para sub-pixel rendering
- Caracteres de bloque █ ▓ ▒ ░ para gradientes

**Técnicas para reutilizar:**
- Framebuffer diferencial por celda
- Sistema de planos (z-order) con daño parcial (dirty rects)
- Color RGB con degradado automático
- El renderer debe ser un backend reemplazable

---

### 3. libtickit — Eventos, no input

**¿Qué problemas resuelve?**
Manejo moderno de eventos de terminal.

**Event Loop:**
- Basado en `poll()` / `select()`
- Despacho de eventos a través de cola de eventos
- Separar la lectura de input del procesamiento de eventos

**Lección clave: No debería existir "Input" como concepto separado.**

Todo es un evento:
- KeyPress, KeyRelease
- MouseMove, MouseClick, MouseWheel
- Resize
- TimerExpired
- Collision
- AnimationFinished
- Quit

**Arquitectura:**
```
Backend
    ↓
Parser ANSI (state machine)
    ↓
Input Event
    ↓
Event Queue
    ↓
Scene
```

**Conceptos para adoptar:**
- Event queue como SDL: `TGE_PollEvent(&event)`
- Todo el motor se comunica mediante eventos
- El backend jamás conoce el juego

---

### 4. libvterm — Parser de bytes, no de ANSI

**¿Qué problemas resuelve?**
Ser un emulador de terminal embebible.

**Lección clave: No hacer un parser ANSI. Hacer un parser de bytes.**

```
Byte
  ↓
State Machine
  ↓
Token
  ↓
Event
```

Porque mañana aparece un protocolo nuevo (Kitty, OSC52, mouse, etc.) y solo hay que agregar estados.

**Generador ANSI:**
```
Renderer
  ↓
Draw Commands
  ↓
ANSI Generator
  ↓
Terminal
```

Nunca mezclar render con ANSI. El generador ANSI es una capa aparte e intercambiable.

**Partes necesarias para backend propio:**
- State machine para parsear bytes de entrada (teclas, mouse, resize)
- Generador de secuencias ANSI para output (independiente del renderer)
- Manejo UTF-8 (encoding/decoding) como módulo propio
- Tabla de ancho de caracteres (wcwidth)

---

### 5. Textual — Scene Tree, no Widget Tree

**¿Qué problemas resuelve?**
Framework de aplicaciones TUI con arquitectura moderna.

**Lección clave: No tener Widget Tree. Tener Scene Tree.**

```
✅ Scene Tree (Godot-like)
Scene
  ├── Player
  │   └── Weapon
  └── Enemy
```

```
❌ Widget Tree (Textual-like)
Widget
  ├── Container
  │   ├── Panel
  │   └── Button
```

**Event Bubbling: sí, pero para escenas, no para widgets.**

```
Player → Scene → Game → Application
```

Si Player no consume ESC, la escena puede consumirlo. Si tampoco, Game. Después App. Exactamente como Godot.

**Layout: eliminado.** No existe layout en un motor de juegos. Las posiciones son absolutas: `x`, `y`, `z`. Nada de CSS.

---

### 6. FTXUI — El patrón Scene → Canvas → Renderer → Backend

**¿Qué problemas resuelve?**
Librería C++ funcional para interfaces de terminal. Separación clara en Screen + DOM + Component.

**Arquitectura limpia de FTXUI:**
```
Component → DOM → Screen
```

**Adaptado a videojuegos:**
```
Scene → Canvas → Renderer → Backend
```

Es exactamente el mismo patrón, pero orientado a juegos en lugar de interfaces.

**Ideas para adoptar:**
- El Canvas de FTXUI (dibujo con braille, líneas, rectángulos)
- Separación estricta Screen/DOM/Component → Scene/Canvas/Renderer

---

### 7. libtcod — El concepto de Tile

**¿Qué problemas resuelve?**
Herramientas específicas para roguelikes.

**Lección clave: No existe "pixel". Existe "Tile".**

Eso cambia muchísimo el diseño. La terminal ya es una cuadrícula de caracteres; cada celda es un tile.

**Módulos opcionales (no en el núcleo):**
```
tge (runtime + engine)
  └── tge-extra
       └── tge-roguelike (FOV, pathfinding, noise)
```

FOV, pathfinding y noise no pertenecen al motor mínimo. Son módulos opt-in.

---

### 8. SDL2 — Backend intercambiable

**¿Qué problemas resuelve?**
Capa de abstracción de hardware: ventanas, input, audio, timing. Multiplataforma.

**Lección clave: Todo debe estar unificado a través de una interfaz.**

```
Backend ANSI → implementa TGE_Backend
Backend Windows Console → implementa TGE_Backend
Backend Notcurses (futuro) → implementa TGE_Backend
```

El resto del motor jamás sabe cuál usa.

**Timing:**
- No exponer `tge_sleep()`. Los motores modernos nunca hacen eso.
- Exponer `TGE_GetTime()` y `TGE_GetDeltaTime()`
- El loop lo controla el motor

---

### 9. Godot — Scene Tree + Events

**¿Qué problemas resuelve?**
Motor de juegos completo con editor integrado.

**Conceptos a adoptar:**
- **Scene Tree**: escenas con entidades dentro. La escena es una unidad lógica.
- **Events (no Signals)**: todo el motor habla mediante eventos genéricos.
  - `Collision`, `EnemyKilled`, `ButtonPressed`, `TimerExpired`
- **Resource system**: recursos gestionados centralizadamente.

**No copiar:**
- La complejidad del sistema de herencia de nodos
- Entity Tree jerárquico. Preferir lista plana de entidades en Scene.
- El editor visual
- GDScript

---

### 10. Love2D — La simplicidad como objetivo

**¿Qué problemas resuelve?**
Framework 2D para juegos en Lua con la API más simple posible.

**Filosofía:**
- `love.load()` → `love.update(dt)` → `love.draw()`
- API coherente: `love.graphics.circle(...)`, `love.graphics.rectangle(...)`
- No hay clases ni herencia obligatoria

**Para Python bindings:**
```python
class PongScene(tge.Scene):
    def init(self): ...
    def update(self, dt): ...
    def draw(self, canvas): ...
    def on_event(self, event): ...
```

**Para C:**
```c
typedef struct {
    void (*init)(TGE_Scene*);
    void (*update)(TGE_Scene*, float dt);
    void (*draw)(TGE_Scene*, TGE_Canvas*);
    bool (*event)(TGE_Scene*, TGE_Event*);
} TGE_SceneCallbacks;
```

La inspiración correcta es Love2D. El objetivo: que alguien pueda escribir Pong en menos de 100 líneas.

---

## Arquitectura

### Capas

```
┌─────────────────────────────────────────────────────────┐
│                      GAME LAYER                          │
│            Pong, Snake, Tetris, Roguelikes...             │
│            (user code, desconoce el backend)              │
├─────────────────────────────────────────────────────────┤
│                                                           │
│                    tge-engine                             │
│                                                           │
│  Scene  │  Entity  │  Canvas  │  Camera  │  Sprite       │
│  Collision  │  Tilemap  │  Geometry  │  Scheduler        │
│                                                           │
│  NO sabe nada de:                                         │
│    terminal, ANSI, framebuffer, raw mode, Unicode         │
│                                                           │
├─────────────────────────────────────────────────────────┤
│                                                           │
│                    tge-runtime                             │
│                                                           │
│  Terminal  │  Framebuffer  │  Backend I/F  │  Event Queue │
│  Unicode   │  Timing       │  Parser (bytes→event)        │
│                                                           │
│  NO sabe nada de:                                         │
│    escenas, entidades, sprites, colisiones, Pong          │
│                                                           │
├─────────────────────────────────────────────────────────┤
│                                                           │
│                    BACKEND LAYER                           │
│                                                           │
│  ┌──────────────────┐  ┌──────────────────┐              │
│  │  ANSI Backend    │  │  WinConsole      │              │
│  │  (Linux/macOS)   │  │  Backend         │              │
│  └──────────────────┘  └──────────────────┘              │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

**Principio clave:** Cada capa debería poder reemplazarse sin modificar las demás.

El runtime ni siquiera sabe que existe una "Scene". Solo conoce terminal, framebuffer, eventos, temporizadores, canvas, colores.

---

### Flujo de datos

```
Terminal (bytes crudos)
    ↓
Backend.read_input()
    ↓
Parser (state machine: bytes → tokens → eventos)
    ↓
Event Queue
    ↓
Engine (Scene.update(), Scene.event())
    ↓
Canvas (dibujo diferencial)
    ↓
Renderer (compara framebuffers, genera solo diferencias)
    ↓
ANSI Generator (convierte comandos a secuencias)
    ↓
Terminal (stdout)
```

---

### El Scheduler

No todo ocurre en Update. Ejemplo: "dentro de 2 segundos explota la bomba".

```c
// En lugar de:
timer += dt;
if (timer >= 2.0f) { explode(); }

// Mejor:
TGE_CallLater(app, 2.0f, explode_cb, NULL);
TGE_CallEvery(app, 0.5f, tick_cb, NULL);  // repetitivo
```

El scheduler forma parte del event loop. Es más declarativo y elimina el boilerplate de temporizadores manuales.

---

### Event Queue (modelo SDL)

```c
TGE_Event event;
while (TGE_PollEvent(app, &event)) {
    switch (event.type) {
        case TGE_EVENT_KEYDOWN: ...
        case TGE_EVENT_MOUSEMOVE: ...
        case TGE_EVENT_RESIZE: ...
        case TGE_EVENT_QUIT: ...
        case TGE_EVENT_TIMER: ...
    }
}
```

Todo entra por el mismo lugar: teclado, mouse, resize, timers, colisiones, animaciones.

---

### Canvas como centro del dibujo

El usuario SIEMPRE dibuja sobre un canvas. Nunca sobre el renderer.

```c
void tge_DrawText(TGE_Canvas *canvas, const char *text, int x, int y, TGE_Color fg);
void tge_DrawSprite(TGE_Canvas *canvas, TGE_Sprite *sprite, int x, int y);
void tge_DrawRect(TGE_Canvas *canvas, int x, int y, int w, int h, TGE_Color fg, TGE_Color bg);
void tge_DrawLine(TGE_Canvas *canvas, int x1, int y1, int x2, int y2, uint32_t ch);
void tge_DrawCircle(TGE_Canvas *canvas, int cx, int cy, int r, uint32_t ch);
```

El renderer es completamente invisible para el usuario.

---

### Entidades tontas

```c
typedef struct {
    TGE_Vector2 position;
    TGE_Sprite  *sprite;
    bool         active;
    void        *userdata;
} TGE_Entity;
```

Sin callbacks por entidad. Toda la lógica vive en la escena:

```c
void my_scene_update(TGE_Scene *scene, float dt) {
    for (int i = 0; i < scene->entity_count; i++) {
        TGE_Entity *e = &scene->entities[i];
        // lógica aquí
    }
}
```

---

### Scene como unidad lógica

```
MainMenu
  ├── Button("Play")
  ├── Button("Options")
  └── Button("Quit")

Game
  ├── Player
  ├── Enemies[]
  ├── HUD
  └── Tilemap

GameOver
  ├── Text("Score: 1000")
  └── Button("Restart")
```

---

### Backend intercambiable

```c
// tge_backend.h
typedef struct {
    bool (*init)(int w, int h);
    void (*term)(void);
    int  (*width)(void);
    int  (*height)(void);
    void (*present)(TGE_Cell *framebuffer, int w, int h);
    int  (*read_input)(char *buf, int bufsize);
    uint64_t (*ticks)(void);
} TGE_Backend;
```

El motor elige el backend en tiempo de compilación o inicialización:

```c
TGE_App *app = TGE_Create(80, 24, "My Game");  // detecta automáticamente
```

---

### Render Diferencial

- Mantener dos framebuffers: `current` y `previous`
- Cada celda: `{ ch (uint32_t), fg (uint32_t RGB), bg (uint32_t RGB), style (uint8_t) }`
- Comparar `current` vs `previous`, enviar solo diferencias
- Agrupar celdas adyacentes con mismo estilo en una sola secuencia
- Dirty rects para optimización

---

### Unicode / UTF-8

Módulo propio (no escondido en el backend), porque lo usan Input, Renderer, Text y Canvas.

- Almacenar caracteres como codepoints Unicode (uint32_t)
- Convertir a UTF-8 para output (hasta 4 bytes)
- `wcwidth()` para ancho de columna
- Caracteres de ancho doble (CJK, emojis)
- Bloque semi-gráficos: █ ▓ ▒ ░ ▄ ▀
- Quadrants para sub-pixel rendering

---

## API Pública en C

### Convenciones

- Prefijo `TGE_` para tipos, `tge_` para funciones
- PascalCase para funciones y tipos
- snake_case para campos de struct
- Structs públicos (como Raylib), sin setters/getters innecesarios
- Tipos básicos desde el día 1: `TGE_Vector2`, `TGE_Rect`, `TGE_Color`, `TGE_Size`
- Sin estado global

### App

```c
TGE_App* tge_Create(int width, int height, const char *title);
void     tge_Run(TGE_App *app);
void     tge_Close(TGE_App *app);
float    tge_GetDeltaTime(TGE_App *app);
uint64_t tge_GetTicks(TGE_App *app);
void     tge_SetFPS(TGE_App *app, int fps);
void     tge_SetTitle(TGE_App *app, const char *title);
```

`tge_Create` + `tge_Run`. Nada más.

### Event Queue

```c
typedef enum {
    TGE_EVENT_NONE,
    TGE_EVENT_KEYDOWN,   TGE_EVENT_KEYUP,
    TGE_EVENT_MOUSEMOVE, TGE_EVENT_MOUSEDOWN, TGE_EVENT_MOUSEUP,
    TGE_EVENT_RESIZE,
    TGE_EVENT_QUIT,
    TGE_EVENT_TIMER,
} TGE_EventType;

typedef struct {
    TGE_EventType type;
    union {
        struct { TGE_Key key; } key;
        struct { int x, y, button; } mouse;
        struct { int width, height; } resize;
        struct { int id; } timer;
    };
} TGE_Event;

bool tge_PollEvent(TGE_App *app, TGE_Event *event);
void tge_PushEvent(TGE_App *app, TGE_Event *event);  // para eventos personalizados
```

### Scene

```c
typedef struct TGE_Scene TGE_Scene;

typedef struct {
    void (*init)(TGE_Scene *scene);
    void (*update)(TGE_Scene *scene, float dt);
    void (*draw)(TGE_Scene *scene, TGE_Canvas *canvas);
    bool (*event)(TGE_Scene *scene, TGE_Event *event);
    void (*cleanup)(TGE_Scene *scene);
} TGE_SceneCallbacks;

TGE_Scene* tge_CreateScene(TGE_App *app, const char *name, TGE_SceneCallbacks *cb);
void       tge_SetScene(TGE_App *app, TGE_Scene *scene);

// Entidades (lista plana dentro de la escena)
void        tge_SceneAddEntity(TGE_Scene *scene, TGE_Entity *entity);
TGE_Entity* tge_CreateEntity(void);
void        tge_DestroyEntity(TGE_Entity *entity);
```

### Canvas

```c
TGE_Canvas* tge_GetCanvas(TGE_Scene *scene);  // scene.canvas

void tge_Clear(TGE_Canvas *canvas);
void tge_SetCell(TGE_Canvas *canvas, int x, int y, uint32_t ch, TGE_Color fg, TGE_Color bg);
void tge_DrawText(TGE_Canvas *canvas, const char *text, int x, int y, TGE_Color fg);
void tge_DrawRect(TGE_Canvas *canvas, int x, int y, int w, int h, TGE_Color fg, TGE_Color bg);
void tge_DrawLine(TGE_Canvas *canvas, int x1, int y1, int x2, int y2, uint32_t ch);
void tge_DrawCircle(TGE_Canvas *canvas, int cx, int cy, int r, uint32_t ch);
void tge_Blit(TGE_Canvas *dst, int dx, int dy, TGE_Canvas *src, int sx, int sy, int w, int h);
```

### Scheduler (en lugar de timers manuales)

```c
typedef void (*TGE_Callback)(void *userdata);

int tge_CallLater(TGE_App *app, float delay_sec, TGE_Callback cb, void *userdata);
int tge_CallEvery(TGE_App *app, float interval_sec, TGE_Callback cb, void *userdata);
void tge_CancelCall(TGE_App *app, int id);
```

Devuelve un ID que permite cancelar. Mucho más cómodo que CreateTimer/DestroyTimer.

### Geometry (en lugar de sistema de colisiones)

```c
typedef struct { float x, y; }       TGE_Vector2;
typedef struct { float x, y, w, h; } TGE_Rect;
typedef struct { uint8_t r, g, b; }  TGE_Color;

bool tge_CheckCollisionRect(TGE_Rect a, TGE_Rect b);
bool tge_CheckCollisionCircle(TGE_Vector2 center, float r, TGE_Rect rect);
bool tge_CheckCollisionLine(TGE_Vector2 a1, TGE_Vector2 a2, TGE_Vector2 b1, TGE_Vector2 b2);
```

### Sprite

```c
typedef struct {
    uint32_t ch;
    TGE_Color fg;
    TGE_Color bg;
    int width;   // en tiles
    int height;  // en tiles
} TGE_Sprite;

TGE_Sprite tge_Sprite(uint32_t ch, TGE_Color fg, TGE_Color bg);
```

### Tilemap

```c
typedef struct {
    uint32_t  ch;
    TGE_Color fg;
    TGE_Color bg;
    bool      solid;
} TGE_Tile;

typedef struct {
    int      cols;
    int      rows;
    TGE_Tile *tiles;
} TGE_Tilemap;

TGE_Tilemap* tge_CreateTilemap(int cols, int rows);
void         tge_DestroyTilemap(TGE_Tilemap *tm);
void         tge_SetTile(TGE_Tilemap *tm, int col, int row, TGE_Tile tile);
TGE_Tile     tge_GetTile(TGE_Tilemap *tm, int col, int row);
```

### Camera

```c
typedef struct {
    float x, y;      // posición de la cámara (en tiles)
    int   width;     // viewport width
    int   height;    // viewport height
} TGE_Camera;

void tge_CameraFollow(TGE_Camera *cam, TGE_Vector2 target, float lerp);
void tge_CameraApply(TGE_Camera *cam, int *x, int *y);  // mundo → pantalla
```

### Math básico (desde el día 1)

```c
TGE_Vector2 tge_Vector2(float x, float y);
TGE_Vector2 tge_Vector2Add(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_Vector2Sub(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_Vector2Scale(TGE_Vector2 v, float s);
float       tge_Vector2Length(TGE_Vector2 v);
float       tge_Vector2Distance(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_Vector2Normalize(TGE_Vector2 v);

TGE_Color tge_Color(uint8_t r, uint8_t g, uint8_t b);

#define TGE_WHITE   ((TGE_Color){255, 255, 255})
#define TGE_BLACK   ((TGE_Color){0, 0, 0})
#define TGE_RED     ((TGE_Color){255, 0, 0})
#define TGE_GREEN   ((TGE_Color){0, 255, 0})
#define TGE_BLUE    ((TGE_Color){0, 0, 255})
#define TGE_YELLOW  ((TGE_Color){255, 255, 0})
```

---

## Binding Futuro para Python

```python
import tge

class PongScene(tge.Scene):
    def init(self):
        self.ball = tge.Entity()
        self.ball.position = (40, 12)
        self.ball.sprite = tge.Sprite('O', fg=tge.WHITE)
        self.ball.velocity = tge.Vector2(1, 0.5)

        self.paddle1 = tge.Entity()
        self.paddle1.position = (2, 10)
        self.paddle1.sprite = tge.Sprite('█', fg=tge.WHITE)

    def update(self, dt):
        self.ball.position += self.ball.velocity * dt * 10

        # colisiones con paredes
        if self.ball.y <= 0 or self.ball.y >= 24:
            self.ball.velocity.y *= -1

    def draw(self, canvas):
        canvas.clear()

    def on_event(self, event):
        if event.type == tge.EVENT_KEYDOWN:
            if event.key == tge.K_UP:
                self.paddle1.y -= 1

app = tge.App(width=80, height=24, title="Pong")
app.run(PongScene())
```

**Principios del binding:**
- `tge.Scene`, `tge.Entity`, `tge.Sprite` son wrappers de los tipos C
- Los callbacks de C se traducen a métodos de Python
- El game loop corre en C; Python solo define la lógica
- Sin decoradores, sin reactividad, sin magia

---

## Principios del Proyecto

1. **API simple**: Aprender en 5 minutos, dominar en una tarde.
2. **Sin magia**: Sin macros ocultas, sin generación de código, sin reflexión.
3. **Arquitectura por capas**: Runtime no sabe de Engine, Engine no sabe de Game.
4. **Bajo acoplamiento**: Cada capa se comunica vía interfaces simples.
5. **Alto rendimiento**: Render diferencial, mínimo overhead por frame.
6. **Multiplataforma**: Linux, macOS, Windows (vía backends intercambiables).
7. **Backend reemplazable**: ANSI para Unix, Windows Console para Windows.
8. **Orientado a videojuegos**: Escenas, entidades, sprites, tilemaps, cámara.
9. **Determinista**: Todo depende de `delta_time`, no de `sleep()`.
10. **Event-driven**: Todo el motor se comunica mediante eventos.
11. **Sin dependencias externas**: Solo libc y termios/Win32 API.
12. **Jugable desde el día 1**: Hello World en 10 líneas, Pong en 50.

---

## Roadmap

### Fase 1 — Runtime

| # | Componente | Objetivo | Depende de |
|---|-----------|----------|------------|
| 1 | Backend ANSI | raw mode, cursor, alternate screen, colores básicos | — |
| 2 | Framebuffer | matriz 2D de celdas, clear, set | 1 |
| 3 | Render diferencial | comparar framebuffers, enviar solo diferencias | 2 |
| 4 | Event Loop | `poll()`, frame timing, `TGE_GetTime()`, `TGE_GetDeltaTime()` | 3 |
| 5 | Parser (bytes → eventos) | state machine para teclas, mouse, resize | 4 |
| 6 | Event Queue | `TGE_PollEvent()`, cola FIFO | 5 |
| 7 | Scheduler | `TGE_CallLater()`, `TGE_CallEvery()` | 4 |
| 8 | Canvas | dibujo de texto, rect, línea, círculo, blit | 2 |
| 9 | Unicode/UTF-8 | codepoints, wcwidth, encoding/decoding | — |
| 10 | Backend Windows | `WriteConsoleOutput`, `CHAR_INFO` | 2 |

### Fase 2 — Engine

| # | Componente | Objetivo | Depende de |
|---|-----------|----------|------------|
| 11 | App | `TGE_Create()`, `TGE_Run()`, configuración | 1-10 |
| 12 | Math | `TGE_Vector2`, `TGE_Rect`, `TGE_Color` | — |
| 13 | Geometry | `CheckCollisionRect`, `CheckCollisionCircle` | 12 |
| 14 | Scene | scene stack, transiciones, callbacks | 11 |
| 15 | Entity | lista plana, position, sprite, userdata | 14 |
| 16 | Camera | viewport, follow, mundo→pantalla | 15 |
| 17 | Sprite | carácter + color como textura | 12 |
| 18 | Tilemap | grid de tiles con propiedades | 15 |

### Fase 3 — Ejemplos y validación

| # | Ejemplo | Validación | Líneas estimadas |
|---|---------|-----------|-----------------|
| A | Hello World | pipeline completo: runtime + engine | ~10 |
| B | Pong | input, física básica, colisiones, puntuación | ~80 |
| C | Snake | scheduler, update loop, cola de entidades | ~100 |
| D | Tetris | tilemap, input, rotación, líneas | ~150 |
| E | Space Invaders | escenas, bullets, oleadas | ~200 |

### Fase 4 — Extensiones opcionales

| # | Módulo | Contenido |
|---|--------|-----------|
| 19 | tge-extra/Animation | keyframes, easing, sprite sheets |
| 20 | tge-extra/ResourceManager | carga y cacheo de sprites, tilemaps, fonts |
| 21 | tge-extra/FOV | shadowcasting |
| 22 | tge-extra/Pathfinding | A* sobre tilemap |
| 23 | tge-extra/Noise | ruido Perlin/Simplex |

---

## Architecture Decision Record (ADR) — Pendiente

Antes de escribir código, responder:

1. **¿Es una biblioteca o un motor?**
   - Biblioteca. El usuario escribe `main()`, llama a `TGE_Create()` + `TGE_Run()`.

2. **¿Quién controla el game loop?**
   - El motor. El usuario define callbacks (init, update, draw, event).

3. **¿Qué forma parte del núcleo y qué son módulos opcionales?**
   - Núcleo: runtime (backend, framebuffer, eventos, scheduler, canvas) + engine (scene, entity, camera, sprite, tilemap, geometry).
   - Opcional: FOV, pathfinding, animation, resource manager, audio.

4. **¿Qué es una entidad y qué responsabilidades tiene?**
   - Un struct con posición, sprite opcional y userdata. Sin lógica. Tonta.

5. **¿La API prioriza simplicidad (Raylib) o flexibilidad (Godot)?**
   - Simplicidad. Raylib para terminal.

6. **¿Qué puede romper la estabilidad de la API?**
   - Versión 1.0: runtime estable. Engine puede cambiar. Extensiones son experimentales.

---

## Comparativa

| Proyecto | Adoptamos | Descartamos | Motivo |
|----------|-----------|-------------|--------|
| **Raylib** | Filosofía de diseño: API pequeña, clara, C99, sin deps | Single header monolítico, variables globales | Headers separables para bindings |
| **notcurses** | Render diferencial, doble buffer, RGB color | Widgets, multimedia | Nosotros hacemos juegos, no TUIs |
| **libtickit** | Event queue, separar lectura de procesamiento | "Input" como concepto separado | Todo es un evento |
| **libvterm** | State machine para bytes, generador ANSI separado | Emulación completa de terminal | Solo necesitamos output correcto |
| **Textual** | Event bubbling para escenas | Widget tree, CSS, layout | Scene tree es mejor para juegos |
| **FTXUI** | Scene→Canvas→Renderer→Backend | Functional reactive, pipe operator | No es idiomático en C |
| **libtcod** | Concepto de Tile, tile properties | FOV/pathfinding en el núcleo | Módulos opcionales |
| **SDL2** | Backend abstraction, event queue, timing | Audio complejo, gráficos 3D | No relevantes |
| **Godot** | Scene tree, events desacoplados | Entity hierarchy, herencia, editor | Complejidad excesiva |
| **Love2D** | update/draw, simplicidad como objetivo | Lua como lenguaje principal | C + Python bindings |
