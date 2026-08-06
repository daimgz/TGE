# API Stability Policy — TGE

## Niveles de API

| Nivel | Alcance | Garantía |
|-------|---------|----------|
| Público | `include/tge/*.h` (`libtge.a`) | Semver estricto (ver abajo) |
| Interno | `src/*`, `src/tge_internal.h` | Cambia libremente en minors |
| Backend | vtable interna del runtime | Interna, no estable |
| Extensiones | `tge-extra/*.h` (`libtge-extra.a`) | **Experimental**: puede romper compatibilidad entre minor versions |

Estado actual: **pre-1.0** (ver `IMPLEMENTATION_PLAN.md`, checklist de pre-1.0).

`libtge-extra.a` es un archive opcional: los juegos que no usan extensiones
solo linkean `-ltge`; los que sí (p.ej. un roguelike) añaden `-ltge-extra`.
Cada módulo es independiente, usa solo la API pública de TGE y nunca
`tge_internal.h`.

Módulos actuales (experimentales): `entity`, `animation`, `collision`,
`vec2i`, `direction`, `timer`, `fixedstep`, `input`, `grid`, `view`,
`input_buffer`, `grid_view`. Los utilitarios (`vec2i`, `direction`, `timer`,
`fixedstep`, `input`) se validan como consumidores de ejemplo
`examples/games/01_snake.c` y `05_swarm.c`; `grid` (capa de dibujo por grilla
con tema visual `TGE_GridTheme` y tiles `TGE_GridTile`) se valida en
`examples/min/08_grid_canvas.c` y `examples/games/06_snake_grid.c`.

Los módulos de layout y entrada — `view` (TGE_View: playfield adaptativo con
tamaño mínimo, validez y primer layout, dueño del espacio local del juego),
`input_buffer` (TGE_InputBuffer: FIFO de direcciones, drop-new) y `grid_view`
(TGE_GridView: wrapper de configuración sobre TGE_Grid, con variantes `_local`
que dibujan en coordenadas locales del view) — se validan como consumidores de
ejemplo en `examples/games/01_snake.c` (`view` + `input_buffer`),
`examples/games/06_snake_grid.c` (`view` + `input_buffer` + `grid_view`,
ejemplo de referencia de la arquitectura world/renderer) y
`examples/games/07_breakout.c` (`view` + `grid_view`).

Operaciones sobre el espacio local del view (candidatas): `tge_view_translate`
(local→superficie por el origen del área), `tge_view_contains` (dentro del
interior half-open) y `tge_view_random_point` (spawn aleatorio). Delegan en
las primitivas de `vec2i`: `tge_vec2i_clamp_rect` (clamp a los límites de un
rect, usado en el resize de los snakes), `tge_rect_random_point` (punto
aleatorio en un rect, usado también por 05_swarm) y `tge_rect_translate_point`
(mapeo local→global por el origen del rect).

## Convención para agregar API

**Solo se agrega una abstracción si elimina un patrón que apareció en al
menos dos juegos** (regla del usuario, revisión 3). El core y tge-extra deben
mantenerse como bibliotecas de bajo nivel; un helper justificado por uso real
es bienvenido, un "helper por las dudas" no. Los helpers de dibujo del core
(`tge_printf`, `tge_draw_centered_text`) y los de escena
(`tge_scene_create`/`tge_scene_destroy`) entran por esta regla: el patrón que
eliminan estaba en todos los juegos de ejemplo.

## Funciones del core añadidas en revisión 3 (candidatas a 1.x)

- `tge_printf()` — formato printf + `tge_draw_text` con buffer de pila fijo
  (sin malloc, seguro para el render path).
- `tge_draw_centered_text()` — texto centrado midiendo el ancho real en
  columnas (wide chars cuentan doble).
- `tge_scene_create()` / `tge_scene_destroy()` — escena heap + userdata en un
  alloc con trampoline de destroy; `destroy` del usuario solo libera recursos
  profundos, nunca el scene ni el userdata. El trampoline hace que
  `TGE_PopScene`/`TGE_ReplaceScene` liberen todo.

## Versiones 0.x (pre-1.0)
- Breaking changes permitidos en cualquier release.
- Se anuncian en el changelog.
- Se evitan dentro de lo posible, pero no se garantizan.

## Versiones 1.x
- Solo additive changes.
- Nada se elimina de la API pública.
- Nada cambia de semántica (bugs exceptuados).
- Deprecación: una función marcada como deprecated se mantiene
  al menos 2 releases antes de eliminarse.

## Versiones 2.x
- Breaking changes permitidos nuevamente.
- Cada breaking change se documenta con guía de migración.

## Verificación
- Los headers públicos deben ser autocontenidos: `make check_headers`.
- Sin malloc en el render path: `make check_no_malloc`.