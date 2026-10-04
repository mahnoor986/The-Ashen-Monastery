/* render.h - draws the 3D world: chunk meshes, exit doors and torch flames.
 * (Phase 2 adds the fog/light shader here.) */
#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"
#include "world.h"

void Render_Init(void);                                /* after Textures_Init */
void Render_Shutdown(void);
void Render_DrawWorld(const World *w, float time);     /* call inside BeginMode3D */

#endif
