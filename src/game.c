/* game.c - game flow: wing loading, per-frame update/draw, and the --autotest run (see game.h). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "screen.h"
#include "textures.h"
#include "render.h"
#include "character.h"
#include "ui.h"
#include "raymath.h"

const WingConfig WINGS[WING_COUNT] = WING_TABLE;

/* ------------------------------------------------------------ setup */

void Game_Init(Game *g, int startWing, bool autotest)
{
    memset(g, 0, sizeof(*g));
    g->autotest = autotest;
    Textures_Init();
    Render_Init();
    Character_Init();
    if (Render_HasShader()) Character_SetShader(Render_Shader());
    UI_Init();

    if (!autotest) {
        if (!Game_LoadWing(g, startWing) && !Game_LoadWing(g, 0)) {
            printf("Could not load any wing - see the errors above.\n");
            g->quit = true;
            return;
        }
        DisableCursor();
        g->mouseLook = true;
        g->mouseSkip = 2;
    }
}

bool Game_LoadWing(Game *g, int wing)
{
    if (wing < 0 || wing >= WING_COUNT) return false;
    if (g->worldLoaded) World_Unload(&g->world);
    g->worldLoaded = false;
    if (!World_Load(&g->world, WINGS[wing].file, WINGS[wing].chests)) return false;
    World_BuildMeshes(&g->world);
    g->worldLoaded = true;
    g->wing = wing;
    g->wingTime = 0.0f;
    g->state = STATE_PLAYING;
    Player_Init(&g->player, g->world.start, g->world.startYaw);
    CameraRig_Init(&g->rig, g->world.startYaw);
    CameraRig_Update(&g->rig, &g->player, &g->world, 1.0f, false);
    return true;
}

void Game_Shutdown(Game *g)
{
    if (g->worldLoaded) World_Unload(&g->world);
    UI_Shutdown();
    Character_Shutdown();
    Render_Shutdown();
    Textures_Shutdown();
}

/* ------------------------------------------------------------ frame */

void Game_Update(Game *g, float dt)
{
    bool input = !g->autotest;
    g->time += dt;
    g->wingTime += dt;

    if (input) {
        /* Esc frees the mouse, clicking the window captures it again */
        if (g->mouseLook && IsKeyPressed(KEY_ESCAPE)) { EnableCursor(); g->mouseLook = false; }
        else if (!g->mouseLook && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            DisableCursor();
            g->mouseLook = true;
            g->mouseSkip = 2;
        }
        if (IsKeyPressed(KEY_F2)) g->showFps = !g->showFps;
    }

    Player_Update(&g->player, &g->rig, &g->world, dt, input);
    CameraRig_Update(&g->rig, &g->player, &g->world, dt, g->mouseLook && g->mouseSkip == 0);
    if (g->mouseSkip > 0) g->mouseSkip--;
}

void Game_Draw(Game *g)
{
    ClearBackground(COL_NEARBLACK);
    Render_BeginFrame(&WINGS[g->wing], g->rig.cam, g->player.pos, g->time);
    BeginMode3D(g->rig.cam);
    Render_DrawWorld(&g->world, g->time);
    Render_UseEntityLight(World_LightAt(&g->world, Vector3Add(g->player.pos, (Vector3){ 0, 1.0f, 0 })));
    Player_Draw(&g->player);
    EndMode3D();
    UI_DrawHUD(g);
}

/* ------------------------------------------------------------ autotest */

/* Draw a close-up of the knight (walking and mid-swing) standing at the wing start. */
static void DrawKnightCloseup(Game *g)
{
    Vector3 base = g->player.pos;
    float yaw = g->world.startYaw;
    Vector3 fwd = { sinf(yaw), 0.0f, cosf(yaw) }, right = { -fwd.z, 0.0f, fwd.x };
    CharPose walk = { 0 }, swing = { 0 };
    Camera3D cam = { 0 };

    walk.pos = Vector3Add(base, Vector3Scale(right, 0.7f));     /* appears on the left of the shot */
    walk.yaw = yaw;                      /* camera is in front, so this faces it */
    walk.walkPhase = 1.2f;
    walk.walkAmount = 1.0f;
    walk.swing = -1.0f;
    swing = walk;
    swing.pos = Vector3Add(base, Vector3Scale(right, -0.7f));
    swing.walkAmount = 0.0f;
    swing.swing = 0.45f;

    cam.position = Vector3Add(Vector3Add(base, Vector3Scale(fwd, 3.4f)), (Vector3){ 0, 1.35f, 0 });
    cam.target = Vector3Add(base, (Vector3){ 0, 0.95f, 0 });
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 50.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    ClearBackground(COL_NEARBLACK);
    Render_BeginFrame(&WINGS[g->wing], cam, base, g->time);
    BeginMode3D(cam);
    Render_DrawWorld(&g->world, g->time);
    Render_UseEntityLight(World_LightAt(&g->world, Vector3Add(base, (Vector3){ 0, 1.0f, 0 })));
    Character_DrawKnight(&walk);
    Character_DrawKnight(&swing);
    EndMode3D();
    UI_TextCentered(false, "Knight: walking (left) and swinging (right)", SCREEN_W * 0.5f, 20, 30, COL_BONE);
}

int Game_Autotest(Game *g)
{
    const float dt = 1.0f / 60.0f;
    int failures = 0, i, f;

    MakeDirectory("shots");

    /* menu */
    Screen_Begin();
    UI_DrawTitleCard();
    Screen_End();
    if (!Screen_Save("shots/menu.png")) { printf("autotest: could not write shots/menu.png\n"); failures++; }

    printf("\n%-6s %-34s %6s %6s %8s %6s %7s %7s\n", "wing", "name", "cells", "chunks", "vertices", "chests", "enemies", "torches");
    for (i = 0; i < WING_COUNT; i++) {
        if (!FileExists(WINGS[i].file)) {
            printf("%-6d (not built yet: %s)\n", i + 1, WINGS[i].file);
            continue;
        }
        if (!Game_LoadWing(g, i)) {
            printf("%-6d FAILED to load/validate %s\n", i + 1, WINGS[i].file);
            failures++;
            continue;
        }
        for (f = 0; f < AUTOTEST_FRAMES; f++) {
            Game_Update(g, dt);
            Screen_Begin();
            Game_Draw(g);
            Screen_End();
        }
        if (!Screen_Save(TextFormat("shots/wing%d.png", i + 1))) failures++;
        printf("%-6d %-34s %6d %6d %8d %6d %7d %7d\n", i + 1, g->world.name, g->world.w * g->world.h,
               g->world.chunkCount, g->world.vertexCount, g->world.chestCount, g->world.spawnCount,
               g->world.torchCount);

        if (i == 0) {            /* character close-up in the first wing */
            Screen_Begin();
            DrawKnightCloseup(g);
            Screen_End();
            if (!Screen_Save("shots/knight.png")) failures++;
        }
    }
    printf("\nautotest %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
