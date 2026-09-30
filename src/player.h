#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include "raylib.h"
#include "map.h"

typedef struct {
    Vector2 pos, facing, knock, dashDir;
    int   hp, maxHp, keys, swingId;
    float attackTimer, attackCooldown;
    float invuln;
    float dashTimer, dashCooldown;
} Player;

void Player_Init(Player *p, Vector2 pos);
void Player_Update(Player *p, const Tilemap *map, float dt);
bool Player_Hurt(Player *p, int dmg, Vector2 from);   /* false if ignored (i-frames) */
void Player_Draw(const Player *p);

#endif
