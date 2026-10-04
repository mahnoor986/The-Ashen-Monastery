/* game.c - game flow (see game.h): the state machine, wing loading and progression, chests and
 * treasures, sword combat with aim assist, checkpoints and respawning, saving, and --autotest. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "screen.h"
#include "textures.h"
#include "render.h"
#include "post.h"
#include "character.h"
#include "audio.h"
#include "ui.h"
#include "rlgl.h"
#include "raymath.h"

const WingConfig WINGS[WING_COUNT] = WING_TABLE;
const char *const TREASURES[WING_COUNT][MAX_CHESTS] = TREASURE_TABLE;
const char *const WING_NAMES[WING_COUNT] = WING_NAME_TABLE;

static const char *const ORDINAL[WING_COUNT] = { "FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH" };

const char *Game_WingTitle(int wing)
{
    return TextFormat("THE %s BELL", ORDINAL[wing < 0 ? 0 : wing % WING_COUNT]);
}

const char *const OREN_LINES[OREN_LINE_COUNT] = {
    "Kael... the fifth bell has fallen. I heard it break from here.",
    "We thought the fire would take us all. Ilsa kept the candles lit. Tobin counted every toll.",
    "The Abbot rang those bells to keep the mountain burning. You walked through his fire, and you came back.",
    "Rest now. Tomorrow, you are no longer an apprentice.",
};

static const WingConfig SANCTUM = SANCTUM_CONFIG;

const char *const INTRO_LINES[INTRO_LINE_COUNT] = {
    "Ten nights ago, the Red Abbot cast five bells from the ashes of the dead.",
    "He hid his life in five cursed relics, and in the serpent he keeps caged in the tower.",
    "Master Oren and the other apprentices are trapped in the Sanctum at the summit.",
    "Climb, Kael. Destroy the relics. Break the bells.",
};

/* ============================================================ small helpers */

static float AngleDiff(float from, float to) { return Wrap(to - from, -PI, PI); }

static float YawTo(Vector3 from, Vector3 to) { return atan2f(to.x - from.x, to.z - from.z); }

static float FlatDist(Vector3 a, Vector3 b)
{
    float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

static Vector3 CellCenter(Cell c) { return (Vector3){ c.x + 0.5f, 0.0f, c.z + 0.5f }; }

static void PushBanner(Game *g, const char *title, const char *sub, Color color, float duration)
{
    Banner *b;
    if (g->bannerCount >= MAX_BANNERS) {               /* drop the oldest */
        memmove(&g->banners[0], &g->banners[1], sizeof(Banner) * (MAX_BANNERS - 1));
        g->bannerCount--;
    }
    /* the banner on screen makes way quickly for the new one */
    if (g->bannerCount > 0 && g->banners[0].duration - g->banners[0].time > 0.7f)
        g->banners[0].duration = g->banners[0].time + 0.7f;
    b = &g->banners[g->bannerCount++];
    memset(b, 0, sizeof(*b));
    strncpy(b->title, title, sizeof(b->title) - 1);
    strncpy(b->sub, sub ? sub : "", sizeof(b->sub) - 1);
    b->color = color;
    b->duration = duration;
}

static void Shake(Game *g, float amount)
{
    if (amount > g->shake) g->shake = amount;
}

static void SetMouseCaptured(Game *g, bool on)
{
    if (g->autotest || g->mouseLook == on) return;
    g->mouseLook = on;
    if (on) { DisableCursor(); g->mouseSkip = 2; }
    else EnableCursor();
}

static void SpawnParticles(Game *g, Vector3 pos, int count, Color color, float speed, float size, float life)
{
    int i, n = 0;
    for (i = 0; i < MAX_PARTICLES && n < count; i++) {
        Particle *p = &g->particles[i];
        float a, up;
        if (p->life > 0.0f) continue;
        a = GetRandomValue(0, 628) / 100.0f;
        up = GetRandomValue(20, 100) / 100.0f;
        p->pos = pos;
        p->vel = (Vector3){ cosf(a) * speed * up, speed * (0.6f + up), sinf(a) * speed * up };
        p->life = p->maxLife = life * (0.6f + 0.4f * up);
        p->size = size;
        p->color = color;
        n++;
    }
}

static void UpdateParticles(Game *g, float dt)
{
    int i;
    for (i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (p->life <= 0.0f) continue;
        p->life -= dt;
        p->vel.y -= 6.0f * dt;
        p->pos = Vector3Add(p->pos, Vector3Scale(p->vel, dt));
        if (p->pos.y < 0.03f) { p->pos.y = 0.03f; p->vel = Vector3Scale(p->vel, 0.4f); p->vel.y = 0.0f; }
    }
}

/* ============================================================ saving */

static int LoadSave(void)
{
    char *text;
    int wing = 0;
    if (!FileExists(SAVE_FILE)) return 0;
    text = LoadFileText(SAVE_FILE);
    if (text) { wing = atoi(text); UnloadFileText(text); }
    return (wing >= 1 && wing <= WING_COUNT) ? wing : 0;
}

static void WriteSave(Game *g, int wing1)
{
    if (g->autotest || wing1 <= g->savedWing) return;
    g->savedWing = wing1;
    SaveFileText(SAVE_FILE, (char *)TextFormat("%d\n", wing1));
}

/* ============================================================ menus */

int Game_MenuOptions(const Game *g, int actions[4], const char *labels[4])
{
    static char cont[48];
    int n = 0;
    actions[n] = 0; labels[n++] = "New Game";
    if (g->savedWing > 0) {
        snprintf(cont, sizeof(cont), "Continue (Wing %d)", g->savedWing);
        actions[n] = 1; labels[n++] = cont;
    }
    actions[n] = 2; labels[n++] = "Quit";
    return n;
}

static void EnterMenu(Game *g)
{
    if (Game_LoadWing(g, 0)) g->bannerCount = 0;   /* wing 1 is the menu background */
    g->state = STATE_MENU;
    g->menuSel = 0;
    g->fade = 1.0f;
    SetMouseCaptured(g, false);
}

/* ============================================================ wing flow */

/* Seconds until the next lightning strike (wing 5 is stormy). */
static float NextLightning(int wing)
{
    float t = LIGHTNING_MIN + (LIGHTNING_MAX - LIGHTNING_MIN) * GetRandomValue(0, 100) / 100.0f;
    return wing == WING_COUNT - 1 ? t * LIGHTNING_STORMY : t;
}

/* Lightning brightness 0..1: two quick flashes within 0.4 s. */
static float LightningFlash(float age)
{
    float a = age < 0.0f ? 0.0f : fmaxf(0.0f, 1.0f - fabsf(age - 0.05f) / 0.08f);
    float b = age < 0.0f ? 0.0f : 0.8f * fmaxf(0.0f, 1.0f - fabsf(age - 0.3f) / 0.1f);
    return fminf(1.0f, a + b);
}

/* Distant bell tolls: every 25-40 s with all five bells, less often as they break, never after. */
static float NextToll(const Game *g)
{
    int left = WING_COUNT - g->wing - (g->world.exitOpen ? 1 : 0);
    float t = BELL_TOLL_MIN + (BELL_TOLL_MAX - BELL_TOLL_MIN) * GetRandomValue(0, 100) / 100.0f;
    return left <= 0 ? 1e9f : t * (float)WING_COUNT / (float)left;
}

bool Game_LoadWing(Game *g, int wing)
{
    int i;
    bool god = g->player.god;
    if (wing < 0 || wing >= WING_COUNT) return false;
    if (g->worldLoaded) World_Unload(&g->world);
    g->worldLoaded = false;
    if (!World_Load(&g->world, WINGS[wing].file, WINGS[wing].chests, true)) return false;
    g->world.theme = wing;
    World_BuildMeshes(&g->world);
    g->worldLoaded = true;
    g->wing = wing;
    g->sanctum = false;
    g->npcCount = 0;
    Render_ResetLights();
    Audio_SetCalm(false);
    g->wingTime = 0.0f;

    /* enemies, bolts, particles */
    g->enemyCount = 0;
    for (i = 0; i < g->world.spawnCount && g->enemyCount < MAX_ENEMIES; i++) {
        const Spawn *s = &g->world.spawns[i];
        Enemy_Spawn(&g->enemies[g->enemyCount++], Enemy_TypeFromChar(s->type), s->pos, s->yaw, &WINGS[wing]);
    }
    memset(g->bolts, 0, sizeof(g->bolts));
    memset(g->particles, 0, sizeof(g->particles));

    /* chests */
    memset(g->chestOpened, 0, sizeof(g->chestOpened));
    memset(g->chestLid, 0, sizeof(g->chestLid));
    memset(g->found[wing], 0, sizeof(g->found[wing]));
    g->chestsOpened = 0;
    g->useChest = -1;
    g->useHold = 0.0f;
    g->abbotDead = false;
    g->abbotAlerted = false;

    /* player (full hearts at the start of every wing) */
    Player_Init(&g->player, g->world.start, g->world.startYaw);
    g->player.god = god;
    g->checkpoint = g->world.start;
    g->checkpointYaw = g->world.startYaw;
    CameraRig_Init(&g->rig, g->world.startYaw);
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);

    Atmos_Reset(&g->atmos, g->world.start);
    g->tollTimer = BELL_TOLL_MIN + (BELL_TOLL_MAX - BELL_TOLL_MIN) * GetRandomValue(0, 100) / 100.0f;
    g->lightningTimer = NextLightning(wing) * 0.4f;     /* the first storm comes sooner */
    g->lightningAge = -1.0f;
    g->torchFlare = 0.0f;

    g->bannerCount = 0;
    PushBanner(g, TextFormat("%s \xE2\x80\x94 %s", Game_WingTitle(wing), g->world.name),
               wing == 0 ? "Find the Ward Seal in every reliquary to break the bell" : "", COL_BONE, BANNER_TIME + 1.0f);
    g->fade = 1.0f;
    g->leaving = false;
    g->hurtFlash = 0.0f;
    g->shake = 0.0f;
    return true;
}

