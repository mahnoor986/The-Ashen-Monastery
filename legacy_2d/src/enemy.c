#include <math.h>
#include <string.h>
#include "enemy.h"
#include "config.h"
#include "vec.h"

/* ---------------------------------------------------------------- spawn */

void Enemy_Spawn(Enemy *e, EntityType type, Vector2 pos, float baseAngle)
{
    float fovDeg = 120.0f;
    memset(e, 0, sizeof(*e));
    e->active = true;
    e->type = type;
    e->pos = pos;
    e->baseAngle = baseAngle;
    e->facing = V2FromAngle(baseAngle);
    e->lastHitSwing = -1;
    e->seed = GetRandomValue(0, 628) / 100.0f;

    switch (type) {
    case ENT_SKELETON: e->hp = 3;  e->radius = 11; e->speed = 62;  e->damage = 10; e->sightRange = 200; fovDeg = 100; break;
    case ENT_GHOUL:    e->hp = 5;  e->radius = 12; e->speed = 44;  e->damage = 18; e->sightRange = 170; fovDeg = 140; e->knockResist = 0.4f; break;
    case ENT_BAT:      e->hp = 1;  e->radius = 8;  e->speed = 115; e->damage = 6;  e->sightRange = 230; fovDeg = 360; break;
    case ENT_CULTIST:  e->hp = 3;  e->radius = 10; e->speed = 55;  e->damage = 10; e->sightRange = 260; fovDeg = 120; e->shootTimer = 1.0f; break;
    case ENT_BOSS:     e->hp = 24; e->radius = 28; e->speed = 58;  e->damage = 25; e->sightRange = 400; fovDeg = 360; e->knockResist = 0.85f; e->shootTimer = 3.0f; break;
    default:           e->hp = 1;  e->radius = 10; e->speed = 40;  e->damage = 5;  e->sightRange = 150; break;
    }
    e->maxHp = e->hp;
    e->fovCos = cosf(DEG2RAD * fovDeg * 0.5f);
}

bool Enemy_Hurt(Enemy *e, int dmg, Vector2 from)
{
    e->hp -= dmg;
    e->hitFlash = 0.12f;
    e->alerted = true;
    e->knock = V2Scale(V2Norm(V2Sub(e->pos, from)), ENEMY_KNOCKBACK * (1.0f - e->knockResist));
    if (e->hp <= 0) { e->active = false; return true; }
    return false;
}

/* ------------------------------------------------------------ projectiles */

void Projectile_Spawn(Projectile *projs, Vector2 pos, Vector2 dir, float speed, int dmg)
{
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        if (!projs[i].active) {
            projs[i].active = true;
            projs[i].pos = pos;
            projs[i].vel = V2Scale(V2Norm(dir), speed);
            projs[i].life = 4.0f;
            projs[i].damage = dmg;
            return;
        }
    }
}

void Projectile_UpdateAll(Projectile *projs, const Tilemap *map, Player *pl, float dt)
{
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &projs[i];
        if (!p->active) continue;
        p->pos = V2Add(p->pos, V2Scale(p->vel, dt));
        p->life -= dt;
        if (p->life <= 0.0f || Map_IsSolidAt(map, p->pos)) { p->active = false; continue; }
        if (pl->invuln <= 0.0f && V2Dist(p->pos, pl->pos) < PLAYER_RADIUS + 4.0f) {
            Player_Hurt(pl, p->damage, V2Sub(p->pos, p->vel));
            p->active = false;
        }
    }
}

void Projectile_DrawAll(const Projectile *projs)
{
    int i;
    for (i = 0; i < MAX_PROJECTILES; i++)
        if (projs[i].active) DrawEntity(ENT_PROJECTILE, projs[i].pos, projs[i].vel);
}

/* --------------------------------------------------------------------- AI */

static void TryMelee(Enemy *e, Player *pl, float dist, float cooldown)
{
    if (e->atkTimer > 0.0f) return;
    if (dist > e->radius + PLAYER_RADIUS + 6.0f) return;
    if (Player_Hurt(pl, e->damage, e->pos)) {
        e->atkTimer = cooldown;
        e->retreatTimer = 0.45f;
    } else {
        e->atkTimer = 0.25f;
    }
}

