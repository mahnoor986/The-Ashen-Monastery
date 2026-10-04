/* render.h - the world shader (fog, ambient, flickering torch light, player light) and
 * drawing of the 3D world: chunk meshes, exit doors and torch flames. */
#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"
#include "config.h"
#include "world.h"

void   Render_Init(void);           /* after Textures_Init; falls back to raylib's shader on failure */
void   Render_Shutdown(void);
bool   Render_HasShader(void);
Shader Render_Shader(void);         /* world shader (or raylib's default) for character materials */
Material Render_WorldMaterial(void);

/* Per-frame uniforms: camera, fog, ambient, flicker and the player light for this wing. */
void Render_BeginFrame(const WingConfig *wing, Camera3D cam, Vector3 playerPos, float time);
/* Switch how the next DrawMesh calls are lit. */
void Render_UseWorldLight(void);                 /* baked vertex light (world chunks) */
void Render_UseEntityLight(Vector3 light);       /* a character standing in this torch light */
void Render_SetEmissive(bool on);                /* glowing: no lighting, only fog */
void Render_SetEmissiveMode(int mode);           /* 0 lit, 1 glowing, 2 glowing + no fog (eyes) */
void Render_SetFlare(float amount);              /* 0..1: torches flare up (bell toll) */
void Render_SetWarm(bool on);                    /* Sanctum: golden fog + ambient instead of red */
void Render_SetFlash(Vector3 pos, Vector3 color, float radius);   /* lightning flash light */

void Render_DrawWorld(const World *w, float time);    /* call inside BeginMode3D */
void Render_DrawWorldTransparent(const World *w);     /* cobwebs etc., after opaque things */
Material Render_Material(int mat);                    /* world material (MaterialId) */
/* A treasure chest; `lid` 0 = closed .. 1 = open (an open chest glows gold inside). */
void Render_DrawChest(Vector3 pos, float yaw, float lid, Vector3 light);
float Render_Flicker(float time);

#endif