void Game_NewGame(Game *g, int wing)
{
    memset(g->found, 0, sizeof(g->found));
    g->enemiesSlain = 0;
    g->playTime = 0.0f;
    g->ghostHintShown = false;
    if (!Game_LoadWing(g, wing) && !Game_LoadWing(g, 0)) {
        printf("Could not load any wing - see the errors above.\n");
        g->quit = true;
        return;
    }
    g->state = STATE_PLAYING;
    WriteSave(g, g->wing + 1);
    SetMouseCaptured(g, true);
}

/* The Sanctum: Master Oren, the apprentices and the freed monks wait in warm candlelight. */
bool Game_LoadSanctum(Game *g)
{
    static const Color apprenticeRobes[3] = { { 34, 52, 120, 255 }, { 112, 74, 40, 255 }, { 44, 104, 56, 255 } };
    static const char *const names[3] = { "Ilsa", "Tobin", "Mira" };
    static const char *const lines[3] = {
        "You're covered in ash. Welcome back.",
        "Five bells. I counted every one.",
        "Was he... was the Abbot still human, at the end?",
    };
    int i, apprentices = 0;
    float midX;

    if (g->worldLoaded) World_Unload(&g->world);
    g->worldLoaded = false;
    if (!World_Load(&g->world, SANCTUM_FILE, 0, false)) return false;
    g->world.theme = WING_COUNT;
    World_BuildMeshes(&g->world);
    g->worldLoaded = true;
    g->sanctum = true;
    g->enemyCount = 0;
    memset(g->bolts, 0, sizeof(g->bolts));
    memset(g->particles, 0, sizeof(g->particles));
    g->chestsOpened = 0;
    g->useChest = -1;
    g->talkNpc = -1;
    g->nearOren = false;
    g->dialogLine = 0;
    g->endFade = 0.0f;
    g->redPulse = 0.0f;
    g->torchFlare = 0.0f;

    midX = g->world.w * 0.5f;
    g->npcCount = 0;
    for (i = 0; i < g->world.npcCount; i++) {
        const Spawn *s = &g->world.npcs[i];
        Npc *n = &g->npcs[g->npcCount++];
        memset(n, 0, sizeof(*n));
        n->pos = s->pos;
        if (s->type == 'O') {
            n->oren = true;
            n->name = "Master Oren";
            n->robe = (Color){ 120, 118, 116, 255 };
        } else if (apprentices < 3) {
            n->name = names[apprentices];
            n->line = lines[apprentices];
            n->robe = apprenticeRobes[apprentices];
            apprentices++;
        } else {
            n->robe = (Color){ 178, 176, 170, 255 };      /* a freed monk in light grey */
        }
        n->yaw = n->pos.x < midX ? PI * 0.5f : -PI * 0.5f;   /* monks face the carpet */
    }

    {
        bool god = g->player.god;
        Player_Init(&g->player, g->world.start, g->world.startYaw);
        g->player.god = god;
    }
    CameraRig_Init(&g->rig, g->world.startYaw);
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);
    Render_ResetLights();
    Audio_SetCalm(true);
    g->bannerCount = 0;
    PushBanner(g, "THE SANCTUM", "The bells are broken. Your friends are waiting.", COL_GOLD, BANNER_TIME + 1.0f);
    g->fade = 1.0f;
    g->leaving = false;
    g->state = STATE_PLAYING;
    return true;
}

static void Victory(Game *g)
{
    g->state = STATE_VICTORY;
    g->leaving = false;
    SetMouseCaptured(g, false);
}

/* All chests open (and, in the last wing, the Queen dead) -> the exit door opens. */
static void CheckWingComplete(Game *g)
{
    bool lastWing = g->wing == WING_COUNT - 1;
    if (g->world.exitOpen || g->chestsOpened < g->world.chestCount) return;
    if (lastWing && !g->abbotDead) {
        PushBanner(g, "The Red Abbot still guards the last bell...", "Destroy him to break it", COL_BLOOD, BANNER_TIME);
        return;
    }
    /* every seal is broken: the wing's cursed bell shatters and the way up opens */
    g->world.exitOpen = true;
    PushBanner(g, TextFormat("THE %s BELL SHATTERS", ORDINAL[g->wing]),
               lastWing ? "The way to the Sanctum is open" : "The way forward is open...", COL_BLOOD, BANNER_TIME + 0.8f);
    Audio_Play(SND_BELL_BREAK, 1.0f);
    Audio_Play(SND_DOOR, 0.8f);
    Shake(g, 1.0f);
    g->redPulse = 1.0f;
}

static void UpdateExit(Game *g, float dt)
{
    int i;
    if (g->world.exitOpen && g->world.doorSlide < 1.0f)
        g->world.doorSlide = fminf(1.0f, g->world.doorSlide + dt / DOOR_OPEN_TIME);

    if (g->world.exitOpen && !g->leaving) {
        for (i = 0; i < g->world.exitCount; i++) {
            Cell e = g->world.exits[i];
            if (g->player.pos.x > e.x - 0.25f && g->player.pos.x < e.x + 1.25f &&
                g->player.pos.z > e.z - 0.25f && g->player.pos.z < e.z + 1.25f) g->leaving = true;
        }
    }
    if (g->leaving) {
        g->fade += dt * FADE_SPEED;
        if (g->fade >= 1.0f) {
            if (g->wing + 1 >= WING_COUNT) { if (!Game_LoadSanctum(g)) Victory(g); }   /* after the last bell */
            else if (!Game_LoadWing(g, g->wing + 1)) Victory(g);
            else WriteSave(g, g->wing + 1);         /* reached a new wing: remember it */
        }
    } else if (g->fade > 0.0f) {
        g->fade = fmaxf(0.0f, g->fade - dt * FADE_SPEED);
    }
}

/* ============================================================ chests */

static void OpenChest(Game *g, int i)
{
    Vector3 c = CellCenter(g->world.chests[i]);
    const char *name = TREASURES[g->wing][i] ? TREASURES[g->wing][i] : "a Ward Seal";
    g->chestOpened[i] = true;
    g->chestsOpened++;
    g->found[g->wing][i] = true;
    if (g->player.hearts < PLAYER_MAX_HEARTS) g->player.hearts++;
    g->checkpoint = g->player.pos;
    g->checkpointYaw = g->player.yaw;
    g->useHold = 0.0f;
    g->useChest = -1;
    PushBanner(g, TextFormat("Ward Seal found: %s", name), "+1 heart   -   checkpoint saved", COL_GOLD, BANNER_TIME);
    Audio_Play(SND_TREASURE, 1.0f);
    Shake(g, SHAKE_CHEST);
    c.y = 0.6f;
    SpawnParticles(g, c, 40, COL_GOLD, 2.2f, 0.06f, 1.4f);
    CheckWingComplete(g);
}

static void UpdateChests(Game *g, const Input *in, float dt)
{
    int i, best = -1;
    float bestD = 1e9f;
    Vector3 p = g->player.pos;

    for (i = 0; i < g->world.chestCount; i++) {
        Vector3 c = CellCenter(g->world.chests[i]);
        float d = FlatDist(p, c), yaw = YawTo(p, c);
        bool facing;
        g->chestLid[i] += (g->chestOpened[i] ? 1.0f : -1.0f) * dt * 2.0f;
        g->chestLid[i] = Clamp(g->chestLid[i], 0.0f, 1.0f);
        if (g->chestOpened[i] || d > CHEST_USE_RANGE) continue;
        /* facing it with either the body or the camera counts (forgiving) */
        facing = fabsf(AngleDiff(g->player.yaw, yaw)) < CHEST_FACE_DEG * DEG2RAD ||
                 fabsf(AngleDiff(g->rig.yaw, yaw)) < CHEST_FACE_DEG * DEG2RAD || d < 1.0f;
        if (facing && d < bestD) { bestD = d; best = i; }
    }
    if (best != g->useChest) g->useHold = 0.0f;
    g->useChest = best;

    if (best >= 0 && in->use) {
        if (g->useHold == 0.0f) Audio_Play(SND_CHEST, 0.8f);
        g->useHold += dt;
        if (g->useHold >= CHEST_HOLD_TIME) OpenChest(g, best);
    } else {
        g->useHold = 0.0f;
    }
}

/* ============================================================ combat */

static void Die(Game *g)
{
    g->state = STATE_DEAD;
    g->useHold = 0.0f;
    SetMouseCaptured(g, false);
}

static void Respawn(Game *g)
{
    int i;
    bool god = g->player.god;
    float yaw = g->checkpointYaw;
    Player_Init(&g->player, g->checkpoint, yaw);
    g->player.god = god;
    g->player.invuln = RESPAWN_INVULN;
    for (i = 0; i < g->enemyCount; i++)
        if (g->enemies[i].alive) Enemy_ResetToSpawn(&g->enemies[i]);
    memset(g->bolts, 0, sizeof(g->bolts));
    CameraRig_Init(&g->rig, yaw);
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);
    g->state = STATE_PLAYING;
    g->fade = 1.0f;
    g->hurtFlash = 0.0f;
    SetMouseCaptured(g, true);
}

