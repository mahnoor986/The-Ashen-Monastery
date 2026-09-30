#ifndef LIGHTING_H
#define LIGHTING_H

#include "raylib.h"
#include "map.h"
#include "player.h"
#include "enemy.h"

void Lighting_Init(void);
void Lighting_Shutdown(void);

/* Call AFTER EndMode2D(): darkens the frame and cuts flickering pools of light. */
void Lighting_Draw(const Tilemap *map, const Player *pl, const Projectile *projs, Camera2D cam, float time);

/* Soft additive glow at a SCREEN position (wrap in BeginBlendMode(BLEND_ADDITIVE)). */
void Lighting_Glow(Vector2 screenPos, float radius, Color tint);

#endif
