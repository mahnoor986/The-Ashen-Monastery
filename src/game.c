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
#include "character.h"
#include "audio.h"
#include "ui.h"
#include "rlgl.h"
#include "raymath.h"

const WingConfig WINGS[WING_COUNT] = WING_TABLE;
const char *const TREASURES[WING_COUNT][MAX_CHESTS] = TREASURE_TABLE;
const char *const WING_NAMES[WING_COUNT] = WING_NAME_TABLE;

static const char *const ROMAN[WING_COUNT] = { "I", "II", "III", "IV", "V" };

const char *Game_WingTitle(int wing)
{
    return TextFormat("Wing %s", ROMAN[wing < 0 ? 0 : wing % WING_COUNT]);
}

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

bool Game_LoadWing(Game *g, int wing)
{
    int i;
    bool god = g->player.god;
    if (wing < 0 || wing >= WING_COUNT) return false;
    if (g->worldLoaded) World_Unload(&g->world);
    g->worldLoaded = false;
    if (!World_Load(&g->world, WINGS[wing].file, WINGS[wing].chests)) return false;
    World_BuildMeshes(&g->world);
    g->worldLoaded = true;
    g->wing = wing;
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
    g->queenDead = false;
    g->queenAlerted = false;

    /* player (full hearts at the start of every wing) */
    Player_Init(&g->player, g->world.start, g->world.startYaw);
    g->player.god = god;
    g->checkpoint = g->world.start;
    g->checkpointYaw = g->world.startYaw;
    CameraRig_Init(&g->rig, g->world.startYaw);
    CameraRig_Update(&g->rig, &g->player, &g->world, NULL, 1.0f);

    g->bannerCount = 0;
    PushBanner(g, TextFormat("%s \xE2\x80\x94 %s", Game_WingTitle(wing), g->world.name),
               wing == 0 ? "Open every chest to unlock the iron door" : "", COL_BONE, BANNER_TIME + 1.0f);
    g->fade = 1.0f;
    g->leaving = false;
    g->hurtFlash = 0.0f;
    g->shake = 0.0f;
    WriteSave(g, wing + 1);
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
    SetMouseCaptured(g, true);
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
    if (lastWing && !g->queenDead) {
        PushBanner(g, "The Witch Queen still bars the gate...", "Destroy her to escape", COL_BLOOD, BANNER_TIME);
        return;
    }
    g->world.exitOpen = true;
    PushBanner(g, lastWing ? "The manor gate is open!" : "The way forward is open...",
               lastWing ? "Escape Blackthorn Manor" : "Find the iron door", COL_GOLD, BANNER_TIME);
    Audio_Play(SND_DOOR, 1.0f);
    Shake(g, SHAKE_DOOR);
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
            if (g->wing + 1 >= WING_COUNT || !Game_LoadWing(g, g->wing + 1)) Victory(g);
        }
    } else if (g->fade > 0.0f) {
        g->fade = fmaxf(0.0f, g->fade - dt * FADE_SPEED);
    }
}

/* ============================================================ chests */

