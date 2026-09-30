/* draw.h - the placeholder-art abstraction.
 *
 * Gameplay code NEVER draws shapes itself; it calls DrawEntity(type, pos, facing).
 * When real sprites arrive, only draw.c changes.  pos = CENTER of the entity.
 */
#ifndef DRAW_H
#define DRAW_H

#include "raylib.h"

typedef enum {
    ENT_NONE = 0,
    ENT_PLAYER,
    ENT_SKELETON,
    ENT_GHOUL,
    ENT_BAT,
    ENT_CULTIST,
    ENT_BOSS,
    ENT_PROJECTILE,
    ENT_KEY,
    ENT_POTION,
    ENT_TORCH
} EntityType;

void DrawEntity(EntityType type, Vector2 pos, Vector2 facing);
/* flash: 0..1, blends the entity toward white (hit feedback) */
void DrawEntityFx(EntityType type, Vector2 pos, Vector2 facing, float flash);

/* Melee swing visual. progress: 0 (swing start) .. 1 (swing end) */
void DrawAttackArc(Vector2 pos, Vector2 facing, float progress);

/* Filled triangle pointing along dir (winding handled for you). */
void DrawTriFacing(Vector2 c, Vector2 dir, float len, float halfWidth, Color col);

#endif