static void HurtPlayer(Game *g, Vector3 from)
{
    if (!Player_Hurt(&g->player, from)) return;
    Audio_Play(SND_HURT, 1.0f);
    Shake(g, SHAKE_HIT);
    g->hurtFlash = HURT_FLASH_TIME;
    g->useHold = 0.0f;               /* taking a hit cancels opening a chest */
    if (g->player.hearts <= 0) Die(g);
}

/* Height of an enemy's chest (where the lightning aims) and how big a target it is. */
static Vector3 EnemyChest(const Enemy *e)
{
    float y = e->type == EN_ABBOT ? 1.9f : (e->type == EN_WRAITH ? 1.4f : 1.1f);
    return (Vector3){ e->pos.x, y, e->pos.z };
}

static float EnemyRadius(const Enemy *e)
{
    return e->type == EN_ABBOT ? 0.95f : e->size * 0.5f + 0.3f;
}

/* Damage one enemy with the wand (sparks, sounds, death, the Queen's fall). */
static void DamageEnemy(Game *g, Enemy *e)
{
    Vector3 fx = EnemyChest(e);
    Audio_Play(SND_HIT, 1.0f);
    if (Enemy_Hurt(e, WAND_DAMAGE, g->player.pos)) {
        Color bits = e->type == EN_MONK   ? (Color){ 255, 110, 30, 255 }      /* embers */
                   : e->type == EN_WRAITH ? (Color){ 210, 208, 214, 255 } : (Color){ 160, 14, 24, 255 };
        g->enemiesSlain++;
        Audio_Play(SND_DEATH, 1.0f);
        SpawnParticles(g, fx, e->type == EN_ABBOT ? 80 : 30, bits, 2.6f, 0.09f, 1.6f);
        if (e->type == EN_ABBOT) {
            g->abbotDead = true;
            Shake(g, 1.0f);
            PushBanner(g, "The Red Abbot is destroyed!", "", COL_GOLD, BANNER_TIME);
            CheckWingComplete(g);
        }
    }
}

/* Left click: Red Lightning. Aim assist turns Kael toward the nearest enemy that can be hurt
 * and is in sight; otherwise the bolt follows the camera. The bolt is a ray marched from the wand
 * tip: it stops at the first wall or enemy and destroys any fireball it passes through. */
static void CastLightning(Game *g, const Input *in)
{
    int i, best = -1, hitEnemy = -1;
    float bestD = 1e9f, yaw = g->rig.yaw, t, len;
    Vector3 origin, aim, dir, hit;
    Vector3 eye = { g->player.pos.x, 1.5f, g->player.pos.z };

    if (!in->swing || !Player_CanSwing(&g->player)) return;
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        float d;
        if (!Enemy_CanBeHurt(e)) continue;
        d = FlatDist(g->player.pos, e->pos);
        if (d < AIM_ASSIST_RANGE && d < bestD && World_LineOfSight(&g->world, eye, EnemyChest(e))) { bestD = d; best = i; }
    }
    if (best >= 0) yaw = YawTo(g->player.pos, g->enemies[best].pos);
    Player_StartSwing(&g->player, yaw);
    origin = Player_WandTip(&g->player);

    if (best >= 0) {
        aim = EnemyChest(&g->enemies[best]);
    } else {
        /* follow the crosshair: the first wall along the camera's view (or max range) */
        Vector3 fwd = Vector3Normalize(Vector3Subtract(g->rig.cam.target, g->rig.cam.position));
        aim = Vector3Add(g->rig.cam.position, Vector3Scale(fwd, WAND_RANGE + g->rig.dist));
        for (t = g->rig.dist + 0.5f; t < WAND_RANGE + g->rig.dist; t += 0.1f) {
            Vector3 q = Vector3Add(g->rig.cam.position, Vector3Scale(fwd, t));
            if (World_IsWallCell(&g->world, (int)floorf(q.x), (int)floorf(q.z)) || q.y < 0.0f || q.y > World_CeilingAt(&g->world, q.x, q.z)) { aim = q; break; }
        }
    }
    dir = Vector3Subtract(aim, origin);
    len = Vector3Length(dir);
    dir = len > 0.001f ? Vector3Scale(dir, 1.0f / len) : (Vector3){ sinf(yaw), 0.0f, cosf(yaw) };

    /* march the ray */
    hit = Vector3Add(origin, Vector3Scale(dir, WAND_RANGE));
    for (t = 0.0f; t < WAND_RANGE && hitEnemy < 0; t += 0.1f) {
        Vector3 p = Vector3Add(origin, Vector3Scale(dir, t));
        if (World_IsWallCell(&g->world, (int)floorf(p.x), (int)floorf(p.z)) || p.y < 0.0f || p.y > World_CeilingAt(&g->world, p.x, p.z)) { hit = p; break; }
        for (i = 0; i < MAX_BOLTS; i++) {
            Bolt *b = &g->bolts[i];
            if (b->active && Vector3Distance(b->pos, p) < 0.4f) {
                b->active = false;
                Audio_Play(SND_BOLT_HIT, 1.0f);
                SpawnParticles(g, b->pos, 12, (Color){ 255, 130, 30, 255 }, 1.5f, 0.05f, 0.6f);
            }
        }
        for (i = 0; i < g->enemyCount; i++) {
            Enemy *e = &g->enemies[i];
            if (!e->alive || Vector3Distance(p, EnemyChest(e)) > EnemyRadius(e)) continue;
            if (!Enemy_CanBeHurt(e)) {
                /* a wraith in the dark: the lightning passes straight through */
                if (!g->ghostHintShown) {
                    g->ghostHintShown = true;
                    PushBanner(g, "Your lightning passes through!", "Choir Wraiths can only be hurt in the light - fight them near you or a torch",
                               (Color){ 170, 200, 255, 255 }, BANNER_TIME + 1.0f);
                }
                continue;
            }
            hitEnemy = i;
            hit = p;
            break;
        }
    }
    if (hitEnemy >= 0) DamageEnemy(g, &g->enemies[hitEnemy]);

    g->beamTime = BEAM_TIME;
    g->beamEnd = hit;
    SpawnParticles(g, hit, 14, (Color){ 255, 50, 40, 255 }, 2.0f, 0.05f, 0.5f);
    Shake(g, 0.15f);
    Audio_Play(SND_ZAP, 1.0f);
}

static void ApplyEnemyEvents(Game *g, const EnemyEvents *ev)
{
    int k;
    for (k = 0; k < ev->playerHits && g->state == STATE_PLAYING; k++) HurtPlayer(g, ev->hitFrom);
    if (ev->scare) { Audio_Play(SND_SCARE, 1.0f); Shake(g, SHAKE_SCARE); }
    if (ev->moan) Audio_Play(SND_MOAN, 0.8f);
    if (ev->fired) Audio_Play(SND_BOLT, 0.8f);
    if (ev->summon) {
        for (k = -1; k <= 1; k += 2) {
            Vector3 p = { Clamp(ev->summonPos.x + 2.0f * k, 1.5f, g->world.w - 1.5f), 0.0f,
                          Clamp(ev->summonPos.z + 1.0f, 1.5f, g->world.h - 1.5f) };
            Enemy *e;
            if (g->enemyCount >= MAX_ENEMIES) break;
            e = &g->enemies[g->enemyCount++];
            Enemy_Spawn(e, EN_WRAITH, p, 0.0f, &WINGS[g->wing]);
            e->alerted = true;
            e->scared = true;
            SpawnParticles(g, (Vector3){ p.x, 1.0f, p.z }, 25, (Color){ 196, 218, 255, 255 }, 2.0f, 0.07f, 1.2f);
        }
        Audio_Play(SND_SCARE, 0.8f);
        PushBanner(g, "The Abbot summons his choir!", "", COL_BLOOD, BANNER_TIME);
    }
}

static void UpdateEnemies(Game *g, float dt)
{
    EnemyEvents ev;
    EnemyEnv env;
    int i;
    memset(&ev, 0, sizeof(ev));
    env.world = &g->world;
    env.wing = &WINGS[g->wing];
    env.playerPos = g->player.pos;
    env.lightRadius = WINGS[g->wing].playerLightRadius;
    for (i = 0; i < g->enemyCount; i++) {
        Enemy_Update(&g->enemies[i], &env, g->bolts, &ev, dt);
        if (g->enemies[i].alive && g->enemies[i].type == EN_ABBOT && g->enemies[i].alerted) g->abbotAlerted = true;
    }
    Enemies_Separate(g->enemies, g->enemyCount, &g->world, g->player.pos);
    Bolts_Update(g->bolts, &g->world, g->player.pos, &ev, dt);
    ApplyEnemyEvents(g, &ev);
}

/* F4: open every chest and defeat the Queen, then leave the wing. */
static void CompleteWing(Game *g)
{
    int i;
    for (i = 0; i < g->world.chestCount; i++)
        if (!g->chestOpened[i]) { g->chestOpened[i] = true; g->found[g->wing][i] = true; g->chestsOpened++; }
    for (i = 0; i < g->enemyCount; i++)
        if (g->enemies[i].alive && g->enemies[i].type == EN_ABBOT) { g->enemies[i].alive = false; g->abbotDead = true; }
    g->abbotDead = g->abbotDead || g->wing == WING_COUNT - 1;
    CheckWingComplete(g);
    g->world.exitOpen = true;
    g->leaving = true;
}

/* ============================================================ per-state update */

static void UpdateBanners(Game *g, float dt);