static void OpenChest(Game *g, int i)
{
    Vector3 c = CellCenter(g->world.chests[i]);
    const char *name = TREASURES[g->wing][i] ? TREASURES[g->wing][i] : "treasure";
    g->chestOpened[i] = true;
    g->chestsOpened++;
    g->found[g->wing][i] = true;
    if (g->player.hearts < PLAYER_MAX_HEARTS) g->player.hearts++;
    g->checkpoint = g->player.pos;
    g->checkpointYaw = g->player.yaw;
    g->useHold = 0.0f;
    g->useChest = -1;
    PushBanner(g, TextFormat("You found the %s", name), "+1 heart   -   checkpoint saved", COL_GOLD, BANNER_TIME);
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

/* Left click: swing. Aim assist turns the knight toward the nearest enemy in reach. */
static void TrySwing(Game *g, const Input *in)
{
    int i, best = -1;
    float bestD = 1e9f, yaw = g->rig.yaw;
    if (!in->swing || !Player_CanSwing(&g->player)) return;
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        float d;
        if (!e->alive) continue;
        d = FlatDist(g->player.pos, e->pos) - e->size * 0.5f;
        if (d < AIM_ASSIST_RANGE && d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) yaw = YawTo(g->player.pos, g->enemies[best].pos);
    Player_StartSwing(&g->player, yaw);
    Audio_Play(SND_SWING, 1.0f);
}

static bool InSwordArc(const Game *g, Vector3 target, float radius)
{
    float d = FlatDist(g->player.pos, target);
    if (d > SWORD_RANGE + radius) return false;
    if (d < 0.6f + radius) return true;
    return fabsf(AngleDiff(g->player.yaw, YawTo(g->player.pos, target))) <= SWORD_ARC_DEG * 0.5f * DEG2RAD;
}

static void SwordHits(Game *g)
{
    int i;
    if (!Player_SwingActive(&g->player)) return;
    for (i = 0; i < g->enemyCount; i++) {
        Enemy *e = &g->enemies[i];
        Vector3 fx = { e->pos.x, 1.1f, e->pos.z };
        if (!e->alive || e->lastSwingId == g->player.swingId || !InSwordArc(g, e->pos, e->size * 0.5f)) continue;
        e->lastSwingId = g->player.swingId;
        if (!Enemy_CanBeHurt(e)) {
            /* a ghost in the dark: the blade passes straight through */
            SpawnParticles(g, fx, 6, (Color){ 150, 170, 220, 255 }, 1.0f, 0.05f, 0.6f);
            if (!g->ghostHintShown) {
                g->ghostHintShown = true;
                PushBanner(g, "Your blade passes through!", "Ghosts can only be hurt in the light - fight them near you or a torch",
                           (Color){ 170, 200, 255, 255 }, BANNER_TIME + 1.0f);
            }
            continue;
        }
        Audio_Play(SND_HIT, 1.0f);
        Shake(g, 0.25f);
        if (Enemy_Hurt(e, SWORD_DAMAGE, g->player.pos)) {
            Color bits = e->type == EN_SKELETON ? (Color){ 222, 214, 192, 255 }
                       : e->type == EN_GHOST    ? (Color){ 196, 218, 255, 255 } : (Color){ 150, 60, 200, 255 };
            g->enemiesSlain++;
            Audio_Play(SND_DEATH, 1.0f);
            SpawnParticles(g, fx, e->type == EN_QUEEN ? 80 : 30, bits, 2.6f, 0.09f, 1.6f);
            if (e->type == EN_QUEEN) {
                g->queenDead = true;
                Shake(g, 1.0f);
                PushBanner(g, "The Witch Queen is destroyed!", "", COL_GOLD, BANNER_TIME);
                CheckWingComplete(g);
            }
        } else {
            SpawnParticles(g, fx, 8, (Color){ 200, 30, 30, 255 }, 1.6f, 0.05f, 0.7f);
        }
    }
    /* the sword also shatters hex bolts */
    for (i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &g->bolts[i];
        if (!b->active || !InSwordArc(g, b->pos, 0.3f)) continue;
        b->active = false;
        Audio_Play(SND_BOLT_HIT, 1.0f);
        SpawnParticles(g, b->pos, 12, (Color){ 190, 90, 255, 255 }, 1.5f, 0.05f, 0.6f);
    }
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
            Enemy_Spawn(e, EN_GHOST, p, 0.0f, &WINGS[g->wing]);
            e->alerted = true;
            e->scared = true;
            SpawnParticles(g, (Vector3){ p.x, 1.0f, p.z }, 25, (Color){ 196, 218, 255, 255 }, 2.0f, 0.07f, 1.2f);
        }
        Audio_Play(SND_SCARE, 0.8f);
        PushBanner(g, "The Queen summons her dead!", "", COL_BLOOD, BANNER_TIME);
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
        if (g->enemies[i].alive && g->enemies[i].type == EN_QUEEN && g->enemies[i].alerted) g->queenAlerted = true;
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
        if (g->enemies[i].alive && g->enemies[i].type == EN_QUEEN) { g->enemies[i].alive = false; g->queenDead = true; }
    g->queenDead = g->queenDead || g->wing == WING_COUNT - 1;
    CheckWingComplete(g);
    g->world.exitOpen = true;
    g->leaving = true;
}

/* ============================================================ per-state update */

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

    TrySwing(g, in);
    UpdateEnemies(g, dt);
    if (g->state != STATE_PLAYING) return;          /* died */
    SwordHits(g);
    UpdateChests(g, in, dt);
    UpdateExit(g, dt);
    UpdateParticles(g, dt);
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
    case 0: Game_NewGame(g, 0); break;
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
    Character_Init();
    if (Render_HasShader()) Character_SetShader(Render_Shader());
    UI_Init();
    Audio_Init(!autotest);
    g->savedWing = autotest ? 0 : LoadSave();

    if (autotest) return;
    if (directStart) Game_NewGame(g, startWing);
    else EnterMenu(g);
}

