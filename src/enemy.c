/* enemy.c - enemy AI, hex bolts and enemy drawing (see enemy.h).
 * Idle enemies sweep their gaze around their starting direction. They notice the player
 * by sight (range + view cone + line of sight) or by hearing (close, any direction), and
 * then stay alerted. Every attack has a wind-up (glow red, stand still) before it lands. */
#include <math.h>
#include <string.h>
#include "enemy.h"
#include "character.h"
#include "render.h"
#include "raymath.h"

static float AngleDiff(float from, float to) { return Wrap(to - from, -PI, PI); }

static float Rand01(void) { return GetRandomValue(0, 1000) / 1000.0f; }

const char *Enemy_Name(EnemyType type)
{
    static const char *names[EN_TYPE_COUNT] = { "Skeleton", "Ghost", "Witch", "The Witch Queen" };
    return ((int)type >= 0 && type < EN_TYPE_COUNT) ? names[type] : "?";
}

EnemyType Enemy_TypeFromChar(char c)
{
    switch (c) {
    case 'g': return EN_GHOST;
    case 'w': return EN_WITCH;
    case 'Q': return EN_QUEEN;
    default:  return EN_SKELETON;
    }
}

void Enemy_Spawn(Enemy *e, EnemyType type, Vector3 pos, float yaw, const WingConfig *wing)
{
    memset(e, 0, sizeof(*e));
    e->alive = true;
    e->type = type;
    e->pos = e->spawnPos = pos;
    e->yaw = e->spawnYaw = yaw;
    e->windup = -1.0f;
    e->lastSwingId = -1;
    e->seed = Rand01() * 6.28f;
    e->size = ENEMY_SIZE;
    e->strafeDir = (Rand01() < 0.5f) ? -1.0f : 1.0f;
    e->moanTimer = 3.0f + Rand01() * 6.0f;
    switch (type) {
    case EN_SKELETON: e->hp = SKELETON_HP; e->speed = SKELETON_SPEED; e->sight = SKELETON_SIGHT; e->reach = SKELETON_REACH; break;
    case EN_GHOST:    e->hp = GHOST_HP;    e->speed = GHOST_SPEED;    e->sight = GHOST_SIGHT;    e->reach = GHOST_REACH; break;
    case EN_WITCH:    e->hp = WITCH_HP;    e->speed = WITCH_SPEED;    e->sight = WITCH_SIGHT;    e->reach = 0.0f;
                      e->fireTimer = 1.0f; break;
    case EN_QUEEN:    e->hp = QUEEN_HP;    e->speed = QUEEN_SPEED;    e->sight = QUEEN_SIGHT;    e->reach = QUEEN_REACH;
                      e->size = QUEEN_SIZE; e->ringTimer = QUEEN_RING_TIME; break;
    default: break;
    }
    e->maxHp = e->hp;
    e->speed *= wing->enemySpeedMul;
    e->sight *= wing->sightMul;
}

void Enemy_ResetToSpawn(Enemy *e)
{
    e->pos = e->spawnPos;
    e->yaw = e->spawnYaw;
    e->alerted = false;
    e->scared = false;
    e->windup = -1.0f;
    e->cooldown = 1.0f;
    e->knock = (Vector3){ 0 };
    e->flash = 0.0f;
}

bool Enemy_CanBeHurt(const Enemy *e)
{
    return e->alive && (e->type != EN_GHOST || e->visible);
}

bool Enemy_Hurt(Enemy *e, int damage, Vector3 from)
{
    Vector3 away;
    if (!Enemy_CanBeHurt(e)) return false;
    e->hp -= damage;
    e->flash = 0.12f;
    e->alerted = true;
    away = Vector3Subtract(e->pos, from);
    away.y = 0.0f;
    if (Vector3Length(away) > 0.01f)
        e->knock = Vector3Scale(Vector3Normalize(away), SWORD_KNOCKBACK * (e->type == EN_QUEEN ? QUEEN_KNOCK_MUL : 1.0f));
    if (e->type != EN_QUEEN) e->windup = -1.0f;   /* a hit staggers normal enemies */
    if (e->hp <= 0) { e->alive = false; return true; }
    return false;
}

static float WindupTime(const Enemy *e)
{
    if (e->attack == ATK_BOLT) return WITCH_WINDUP;
    if (e->type == EN_QUEEN) return QUEEN_WINDUP;
    return ENEMY_WINDUP;
}

