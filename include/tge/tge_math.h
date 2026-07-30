#ifndef TGE_MATH_H_
#define TGE_MATH_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y;
} TGE_Vector2;

typedef struct {
    int x, y, w, h;
} TGE_Rect;

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

#ifdef __cplusplus
}
#endif

#endif
