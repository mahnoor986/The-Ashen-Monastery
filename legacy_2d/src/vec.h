/* vec.h - tiny Vector2 helpers (no raymath dependency). */
#ifndef VEC_H
#define VEC_H

#include <math.h>
#include "raylib.h"

static inline Vector2 V2(float x, float y)              { Vector2 v = { x, y }; return v; }
static inline Vector2 V2Add(Vector2 a, Vector2 b)       { return V2(a.x + b.x, a.y + b.y); }
static inline Vector2 V2Sub(Vector2 a, Vector2 b)       { return V2(a.x - b.x, a.y - b.y); }
static inline Vector2 V2Scale(Vector2 a, float s)       { return V2(a.x * s, a.y * s); }
static inline float   V2Dot(Vector2 a, Vector2 b)       { return a.x * b.x + a.y * b.y; }
static inline float   V2Len(Vector2 a)                  { return sqrtf(a.x * a.x + a.y * a.y); }
static inline float   V2Dist(Vector2 a, Vector2 b)      { return V2Len(V2Sub(a, b)); }
static inline Vector2 V2Norm(Vector2 a)                 { float l = V2Len(a); return (l > 0.0001f) ? V2(a.x / l, a.y / l) : V2(0.0f, 0.0f); }
static inline Vector2 V2FromAngle(float rad)            { return V2(cosf(rad), sinf(rad)); }
static inline float   Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float   Lerpf(float a, float b, float t)  { return a + (b - a) * t; }
static inline float   Maxf(float a, float b)            { return a > b ? a : b; }

#endif
