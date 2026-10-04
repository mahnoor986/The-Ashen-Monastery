/* meshgen.c - CPU mesh builder for blocky geometry (see meshgen.h). */
#include <string.h>
#include "meshgen.h"
#include "textures.h"
#include "raymath.h"

static void Grow(MeshBuilder *mb, int extra)
{
    int cap;
    if (mb->count + extra <= mb->cap) return;
    cap = mb->cap ? mb->cap * 2 : 1024;
    while (cap < mb->count + extra) cap *= 2;
    mb->verts   = MemRealloc(mb->verts,   (unsigned int)(cap * 3 * sizeof(float)));
    mb->uvs     = MemRealloc(mb->uvs,     (unsigned int)(cap * 2 * sizeof(float)));
    mb->normals = MemRealloc(mb->normals, (unsigned int)(cap * 3 * sizeof(float)));
    mb->colors  = MemRealloc(mb->colors,  (unsigned int)(cap * 4));
    mb->cap = cap;
}

void MB_Begin(MeshBuilder *mb)
{
    memset(mb, 0, sizeof(*mb));
}

float MB_FaceShade(Vector3 n)
{
    if (n.y > 0.5f)  return 1.0f;
    if (n.y < -0.5f) return 0.5f;
    if (n.z > 0.5f || n.z < -0.5f) return 0.8f;
    return 0.65f;
}

/* How light + face shade are stored in the vertex color.
 * Phase 1 (raylib default shader): rgb = light * shade, alpha = 255. */
static Color Encode(Color light, float shade)
{
    return (Color){ (unsigned char)(light.r * shade), (unsigned char)(light.g * shade),
                    (unsigned char)(light.b * shade), 255 };
}

static void PushVertex(MeshBuilder *mb, Vector3 p, float u, float v, Vector3 n, Color c)
{
    int i = mb->count++;
    mb->verts[i * 3 + 0] = p.x;  mb->verts[i * 3 + 1] = p.y;  mb->verts[i * 3 + 2] = p.z;
    mb->uvs[i * 2 + 0] = u;      mb->uvs[i * 2 + 1] = v;
    mb->normals[i * 3 + 0] = n.x; mb->normals[i * 3 + 1] = n.y; mb->normals[i * 3 + 2] = n.z;
    mb->colors[i * 4 + 0] = c.r; mb->colors[i * 4 + 1] = c.g;
    mb->colors[i * 4 + 2] = c.b; mb->colors[i * 4 + 3] = c.a;
}

void MB_Face(MeshBuilder *mb, Vector3 o, Vector3 a, Vector3 b, int tile, const Color light[4])
{
    Vector3 p[4], n = Vector3Normalize(Vector3CrossProduct(a, b));
    float u0, v0, u1, v1, shade = MB_FaceShade(n);
    float us[4], vs[4];
    Color c[4];
    int k;
    static const int order[6] = { 0, 1, 2, 0, 2, 3 };   /* two counter-clockwise triangles */

    Textures_TileUV(tile, &u0, &v0, &u1, &v1);
    p[0] = o;
    p[1] = Vector3Add(o, a);
    p[2] = Vector3Add(p[1], b);
    p[3] = Vector3Add(o, b);
    us[0] = u0; vs[0] = v1;     /* bottom-left of the tile */
    us[1] = u1; vs[1] = v1;
    us[2] = u1; vs[2] = v0;
    us[3] = u0; vs[3] = v0;
    for (k = 0; k < 4; k++) c[k] = Encode(light[k], shade);

    Grow(mb, 6);
    for (k = 0; k < 6; k++) {
        int i = order[k];
        PushVertex(mb, p[i], us[i], vs[i], n, c[i]);
    }
}

void MB_Box(MeshBuilder *mb, Vector3 mn, Vector3 mx, int tile, Color light)
{
    const Color l[4] = { light, light, light, light };
    float dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    MB_Face(mb, (Vector3){ mn.x, mx.y, mx.z }, (Vector3){ dx, 0, 0 }, (Vector3){ 0, 0, -dz }, tile, l); /* +Y */
    MB_Face(mb, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ dx, 0, 0 }, (Vector3){ 0, 0, dz }, tile, l);  /* -Y */
    MB_Face(mb, (Vector3){ mx.x, mn.y, mx.z }, (Vector3){ 0, 0, -dz }, (Vector3){ 0, dy, 0 }, tile, l); /* +X */
    MB_Face(mb, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ 0, 0, dz }, (Vector3){ 0, dy, 0 }, tile, l);  /* -X */
    MB_Face(mb, (Vector3){ mn.x, mn.y, mx.z }, (Vector3){ dx, 0, 0 }, (Vector3){ 0, dy, 0 }, tile, l);  /* +Z */
    MB_Face(mb, (Vector3){ mx.x, mn.y, mn.z }, (Vector3){ -dx, 0, 0 }, (Vector3){ 0, dy, 0 }, tile, l); /* -Z */
}

Mesh MB_End(MeshBuilder *mb)
{
    Mesh m = { 0 };
    if (mb->count == 0) {
        MemFree(mb->verts); MemFree(mb->uvs); MemFree(mb->normals); MemFree(mb->colors);
        memset(mb, 0, sizeof(*mb));
        return m;
    }
    m.vertexCount = mb->count;
    m.triangleCount = mb->count / 3;
    m.vertices = mb->verts;
    m.texcoords = mb->uvs;
    m.normals = mb->normals;
    m.colors = mb->colors;
    UploadMesh(&m, false);
    memset(mb, 0, sizeof(*mb));   /* the mesh owns the arrays now */
    return m;
}
