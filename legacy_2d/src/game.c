#include <math.h>
#include <string.h>
#include "game.h"
#include "vec.h"
#include "lighting.h"
#include "ui.h"

/* ============================================================ level flow */

static EntityType SpawnType(char c)
{
    switch (c) {
    case 's': return ENT_SKELETON;
    case 'g': return ENT_GHOUL;
    case 'b': return ENT_BAT;
    case 'c': return ENT_CULTIST;
    case 'B': return ENT_BOSS;
    case 'k': return ENT_KEY;
    case 'h': return ENT_POTION;
    default:  return ENT_NONE;
    }
}

static void UpdateGameCamera(Game *g, float dt, bool snap)
{
    float halfW = SCREEN_W * 0.5f / CAMERA_ZOOM;
    float halfH = SCREEN_H * 0.5f / CAMERA_ZOOM;
    float mapW = (float)(MAP_W * TILE_SIZE), mapH = (float)(MAP_H * TILE_SIZE);
    Vector2 want = g->player.pos;
    float k = snap ? 1.0f : Clampf(CAMERA_LERP * dt, 0.0f, 1.0f);

    want.x = (mapW > halfW * 2.0f) ? Clampf(want.x, halfW, mapW - halfW) : mapW * 0.5f;
    want.y = (mapH > halfH * 2.0f) ? Clampf(want.y, halfH, mapH - halfH) : mapH * 0.5f;

    g->camera.target.x = roundf(Lerpf(g->camera.target.x, want.x, k));
    g->camera.target.y = roundf(Lerpf(g->camera.target.y, want.y, k));
    g->camera.zoom = CAMERA_ZOOM;
    g->camera.rotation = 0.0f;
    g->camera.offset = V2(SCREEN_W * 0.5f + GetRandomValue(-100, 100) / 100.0f * g->shake,
                          SCREEN_H * 0.5f + GetRandomValue(-100, 100) / 100.0f * g->shake);
}

static void StartLevel(Game *g, int level, bool keepHp)
{
    int hp = g->player.hp, ei = 0, pi = 0, i;

    Map_Load(&g->map, level);
    g->level = level;
    memset(g->enemies, 0, sizeof(g->enemies));
    memset(g->projectiles, 0, sizeof(g->projectiles));
    memset(g->pickups, 0, sizeof(g->pickups));

    for (i = 0; i < g->map.spawnCount; i++) {
        Spawn *sp = &g->map.spawns[i];
        EntityType t = SpawnType(sp->type);
        Vector2 p = MAP_TILE_CENTER(sp->tx, sp->ty);
        if (t == ENT_KEY || t == ENT_POTION) {
            if (pi < MAX_PICKUPS) {
                g->pickups[pi].active = true;
                g->pickups[pi].type = t;
                g->pickups[pi].pos = p;
                pi++;
            }
        } else if (t != ENT_NONE && ei < MAX_ENEMIES) {
            Enemy_Spawn(&g->enemies[ei++], t, p, GetRandomValue(0, 359) * DEG2RAD);
        }
    }

    Player_Init(&g->player, g->map.playerStart);
    if (keepHp && hp > 0) g->player.hp = hp;

    g->bannerTimer = 3.0f;
    g->hurtFlash = 0.0f;
    g->shake = 0.0f;
    UpdateGameCamera(g, 0.0f, true);
}

static void NewRun(Game *g)
{
    g->kills = 0;
    g->time = 0.0f;
    StartLevel(g, 0, false);
    g->state = STATE_PLAYING;
}

/* ============================================================== gameplay */

static void DropPickup(Game *g, EntityType type, Vector2 pos)
{
    int i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        if (!g->pickups[i].active) {
            g->pickups[i].active = true;
            g->pickups[i].type = type;
            g->pickups[i].pos = pos;
            return;
        }
    }
}

static void ResolvePlayerAttack(Game *g)
{
    Player *p = &g->player;
    float cosHalf = cosf(DEG2RAD * PLAYER_ATTACK_ARC_DEG * 0.5f);
    int i;

    if (p->attackTimer <= 0.0f) return;

    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        Vector2 to;
        float d;
        if (!e->active || e->lastHitSwing == p->swingId) continue;

        to = V2Sub(e->pos, p->pos);
        d = V2Len(to);
        if (d > PLAYER_ATTACK_RANGE + e->radius) continue;
        if (d > e->radius + 6.0f && V2Dot(V2Norm(to), p->facing) < cosHalf) continue;

        e->lastHitSwing = p->swingId;
        g->shake = Maxf(g->shake, 2.0f);
        if (Enemy_Hurt(e, PLAYER_ATTACK_DAMAGE, p->pos)) {
            g->kills++;
            g->shake = Maxf(g->shake, 4.0f);
            if (e->type == ENT_BOSS) {
                g->state = STATE_VICTORY;
            } else if (GetRandomValue(0, 99) < POTION_DROP_CHANCE) {
                DropPickup(g, ENT_POTION, e->pos);
            }
        }
    }
}

