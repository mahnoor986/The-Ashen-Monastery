#ifndef ENEMY_H
#define ENEMY_H

#include <stdbool.h>
#include "raylib.h"
#include "draw.h"
#include "map.h"
#include "player.h"

typedef struct {
    bool  active;
    EntityType type;            /* ENT_SKELETON / GHOUL / BAT / CULTIST / BOSS */
    Vector2 pos, facing, knock;
    int   hp, maxHp, damage;
    float radius, speed;
    float sightRange, fovCos;   /* vision cone */
    float knockResist;          /* 0..1 */
    float hitFlash, atkTimer, shootTimer, retreatTimer;
    float baseAngle, seed, time;
    bool  alerted;
    int   lastHitSwing;
} Enemy;

typedef struct {
    bool  active;
    Vector2 pos, vel;
    float life;
    int   damage;
} Projectile;

void Enemy_Spawn(Enemy *e, EntityType type, Vector2 pos, float baseAngle);
void Enemy_Update(Enemy *e, const Tilemap *map, Player *pl, Projectile *projs, float dt);
bool Enemy_Hurt(Enemy *e, int dmg, Vector2 from);       /* true if it died */
void Enemy_Separate(Enemy *list, int n, const Tilemap *map);
void Enemy_Draw(const Enemy *e);

void Projectile_Spawn(Projectile *projs, Vector2 pos, Vector2 dir, float speed, int dmg);
void Projectile_UpdateAll(Projectile *projs, const Tilemap *map, Player *pl, float dt);
void Projectile_DrawAll(const Projectile *projs);

#endif
