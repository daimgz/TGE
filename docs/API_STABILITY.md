# API Stability Policy — TGE

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