static void UpdatePickups(Game *g)
{
    int i;
    for (i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pk = &g->pickups[i];
        if (!pk->active || V2Dist(pk->pos, g->player.pos) > 16.0f) continue;
        if (pk->type == ENT_KEY) {
            g->player.keys++;
            pk->active = false;
        } else if (pk->type == ENT_POTION && g->player.hp < g->player.maxHp) {
            g->player.hp += POTION_HEAL;
            if (g->player.hp > g->player.maxHp) g->player.hp = g->player.maxHp;
            pk->active = false;
        }
    }
}

static void UpdatePlaying(Game *g, float dt)
{
    Player *p = &g->player;
    int hpBefore = p->hp, i;

    g->time += dt;
    if (g->bannerTimer > 0.0f) g->bannerTimer -= dt;
    if (g->hurtFlash   > 0.0f) g->hurtFlash   -= dt * 2.5f;
    g->shake = Maxf(0.0f, g->shake - dt * 18.0f);

    Player_Update(p, &g->map, dt);

    /* keys open doors on touch */
    if (p->keys > 0) {
        Rectangle probe = {
            p->pos.x - PLAYER_SIZE * 0.5f - 3,
            p->pos.y - PLAYER_SIZE * 0.5f - 3,
            PLAYER_SIZE + 6,
            PLAYER_SIZE + 6
        };
        if (Map_TryOpenDoor(&g->map, probe)) {
            p->keys--;
            g->shake = 3.0f;
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++)
        Enemy_Update(&g->enemies[i], &g->map, p, g->projectiles, dt);

    Enemy_Separate(g->enemies, MAX_ENEMIES, &g->map);
    Projectile_UpdateAll(g->projectiles, &g->map, p, dt);

    ResolvePlayerAttack(g);
    UpdatePickups(g);

    if (p->hp < hpBefore) {
        g->hurtFlash = 1.0f;
        g->shake = Maxf(g->shake, 6.0f);
    }

    if (p->hp <= 0) {
        g->state = STATE_GAME_OVER;
        return;
    }

    if (Map_TileAt(&g->map, p->pos) == TILE_EXIT) {
        if (g->level + 1 < Map_LevelCount())
            StartLevel(g, g->level + 1, true);
        else
            g->state = STATE_VICTORY;
        return;
    }

    UpdateGameCamera(g, dt, false);
}

/* ================================================================ public */

void Game_Init(Game *g)
{
    memset(g, 0, sizeof(*g));
    g->state = STATE_MENU;
    g->debugDraw = DEBUG_DRAW ? true : false;
    g->lighting = true;
    g->camera.zoom = CAMERA_ZOOM;
    UI_Init();
    Lighting_Init();
}

void Game_Shutdown(Game *g)
{
    (void)g;
    Lighting_Shutdown();
    UI_Shutdown();
}

void Game_Update(Game *g, float dt)
{
    if (IsKeyPressed(KEY_F1)) g->debugDraw = !g->debugDraw;
    if (IsKeyPressed(KEY_F2)) g->lighting = !g->lighting;

    switch (g->state) {
    case STATE_MENU:
        g->time += dt;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) NewRun(g);
        if (IsKeyPressed(KEY_Q)) g->quit = true;
        break;

    case STATE_PLAYING:
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
            g->state = STATE_PAUSED;
            break;
        }
        if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
            g->state = STATE_INVENTORY;
            break;
        }
        UpdatePlaying(g, dt);
        break;

    case STATE_PAUSED:
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P))
            g->state = STATE_PLAYING;
        if (IsKeyPressed(KEY_Q))
            g->state = STATE_MENU;
        break;

    case STATE_INVENTORY:
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB))
            g->state = STATE_PLAYING;
        break;

    case STATE_GAME_OVER:
        g->time += dt;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)) {
            StartLevel(g, g->level, false);
            g->state = STATE_PLAYING;
        }
        if (IsKeyPressed(KEY_ESCAPE))
            g->state = STATE_MENU;
        break;

    case STATE_VICTORY:
        g->time += dt;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
            g->state = STATE_MENU;
        break;
    }
}

/* ================================================================== draw */

