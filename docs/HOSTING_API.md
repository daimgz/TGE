# Hosting & Binding API — Contrato de integración

**Estado:** Implementado (secciones 3.1–3.3) y adoptado (3.4). Decisiones
registradas en ADR-029 a ADR-032.
**Fecha:** 2026-08-08
**Alcance:** API de integración/hosting para que TGE pueda envolverse desde un
lenguaje con paradigma orientado a objetos (C++, Python, etc.) sin hacks. El
documento es un *contrato*: fija qué debe ofrecer la API y qué garantiza antes
de tocar la implementación. Ver también `docs/API_STABILITY.md` y los ADR de
`ADR.md`.

---

## 1. Contexto y motivación

Los primeros juegos de ejemplo validaban la pregunta "¿puede TGE ejecutar este
juego correctamente?". Los siguientes consumidores (Pac-Man → `TileMap` /
`TGE_Playfield` / `TGE_Actor` → bindings) validan además una pregunta distinta
y más importante para el futuro del motor: "¿puedo integrar TGE dentro de otro
runtime sin pelearme con él?".

El plan del proyecto ya fija el modelo esperado de binding
(`PLAN_ENTRENAMIENTO.md`): el game loop corre en C; el lenguaje embebido solo
define la lógica (callbacks). Ese modelo es el correcto y se mantiene. Lo que
falta son unos pocos puntos de integración públicos para que C++ y Python
puedan envolver el motor sin registros globales frágiles, sin varargs
incruzables y sin copiar el loop interno.

### Principio rector

> El engine administra el ciclo de vida de sus estructuras C. El host
> administra la identidad y el ciclo de vida de su objeto de alto nivel. TGE no
> necesita saber qué hay detrás de un `void *`.

```
             Host / Binding
                  │
        ┌─────────┴─────────┐
        │                   │
     Python               C++
        │                   │
   objeto Python       objeto C++ real
        │                   │
        └────── userdata ──┘
                  │
                  ▼
              TGE_App
                  │
            TGE_Game / Scene
                  │
              TGE core
```

---

## 2. Análisis: puntos débiles actuales para el acoplamiento POO

Inventario del acoplamiento con la API pública (`include/tge/*.h`) y el loop
interno (`src/app.c`, `src/tge_internal.h`), clasificado por severidad.

### 2.1 Severidad alta

**a) Control invertido sin una unidad de ejecución pública.**
`TGE_Run()` posee el bucle (`tge_app.h:36`); el paso por frame solo existe
como función interna (`src/tge_internal.h:151`). `TGE_PollEvent` y
`TGE_GetCanvas` son públicos, pero no hay forma pública de presentar un frame:
el `TGE_Diff` y el renderer diferencial son internos
(`src/tge_internal.h:144`). Un host no puede tener su propio loop, ni correr
varias apps, ni intercalar con su propia UI.

**b) Callbacks de app sin `userdata`.**
Los callbacks de `TGE_Run` reciben solo `TGE_App *` (`tge_app.h:18-21`), un
tipo opaco. Las capas `TGE_Scene` y `TGE_GameContext` sí tienen
`userdata`/`instance`, pero la vía cruda del app no: para reencontrar el objeto
del host, un binding debe mantener un registro global `TGE_App * → wrapper`.

**c) El motor aloca instancias en memoria zeroed (sin constructor).**
`tge_scene_create` y `tge_game_scene_create` alocan y zeroan el bloque del
usuario (`tge_scene.h:57`, `tge-extra/game.h:68`). En C++ un objeto con
constructor/destructor/virtuales no puede vivir ahí tal cual. No se debe
"arreglar" metiendo C++ en el core; el contrato es el patrón de instancia
host-construida (sección 3.4).

### 2.2 Severidad media

**d) Identidad implícita offset-0 de `TGE_GameContext`.**
`TGE_GameContext` debe ser el primer miembro de la instancia (`game.h:18-30`).
Es una peculiaridad de representación útil en C/C++ y sin valor para Python;
el binding es responsable de mantener la identidad entre el puntero C y el
objeto de alto nivel (sección 5).

