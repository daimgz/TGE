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
`vec2i`, `direction`, `timer`, `fixedstep`, `input`, `grid`. Los
utilitarios (`vec2i`, `direction`, `timer`, `fixedstep`, `input`) se validan
como consumidores de ejemplo `examples/games/01_snake.c` y `05_swarm.c`;
`grid` (capa de dibujo por grilla con tema visual `TGE_GridTheme` y tiles
`TGE_GridTile`) se valida en `examples/min/08_grid_canvas.c` y
`examples/games/06_snake_grid.c`.

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