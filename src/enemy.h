/* enemy.h - Ashen Monks, Choir Wraiths, Ember Priests, the Red Abbot, and the priests' fireballs.
 * Enemies don't touch the player directly: Enemy_Update reports what happened in an
 * EnemyEvents struct and the game applies damage, sounds and camera shake. */
#ifndef ENEMY_H
#define ENEMY_H

#include "raylib.h"
#include "config.h"
#include "world.h"

typedef enum { EN_MONK = 0, EN_WRAITH, EN_PRIEST, EN_ABBOT, EN_TYPE_COUNT } EnemyType;

typedef enum { ATK_MELEE = 0, ATK_BOLT, ATK_RING } AttackKind;

typedef struct {
    bool      alive;
    EnemyType type;
    Vector3   pos, spawnPos;
    float     yaw, spawnYaw;
    int       hp, maxHp;
    float     speed, sight, reach, size;
    bool      alerted;        /* has noticed the player (stays alerted) */
    bool      scared;         /* ghost: scare already played */
    bool      summoned;       /* queen: ghosts already summoned */
    bool      visible;        /* ghost: in the light, can be hurt */
    float     windup;         /* < 0: not attacking; else seconds into the wind-up */
    AttackKind attack;        /* what the current wind-up will do */
    float     cooldown;       /* time until the next melee attack */
    float     fireTimer;      /* witch: time until next bolt */
    float     ringTimer;      /* queen: time until next bolt ring */
    float     strafeTimer, strafeDir;
    float     moanTimer;
    Vector3   knock;          /* knockback velocity (decays) */
    float     flash;          /* hit flash timer */
    float     walkPhase, walkAmount, time, seed;
    int       lastSwingId;    /* (unused since the wand) */
    float     twitchTimer;    /* time until the next twitch */
    float     twitchTime;     /* > 0 while the head is twisted */
    float     twitchYaw, twitchRoll;
} Enemy;

typedef struct {
    bool    active;
    Vector3 pos, vel;
    float   life, spin;
} Bolt;

typedef struct {
    int     playerHits;       /* attacks that landed on the player this frame */
    Vector3 hitFrom;
    bool    scare;            /* a ghost noticed the player up close */
    bool    fired;            /* a bolt was fired */
    bool    moan;             /* a ghost moaned */
    bool    summon;           /* the queen calls her ghosts */
    Vector3 summonPos;
} EnemyEvents;

typedef struct {
    const World      *world;
    const WingConfig *wing;
    Vector3           playerPos;
    float             lightRadius;    /* player light radius (ghost visibility) */
} EnemyEnv;

EnemyType Enemy_TypeFromChar(char c);
void Enemy_Spawn(Enemy *e, EnemyType type, Vector3 pos, float yaw, const WingConfig *wing);
void Enemy_Update(Enemy *e, const EnemyEnv *env, Bolt *bolts, EnemyEvents *ev, float dt);
void Enemies_Separate(Enemy *list, int count, const World *w, Vector3 playerPos);
/* Sword hit from `from`. Returns true if the enemy died. Ghosts in the dark take no damage. */
bool Enemy_Hurt(Enemy *e, int damage, Vector3 from);
bool Enemy_CanBeHurt(const Enemy *e);
void Enemy_ResetToSpawn(Enemy *e);
void Enemy_Draw(const Enemy *e, const World *w);
const char *Enemy_Name(EnemyType type);

void Bolt_Spawn(Bolt *bolts, Vector3 pos, Vector3 dir);
/* Moves bolts; counts hits on the player in ev->playerHits. */
void Bolts_Update(Bolt *bolts, const World *w, Vector3 playerPos, EnemyEvents *ev, float dt);
void Bolts_Draw(const Bolt *bolts);

#endif