void Game_Shutdown(Game *g)
{
    if (g->worldLoaded) World_Unload(&g->world);
    Audio_Shutdown();
    UI_Shutdown();
    Character_Shutdown();
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

typedef struct { int index; float dist; } DrawOrder;

static int CompareFar(const void *a, const void *b)
{
    float da = ((const DrawOrder *)a)->dist, db = ((const DrawOrder *)b)->dist;
    return (da < db) - (da > db);      /* farthest first */
}

static void DrawScene(Game *g, Camera3D cam, bool showPlayer)
{
    DrawOrder ghosts[MAX_ENEMIES];
    int i, nGhosts = 0;
    Vector3 lightAt = showPlayer ? g->player.pos
                                 : (Vector3){ cam.position.x, cam.position.y - PLAYER_LIGHT_HEIGHT, cam.position.z };

    Render_BeginFrame(&WINGS[g->wing], cam, lightAt, g->time);
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
        if (e->type == EN_GHOST) {
            ghosts[nGhosts].index = i;
            ghosts[nGhosts].dist = Vector3Distance(cam.position, e->pos);
            nGhosts++;
        } else {
            Enemy_Draw(e, &g->world);
        }
    }
    Bolts_Draw(g->bolts);
    for (i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &g->particles[i];
        float s = p->size * fminf(1.0f, p->life / (p->maxLife * 0.5f + 0.001f));
        if (p->life > 0.0f) DrawCube(p->pos, s, s, s, p->color);
    }
    if (g->debug) DrawDebug3D(g);

    /* transparent pass: ghosts, far to near, without writing depth */
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

void Game_Draw(Game *g)
{
    Camera3D cam = g->rig.cam;
    ClearBackground(COL_NEARBLACK);

    if (g->shake > 0.0f && !g->autotest) {
        float s = g->shake * g->shake * SHAKE_SIZE;
        Vector3 off = { GetRandomValue(-100, 100) / 100.0f * s, GetRandomValue(-100, 100) / 100.0f * s,
                        GetRandomValue(-100, 100) / 100.0f * s };
        cam.position = Vector3Add(cam.position, off);
        cam.target = Vector3Add(cam.target, off);
    }
    if (g->worldLoaded) DrawScene(g, cam, g->state != STATE_MENU);

    switch (g->state) {
    case STATE_MENU:      UI_DrawMenu(g); break;
    case STATE_PLAYING:   UI_DrawHUD(g); break;
    case STATE_PAUSED:    UI_DrawHUD(g); UI_DrawPause(g); break;
    case STATE_INVENTORY: UI_DrawInventory(g); break;
    case STATE_DEAD:      UI_DrawDeath(g); break;
    case STATE_VICTORY:   UI_DrawVictory(g); break;
    }
    UI_DrawFade(g);
    if (g->showFps) DrawFPS(10, SCREEN_H - 24);
}

/* ============================================================ autotest */

static void Frame(Game *g, const Input *in, float dt, bool draw)
{
    Game_Update(g, in, dt);
    if (!draw) return;
    Screen_Begin();
    Game_Draw(g);
    Screen_End();
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
    fails += Check(g->world.doorSlide >= 1.0f, "door slides into the floor");

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
    Enemy_Spawn(e, EN_SKELETON, Vector3Add(g->player.pos, (Vector3){ 1.0f, 0.0f, 0.8f }), 0.0f, &WINGS[0]);
    g->enemyCount = 1;
    slain = g->enemiesSlain;
    in = none;
    in.swing = true;
    Frame(g, &in, 1.0f / 60.0f, false);
    Simulate(g, &none, SWORD_COOLDOWN + 0.1f);
    fails += Check(e->hp == SKELETON_HP - 1, "sword hit (aim assist) damages a skeleton");
    e->pos = Vector3Add(g->player.pos, (Vector3){ -0.9f, 0.0f, -0.9f });
    Frame(g, &in, 1.0f / 60.0f, false);
    Simulate(g, &none, SWORD_COOLDOWN + 0.1f);
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
    Enemy_Spawn(e, EN_SKELETON, Vector3Add(g->player.pos, (Vector3){ 1.0f, 0.0f, 0.0f }), 0.0f, &WINGS[0]);
    e->alerted = true;
    g->enemyCount = 1;
    for (i = 0; i < 300 && g->state == STATE_PLAYING; i++) Frame(g, &none, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_DEAD, "a skeleton's telegraphed attack can kill you");
    in = none;
    in.confirm = true;
    Frame(g, &in, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_PLAYING && g->player.hearts == PLAYER_MAX_HEARTS &&
                   FlatDist(g->player.pos, g->checkpoint) < 0.01f && g->chestOpened[0],
                   "Enter rises again at the chest checkpoint with full hearts, chest stays open");
    fails += Check(Vector3Distance(e->pos, e->spawnPos) < 0.01f && !e->alerted, "surviving enemies return to their spawn");
    return fails;
}

/* Wing 5: the gate needs every chest AND the Queen; she summons ghosts at half health. */
static int QueenTest(Game *g)
{
    Input none = { 0 }, in;
    Enemy *queen = NULL;
    int fails = 0, i, before, safety;

    printf("\nqueen test (wing 5):\n");
    Game_NewGame(g, WING_COUNT - 1);
    g->player.god = true;
    for (i = 0; i < g->enemyCount; i++) if (g->enemies[i].type == EN_QUEEN) queen = &g->enemies[i];
    fails += Check(queen != NULL, "the Witch Queen is in the throne room");
    if (!queen) return fails;
    for (i = 0; i < g->world.chestCount; i++) OpenChest(g, i);
    fails += Check(!g->world.exitOpen, "gate stays shut while the Queen lives");

    /* stand next to her and fight */
    before = g->enemyCount;
    g->player.pos = Vector3Add(queen->pos, (Vector3){ 0.0f, 0.0f, 1.4f });
    in = none;
    in.swing = true;
    for (safety = 0; safety < 200 && queen->alive; safety++) {
        Frame(g, &in, 1.0f / 60.0f, false);
        Simulate(g, &none, SWORD_COOLDOWN);
        if (queen->alive) {             /* keep the fight in one place */
            g->player.pos = Vector3Add(queen->pos, (Vector3){ 0.0f, 0.0f, 1.4f });
            queen->visible = true;
        }
        if (safety == 0) fails += Check(g->queenAlerted, "the boss bar appears once she is alerted");
    }
    fails += Check(g->enemyCount == before + 2, "at half health she summons two ghosts");
    fails += Check(!queen->alive && g->queenDead, "the Queen can be destroyed");
    fails += Check(g->world.exitOpen, "the manor gate opens");
    StandNextTo(g, g->world.exits[0]);
    in = none;
    in.move.y = 1.0f;
    for (i = 0; i < 400 && g->state == STATE_PLAYING; i++) Frame(g, &in, 1.0f / 60.0f, false);
    fails += Check(g->state == STATE_VICTORY, "escaping through the gate shows the victory screen");
    g->player.god = false;
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
        if (e->type != EN_QUEEN) { e->alive = false; continue; }
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

/* Every enemy type standing in front of the player (camera behind the player). */
static void EnemyShowcase(Game *g)
{
    static const EnemyType types[4] = { EN_SKELETON, EN_GHOST, EN_WITCH, EN_QUEEN };
    static const float offs[4] = { -2.6f, -0.9f, 0.7f, 2.6f };
    Vector3 base = g->player.pos;
    float yaw = g->world.startYaw;
    Vector3 fwd = { sinf(yaw), 0.0f, cosf(yaw) }, right = { -fwd.z, 0.0f, fwd.x };
    Camera3D cam = { 0 };
    int i;

    g->enemyCount = 4;
    for (i = 0; i < 4; i++) {
        Enemy *e = &g->enemies[i];
        Vector3 p = Vector3Add(Vector3Add(base, Vector3Scale(fwd, types[i] == EN_QUEEN ? 4.6f : 3.4f)), Vector3Scale(right, offs[i]));
        Enemy_Spawn(e, types[i], p, yaw + PI, &WINGS[g->wing]);
        e->alerted = true;
        e->visible = true;
        e->time = 0.4f * i;
    }
    g->enemies[2].attack = ATK_BOLT;           /* show the red wind-up glow on the witch */
    g->enemies[2].windup = WITCH_WINDUP * 0.8f;
    g->player.yaw = yaw;

    cam.position = Vector3Add(Vector3Subtract(base, Vector3Scale(fwd, 1.6f)), (Vector3){ 0, 2.1f, 0 });
    cam.target = Vector3Add(Vector3Add(base, Vector3Scale(fwd, 4.0f)), (Vector3){ 0, 1.3f, 0 });
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 62.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Screen_Begin();
    ClearBackground(COL_NEARBLACK);
    DrawScene(g, cam, true);
    UI_TextCentered(false, "Skeleton  -  Ghost  -  Witch (winding up)  -  The Witch Queen", SCREEN_W * 0.5f, 16, 30, COL_BONE);
    Screen_End();
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

    printf("\n%-6s %-34s %6s %6s %8s %6s %7s %7s\n", "wing", "name", "cells", "chunks", "vertices", "chests", "enemies", "torches");
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
        printf("%-6d %-34s %6d %6d %8d %6d %7d %7d   %.1f ms/frame\n", i + 1, g->world.name, g->world.w * g->world.h,
               g->world.chunkCount, g->world.vertexCount, g->world.chestCount, g->world.spawnCount,
               g->world.torchCount, frameMs);
        g->player.god = false;
    }

    /* enemy close-up in wing 1, then the scripted play test */
    if (Game_LoadWing(g, 0)) {
        g->state = STATE_PLAYING;
        EnemyShowcase(g);
        if (!Screen_Save("shots/enemies.png")) failures++;
        failures += FlowTest(g);
        failures += QueenTest(g);
        BossShot(g);
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
        Die(g);
        Frame(g, &none, dt, true);
        Screen_Save("shots/death.png");
        Victory(g);
        Frame(g, &none, dt, true);
        Screen_Save("shots/victory.png");
    }

    printf("\nautotest %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