/* The wind-up is over: the attack happens now. */
static void ResolveAttack(Enemy *e, const EnemyEnv *env, Bolt *bolts, EnemyEvents *ev, float dist)
{
    Vector3 fwd = { sinf(e->yaw), 0.0f, cosf(e->yaw) };
    Vector3 hand = Vector3Add(e->pos, Vector3Scale(fwd, 0.5f));
    int k;
    hand.y = BOLT_HEIGHT;

    switch (e->attack) {
    case ATK_MELEE:
        if (dist <= e->reach + 0.35f) {        /* only lands if the player is still in range */
            ev->playerHits++;
            ev->hitFrom = e->pos;
        }
        e->cooldown = e->type == EN_QUEEN ? QUEEN_COOLDOWN : (e->type == EN_GHOST ? GHOST_COOLDOWN : SKELETON_COOLDOWN);
        break;
    case ATK_BOLT: {
        Vector3 aim = Vector3Subtract((Vector3){ env->playerPos.x, BOLT_HEIGHT, env->playerPos.z }, hand);
        Bolt_Spawn(bolts, hand, aim);
        ev->fired = true;
        break;
    }
    case ATK_RING:
        for (k = 0; k < 8; k++) {
            float a = e->yaw + k * PI / 4.0f;
            Vector3 c = { e->pos.x, BOLT_HEIGHT, e->pos.z };
            Bolt_Spawn(bolts, c, (Vector3){ sinf(a), 0.0f, cosf(a) });
        }
        ev->fired = true;
        e->cooldown = fmaxf(e->cooldown, 0.6f);
        break;
    }
}

static void StartWindup(Enemy *e, AttackKind kind)
{
    e->windup = 0.0f;
    e->attack = kind;
}