static void UpdateSanctum(Game *g, const Input *in, float dt)
{
    int i, near = -1;
    float nearD = 1e9f;
    for (i = 0; i < g->npcCount; i++) {
        Npc *n = &g->npcs[i];
        float d = FlatDist(g->player.pos, n->pos);
        if (n->name) {          /* named people turn to look at Kael */
            float want = YawTo(n->pos, g->player.pos);
            n->yaw += AngleDiff(n->yaw, want) * fminf(1.0f, 3.0f * dt);
        }
        if (n->name && d < NPC_TALK_RANGE && d < nearD) { nearD = d; near = i; }
    }
    g->talkNpc = (near >= 0 && g->npcs[near].line) ? near : -1;
    g->nearOren = near >= 0 && g->npcs[near].oren;
    if (g->nearOren && (in->usePressed || (g->autotest && in->use))) {
        g->state = STATE_DIALOGUE;
        g->bannerCount = 0;
        g->dialogLine = 0;
        g->endFade = 0.0f;
        SetMouseCaptured(g, false);
    }
}

static void UpdateDialogue(Game *g, const Input *in, float dt)
{
    if (g->dialogLine >= OREN_LINE_COUNT) {               /* fade to black, then the final screen */
        g->endFade += dt * 0.6f;
        if (g->endFade >= 1.0f) { g->endFade = 0.0f; Victory(g); }
        return;
    }
    if (in->confirm || in->click || in->usePressed) {
        g->dialogLine++;
        Audio_Play(SND_CLICK, 0.4f);
    }
}

static void UpdatePlaying(Game *g, const Input *in, float dt)
{
    Input cam = *in;
    int events;

    if (in->pause) { g->state = STATE_PAUSED; g->pauseSel = 0; SetMouseCaptured(g, false); return; }
    if (in->inventory) { g->state = STATE_INVENTORY; SetMouseCaptured(g, false); return; }
    if (!g->mouseLook && in->click && !g->autotest) { SetMouseCaptured(g, true); return; }

    /* debug keys */
    if (in->debug) g->debug = !g->debug;
    if (in->god) {
        g->player.god = !g->player.god;
        PushBanner(g, g->player.god ? "God mode ON" : "God mode OFF", "", COL_BONE, 1.5f);
    }
    if (in->skipWing && !g->leaving) CompleteWing(g);

    g->playTime += dt;
    g->wingTime += dt;

    if (g->mouseSkip > 0 || (!g->mouseLook && !g->autotest)) cam.look = (Vector2){ 0 };
    if (g->mouseSkip > 0) g->mouseSkip--;

    events = Player_Update(&g->player, &g->rig, &g->world, in, dt);
    if (events & PLAYER_EV_STEP) Audio_Play(SND_STEP, 1.0f);
    if (events & PLAYER_EV_DASH) Audio_Play(SND_DASH, 1.0f);
    CameraRig_Update(&g->rig, &g->player, &g->world, &cam, dt);

    if (g->beamTime > 0.0f) g->beamTime -= dt;
    if (g->sanctum) {                       /* peaceful: no wand, no enemies, no bells */
        UpdateSanctum(g, in, dt);
        UpdateBanners(g, dt);
        if (g->fade > 0.0f) g->fade = fmaxf(0.0f, g->fade - dt * FADE_SPEED);
        return;
    }
    CastLightning(g, in);
    UpdateEnemies(g, dt);
    if (g->state != STATE_PLAYING) return;          /* died */
    UpdateChests(g, in, dt);
    UpdateExit(g, dt);
    UpdateParticles(g, dt);

    /* lightning: the windows blaze white-blue twice, thunder follows 1-2 s later */
    if (LIGHTNING_ENABLED) {
        g->lightningTimer -= dt;
        if (g->lightningTimer <= 0.0f) {
            g->lightningTimer = NextLightning(g->wing);
            g->lightningAge = 0.0f;
            g->thunderPlayed = false;
            Shake(g, 0.25f);
        }
        if (g->lightningAge >= 0.0f) {
            g->lightningAge += dt;
            if (!g->thunderPlayed && g->lightningAge > 1.3f) { g->thunderPlayed = true; Audio_Play(SND_THUNDER, 0.9f); }
            if (g->lightningAge > 4.0f) g->lightningAge = -1.0f;
        }
    }

    /* a distant bell tolls: the lights flare up (red is kept for danger and magic) */
    g->tollTimer -= dt;
    if (g->tollTimer <= 0.0f) {
        g->tollTimer = NextToll(g);
        Audio_Play(SND_BELL, 1.0f);
        g->torchFlare = 1.0f;
    }
}

/* Keyboard (W/S, arrows, Enter) and mouse (hover, click) menu navigation.
 * Returns the chosen index, or -1 if nothing was chosen this frame. */
static int MenuPick(int *sel, int count, const Input *in, Rectangle (*rectOf)(int, int))
{
    int i;
    if (in->up) { *sel = (*sel + count - 1) % count; Audio_Play(SND_CLICK, 0.5f); }
    if (in->down) { *sel = (*sel + 1) % count; Audio_Play(SND_CLICK, 0.5f); }
    for (i = 0; i < count; i++) {
        if (CheckCollisionPointRec(in->mouse, rectOf(i, count))) {
            if (in->click) { *sel = i; return i; }
            if (in->look.x != 0.0f || in->look.y != 0.0f) *sel = i;   /* hover follows the mouse */
        }
    }
    return in->confirm ? *sel : -1;
}

static void UpdateMenu(Game *g, const Input *in, float dt)
{
    int actions[4], count, pick;
    const char *labels[4];
    float a = g->time * 0.12f;
    Vector3 c = { g->world.start.x, 0.0f, g->world.start.z - 2.0f };

    count = Game_MenuOptions(g, actions, labels);
    if (g->menuSel >= count) g->menuSel = 0;
    pick = MenuPick(&g->menuSel, count, in, UI_MenuItemRect);
    if (g->fade > 0.0f) g->fade = fmaxf(0.0f, g->fade - dt);

    /* slowly orbit inside the entrance hall */
    g->rig.cam.position = (Vector3){ c.x + sinf(a) * 4.0f, 2.3f, c.z + cosf(a) * 4.0f };
    g->rig.cam.target = (Vector3){ c.x - sinf(a) * 2.0f, 1.4f, c.z - cosf(a) * 2.0f };

    if (pick < 0) return;
    Audio_Play(SND_CLICK, 1.0f);
    switch (actions[pick]) {
    case 0: g->state = STATE_INTRO; g->introTime = 0.0f; break;      /* story first, then wing 1 */
    case 1: Game_NewGame(g, g->savedWing - 1); break;
    default: g->quit = true; break;
    }
}

static void UpdatePause(Game *g, const Input *in)
{
    int pick;
    if (in->pause) { g->state = STATE_PLAYING; SetMouseCaptured(g, true); return; }
    pick = MenuPick(&g->pauseSel, 3, in, UI_PauseItemRect);
    if (pick < 0) return;
    Audio_Play(SND_CLICK, 1.0f);
    if (pick == 0) { g->state = STATE_PLAYING; SetMouseCaptured(g, true); }
    else if (pick == 1) { if (Game_LoadWing(g, g->wing)) { g->state = STATE_PLAYING; SetMouseCaptured(g, true); } }
    else EnterMenu(g);
}

static void UpdateBanners(Game *g, float dt)
{
    if (g->bannerCount == 0) return;
    g->banners[0].time += dt;
    if (g->banners[0].time >= g->banners[0].duration) {
        memmove(&g->banners[0], &g->banners[1], sizeof(Banner) * (MAX_BANNERS - 1));
        g->bannerCount--;
    }
}

void Game_Update(Game *g, const Input *in, float dt)
{
    g->time += dt;
    Audio_Update();
    if (in->showFps) g->showFps = !g->showFps;
    if (in->togglePost) Post_Toggle();
    if (g->redPulse > 0.0f) g->redPulse = fmaxf(0.0f, g->redPulse - dt * 1.5f);
    if (g->torchFlare > 0.0f) g->torchFlare = fmaxf(0.0f, g->torchFlare - dt * 0.8f);
    if (g->worldLoaded && !g->sanctum) Atmos_Update(&g->atmos, g->rig.cam.position, &g->world, dt);
    if (g->shake > 0.0f) g->shake = fmaxf(0.0f, g->shake - SHAKE_DECAY * dt);
    if (g->hurtFlash > 0.0f) g->hurtFlash -= dt;

    switch (g->state) {
    case STATE_MENU:      UpdateMenu(g, in, dt); break;
    case STATE_PLAYING:   UpdatePlaying(g, in, dt); UpdateBanners(g, dt); break;
    case STATE_PAUSED:    UpdatePause(g, in); break;
    case STATE_INVENTORY:
        if (in->inventory || in->pause || in->confirm) { g->state = STATE_PLAYING; SetMouseCaptured(g, true); }
        break;
    case STATE_DEAD:
        if (in->confirm || in->click) Respawn(g);
        break;
    case STATE_DIALOGUE:  UpdateDialogue(g, in, dt); break;
    case STATE_INTRO:
        g->introTime += dt;
        if (in->confirm || in->click || in->pause || g->introTime >= INTRO_LINE_COUNT * INTRO_LINE_TIME + 0.5f)
            Game_NewGame(g, 0);
        break;
    case STATE_VICTORY:
        if (in->confirm || in->click) EnterMenu(g);
        break;
    }
}

/* ============================================================ setup */

