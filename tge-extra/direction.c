#include "direction.h"

TGE_Direction tge_direction_opposite(TGE_Direction d)
{
    switch (d) {
    case TGE_DIR_UP:
        return TGE_DIR_DOWN;
    case TGE_DIR_DOWN:
        return TGE_DIR_UP;
    case TGE_DIR_LEFT:
        return TGE_DIR_RIGHT;
    case TGE_DIR_RIGHT:
        return TGE_DIR_LEFT;
    default:
        return TGE_DIR_NONE;
    }
}

int tge_direction_dx(TGE_Direction d)
{
    switch (d) {
    case TGE_DIR_LEFT:
        return -1;
    case TGE_DIR_RIGHT:
        return 1;
    default:
        return 0;
    }
}

int tge_direction_dy(TGE_Direction d)
{
    switch (d) {
    case TGE_DIR_UP:
        return -1;
    case TGE_DIR_DOWN:
        return 1;
    default:
        return 0;
    }
}

TGE_Vec2i tge_direction_vec(TGE_Direction d)
{
    TGE_Vec2i v = { tge_direction_dx(d), tge_direction_dy(d) };
    return v;
}