void Enemy_Update(Enemy *e, const Tilemap *map, Player *pl, Projectile *projs, float dt)
{
    Vector2 move = V2(0.0f, 0.0f), toP, dir;
    float dist, size;

    if (!e->active) return;

    e->time += dt;
    if (e->hitFlash     > 0.0f) e->hitFlash     -= dt;
    if (e->atkTimer     > 0.0f) e->atkTimer     -= dt;
    if (e->shootTimer   > 0.0f) e->shootTimer   -= dt;
    if (e->retreatTimer > 0.0f) e->retreatTimer -= dt;

    toP  = V2Sub(pl->pos, e->pos);
    dist = V2Len(toP);
    dir  = V2Norm(toP);

    /* --- perception: idle enemies sweep their gaze until they notice you --- */
    if (!e->alerted) {
        bool noticed = false;
        if (dist < e->sightRange && Map_HasLineOfSight(map, e->pos, pl->pos)) {
            if (dist < ENEMY_HEAR_RANGE || V2Dot(e->facing, dir) >= e->fovCos) noticed = true;
        }
        if (noticed) e->alerted = true;
        else e->facing = V2FromAngle(e->baseAngle + sinf(e->time * 0.9f + e->seed) * 0.9f);
    }

    /* --- behaviour --- */
    if (e->alerted) {
        switch (e->type) {
        case ENT_SKELETON:
        case ENT_GHOUL:
            e->facing = dir;
            if (dist > e->radius + PLAYER_RADIUS + 2.0f) move = V2Scale(dir, e->speed * dt);
            TryMelee(e, pl, dist, (e->type == ENT_GHOUL) ? 1.2f : 1.0f);
            break;

        case ENT_BAT: {
            float ang = atan2f(dir.y, dir.x) + sinf(e->time * 7.0f + e->seed) * 1.3f;
            Vector2 md = V2FromAngle(ang);
            float spd = e->speed * (1.0f + 0.4f * sinf(e->time * 3.0f + e->seed));
            if (e->retreatTimer > 0.0f) md = V2Scale(dir, -1.0f);
            move = V2Scale(md, spd * dt);
            e->facing = md;
            TryMelee(e, pl, dist, 0.7f);
            break;
        }

        case ENT_CULTIST: {
            Vector2 perp = V2(-dir.y, dir.x);
            float side = (sinf(e->time * 0.8f + e->seed) > 0.0f) ? 1.0f : -1.0f;
            e->facing = dir;
            if (dist < 130.0f)      move = V2Scale(dir, -e->speed * dt);
            else if (dist > 210.0f) move = V2Scale(dir,  e->speed * dt);
            else                    move = V2Scale(perp, side * e->speed * 0.5f * dt);
            if (e->shootTimer <= 0.0f && dist < 320.0f && Map_HasLineOfSight(map, e->pos, pl->pos)) {
                Projectile_Spawn(projs, e->pos, dir, 180.0f, e->damage);
                e->shootTimer = 1.8f;
            }
            break;
        }

        case ENT_BOSS:
            e->facing = dir;
            if (dist > e->radius + PLAYER_RADIUS + 2.0f) move = V2Scale(dir, e->speed * dt);
            TryMelee(e, pl, dist, 1.4f);
            if (e->shootTimer <= 0.0f) {          /* ring of 8 bolts */
                int k;
                for (k = 0; k < 8; k++)
                    Projectile_Spawn(projs, e->pos, V2FromAngle(e->time + k * (PI / 4.0f)), 130.0f, 12);
                e->shootTimer = 3.0f;
            }
            break;

        default: break;
        }
    }

    /* knockback */
    move = V2Add(move, V2Scale(e->knock, dt));
    e->knock = V2Scale(e->knock, Maxf(0.0f, 1.0f - KNOCKBACK_DAMPING * dt));

    size = e->radius * 1.6f;
    Map_Move(map, &e->pos, V2(size, size), move);
}

void Enemy_Separate(Enemy *list, int n, const Tilemap *map)
{
    int i, j;
    for (i = 0; i < n; i++) {
        if (!list[i].active) continue;
        for (j = i + 1; j < n; j++) {
            Vector2 d, push;
            float dist, minD, sa, sb;
            if (!list[j].active) continue;
            d = V2Sub(list[j].pos, list[i].pos);
            dist = V2Len(d);
            minD = list[i].radius + list[j].radius;
            if (dist >= minD || dist < 0.01f) continue;
            push = V2Scale(V2Norm(d), (minD - dist) * 0.5f);
            sa = list[i].radius * 1.6f; sb = list[j].radius * 1.6f;
            Map_Move(map, &list[i].pos, V2(sa, sa), V2Scale(push, -1.0f));
            Map_Move(map, &list[j].pos, V2(sb, sb), push);
        }
    }
}

void Enemy_Draw(const Enemy *e)
{
    if (!e->active) return;
    DrawEntityFx(e->type, e->pos, e->facing, (e->hitFlash > 0.0f) ? e->hitFlash / 0.12f : 0.0f);
    if (e->type != ENT_BOSS && e->hp < e->maxHp) {      /* tiny health bar */
        float w = 20.0f * (float)e->hp / (float)e->maxHp;
        DrawRectangle((int)(e->pos.x - 10), (int)(e->pos.y - e->radius - 7), 20, 3, (Color){ 20, 6, 8, 255 });
        DrawRectangle((int)(e->pos.x - 10), (int)(e->pos.y - e->radius - 7), (int)w, 3, COL_BLOOD);
    }
}