void Game_Init(Game *g, int startWing, bool directStart, bool autotest)
{
    memset(g, 0, sizeof(*g));
    g->autotest = autotest;
    Textures_Init();
    Render_Init();
    Post_Init();
    Character_Init();
    if (Render_HasShader()) Character_SetShader(Render_Shader());
    UI_Init();
    Audio_Init(!autotest);
    g->savedWing = autotest ? 0 : LoadSave();

    if (autotest) return;
    if (directStart && startWing >= WING_COUNT) {          /* --sanctum / --wing 6 */
        if (Game_LoadSanctum(g)) SetMouseCaptured(g, true);
        else EnterMenu(g);
    } else if (directStart) Game_NewGame(g, startWing);
    else EnterMenu(g);
}

void Game_Shutdown(Game *g)
{
    if (g->worldLoaded) World_Unload(&g->world);
    Audio_Shutdown();
    UI_Shutdown();
    Character_Shutdown();
    Post_Shutdown();
    Render_Shutdown();
    Textures_Shutdown();
}

/* ============================================================ drawing */

static void DrawDebug3D(const Game *g)
{
    int i;
    DrawCubeWires((Vector3){ g->player.pos.x, 0.9f, g->player.pos.z }, PLAYER_SIZE, 1.8f, PLAYER_SIZE, GREEN);
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        float half = ENEMY_FOV_DEG * 0.5f * DEG2RAD;
        Vector3 p = { e->pos.x, 0.05f, e->pos.z };
        if (!e->alive) continue;
        DrawCubeWires((Vector3){ e->pos.x, 0.9f, e->pos.z }, e->size, 1.8f, e->size, e->alerted ? RED : YELLOW);
        DrawCircle3D(p, e->sight, (Vector3){ 1, 0, 0 }, 90.0f, Fade(e->alerted ? RED : YELLOW, 0.5f));
        DrawLine3D(p, Vector3Add(p, (Vector3){ sinf(e->yaw - half) * e->sight, 0, cosf(e->yaw - half) * e->sight }), ORANGE);
        DrawLine3D(p, Vector3Add(p, (Vector3){ sinf(e->yaw + half) * e->sight, 0, cosf(e->yaw + half) * e->sight }), ORANGE);
    }
    for (i = 0; i < g->world.chestCount; i++)
        DrawCircle3D((Vector3){ g->world.chests[i].x + 0.5f, 0.05f, g->world.chests[i].z + 0.5f }, CHEST_USE_RANGE,
                     (Vector3){ 1, 0, 0 }, 90.0f, GOLD);
}

/* Red Lightning: a jagged bolt from the wand tip to the hit point. The kinks are re-randomised
 * every frame so it crackles; each segment is a wide red glow plus a thin bright core. */
static void BeamSegment(Vector3 a, Vector3 b, float fade)
{
    DrawCylinderEx(a, b, 0.07f, 0.07f, 5, (Color){ 220, 20, 30, (unsigned char)(150 * fade) });
    DrawCylinderEx(a, b, 0.02f, 0.02f, 4, (Color){ 255, 190, 190, (unsigned char)(255 * fade) });
}

static float Jitter(void) { return GetRandomValue(-100, 100) / 100.0f; }