void Enemy_Update(Enemy *e, const EnemyEnv *env, Bolt *bolts, EnemyEvents *ev, float dt)
{
    const World *w = env->world;
    Vector3 to, move = { 0 }, before;
    float dist, yawTo;

    if (!e->alive) return;
    e->time += dt;
    if (e->flash > 0.0f) e->flash -= dt;
    if (e->cooldown > 0.0f) e->cooldown -= dt;

    to = Vector3Subtract(env->playerPos, e->pos);
    to.y = 0.0f;
    dist = Vector3Length(to);
    yawTo = atan2f(to.x, to.z);

    /* ghosts can only be hurt (and seen clearly) in the light */
    if (e->type == EN_GHOST) {
        Vector3 l = World_LightAt(w, (Vector3){ e->pos.x, 1.0f, e->pos.z });
        e->visible = dist <= env->lightRadius * 0.85f || (l.x + l.y + l.z) / 3.0f > GHOST_LIGHT_NEEDED;
        if (dist < GHOST_MOAN_RANGE) {
            e->moanTimer -= dt;
            if (e->moanTimer <= 0.0f) { ev->moan = true; e->moanTimer = 7.0f + Rand01() * 8.0f; }
        }
    }

    /* ---- perception ---- */
    if (!e->alerted) {
        bool notice = false;
        if (dist < e->sight) {
            bool los = e->type == EN_GHOST ||
                       World_LineOfSight(w, (Vector3){ e->pos.x, 1.5f, e->pos.z }, (Vector3){ env->playerPos.x, 1.5f, env->playerPos.z });
            bool inCone = fabsf(AngleDiff(e->yaw, yawTo)) < ENEMY_FOV_DEG * 0.5f * DEG2RAD;
            notice = los && (dist < ENEMY_HEAR_RANGE || inCone);
        }
        if (notice) e->alerted = true;
        else e->yaw = e->spawnYaw + sinf(e->time * 0.9f + e->seed) * 0.9f;   /* sweep the gaze */
    }
    if (e->alerted && e->type == EN_GHOST && !e->scared && dist < GHOST_SCARE_RANGE) {
        e->scared = true;
        ev->scare = true;
    }

    /* ---- behaviour ---- */
    if (e->alerted) {
        if (e->windup >= 0.0f) {
            /* winding up: stand still, glow red, face the player */
            e->windup += dt;
            if (e->attack != ATK_RING) e->yaw += AngleDiff(e->yaw, yawTo) * fminf(1.0f, 4.0f * dt);
            if (e->windup >= WindupTime(e)) {
                ResolveAttack(e, env, bolts, ev, dist);
                e->windup = -1.0f;
            }
        } else {
            Vector3 dir = dist > 0.01f ? Vector3Scale(to, 1.0f / dist) : (Vector3){ 0 };
            e->yaw = yawTo;
            switch (e->type) {
            case EN_SKELETON:
            case EN_GHOST:
                if (dist <= e->reach && e->cooldown <= 0.0f) StartWindup(e, ATK_MELEE);
                else if (dist > e->reach * 0.75f) move = Vector3Scale(dir, e->speed);
                break;
            case EN_WITCH: {
                Vector3 side = { -dir.z, 0.0f, dir.x };
                e->fireTimer -= dt * env->wing->witchFireMul;
                e->strafeTimer -= dt;
                if (e->strafeTimer <= 0.0f) { e->strafeDir = -e->strafeDir; e->strafeTimer = 1.5f + Rand01() * 1.5f; }
                if (dist < WITCH_MIN_DIST)      move = Vector3Scale(dir, -e->speed);
                else if (dist > WITCH_MAX_DIST) move = Vector3Scale(dir, e->speed);
                else                            move = Vector3Scale(side, e->speed * 0.6f * e->strafeDir);
                if (e->fireTimer <= 0.0f && dist < e->sight * 1.4f &&
                    World_LineOfSight(w, (Vector3){ e->pos.x, 1.3f, e->pos.z }, (Vector3){ env->playerPos.x, 1.3f, env->playerPos.z })) {
                    StartWindup(e, ATK_BOLT);
                    e->fireTimer = WITCH_FIRE_TIME;
                }
                break;
            }
            case EN_QUEEN:
                e->ringTimer -= dt;
                if (!e->summoned && e->hp <= e->maxHp / 2) {
                    e->summoned = true;
                    ev->summon = true;
                    ev->summonPos = e->pos;
                }
                if (e->ringTimer <= 0.0f) { StartWindup(e, ATK_RING); e->ringTimer = QUEEN_RING_TIME; }
                else if (dist <= e->reach && e->cooldown <= 0.0f) StartWindup(e, ATK_MELEE);
                else if (dist > e->reach * 0.7f) move = Vector3Scale(dir, e->speed);
                break;
            default: break;
            }
        }
    }

    /* ---- movement (+ knockback) ---- */
    move = Vector3Add(move, e->knock);
    e->knock = Vector3Scale(e->knock, fmaxf(0.0f, 1.0f - 6.0f * dt));
    before = e->pos;
    if (e->type == EN_GHOST) {
        /* ghosts float through walls but stay inside the manor */
        e->pos.x = Clamp(e->pos.x + move.x * dt, 1.3f, w->w - 1.3f);
        e->pos.z = Clamp(e->pos.z + move.z * dt, 1.3f, w->h - 1.3f);
    } else {
        World_Move(w, &e->pos, e->size, Vector3Scale(move, dt));
        /* witch hit a wall while strafing: go the other way */
        if (e->type == EN_WITCH && Vector3Distance(before, e->pos) < 0.2f * Vector3Length(move) * dt) e->strafeDir = -e->strafeDir;
    }
    e->walkAmount = fminf(1.0f, Vector3Distance(before, e->pos) / fmaxf(dt, 0.0001f) / 2.0f);
    e->walkPhase += Vector3Distance(before, e->pos) * 2.6f;
}

void Enemies_Separate(Enemy *list, int count, const World *w, Vector3 playerPos)
{
    int i, j;
    for (i = 0; i < count; i++) {
        Enemy *a = &list[i];
        Vector3 d;
        float dist, minD;
        if (!a->alive) continue;
        for (j = i + 1; j < count; j++) {
            Enemy *b = &list[j];
            Vector3 push;
            if (!b->alive) continue;
            d = Vector3Subtract(b->pos, a->pos);
            d.y = 0.0f;
            dist = Vector3Length(d);
            minD = (a->size + b->size) * 0.5f + 0.15f;
            if (dist >= minD || dist < 0.001f) continue;
            push = Vector3Scale(d, (minD - dist) * 0.5f / dist);
            if (a->type == EN_GHOST) a->pos = Vector3Subtract(a->pos, push); else World_Move(w, &a->pos, a->size, Vector3Negate(push));
            if (b->type == EN_GHOST) b->pos = Vector3Add(b->pos, push);      else World_Move(w, &b->pos, b->size, push);
        }
        /* don't stand inside the player */
        d = Vector3Subtract(a->pos, playerPos);
        d.y = 0.0f;
        dist = Vector3Length(d);
        minD = (a->size + PLAYER_SIZE) * 0.5f + 0.1f;
        if (dist < minD && dist > 0.001f) {
            Vector3 push = Vector3Scale(d, (minD - dist) / dist);
            if (a->type == EN_GHOST) a->pos = Vector3Add(a->pos, push); else World_Move(w, &a->pos, a->size, push);
        }
    }
}