static void DrawDebug(const Game *g, Rectangle view)
{
    int x, y, i;
    int x1 = (int)floorf(view.x / TILE_SIZE);
    int y1 = (int)floorf(view.y / TILE_SIZE);
    int x2 = (int)floorf((view.x + view.width) / TILE_SIZE);
    int y2 = (int)floorf((view.y + view.height) / TILE_SIZE);
    const Player *p = &g->player;
    float a;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= MAP_W) x2 = MAP_W - 1;
    if (y2 >= MAP_H) y2 = MAP_H - 1;

    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            DrawRectangleLines(
                x * TILE_SIZE,
                y * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                Fade(WHITE, 0.10f)
            );

    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &g->enemies[i];
        if (!e->active) continue;

        DrawCircleLinesV(e->pos, e->radius, RED);
        a = atan2f(e->facing.y, e->facing.x) * RAD2DEG;

        if (e->fovCos <= -0.99f) {
            DrawCircleLinesV(
                e->pos,
                e->sightRange,
                Fade(YELLOW, e->alerted ? 0.1f : 0.35f)
            );
        } else {
            float half = acosf(e->fovCos) * RAD2DEG;
            DrawCircleSectorLines(
                e->pos,
                e->sightRange,
                a - half,
                a + half,
                24,
                Fade(YELLOW, e->alerted ? 0.1f : 0.45f)
            );
        }
    }

    DrawCircleLinesV(p->pos, PLAYER_RADIUS, GREEN);
    DrawRectangleLines(
        (int)(p->pos.x - PLAYER_SIZE * 0.5f),
        (int)(p->pos.y - PLAYER_SIZE * 0.5f),
        (int)PLAYER_SIZE,
        (int)PLAYER_SIZE,
        LIME
    );

    a = atan2f(p->facing.y, p->facing.x) * RAD2DEG;

    DrawCircleSectorLines(
        p->pos,
        PLAYER_ATTACK_RANGE,
        a - PLAYER_ATTACK_ARC_DEG * 0.5f,
        a + PLAYER_ATTACK_ARC_DEG * 0.5f,
        16,
        SKYBLUE
    );
}

static void DrawWorld(Game *g)
{
    Rectangle view = {
        g->camera.target.x - SCREEN_W * 0.5f / CAMERA_ZOOM - TILE_SIZE,
        g->camera.target.y - SCREEN_H * 0.5f / CAMERA_ZOOM - TILE_SIZE,
        SCREEN_W / CAMERA_ZOOM + TILE_SIZE * 2,
        SCREEN_H / CAMERA_ZOOM + TILE_SIZE * 2
    };

    int i;

    BeginMode2D(g->camera);

        Map_Draw(&g->map, view);

        for (i = 0; i < MAX_PICKUPS; i++) {
            const Pickup *pk = &g->pickups[i];

            if (pk->active)
                DrawEntity(
                    pk->type,
                    V2(pk->pos.x, pk->pos.y + sinf(g->time * 3.0f + pk->pos.x) * 1.5f),
                    V2(0, 1)
                );
        }

        for (i = 0; i < MAX_ENEMIES; i++)
            Enemy_Draw(&g->enemies[i]);

        Projectile_DrawAll(g->projectiles);
        Player_Draw(&g->player);

        if (g->debugDraw)
            DrawDebug(g, view);

    EndMode2D();

    if (g->lighting)
        Lighting_Draw(
            &g->map,
            &g->player,
            g->projectiles,
            g->camera,
            g->time
        );
}

void Game_Draw(Game *g)
{
    BeginDrawing();
    ClearBackground(COL_BG);

    if (g->state == STATE_MENU) {
        UI_DrawMenu(g->time);
    } else {
        DrawWorld(g);
        UI_DrawVignette();

        if (g->hurtFlash > 0.0f)
            DrawRectangle(
                0,
                0,
                SCREEN_W,
                SCREEN_H,
                Fade(COL_BLOOD, 0.35f * g->hurtFlash)
            );

        UI_DrawHUD(g);

        if (g->bannerTimer > 0.0f && g->state == STATE_PLAYING)
            UI_DrawBanner(
                g->level + 1,
                Map_LevelName(g->level),
                g->bannerTimer
            );

        switch (g->state) {
        case STATE_PAUSED:
            UI_DrawPause();
            break;

        case STATE_INVENTORY:
            UI_DrawInventory(g);
            break;

        case STATE_GAME_OVER:
            UI_DrawGameOver(g->time);
            break;

        case STATE_VICTORY:
            UI_DrawVictory(g->kills, g->time);
            break;

        default:
            break;
        }
    }

    EndDrawing();
}