**e) `tge_printf` es varargs** (`tge_canvas.h:91`). Los bindings (cffi/ctypes)
no cruzan varargs de forma portable.

**f) Macros compound-literal de color** (`tge_canvas.h:37-46`). Son compound
literals C99 con initializers designados; no compilan en C++ estricto
(`-std=c++11` es error, `-pedantic` en C++20 es warning). C++ debe usar
`tge_color_rgb()` / `tge_color_indexed()`.

**g) Vtable de backend interna.** `TGE_Backend` es un struct de function
pointers clásico (`src/tge_internal.h:31`) pero vive en un header interno. Un
host no puede inyectar su propio backend sin forkear.

**h) Estado global del motor.** El modo Unicode se resuelve/sobreescribe con
estado estático (`src/unicode.c`). Compartido por todos los `TGE_App` del
proceso.

### 2.3 Severidad baja (los absorbe el binding)

- **Uniones tagged**: `TGE_Event.data` (`tge_events.h:73-83`) y `TGE_Color`.
  El binding presenta eventos tipados y colores según `mode`.
- **Out-params**: `tge_runtime_poll_event`, `tge_grid_size_for`,
  `tge_grid_view_size_for`, `tge_input_buffer_pop`, `tge_utf8_decode` → el
  binding devuelve tuplas.
- **Ownership por convención**: no está marcado en las firmas (sección 4).
- **Sin modelo de errores**: `NULL`/`false` + mensajes a stderr (ADR-021). El
  binding traduce a excepciones.

---

## 3. Contrato — API de integración a implementar

Todo lo siguiente es aditivo: no rompe la API/ABI existente y no cambia la
semántica de ninguna función actual.

### 3.1 `TGE_Step()` — una iteración del pipeline normal

```c
void TGE_Step(TGE_App *app);
```

**Garantías:**

- `TGE_Step()` **no crea un nuevo modelo de ejecución**; extrae una iteración
  del loop que ya controla `TGE_Run()`. No convierte a TGE en una librería
  "sin opinión" sobre el loop: `TGE_Run()` sigue siendo la API principal y el
  modelo original (ADR-001) queda intacto. `Step()` hace visible una unidad de
  ejecución que ya existía internamente.
- Equivalencia contractual: `TGE_Run(app)` ≡ `while (!quit) TGE_Step(app)`.
  Python/C++ pueden depender de que `Step` sea **semánticamente idéntico** a
  una iteración de `Run`, no una variante simplificada.
- `TGE_Step()` ejecuta exactamente una iteración del pipeline normal con su
  orden existente (el de ADR-025: eventos → update → draw → render/present →
  scene ops → frame limiting). El frame limiter se conserva en su posición
  exacta (después de scene ops); `Step` no lo omite ni lo mueve.
- El estado de quit es estado de la app y se consulta por la API existente;
  `Step` no lo devuelve como retorno.
- El contrato dice *qué garantiza* (el pipeline completo, en orden); no promete
  *cómo* se implementa (sin nombres de funciones internas, sin detalle de
  llamadas).

### 3.2 `userdata` como slot de `TGE_App`

```c
void  TGE_SetUserData(TGE_App *app, void *userdata);
void *TGE_GetUserData(TGE_App *app);
```

**Garantías:**

- Un slot del host en `TGE_App`, legible/escribible en cualquier momento desde
  el thread del game loop (ADR-023).
- No se introduce una segunda familia de callbacks ni `TGE_RunUser`: la API de
  callbacks existente (`tge_app.h`) permanece ABI/API-compatible.
- Es la vía por la que los callbacks de app reencuentran el objeto del host sin
  registros globales: `init/update/draw/event` leen `TGE_GetUserData(app)`.

### 3.3 `tge_vprintf()` — API de compatibilidad C para bindings

`tge_vprintf` es **API de compatibilidad C para bindings**, no una API de
formateo de Python.

```c
void tge_vprintf(TGE_Canvas *canvas, int x, int y, TGE_Color fg, TGE_Color bg,
                 const char *fmt, va_list ap);
```

**Garantías:**

- Mismo comportamiento que `tge_printf` (buffer de pila fijo, sin malloc,
  truncamiento estilo `snprintf`) pero recibiendo `va_list`.
