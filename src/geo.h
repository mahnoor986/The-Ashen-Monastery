/* geo.h - the static-geometry builder shared by architecture.c and props.c.
 * Everything is added to one MeshBuilder per material; positions are world coordinates, UVs are
 * world-space (MB_WorldUV) unless given, and the sconce/candle light is baked into the vertices. */
#ifndef GEO_H
#define GEO_H

#include "raylib.h"
#include "meshgen.h"
#include "textures.h"
#include "world.h"

typedef struct {
    MeshBuilder mb[MAT_COUNT];
    World *w;
} Geo;

/* A quad a,b,c,d (counter-clockwise seen from the front), world-space UVs. */
void Geo_Quad(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d);
/* The same quad, seen from both sides. */
void Geo_Quad2(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d);
/* A quad with explicit UVs (pictures: banners, paintings, stained glass, cobwebs). */
void Geo_QuadUV(Geo *g, int mat, const Vector3 p[4], const Vector2 uv[4], bool twoSided);
/* An axis-aligned box. */
void Geo_Box(Geo *g, int mat, Vector3 mn, Vector3 mx);
/* A box rotated around the vertical axis: centre, yaw (local +z = (sin yaw, 0, cos yaw)), half sizes. */
void Geo_OBox(Geo *g, int mat, Vector3 center, float yaw, Vector3 half);
/* A lathe (turned) shape around a vertical axis at `base`: ring k has radius r[k] at height y[k]
 * above base. `capTop` closes the last ring. */
void Geo_Lathe(Geo *g, int mat, Vector3 base, const float *r, const float *y, int rings, int sides, bool capTop);

/* A local frame for building props: origin on the floor, yaw = direction of local +z. */
typedef struct { Vector3 o; float yaw; } Frame;
Vector3 Frame_Point(Frame f, float x, float y, float z);
/* Box in frame coordinates: centre (x, y, z) and half sizes. */
void Geo_FBox(Geo *g, int mat, Frame f, float x, float y, float z, float hx, float hy, float hz);

/* Props (props.c): choose and register the props of a wing (lights, flames, colliders), then
 * build their geometry. Place runs before the world meshes are baked. */
void Props_Place(World *w);
void Props_Build(Geo *g);

#endif