static void DrawBeam(const Game *g)
{
    Vector3 a = Player_WandTip(&g->player), d = Vector3Subtract(g->beamEnd, a), side, up;
    Vector3 pts[BEAM_SEGMENTS + 1];
    float len = Vector3Length(d), fade = g->beamTime / BEAM_TIME, wiggle;
    int i, k, j;

    if (g->beamTime <= 0.0f || len < 0.05f) return;
    d = Vector3Scale(d, 1.0f / len);
    side = Vector3CrossProduct(d, (Vector3){ 0.0f, 1.0f, 0.0f });
    side = Vector3Length(side) < 0.01f ? (Vector3){ 1.0f, 0.0f, 0.0f } : Vector3Normalize(side);
    up = Vector3CrossProduct(side, d);
    wiggle = 0.22f * fminf(1.0f, len / 3.0f);
    for (i = 0; i <= BEAM_SEGMENTS; i++) {
        float t = (float)i / BEAM_SEGMENTS, j1 = (i == 0 || i == BEAM_SEGMENTS) ? 0.0f : wiggle * sinf(PI * t);
        pts[i] = Vector3Add(Vector3Add(Vector3Add(a, Vector3Scale(d, len * t)), Vector3Scale(side, Jitter() * j1)),
                            Vector3Scale(up, Jitter() * j1));
    }

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (i = 0; i < BEAM_SEGMENTS; i++) BeamSegment(pts[i], pts[i + 1], fade);
    for (k = 0; k < 3; k++) {                           /* short forked branches */
        Vector3 p = pts[2 + GetRandomValue(0, BEAM_SEGMENTS - 4)];
        Vector3 fd = Vector3Normalize(Vector3Add(d, Vector3Add(Vector3Scale(side, Jitter() * 1.5f), Vector3Scale(up, Jitter() * 1.5f))));
        for (j = 0; j < 3; j++) {
            Vector3 q = Vector3Add(Vector3Add(p, Vector3Scale(fd, 0.3f)), Vector3Scale(side, Jitter() * 0.12f));
            BeamSegment(p, q, fade * 0.7f);
            p = q;
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

typedef struct { int index; float dist; } DrawOrder;

static int CompareFar(const void *a, const void *b)
{
    float da = ((const DrawOrder *)a)->dist, db = ((const DrawOrder *)b)->dist;
    return (da < db) - (da > db);      /* farthest first */
}

/* Wing 5: a red glow around the Red Abbot while he lives. */
static void BossLight(const Game *g)
{
    int i;
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        if (e->alive && e->type == EN_ABBOT)
            Render_AddDynamicLight((Vector3){ e->pos.x, 2.2f, e->pos.z }, (Vector3){ 1.1f, 0.12f, 0.08f }, 7.0f);
    }
}

static void DrawScene(Game *g, Camera3D cam, bool showPlayer)
{
    DrawOrder ghosts[MAX_ENEMIES];
    int i, nGhosts = 0;
    Vector3 lightAt = showPlayer ? g->player.pos
                                 : (Vector3){ cam.position.x, cam.position.y - PLAYER_LIGHT_HEIGHT, cam.position.z };

    Render_SetFlare(g->torchFlare);
    Render_SetLightning(g->sanctum ? 0.0f : LightningFlash(g->lightningAge));
    BossLight(g);
    Render_BeginFrame(g->sanctum ? &SANCTUM : &WINGS[g->wing], &g->world, cam, lightAt, g->time);
    if (g->beamTime > 0.0f) {
        float f = g->beamTime / BEAM_TIME;
        Render_SetFlash(g->beamEnd, (Vector3){ 2.6f * f, 0.15f * f, 0.1f * f }, FLASH_RADIUS);
    }
    BeginMode3D(cam);

    /* opaque: world, chests, door, torches, characters, then glowing bolts and particles */
    Render_DrawWorld(&g->world, g->time);
    for (i = 0; i < g->world.chestCount; i++) {
        Vector3 c = CellCenter(g->world.chests[i]);
        Render_DrawChest(c, g->world.chestYaw[i], g->chestLid[i], World_LightAt(&g->world, (Vector3){ c.x, 0.8f, c.z }));
    }
    if (showPlayer) {
        Render_UseEntityLight(World_LightAt(&g->world, Vector3Add(g->player.pos, (Vector3){ 0, 1.0f, 0 })));
        Player_Draw(&g->player);
    }
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        if (e->type == EN_WRAITH) {
            ghosts[nGhosts].index = i;
            ghosts[nGhosts].dist = Vector3Distance(cam.position, e->pos);
            nGhosts++;
        } else {
            Enemy_Draw(e, &g->world);
        }
    }
    Bolts_Draw(g->bolts);
    DrawBeam(g);
    if (!g->sanctum) Atmos_Draw(&g->atmos, g->wing == WING_COUNT - 1);
    for (i = 0; i < g->npcCount; i++) {
        const Npc *n = &g->npcs[i];
        CharPose np = { 0 };
        np.pos = n->pos;
        np.yaw = n->yaw;
        np.time = g->time + i * 0.7f;
        np.swing = -1.0f;
        Render_UseEntityLight(World_LightAt(&g->world, (Vector3){ n->pos.x, 1.0f, n->pos.z }));
        Character_DrawRobedNpc(&np, n->robe, n->oren);
    }
    for (i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &g->particles[i];
        float s = p->size * fminf(1.0f, p->life / (p->maxLife * 0.5f + 0.001f));
        if (p->life > 0.0f) DrawCube(p->pos, s, s, s, p->color);
    }
    if (g->debug) DrawDebug3D(g);

    /* transparent pass: cobwebs, then ghosts far to near, without writing depth */
    Render_DrawWorldTransparent(&g->world);
    Render_DrawFlames(&g->world, g->time);
    if (!g->sanctum) Render_DrawWindowShafts(&g->world, g->time);
    qsort(ghosts, (size_t)nGhosts, sizeof(ghosts[0]), CompareFar);
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    for (i = 0; i < nGhosts; i++) Enemy_Draw(&g->enemies[ghosts[i].index], &g->world);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();

    EndMode3D();
}

/* During Master Oren's dialogue: a side view framing Kael and Oren. */
static Camera3D DialogueCamera(const Game *g)
{
    Camera3D cam = g->rig.cam;
    int i;
    for (i = 0; i < g->npcCount; i++) {
        Vector3 o = g->npcs[i].pos, p = g->player.pos, d, side, mid;
        if (!g->npcs[i].oren) continue;
        d = Vector3Subtract(o, p);
        d.y = 0.0f;
        d = Vector3Length(d) > 0.01f ? Vector3Normalize(d) : (Vector3){ 0, 0, -1 };
        side = (Vector3){ -d.z, 0.0f, d.x };
        mid = Vector3Scale(Vector3Add(o, p), 0.5f);
        cam.position = Vector3Add(Vector3Add(mid, Vector3Scale(side, 2.8f)), (Vector3){ -d.x * 0.6f, 1.75f, -d.z * 0.6f });
        cam.target = Vector3Add(mid, (Vector3){ d.x * 0.3f, 1.45f, d.z * 0.3f });
    }
    return cam;
}

/* Background = this wing's fog colour, so distant walls melt into it. */
static Color FogColor(const Game *g)
{
    const WingConfig *c = g->sanctum ? &SANCTUM : &WINGS[g->wing];
    return (Color){ (unsigned char)(c->fog[0] * 255.0f), (unsigned char)(c->fog[1] * 255.0f), (unsigned char)(c->fog[2] * 255.0f), 255 };
}

void Game_Draw(Game *g)
{
    Camera3D cam = g->state == STATE_DIALOGUE ? DialogueCamera(g) : g->rig.cam;

    if (g->shake > 0.0f && !g->autotest) {
        float s = g->shake * g->shake * SHAKE_SIZE;
        Vector3 off = { GetRandomValue(-100, 100) / 100.0f * s, GetRandomValue(-100, 100) / 100.0f * s,
                        GetRandomValue(-100, 100) / 100.0f * s };
        cam.position = Vector3Add(cam.position, off);
        cam.target = Vector3Add(cam.target, off);
    }
    /* 3D at low resolution, then up-scaled through the post-process, then crisp UI on top */
    Post_BeginScene();
    ClearBackground(FogColor(g));
    if (g->worldLoaded && g->state != STATE_INTRO) DrawScene(g, cam, g->state != STATE_MENU);
    Post_EndScene();

    Screen_Begin();
    ClearBackground(BLACK);
    Post_Draw(g->time, g->sanctum ? 1 : 0, g->redPulse);
    switch (g->state) {
    case STATE_MENU:      UI_DrawMenu(g); break;
    case STATE_PLAYING:   if (!g->hideHud) UI_DrawHUD(g); break;
    case STATE_PAUSED:    UI_DrawHUD(g); UI_DrawPause(g); break;
    case STATE_INVENTORY: UI_DrawInventory(g); break;
    case STATE_DEAD:      UI_DrawDeath(g); break;
    case STATE_INTRO:     UI_DrawIntro(g); break;
    case STATE_DIALOGUE:  UI_DrawHUD(g); UI_DrawDialogue(g); break;
    case STATE_VICTORY:   UI_DrawVictory(g); break;
    }
    UI_DrawFade(g);
    if (g->showFps) DrawFPS(10, SCREEN_H - 24);
    Screen_End();
}

/* ============================================================ autotest */

static void Frame(Game *g, const Input *in, float dt, bool draw)
{
    Game_Update(g, in, dt);
    if (draw) Game_Draw(g);
}

static void Simulate(Game *g, const Input *in, float seconds)
{
    int i, n = (int)(seconds * 60.0f);
    for (i = 0; i < n; i++) Frame(g, in, 1.0f / 60.0f, false);
}

/* Put the player on a free cell next to `c`, facing it. Returns false if there is none. */
static bool StandNextTo(Game *g, Cell c)
{
    static const int dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
    int d;
    for (d = 0; d < 4; d++) {
        int x = c.x + dx[d], z = c.z + dz[d];
        if (World_IsSolid(&g->world, x, z)) continue;
        g->player.pos = (Vector3){ x + 0.5f, 0.0f, z + 0.5f };
        g->player.yaw = g->rig.yaw = YawTo(g->player.pos, CellCenter(c));
        return true;
    }
    return false;
}

static int Check(bool ok, const char *what)
{
    printf("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    return ok ? 0 : 1;
}

/* Plays wing 1 with scripted input: chests, door, wing exit, sword, death + checkpoint. */
static int FlowTest(Game *g)
{
    Input none = { 0 }, in;
    int fails = 0, i, slain;
    Enemy *e;

    printf("\nflow test (wing 1, scripted input):\n");
    Game_NewGame(g, 0);
    g->player.god = true;                 /* enemies must not interfere with the chest test */
    for (i = 0; i < g->world.chestCount; i++) {
        if (!StandNextTo(g, g->world.chests[i])) { fails += Check(false, "chest has a free neighbour"); continue; }
        in = none;
        in.use = true;
        Simulate(g, &in, CHEST_HOLD_TIME + 0.2f);
        fails += Check(g->chestOpened[i], TextFormat("hold E opens chest %d (%s)", i + 1, TREASURES[0][i]));
    }
    fails += Check(g->world.exitOpen, "exit door opens after the last chest");
    Simulate(g, &none, DOOR_OPEN_TIME + 0.2f);
    fails += Check(g->world.doorSlide >= 1.0f, "both door leaves swing open");

    /* walk through the exit */
    StandNextTo(g, g->world.exits[0]);
    in = none;
    in.move.y = 1.0f;
    for (i = 0; i < 400 && g->wing == 0 && g->state == STATE_PLAYING; i++) Frame(g, &in, 1.0f / 60.0f, false);
    fails += Check(g->wing == 1 || g->state == STATE_VICTORY, "walking through the exit leaves the wing");
    g->player.god = false;

    /* sword: two hits kill a skeleton, aim assist turns toward it */
    Game_NewGame(g, 0);
    g->player.god = true;
    e = &g->enemies[0];
    Enemy_Spawn(e, EN_MONK, Vector3Add(g->player.pos, (Vector3){ 1.0f, 0.0f, 0.8f }), 0.0f, &WINGS[0]);
    g->enemyCount = 1;
    slain = g->enemiesSlain;
    in = none;
    in.swing = true;
    Frame(g, &in, 1.0f / 60.0f, false);
    Simulate(g, &none, WAND_COOLDOWN + 0.1f);
    fails += Check(e->hp == MONK_HP - 1, "red lightning (aim assist) damages an Ashen Monk");
    e->pos = Vector3Add(g->player.pos, (Vector3){ -0.9f, 0.0f, -0.9f });
    Frame(g, &in, 1.0f / 60.0f, false);
    Simulate(g, &none, WAND_COOLDOWN + 0.1f);
    fails += Check(!e->alive && g->enemiesSlain == slain + 1, "second hit kills it");
    g->player.god = false;

    /* death + respawn at the checkpoint */
    Game_NewGame(g, 0);
    StandNextTo(g, g->world.chests[0]);
    in = none;
    in.use = true;
    Simulate(g, &in, CHEST_HOLD_TIME + 0.2f);
    g->player.hearts = 1;
    g->player.invuln = 0.0f;
    e = &g->enemies[0];
    Enemy_Spawn(e, EN_MONK, Vector3Add(g->player.pos, (Vector3){ 1.0f, 0.0f, 0.0f }), 0.0f, &WINGS[0]);
    e->alerted = true;
    g->enemyCount = 1;
    for (i = 0; i < 300 && g->state == STATE_PLAYING; i++) Frame(g, &none, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_DEAD, "an Ashen Monk's telegraphed attack can kill you");
    in = none;
    in.confirm = true;
    Frame(g, &in, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_PLAYING && g->player.hearts == PLAYER_MAX_HEARTS &&
                   FlatDist(g->player.pos, g->checkpoint) < 0.01f && g->chestOpened[0],
                   "Enter rises again at the chest checkpoint with full hearts, chest stays open");
    fails += Check(Vector3Distance(e->pos, e->spawnPos) < 0.01f && !e->alerted, "surviving enemies return to their spawn");
    return fails;
}

/* Balance: a player who only mashes the attack button vs. two wing-1 skeletons at once. */
static int BalanceTest(Game *g)
{
    Input in = { 0 };
    int i, fails = 0;
    printf("\nbalance test (wing 1, mashing left click vs 2 Ashen Monks):\n");
    Game_NewGame(g, 0);
    g->enemyCount = 2;
    for (i = 0; i < 2; i++) {
        Vector3 p = Vector3Add(g->player.pos, (Vector3){ i ? 2.5f : -2.5f, 0.0f, -2.0f });
        Enemy_Spawn(&g->enemies[i], EN_MONK, p, 0.0f, &WINGS[0]);
        g->enemies[i].alerted = true;
    }
    in.swing = true;
    for (i = 0; i < 60 * 20 && g->state == STATE_PLAYING && (g->enemies[0].alive || g->enemies[1].alive); i++)
        Frame(g, &in, 1.0f / 60.0f, false);
    printf("  hearts left: %d of %d after %.1f s\n", g->player.hearts, PLAYER_MAX_HEARTS, i / 60.0f);
    fails += Check(g->state == STATE_PLAYING && !g->enemies[0].alive && !g->enemies[1].alive,
                   "both monks are beaten");
    fails += Check(g->player.hearts >= 3, "with at least 3 hearts to spare");
    return fails;
}

/* Red Lightning in action: cast at a skeleton a few steps ahead and capture the beam. */
static void LightningShot(Game *g)
{
    Input in = { 0 };
    Vector3 fwd;
    Game_NewGame(g, 0);
    g->player.god = true;
    g->bannerCount = 0;
    fwd = (Vector3){ sinf(g->rig.yaw), 0.0f, cosf(g->rig.yaw) };
    g->enemyCount = 1;
    Enemy_Spawn(&g->enemies[0], EN_MONK, Vector3Add(g->player.pos, Vector3Add(Vector3Scale(fwd, 6.0f), (Vector3){ 1.2f, 0, 0 })),
                g->rig.yaw + PI, &WINGS[0]);
    g->enemies[0].hp = 99;                      /* survives so it is in the picture */
    Simulate(g, &in, 1.2f);                     /* let the fade-in finish */
    in.swing = true;
    Frame(g, &in, 1.0f / 60.0f, true);
    Screen_Save("shots/lightning.png");
    g->player.god = false;
}

/* Wing 5: the gate needs every chest AND the Queen; she summons ghosts at half health. */
static int QueenTest(Game *g)
{
    Input none = { 0 }, in;
    Enemy *queen = NULL;
    int fails = 0, i, before, safety;

    printf("\nabbot test (wing 5):\n");
    Game_NewGame(g, WING_COUNT - 1);
    g->player.god = true;
    for (i = 0; i < g->enemyCount; i++) if (g->enemies[i].type == EN_ABBOT) queen = &g->enemies[i];
    fails += Check(queen != NULL, "the Red Abbot is in the bell tower");
    if (!queen) return fails;
    for (i = 0; i < g->world.chestCount; i++) OpenChest(g, i);
    fails += Check(!g->world.exitOpen, "exit stays shut while the Abbot lives");

    /* stand next to her and fight */
    before = g->enemyCount;
    g->player.pos = Vector3Add(queen->pos, (Vector3){ 0.0f, 0.0f, 1.4f });
    in = none;
    in.swing = true;
    for (safety = 0; safety < 200 && queen->alive; safety++) {
        Frame(g, &in, 1.0f / 60.0f, false);
        Simulate(g, &none, WAND_COOLDOWN);
        if (queen->alive) {             /* keep the fight in one place */
            g->player.pos = Vector3Add(queen->pos, (Vector3){ 0.0f, 0.0f, 1.4f });
            queen->visible = true;
        }
        if (safety == 0) fails += Check(g->abbotAlerted, "the boss bar appears once he is alerted");
    }
    fails += Check(g->enemyCount == before + 2, "at half health he summons two Choir Wraiths");
    fails += Check(!queen->alive && g->abbotDead, "the Abbot can be destroyed");
    fails += Check(g->world.exitOpen, "the exit opens");
    StandNextTo(g, g->world.exits[0]);
    in = none;
    in.move.y = 1.0f;
    for (i = 0; i < 400 && g->state == STATE_PLAYING; i++) Frame(g, &in, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_PLAYING && g->sanctum, "breaking the fifth bell leads to the Sanctum");
    g->player.god = false;
    return fails;
}

/* The Sanctum: render it, talk to a friend, then Master Oren's dialogue -> final screen. */
static int SanctumTest(Game *g)
{
    Input none = { 0 }, in;
    int fails = 0, i, f, oren = -1, friendNpc = -1;
    printf("\nsanctum test:\n");
    fails += Check(Game_LoadSanctum(g), "the Sanctum loads (no chests, no exit)");
    if (!g->sanctum) return fails;
    for (i = 0; i < g->npcCount; i++) {
        if (g->npcs[i].oren) oren = i;
        if (g->npcs[i].line && friendNpc < 0) friendNpc = i;
    }
    fails += Check(oren >= 0 && friendNpc >= 0, "Master Oren and his apprentices are there");
    for (f = 0; f < 90; f++) Frame(g, &none, 1.0f / 60.0f, f == 89);
    Screen_Save("shots/sanctum.png");
    if (oren < 0 || friendNpc < 0) return fails;

    g->player.pos = Vector3Add(g->npcs[friendNpc].pos, (Vector3){ 0.0f, 0.0f, 1.2f });
    Frame(g, &none, 1.0f / 60.0f, false);
    fails += Check(g->talkNpc == friendNpc, "a friend speaks when Kael comes close");

    g->player.pos = Vector3Add(g->npcs[oren].pos, (Vector3){ 0.0f, 0.0f, 1.3f });
    g->player.yaw = g->rig.yaw = PI;
    Frame(g, &none, 1.0f / 60.0f, false);
    fails += Check(g->nearOren, "\"[E] Speak with Master Oren\" appears");
    in = none;
    in.use = true;
    Frame(g, &in, 1.0f / 60.0f, true);
    Screen_Save("shots/dialogue.png");
    fails += Check(g->state == STATE_DIALOGUE, "E starts the dialogue");
    in = none;
    in.confirm = true;
    for (i = 0; i < OREN_LINE_COUNT; i++) { Frame(g, &in, 1.0f / 60.0f, false); Frame(g, &none, 1.0f / 60.0f, false); }
    Simulate(g, &none, 2.5f);
    fails += Check(g->state == STATE_VICTORY, "after his last line: fade to the final screen");
    Frame(g, &none, 1.0f / 60.0f, true);
    Screen_Save("shots/victory.png");
    return fails;
}

/* The Queen alerted right in front of the player, to see her and the boss bar. */
static void BossShot(Game *g)
{
    Input none = { 0 };
    int i, f;
    Game_NewGame(g, WING_COUNT - 1);
    g->player.god = true;
    for (i = 0; i < g->enemyCount; i++) {
        Enemy *e = &g->enemies[i];
        if (e->type != EN_ABBOT) { e->alive = false; continue; }
        g->player.pos = Vector3Add(e->pos, (Vector3){ 0.0f, 0.0f, 5.0f });
        g->player.yaw = g->rig.yaw = PI;
        e->alerted = true;
        e->hp = e->maxHp * 2 / 3;
        e->ringTimer = 100.0f;
    }
    g->bannerCount = 0;
    for (f = 0; f < 60; f++) Frame(g, &none, 1.0f / 60.0f, f == 59);
    Screen_Save("shots/boss.png");
    g->player.god = false;
}

/* The wing 1 exit door, half open, seen from a few steps inside. */
static void DoorShot(Game *g)
{
    Input none = { 0 };
    const Doorway *d;
    Vector3 mid;
    Game_NewGame(g, 0);
    g->player.god = true;
    if (g->world.doorwayCount == 0) return;
    d = &g->world.doorways[0];
    mid = Vector3Scale(Vector3Add(d->a, d->b), 0.5f);
    g->player.pos = Vector3Add(mid, Vector3Scale(d->inward, 3.2f));
    g->player.yaw = g->rig.yaw = YawTo(g->player.pos, mid);
    g->rig.pitch = 0.12f;
    g->world.exitOpen = true;
    g->world.doorSlide = 0.45f;
    g->enemyCount = 0;
    g->bannerCount = 0;
    g->fade = 0.0f;
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);
    g->leaving = true;                     /* keeps UpdateExit from moving the door further */
    g->fade = 0.0f;
    Game_Draw(g);
    Screen_Save("shots/door.png");
    (void)none;
    g->leaving = false;
    g->player.god = false;
}

/* A stained-glass window with its moonlight shaft, during a lightning flash (shots/window.png)
 * and without (shots/window_dark.png). */
static void WindowShot(Game *g, int wing)
{
    const Window *wn;
    Game_NewGame(g, wing);
    g->player.god = true;
    if (g->world.windowCount == 0) return;
    wn = &g->world.windows[g->world.windowCount / 2];
    g->player.pos = Vector3Add(wn->center, Vector3Scale(wn->normal, 3.0f));
    g->player.pos.y = 0.0f;
    g->player.pos = Vector3Add(g->player.pos, Vector3Scale(wn->side, 1.2f));
    g->player.yaw = g->rig.yaw = YawTo(g->player.pos, wn->center);
    g->rig.pitch = 0.18f;
    g->enemyCount = 0;
    g->bannerCount = 0;
    g->fade = 0.0f;
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);
    g->lightningAge = -1.0f;
    g->lightningTimer = 100.0f;
    Game_Draw(g);
    Screen_Save("shots/window_dark.png");
    g->lightningAge = 0.05f;
    Game_Draw(g);
    Screen_Save("shots/window.png");
    g->lightningAge = -1.0f;
    g->player.god = false;
}