- `tge_printf` sigue siendo la API cómoda en C y delega en `tge_vprintf`:

  ```text
  tge_printf() → tge_vprintf() → formateador C
  ```

- Permite que código C intermedio o trampolines del binding deleguen en el
  mismo formateador interno **sin atravesar una frontera C varargs**.
- **No** resuelve el formateo desde Python por sí sola: cffi/ctypes no pueden
  fabricar un `va_list` portable. El binding de alto nivel debe proporcionar su
  propia adaptación de formato (strings ya formateados o una API propia), que
  baja a la API C no-varargs:

  ```text
  Python → formatea/adapta → API C no-varargs (tge_vprintf o draw)
  ```

  Esto deja abierta una futura API cómoda —p. ej. `canvas.print(x, y,
  "Score: {}", score)` en Python— sin contaminar el contrato C ahora.
- Exige `stdarg.h` en `tge_canvas.h` (cabecera autocontenida, `make
  check_headers`).

### 3.4 Patrón de instancia host-construida y regla offset-0

- **El host construye su objeto** (`new` en C++, wrapper en Python) y lo
  referencia; el motor aloja solo sus propias estructuras (`TGE_App`,
  `TGE_Canvas`, `TGE_Scene`, recursos) y nunca construye tipos del host.
- El engine administra el ciclo de vida de sus estructuras C; el host
  administra la identidad y el ciclo de vida de su objeto de alto nivel.
- **offset-0, en tres planos:**
  - **C/C++:** `TGE_GameContext` como primer miembro (herencia pública +
    `reinterpret_cast`) es una convención útil.
  - **Python:** no es un requisito de integración.
  - **Binding:** es responsabilidad del wrapper mantener la identidad entre
    `TGE_GameContext *` y el objeto de alto nivel.
- El offset-0 es una peculiaridad de representación C, no una exigencia
  arquitectónica del binding.

---

## 4. Reglas de ownership y ciclo de vida

La política general es la de ADR-020 (quien crea, destruye, salvo
transferencia documentada). Este contrato la hace explícita para los tipos que
un binding toca:

| Tipo | Dueño | Regla |
|------|-------|-------|
| `TGE_App` | Host | `TGE_Create` / `TGE_Destroy`. `TGE_Run` no asume ownership. `[takes ownership]` en create. |
| `TGE_Scene` (heap, vía `tge_scene_create`) | App mientras está push | Al pushear, el ownership pasa al app. Al popear/reemplazar, el app lo destruye (ver comportamiento observable abajo). |
| `TGE_Scene` (manual, `destroy == NULL`) | Host | El engine nunca lo libera. |
| `TGE_GameContext` / instancia | Host | El engine lo referencia, no lo posee. La identidad `ctx → objeto` es del binding. |
| `TGE_Canvas` | `TGE_App` | El host no lo crea ni destruye; lo recibe en `draw`. `[borrowed]`. |
| `TGE_Grid` / `TGE_GridView` | Host | Contienen `TGE_Canvas` prestado; el canvas debe vivir más que la vista. `[borrowed]`. |
| `TGE_EntityPool`, `TGE_CollisionWorld`, `TGE_Anim` (tge-extra) | Host | `create`/`destroy` propios de cada módulo. `[takes ownership]`. |
| Recursos de tge-extra | Host o gestor | Creados por el host → los destruye el host; gestionados por un `resources` futuro → el gestor. |

**Ciclo de vida `Scene`/`Game` (comportamiento observable, verificado contra
`src/scene.c` y `src/app.c`):** `TGE_PopScene` y `TGE_ReplaceScene` solo
encolan la operación; la destrucción ocurre **al final del frame**, al
procesar las scene ops (`tge_app_process_scene_ops`), que llama al `destroy`
de la escena saliente. Para una escena creada con `tge_scene_create`, `destroy`
es el destructor de bloque: invoca el callback del usuario y libera el bloque
(escena + userdata) en una sola allocación. `TGE_Destroy` destruye las escenas
que aún están en el stack al apagar la app; las ops pendientes sin procesar se
descartan (la escena sigue en el stack, así que no hay doble liberación). Una
escena manual (`destroy == NULL`) nunca es liberada por el engine. El callback
`destroy` debe liberar recursos profundos, nunca el scene ni su userdata. En un
binding, `destroy` debe soltar la referencia al wrapper, no destruir el objeto
Python/C++ por cuenta propia salvo que el contrato del wrapper lo defina así.

