# Arquitectura de TGE

Cómo está construido TGE: las capas, el stack en runtime, el modelo de memoria y
las proyecciones a otros lenguajes. Para decisiones arquitectónicas puntuales ver
`ADR.md`; para el contrato de hosting (callbacks, slot de userdata) ver
`docs/HOSTING_API.md`.

## Capas

Una sola dirección, sin ciclos:

```
libtge.a  (core: runtime, canvas, renderer, escenas, scheduler)
   │  solo headers públicos de include/tge/
   ▼
libtge-extra.a  (módulos opcionales: grid, view, playfield, sprite, ...)
   │  usa solo la API pública de TGE, nunca tge_internal.h
   ▼
examples/  (juegos y ejemplos mínimos)
   │  usan solo headers públicos de tge/ y tge-extra/
```

Ningún módulo de `tge-extra/` incluye `src/` ni `tge_internal.h`; ningún
`examples/` incluye internos. El core no depende de `tge-extra`.

## Stack en runtime

```
TGE_App
 └─ TGE_Runtime
      ├─ Backend          (raw mode, colores, cursor, input, resize)
      ├─ Canvas           (framebuffer doble, diff)
      ├─ Parser           (bytes → eventos)
      ├─ Event Queue
      ├─ Scheduler        (timers por prioridad)
      └─ Timing
TGE_Engine
 └─ Scene Stack / GameContext
      └─ Canvas (vía App) → Renderer (stateless, row-oriented) → Backend
```

`TGE_App` es el único dueño de todos los subsistemas (ADR-017): no hay variables
globales; `TGE_Create` reserva todo y `TGE_Destroy` libera todo. El motor controla
el loop (`TGE_Run`); el usuario registra callbacks, no escribe loops (ADR-001).

## Playfield

`TGE_Playfield` es la composición que Snake y Breakout construían a mano, empaquetada
en un struct value (sin heap):

```
TGE_Playfield
 ├─ TGE_View       (área lógica que se adapta a la terminal: mínimo, validez, layout)
 ├─ TGE_GridView   (superficie de dibujo: tema, cell size, origen)
 └─ TGE_GridLayout (cache del tamaño de grilla ya aplicado)
```

No lleva cámara, entidades ni reglas de juego. El juego decide cómo reaccionar al
resize y es dueño de toda su lógica.

## Qué TGE NO es

Para evitar que el diseño se desvíe:

- **No es un ECS** (Entity-Component-System). `entity`/`actor` son utilidades, no un framework.
- **No es UI/DOM**. `ui` son helpers de paneles/HUD, no un sistema de widgets.
- **No maneja el gameplay**. Las reglas (colisiones, puntos, IA) son del juego.
- **No impone cámara**. `view` es un área lógica; una cámara queda para post-1.0.
- **No impone fixed-step**. `fixedstep` existe, pero `update(dt)` recibe `dt` real.
- **No obliga a usar Scene/Game**. `TGE_GameContext` solo elimina pegamento repetido.
- **No tiene dependencias pesadas**. Solo C99 + POSIX terminal.

## Modelo de memoria y ownership

Filosofía fuerte (ADR-015): **sin `malloc` en el render path**. El crecimiento de
contenedores dinámicos (`array`, `collision`, `entity`, `animation`) ocurre en
init/teardown o de forma amortizada en runtime — se evita con pre-reserva
(`reserve`). El render y las queries por frame no asignan.

Reglas de ownership por tipo:

| Tipo | Categoría | Dueño de los punteros que contiene |
|------|-----------|------------------------------------|
| `TGE_Sprite` / `TGE_MaskSprite` | value | `utf8`/`ascii`/`art`/`mask` son `const char *const *` **prestados** (literales del llamador) |
| `TGE_GridTheme` | value | sus `const TGE_Sprite *` son **prestados** (el llamador dueño de los sprites) |
| `TGE_TileSet` (TileMap) | value | sus `const TGE_Sprite *` son **prestados** |
| `TGE_Actor` | value | `sprite` es `const TGE_Sprite *` **prestado** |
| `TGE_GridView` / `TGE_Playfield` | value (composición) | sin heap; **deben** `_init`; cero-init es inválido |
| `TGE_TileMap` | value | autocontenido (celdas embebidas, sin alloc) |
| `TGE_Canvas` | prestado | dueño `TGE_App`/`TGE_Runtime`; grids/views lo toman vía `attach` cada frame |

Consecuencia: no libres un sprite mientras un `GridTheme`/`TileSet`/`Actor` lo
referencia, y no uses un `Playfield` sin `tge_playfield_init`.

## Bindings / proyecciones de lenguaje

```
C        → contrato canónico de ABI y semántica (include/tge/*.h)
C++      → bindings/cpp/: proyección idiomática tge:: (laboratorio de ergonomía)
Python/Rust/Zig → FFI directo sobre la API C, no vía C++
```

`bindings/cpp/` **no es una dependencia** del engine ni del C API: es una proyección
que consume la API C estable para impulsar decisiones de ergonomía. Está congelada
en "C++ 1.0" dentro de Beta 1 y **queda fuera** de la política de estabilidad
(`docs/API_STABILITY.md`). Una proyección de alto nivel formatea a su manera y baja
al C API; no se cruza un límite de varargs de C.