void Enemy_Draw(const Enemy *e, const World *w)
{
    CharPose pose = { 0 };
    if (!e->alive) return;
    pose.pos = e->pos;
    pose.yaw = e->yaw;
    pose.walkPhase = e->walkPhase;
    pose.walkAmount = e->walkAmount;
    pose.time = e->time + e->seed;
    pose.swing = -1.0f;
    pose.windup = e->windup >= 0.0f ? fminf(1.0f, e->windup / WindupTime(e)) : 0.0f;
    pose.flash = e->flash > 0.0f ? e->flash / 0.12f : 0.0f;
    pose.alerted = e->alerted;
    Render_UseEntityLight(World_LightAt(w, (Vector3){ e->pos.x, 1.0f, e->pos.z }));
    switch (e->type) {
    case EN_SKELETON: Character_DrawSkeleton(&pose); break;
    case EN_WITCH:    Character_DrawWitch(&pose); break;
    case EN_QUEEN: {
        /* the Queen glows faintly purple so she is always readable in her dark throne room */
        Vector3 l = World_LightAt(w, (Vector3){ e->pos.x, 1.0f, e->pos.z });
        Render_UseEntityLight((Vector3){ fmaxf(l.x, 0.45f), fmaxf(l.y, 0.30f), fmaxf(l.z, 0.55f) });
        Character_DrawQueen(&pose);
        break;
    }
    case EN_GHOST:
        pose.pos.y = 0.35f + 0.1f * sinf(e->time * 2.0f + e->seed);      /* floats 0.25 - 0.45 above the floor */
        pose.alpha = e->visible ? GHOST_ALPHA : GHOST_FAINT_ALPHA;
        if (e->windup >= 0.0f) pose.alpha = fmaxf(pose.alpha, 0.35f);    /* always show the telegraph */
        Render_UseEntityLight((Vector3){ 0.55f, 0.6f, 0.75f });          /* ghosts glow faintly by themselves */
        Character_DrawGhost(&pose);
        break;
    default: break;
    }
}

/* ------------------------------------------------------------ hex bolts */

void Bolt_Spawn(Bolt *bolts, Vector3 pos, Vector3 dir)
{
    int i;
    dir.y = 0.0f;
    if (Vector3Length(dir) < 0.001f) return;
    for (i = 0; i < MAX_BOLTS; i++) {
        if (bolts[i].active) continue;
        bolts[i].active = true;
        bolts[i].pos = pos;
        bolts[i].vel = Vector3Scale(Vector3Normalize(dir), BOLT_SPEED);
        bolts[i].life = BOLT_LIFE;
        bolts[i].spin = 0.0f;
        return;
    }
}

void Bolts_Update(Bolt *bolts, const World *w, Vector3 playerPos, EnemyEvents *ev, float dt)
{
    int i;
    for (i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &bolts[i];
        float dx, dz;
        if (!b->active) continue;
        b->pos = Vector3Add(b->pos, Vector3Scale(b->vel, dt));
        b->life -= dt;
        b->spin += dt * 6.0f;
        if (b->life <= 0.0f || World_IsWallCell(w, (int)floorf(b->pos.x), (int)floorf(b->pos.z))) { b->active = false; continue; }
        dx = b->pos.x - playerPos.x;
        dz = b->pos.z - playerPos.z;
        if (dx * dx + dz * dz < BOLT_HIT_RADIUS * BOLT_HIT_RADIUS) {
            ev->playerHits++;
            ev->hitFrom = Vector3Subtract(b->pos, b->vel);
            b->active = false;
        }
    }
}

void Bolts_Draw(const Bolt *bolts)
{
    int i;
    for (i = 0; i < MAX_BOLTS; i++)
        if (bolts[i].active) Character_DrawBolt(bolts[i].pos, bolts[i].spin);
}