---

## 5. Patrones por lenguaje

### 5.1 C++

- Wrapper RAII por objeto del motor (`TGE_App`, canvas, escenas, pools), con
  destructor que llama al `destroy` correspondiente.
- Trampolines estáticos + baton: cada callback es una función `extern "C"` que
  lee `TGE_GetUserData(app)` (o el `userdata` del scene/game) y despacha a un
  método virtual del objeto.
- Para `TGE_GameContext`, herencia pública sobre el struct + `reinterpret_cast`
  (el offset-0 es una convención C/C++ válida, sección 3.4).
- Colores: usar `tge_color_rgb()` / `tge_color_indexed()`; las macros
  `TGE_COLOR_*` no compilan en C++ estricto (punto 2.2.f).
- Varargs: `tge_printf` se llama directo desde C++ (varargs nativos); usar
  `tge_vprintf` (punto 3.3) cuando un trampolín C intermedio reciba `va_list`.

### 5.2 Python (cffi/ctypes)

- El wrapper Python posee el struct C como atributo; el struct se libera con
  finalizers (`ffi.gc`).
- Mapa de identidad `TGE_GameContext * → PyObject *` (registro del binding);
  offset-0 no aplica.
- Upcalls con `ffi.callback`: cada callback C re-entra a Python
  (GIL gestionado por cffi) y reencuentra el wrapper.
- Loop propio: posible vía `TGE_Step` (sección 3.1) para estilos pygame; el
  modo recomendado sigue siendo `TGE_Run` con callbacks.
- Formateo: el wrapper ofrece su propia adaptación (strings ya formateados o
  una API Python propia que baje a la API C no-varargs); `tge_vprintf`
  (sección 3.3) solo para trampolines C intermedios del binding.
- `NULL` → `None`, `bool` → `True/False` (ADR-021); out-params → tuplas.

---

## 6. Diferido explícitamente

No se implementan en este contrato; solo ante una necesidad real. Cada uno
lleva su gatillo.

| Mejora | Gatillo |
|--------|---------|
| `TGE_Backend` público (vtable estable) | Un host que necesite su propio backend (salida a widget, captura, backend en C++/Python). |
| Header C++ (`tge_cpp.h` con colores `constexpr`, helpers inline) | Un consumidor C++ real que lo pida. |
| Eventos de alto nivel (tipados por modo) | Un binding que necesite eventos "objetos" sin costura de uniones. |
| Errores estructurados (p.ej. `tge_last_error`) | Diagnóstico real desde bindings; hoy `NULL`/`false` + stderr basta. |
| Eliminar globals de Unicode (modo por `TGE_App`) | Varias apps/intérpretes en un proceso con modos distintos. Breaking; se haría antes de 1.0 o en una 2.x. |

---

## 7. Verificación del contrato (implementada y en regresión)

- `make check_headers` (headers autocontenidos, ahora con `stdarg.h`).
- `make test` (suites completas sin cambios de comportamiento).
- `make games` / `make examples` sin warnings.
- Ejemplo mínimo de host loop: `while (!done) { TGE_Step(app); host_work(); }`
  debe ser **observacionalmente igual** a `TGE_Run`. El test compara
  *comportamiento*, no estructura: mismo orden de eventos, scene ops aplicadas
  al final del frame, y frame limiter aplicado en la posición exacta (después
  de scene ops), como en una iteración de `Run`.
- Smoke de binding: `tge_vprintf` invocado desde cffi y `TGE_GetUserData`
  leído dentro de un callback de app.
- `check_no_malloc` sigue pasando (ni `tge_vprintf` ni `TGE_Step` alocan).

---

*Fin del contrato. Las secciones 1–2 y 4–7 son análisis y reglas estables; la
sección 3 fija el contrato, implementado y registrado en ADR-029 a ADR-032
con referencia cruzada a este documento.*
