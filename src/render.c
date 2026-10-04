/* render.c - world drawing (see render.h). */
#include <math.h>
#include "render.h"
#include "textures.h"
#include "raymath.h"

static Material worldMat;   /* atlas texture + world shader */

void Render_Init(void)
{
    worldMat = LoadMaterialDefault();
    worldMat.maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Atlas();
}

void Render_Shutdown(void)
{
    worldMat.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* owned by textures.c */
    UnloadMaterial(worldMat);
}

void Render_DrawWorld(const World *w, float time)
{
    int i;
    for (i = 0; i < w->chunkCount; i++)
        DrawMesh(w->chunks[i], worldMat, MatrixIdentity());

    /* exit doors slide down into the floor as doorSlide goes 0 -> 1 */
    if (w->doorSlide < 1.0f) {
        for (i = 0; i < w->exitCount; i++) {
            Matrix m = MatrixMultiply(MatrixRotateY(w->exitYaw[i]),
                                      MatrixTranslate(w->exits[i].x + 0.5f, -w->doorSlide * WALL_HEIGHT,
                                                      w->exits[i].z + 0.5f));
            DrawMesh(w->doorMesh, worldMat, m);
        }
    }

    /* torch flames: small glowing cubes that flicker in size */
    for (i = 0; i < w->torchCount; i++) {
        Vector3 p = w->torches[i].pos;
        float f = 1.0f + 0.15f * sinf(time * 13.0f + i * 1.7f) + 0.08f * sinf(time * 23.0f + i);
        DrawCube((Vector3){ p.x, p.y + 0.02f, p.z }, 0.16f * f, 0.2f * f, 0.16f * f, (Color){ 255, 120, 30, 255 });
        DrawCube((Vector3){ p.x, p.y + 0.0f, p.z }, 0.09f, 0.13f * f, 0.09f, (Color){ 255, 230, 140, 255 });
    }
}
