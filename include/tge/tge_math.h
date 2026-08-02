#ifndef TGE_MATH_H_
#define TGE_MATH_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 2D vector with float components. */
typedef struct {
    float x, y;
} TGE_Vector2;

/* Axis-aligned rectangle in cell (integer) coordinates. */
typedef struct {
    int x, y, w, h;
} TGE_Rect;

TGE_Vector2 tge_vec2(float x, float y);
TGE_Vector2 tge_vec2_add(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_vec2_sub(TGE_Vector2 a, TGE_Vector2 b);
TGE_Vector2 tge_vec2_scale(TGE_Vector2 v, float s);
float       tge_vec2_length(TGE_Vector2 v);
float       tge_vec2_distance(TGE_Vector2 a, TGE_Vector2 b);
/* Returns a unit vector in the same direction as v, or (0,0) if v has
 * (near-)zero length. */
TGE_Vector2 tge_vec2_normalize(TGE_Vector2 v);

TGE_Rect tge_rect(int x, int y, int w, int h);
bool     tge_rect_contains(TGE_Rect r, int x, int y);
bool     tge_rect_intersects(TGE_Rect a, TGE_Rect b);
bool     tge_circle_intersects(TGE_Vector2 center, float r, TGE_Rect rect);
/* Segment-vs-segment intersection test. Returns true when the segments
 * touch or cross; returns false for parallel/collinear segments. */
bool     tge_segments_intersect(TGE_Vector2 p1, TGE_Vector2 p2,
                               TGE_Vector2 q1, TGE_Vector2 q2);

#ifdef __cplusplus
}
#endif

#endif
