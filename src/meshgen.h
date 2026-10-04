/* meshgen.h - helper for building block meshes (non-indexed triangles) on the CPU.
 * Faces are textured with a tile from the atlas. Each face gets a brightness from its
 * direction (top 1.0, N/S 0.8, E/W 0.65, bottom 0.5) plus a per-vertex light color. */
#ifndef MESHGEN_H
#define MESHGEN_H

#include "raylib.h"

typedef struct {
    float *verts, *uvs, *normals;
    unsigned char *colors;
    int count, cap;           /* vertices used / allocated */
} MeshBuilder;

void MB_Begin(MeshBuilder *mb);
/* One quad: corner o, edge a (texture "right") and edge b (texture "up"); a x b points out of
 * the face. light[4] = light color at o, o+a, o+a+b, o+b. */
void MB_Face(MeshBuilder *mb, Vector3 o, Vector3 a, Vector3 b, int tile, const Color light[4]);
/* An axis-aligned box, all six faces, with one light color everywhere. */
void MB_Box(MeshBuilder *mb, Vector3 min, Vector3 max, int tile, Color light);
/* Upload to the GPU and hand over ownership of the arrays (free with UnloadMesh).
 * Returns a mesh with vertexCount 0 if nothing was added. */
Mesh MB_End(MeshBuilder *mb);

/* true (default): vertex color = baked light + shade in alpha, for the world shader.
 * false: bake a plain look for raylib's default shader (used if the shader fails to load). */
void MB_SetShaderEncoding(bool on);

/* Brightness of a face by its outward normal. */
float MB_FaceShade(Vector3 n);

#endif