/* Every enemy type standing in front of the player (camera behind the player). */
static void EnemyShowcase(Game *g)
{
    static const EnemyType types[4] = { EN_MONK, EN_WRAITH, EN_PRIEST, EN_ABBOT };
    static const float offs[4] = { -2.6f, -0.9f, 0.7f, 2.6f };
    Vector3 base = g->player.pos;
    float yaw = g->world.startYaw;
    Vector3 fwd = { sinf(yaw), 0.0f, cosf(yaw) }, right = { -fwd.z, 0.0f, fwd.x };
    Camera3D cam = { 0 };
    int i;

    g->enemyCount = 4;
    for (i = 0; i < 4; i++) {
        Enemy *e = &g->enemies[i];
        Vector3 p = Vector3Add(Vector3Add(base, Vector3Scale(fwd, types[i] == EN_ABBOT ? 4.6f : 3.4f)), Vector3Scale(right, offs[i]));
        Enemy_Spawn(e, types[i], p, yaw + PI, &WINGS[g->wing]);
        e->alerted = true;
        e->visible = true;
        e->time = 0.4f * i;
    }
    g->enemies[2].attack = ATK_BOLT;           /* show the red wind-up glow on the witch */
    g->enemies[2].windup = PRIEST_WINDUP * 0.8f;
    g->player.yaw = yaw;

    cam.position = Vector3Add(Vector3Subtract(base, Vector3Scale(fwd, 1.6f)), (Vector3){ 0, 2.1f, 0 });
    cam.target = Vector3Add(Vector3Add(base, Vector3Scale(fwd, 4.0f)), (Vector3){ 0, 1.3f, 0 });
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 62.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Post_BeginScene();
    ClearBackground(COL_NEARBLACK);
    DrawScene(g, cam, true);
    Post_EndScene();
    Screen_Begin();
    Post_Draw(g->time, 0, 0.0f);
    UI_TextCentered(false, "Ashen Monk  -  Choir Wraith  -  Ember Priest (winding up)  -  The Red Abbot", SCREEN_W * 0.5f, 16, 30, COL_BONE);
    Screen_End();
}

