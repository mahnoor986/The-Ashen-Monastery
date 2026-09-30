#include <math.h>
#include <stdio.h>
#include "ui.h"
#include "config.h"
#include "vec.h"
#include "lighting.h"

/* Drop gothic .ttf files here and they are picked up automatically:
 *   assets/fonts/title.ttf  (UnifrakturCook, Pirata One, ...)
 *   assets/fonts/body.ttf   (a readable serif)                     */
#define TITLE_FONT_PATH "assets/fonts/title.ttf"
#define BODY_FONT_PATH  "assets/fonts/body.ttf"

static Font titleFont, bodyFont;
static bool customTitle = false, customBody = false;

void UI_Init(void)
{
    titleFont = GetFontDefault();
    bodyFont  = GetFontDefault();
    if (FileExists(TITLE_FONT_PATH)) {
        titleFont = LoadFontEx(TITLE_FONT_PATH, 96, NULL, 0);
        SetTextureFilter(titleFont.texture, TEXTURE_FILTER_BILINEAR);
        customTitle = true;
    }
    if (FileExists(BODY_FONT_PATH)) {
        bodyFont = LoadFontEx(BODY_FONT_PATH, 48, NULL, 0);
        SetTextureFilter(bodyFont.texture, TEXTURE_FILTER_BILINEAR);
        customBody = true;
    }
}

void UI_Shutdown(void)
{
    if (customTitle) UnloadFont(titleFont);
    if (customBody)  UnloadFont(bodyFont);
}

/* ------------------------------------------------------------- helpers */

static void Txt(Font f, const char *s, float x, float y, float size, Color c)
{
    DrawTextEx(f, s, V2(x, y), size, size * 0.1f, c);
}

static void TxtC(Font f, const char *s, float y, float size, Color c)
{
    Vector2 m = MeasureTextEx(f, s, size, size * 0.1f);
    Txt(f, s, (SCREEN_W - m.x) * 0.5f, y, size, c);
}

static void TxtShadowC(Font f, const char *s, float y, float size, Color c)
{
    TxtC(f, s, y + 3, size, Fade(BLACK, 0.8f));
    TxtC(f, s, y, size, c);
}

static void Dim(float a) { DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, a)); }

/* ---------------------------------------------------------------- menu */

void UI_DrawMenu(float t)
{
    int i;
    float pulse = 0.6f + 0.4f * sinf(t * 3.0f);

    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){ 8, 8, 16, 255 }, (Color){ 40, 12, 22, 255 });

    BeginBlendMode(BLEND_ADDITIVE);
        for (i = 0; i < 4; i++) {
            float fl = 0.85f + 0.15f * sinf(t * 9.0f + i * 1.7f);
            Lighting_Glow(V2(180.0f + i * 306.0f, 600.0f), 210.0f * fl, (Color){ 210, 120, 50, 255 });
        }
    EndBlendMode();
    for (i = 0; i < 4; i++) DrawEntity(ENT_TORCH, V2(180.0f + i * 306.0f, 600.0f), V2(0, 1));

    TxtShadowC(titleFont, "GOTHIC", 110, 120, COL_BONE);
    TxtShadowC(titleFont, "DUNGEON", 225, 120, COL_BLOOD);

    TxtC(bodyFont, "[ ENTER ]   Begin the Descent", 430, 30, Fade(COL_CANDLE, pulse));
    TxtC(bodyFont, "[ Q ]   Flee", 472, 24, Fade(COL_BONE, 0.6f));

    TxtC(bodyFont, "WASD / Arrows: move     SPACE / J / Click: strike     SHIFT: dash", 660, 18, Fade(COL_BONE, 0.5f));
    TxtC(bodyFont, "I: inventory     ESC: pause     F1: debug view     F2: toggle lighting", 684, 18, Fade(COL_BONE, 0.5f));
}

/* ----------------------------------------------------------------- HUD */

void UI_DrawHUD(const Game *g)
{
    const Player *p = &g->player;
    char buf[96];
    int i, foes = 0;
    const Enemy *boss = NULL;
    float frac = (float)p->hp / (float)p->maxHp;

    /* health bar */
    DrawRectangle(24, 24, 264, 26, (Color){ 12, 4, 8, 230 });
    DrawRectangle(26, 26, (int)(260.0f * frac), 22, COL_BLOOD);
    DrawRectangleLinesEx((Rectangle){ 24, 24, 264, 26 }, 2, COL_BONE);
    snprintf(buf, sizeof(buf), "%d / %d", p->hp, p->maxHp);
    Txt(bodyFont, buf, 34, 28, 18, COL_BONE);

    /* dash meter */
    {
        float ready = 1.0f - Clampf(p->dashCooldown / PLAYER_DASH_COOLDOWN, 0.0f, 1.0f);
        DrawRectangle(24, 56, 264, 5, (Color){ 12, 10, 4, 230 });
        DrawRectangle(24, 56, (int)(264.0f * ready), 5, (ready >= 1.0f) ? COL_CANDLE : Fade(COL_CANDLE, 0.45f));
    }

    /* keys */
    DrawCircle(34, 82, 6, COL_CANDLE);
    snprintf(buf, sizeof(buf), "x %d", p->keys);
    Txt(bodyFont, buf, 48, 72, 20, COL_CANDLE);

    /* level + foes */
    snprintf(buf, sizeof(buf), "%s", Map_LevelName(g->level));
    TxtC(bodyFont, buf, 20, 22, Fade(COL_BONE, 0.75f));

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!g->enemies[i].active) continue;
        foes++;
        if (g->enemies[i].type == ENT_BOSS) boss = &g->enemies[i];
    }
    snprintf(buf, sizeof(buf), "Foes: %d", foes);
    Txt(bodyFont, buf, SCREEN_W - 130, 24, 20, Fade(COL_BONE, 0.75f));

    /* boss bar */
    if (boss && boss->alerted) {
        float bf = (float)boss->hp / (float)boss->maxHp;
        DrawRectangle(390, 660, 500, 20, (Color){ 12, 4, 8, 230 });
        DrawRectangle(392, 662, (int)(496.0f * bf), 16, (Color){ 190, 24, 44, 255 });
        DrawRectangleLinesEx((Rectangle){ 390, 660, 500, 20 }, 2, COL_BONE);
        TxtC(bodyFont, "THE CRIMSON WARDEN", 634, 20, COL_BONE);
    }

    if (g->debugDraw) {
        snprintf(buf, sizeof(buf), "DEBUG  %d FPS", GetFPS());
        Txt(bodyFont, buf, SCREEN_W - 190, SCREEN_H - 30, 18, LIME);
    }
}