/* ============================================================ --tour */

/* Draw a few frames from the current placement (no input) and save the last one. */
static double TourShot(Game *g, const char *path, int *frames)
{
    Input none = { 0 };
    char file[64];
    double t0, took;
    int f;
    TextCopy(file, path);              /* TextFormat's buffer is reused while drawing */
    g->bannerCount = 0;
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);
    Frame(g, &none, 1.0f / 60.0f, true);        /* first frame (uploads) is not timed */
    t0 = GetTime();
    for (f = 0; f < 30; f++) Frame(g, &none, 1.0f / 60.0f, true);
    took = GetTime() - t0;
    *frames += 30;
    Screen_Save(file);
    return took;
}

static void Place(Game *g, float x, float z, float yaw, float pitch)
{
    g->player.pos = (Vector3){ x, 0.0f, z };
    g->player.vel = (Vector3){ 0 };
    g->player.yaw = g->rig.yaw = yaw;
    g->rig.pitch = pitch;
    g->rig.dist = CAM_DISTANCE;
}

/* The longest straight run of corridor cells: start cell + direction (yaw). */
static bool LongestCorridor(const World *w, float *x, float *z, float *yaw)
{
    int best = 0, cx, cz, axis;
    for (axis = 0; axis < 2; axis++) {
        for (cz = 0; cz < w->h; cz++) {
            for (cx = 0; cx < w->w; cx++) {
                int len = 0;
                while (len < 60) {
                    int px = cx + (axis == 0 ? len : 0), pz = cz + (axis == 1 ? len : 0);
                    if (px >= w->w || pz >= w->h || w->area[pz][px] != AREA_CORRIDOR || World_IsSolid(w, px, pz)) break;
                    len++;
                }
                if (len > best) {
                    best = len;
                    *x = cx + 0.5f;
                    *z = cz + 0.5f;
                    *yaw = axis == 0 ? PI * 0.5f : 0.0f;     /* look toward +X or +Z along the run */
                }
            }
        }
    }
    return best >= 3;
}

/* --tour: 4 screenshots per wing (start, largest room, corridor, start with HUD) + average FPS. */
int Game_Tour(Game *g)
{
    double seconds = 0.0;
    int frames = 0, wing, i;
    MakeDirectory("shots");
    for (wing = 0; wing <= WING_COUNT; wing++) {
        const World *w = &g->world;
        const Room *big = NULL;
        float x, z, yaw;
        bool ok = wing < WING_COUNT ? (Game_NewGame(g, wing), g->state == STATE_PLAYING && g->wing == wing)
                                    : Game_LoadSanctum(g);
        if (!ok) { printf("tour: wing %d failed to load\n", wing + 1); return 1; }
        g->player.god = true;
        g->fade = 0.0f;

        /* 1: third-person view at the start, no HUD */
        g->hideHud = true;
        Place(g, w->start.x, w->start.z, w->startYaw, CAM_PITCH_DEFAULT * DEG2RAD);
        seconds += TourShot(g, TextFormat("shots/tour_w%d_1.png", wing + 1), &frames);

        /* 2: the largest room, looking at its longest wall from the middle */
        for (i = 0; i < w->roomCount; i++) if (!big || w->rooms[i].cells > big->cells) big = &w->rooms[i];
        if (big) {
            /* stand near the middle facing a wall (longest first); nudge sideways until the
             * camera behind Kael has room (pillars, islands) */
            float cx = (big->x0 + big->x1 + 1) * 0.5f, cz = (big->z0 + big->z1 + 1) * 0.5f, bestDist = -1.0f;
            bool alongX = (big->x1 - big->x0) >= (big->z1 - big->z0);
            float yaws[2] = { alongX ? PI : -PI * 0.5f, alongX ? -PI * 0.5f : PI }, bx = cx, bz = cz, byaw = yaws[0];
            int k, s;
            for (k = 0; k < 2; k++) {
                for (s = 0; s < 7; s++) {
                    float side = (s % 2 ? 1.0f : -1.0f) * ((s + 1) / 2) * 0.75f;
                    float y = yaws[k], px = cx - sinf(y) * 1.5f + cosf(y) * side, pz = cz - cosf(y) * 1.5f - sinf(y) * side;
                    if (World_BoxBlocked(w, px, pz, PLAYER_SIZE)) continue;
                    Place(g, px, pz, y, 0.08f);
                    CameraRig_Update(&g->rig, &g->player, w, NULL, 1.0f);
                    if (g->rig.dist > bestDist + 0.3f) { bestDist = g->rig.dist; bx = px; bz = pz; byaw = y; }
                }
                if (bestDist > CAM_DISTANCE - 0.3f) break;
            }
            Place(g, bx, bz, byaw, 0.08f);
        }
        seconds += TourShot(g, TextFormat("shots/tour_w%d_2.png", wing + 1), &frames);

        /* 3: a corridor, looking along it */
        if (LongestCorridor(w, &x, &z, &yaw)) Place(g, x, z, yaw, -0.05f);
        seconds += TourShot(g, TextFormat("shots/tour_w%d_3.png", wing + 1), &frames);

        /* 4: view 1 again with the HUD (and minimap) */
        g->hideHud = false;
        Place(g, w->start.x, w->start.z, w->startYaw, CAM_PITCH_DEFAULT * DEG2RAD);
        seconds += TourShot(g, TextFormat("shots/tour_w%d_4.png", wing + 1), &frames);
        printf("tour: %-22s rooms %2d  largest %3d cells\n", w->name, w->roomCount, big ? big->cells : 0);
    }
    printf("tour: %d frames, average %.1f FPS\n", frames, seconds > 0.0 ? frames / seconds : 0.0);
    return 0;
}

int Game_Autotest(Game *g)
{
    const float dt = 1.0f / 60.0f;
    Input none = { 0 };
    int failures = 0, i, f;
    double t0, frameMs;

    MakeDirectory("shots");

    /* menu over the 3D manor */
    EnterMenu(g);
    for (f = 0; f < 60; f++) Frame(g, &none, dt, f == 59);
    if (!Screen_Save("shots/menu.png")) { printf("autotest: could not write shots/menu.png\n"); failures++; }
    g->state = STATE_INTRO;
    g->introTime = INTRO_LINE_TIME * 3.0f + 1.5f;       /* the last line, fully faded in */
    Game_Draw(g);
    Screen_Save("shots/intro.png");

    printf("\n%-6s %-24s %6s %6s %8s %6s %7s %6s %6s\n", "wing", "name", "cells", "chunks", "vertices", "chests", "enemies", "lights", "props");
    for (i = 0; i < WING_COUNT; i++) {
        if (!FileExists(WINGS[i].file)) {
            printf("%-6d MISSING %s\n", i + 1, WINGS[i].file);
            failures++;
            continue;
        }
        Game_NewGame(g, i);
        if (g->state != STATE_PLAYING || g->wing != i) {
            printf("%-6d FAILED to load/validate %s\n", i + 1, WINGS[i].file);
            failures++;
            g->quit = false;
            continue;
        }
        g->player.god = true;
        t0 = GetTime();
        for (f = 0; f < AUTOTEST_FRAMES; f++) Frame(g, &none, dt, true);
        frameMs = (GetTime() - t0) * 1000.0 / AUTOTEST_FRAMES;
        if (!Screen_Save(TextFormat("shots/wing%d.png", i + 1))) failures++;
        printf("%-6d %-24s %6d %6d %8d %6d %7d %6d %6d   %.1f ms/frame\n", i + 1, g->world.name, g->world.w * g->world.h,
               g->world.chunkCount, g->world.vertexCount, g->world.chestCount, g->world.spawnCount,
               g->world.torchCount, g->world.propCount, frameMs);
        g->player.god = false;
    }

    /* enemy close-up in wing 1, then the scripted play test */
    if (Game_LoadWing(g, 0)) {
        g->state = STATE_PLAYING;
        EnemyShowcase(g);
        if (!Screen_Save("shots/enemies.png")) failures++;
        failures += FlowTest(g);
        LightningShot(g);
        failures += BalanceTest(g);
        failures += QueenTest(g);
        failures += SanctumTest(g);
        BossShot(g);
        DoorShot(g);
        WindowShot(g, 1);
    } else {
        failures++;
    }

    /* HUD with a chest prompt, then the death and victory screens */
    if (Game_LoadWing(g, 0)) {
        Input use = none;
        g->state = STATE_PLAYING;
        g->player.god = true;
        StandNextTo(g, g->world.chests[0]);
        g->rig.yaw += 0.5f;                       /* look at the chest from the side */
        use.use = true;
        for (f = 0; f < 50; f++) Frame(g, &use, dt, f == 49);
        Screen_Save("shots/hud.png");
        for (f = 0; f < 70; f++) Frame(g, &use, dt, f == 69);
        Screen_Save("shots/chest_open.png");
        g->found[1][0] = g->found[2][2] = true;   /* a few treasures from other wings, for the list */
        g->state = STATE_INVENTORY;
        Frame(g, &none, dt, true);
        Screen_Save("shots/inventory.png");
        g->state = STATE_PAUSED;
        g->pauseSel = 1;
        Frame(g, &none, dt, true);
        Screen_Save("shots/pause.png");
        g->state = STATE_PLAYING;
        Die(g);
        Frame(g, &none, dt, true);
        Screen_Save("shots/death.png");

    }

    printf("\nautotest %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