void UI_DrawBanner(int n, const char *name, float timer)
{
    float a = Clampf(timer, 0.0f, 1.0f);
    char buf[32];
    snprintf(buf, sizeof(buf), "LEVEL %d", n);
    TxtC(bodyFont, buf, 250, 22, Fade(COL_CANDLE, a));
    TxtShadowC(titleFont, name, 280, 56, Fade(COL_BONE, a));
}

void UI_DrawVignette(void)
{
    Color dark = (Color){ 0, 0, 0, 170 }, none = (Color){ 0, 0, 0, 0 };
    DrawRectangleGradientV(0, 0, SCREEN_W, 140, dark, none);
    DrawRectangleGradientV(0, SCREEN_H - 140, SCREEN_W, 140, none, dark);
    DrawRectangleGradientH(0, 0, 200, SCREEN_H, dark, none);
    DrawRectangleGradientH(SCREEN_W - 200, 0, 200, SCREEN_H, none, dark);
}

/* --------------------------------------------------------- overlays */

void UI_DrawPause(void)
{
    Dim(0.6f);
    TxtShadowC(titleFont, "PAUSED", 240, 90, COL_BONE);
    TxtC(bodyFont, "ESC / P : resume", 370, 26, COL_CANDLE);
    TxtC(bodyFont, "Q : abandon run", 410, 22, Fade(COL_BONE, 0.6f));
}

void UI_DrawInventory(const Game *g)
{
    char buf[96];
    const Player *p = &g->player;

    Dim(0.7f);
    DrawRectangle(440, 130, 400, 460, (Color){ 14, 12, 22, 245 });
    DrawRectangleLinesEx((Rectangle){ 440, 130, 400, 460 }, 3, COL_SLATE);

    TxtC(titleFont, "SATCHEL", 150, 56, COL_BONE);
    snprintf(buf, sizeof(buf), "Vitality   %d / %d", p->hp, p->maxHp);   TxtC(bodyFont, buf, 240, 24, COL_BONE);
    snprintf(buf, sizeof(buf), "Keys   %d", p->keys);                    TxtC(bodyFont, buf, 280, 24, COL_CANDLE);
    snprintf(buf, sizeof(buf), "Foes slain   %d", g->kills);             TxtC(bodyFont, buf, 320, 24, COL_BLOOD);
    snprintf(buf, sizeof(buf), "Depth   %d / %d", g->level + 1, Map_LevelCount()); TxtC(bodyFont, buf, 360, 24, COL_BONE);

    TxtC(bodyFont, "(items & equipment: coming soon)", 450, 18, Fade(COL_BONE, 0.45f));
    TxtC(bodyFont, "I / TAB / ESC : close", 540, 18, Fade(COL_BONE, 0.6f));
}

void UI_DrawGameOver(float t)
{
    float pulse = 0.6f + 0.4f * sinf(t * 3.0f);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade((Color){ 40, 0, 6, 255 }, 0.72f));
    TxtShadowC(titleFont, "YOU HAVE FALLEN", 250, 90, COL_BLOOD);
    TxtC(bodyFont, "[ ENTER ]  Rise again", 400, 28, Fade(COL_CANDLE, pulse));
    TxtC(bodyFont, "[ ESC ]  Return to the abyss", 445, 22, Fade(COL_BONE, 0.6f));
}

void UI_DrawVictory(int kills, float t)
{
    char buf[64];
    float pulse = 0.6f + 0.4f * sinf(t * 3.0f);
    Dim(0.75f);
    TxtShadowC(titleFont, "THE CRYPT IS SILENT", 230, 80, COL_CANDLE);
    snprintf(buf, sizeof(buf), "Foes slain: %d", kills);
    TxtC(bodyFont, buf, 360, 28, COL_BONE);
    TxtC(bodyFont, "[ ENTER ]  Return to the menu", 430, 26, Fade(COL_CANDLE, pulse));
}
