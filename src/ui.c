/* ui.c - fonts and all 2D screens (see ui.h). */
#include <string.h>
#include <math.h>
#include "ui.h"
#include "game.h"
#include "config.h"
#include "post.h"
#include "minimap.h"

static Font      titleFont, bodyFont;     /* module-private GPU resources */
static bool      titleLoaded, bodyLoaded;
static Texture2D vignette;

#define COL_FADED (Color){ 150, 140, 128, 255 }

static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
#define CONTROLS_TEXT "WASD move  \xC2\xB7  Mouse look  \xC2\xB7  Left click Red Lightning  \xC2\xB7  Shift dash  \xC2\xB7  " \
                      "Hold E open chest  \xC2\xB7  M map  \xC2\xB7  I inventory  \xC2\xB7  Esc pause"

/* ============================================================ fonts + text */

/* Search FONT_DIR recursively for a .ttf with this exact file name. */
static bool LoadFontByName(const char *name, int size, Font *out)
{
    /* ASCII plus a few typographic characters used in the UI: middle dot, em dash, apostrophe */
    int codepoints[95 + 3], n = 0, c;
    FilePathList files;
    unsigned int i;
    bool ok = false;

    for (c = 32; c < 127; c++) codepoints[n++] = c;
    codepoints[n++] = 0x00B7;
    codepoints[n++] = 0x2014;
    codepoints[n++] = 0x2019;
    if (!DirectoryExists(FONT_DIR)) return false;
    files = LoadDirectoryFilesEx(FONT_DIR, ".ttf", true);
    for (i = 0; i < files.count && !ok; i++) {
        if (strcmp(GetFileName(files.paths[i]), name) == 0) {
            *out = LoadFontEx(files.paths[i], size, codepoints, n);
            ok = IsFontValid(*out);
            if (ok) SetTextureFilter(out->texture, TEXTURE_FILTER_BILINEAR);
        }
    }
    UnloadDirectoryFiles(files);
    return ok;
}

void UI_Init(void)
{
    Image img;
    titleLoaded = LoadFontByName(FONT_TITLE_NAME, FONT_TITLE_SIZE, &titleFont);
    bodyLoaded = LoadFontByName(FONT_BODY_NAME, FONT_BODY_SIZE, &bodyFont);
    if (!titleLoaded) titleFont = GetFontDefault();
    if (!bodyLoaded) bodyFont = GetFontDefault();

    /* dark vignette: transparent center, black edges */
    img = GenImageGradientRadial(SCREEN_W / 4, SCREEN_H / 4, 0.25f, (Color){ 0, 0, 0, 0 }, (Color){ 0, 0, 0, 230 });
    vignette = LoadTextureFromImage(img);
    SetTextureFilter(vignette, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
}

void UI_Shutdown(void)
{
    if (titleLoaded) UnloadFont(titleFont);
    if (bodyLoaded) UnloadFont(bodyFont);
    UnloadTexture(vignette);
}

static Vector2 Measure(bool title, const char *text, float size)
{
    return MeasureTextEx(title ? titleFont : bodyFont, text, size, title ? size * 0.03f : size * 0.02f);
}

void UI_Text(bool title, const char *text, float x, float y, float size, Color c)
{
    Font f = title ? titleFont : bodyFont;
    float spacing = title ? size * 0.03f : size * 0.02f;
    Color shadow = { 0, 0, 0, (unsigned char)(c.a * 0.85f) };
    DrawTextEx(f, text, (Vector2){ x + 2, y + 2 }, size, spacing, shadow);
    DrawTextEx(f, text, (Vector2){ x, y }, size, spacing, c);
}

void UI_TextCentered(bool title, const char *text, float cx, float y, float size, Color c)
{
    UI_Text(title, text, cx - Measure(title, text, size).x * 0.5f, y, size, c);
}

/* Title text appearing letter by letter: `letters` = how many are shown (fractional = fading
 * in), each with a soft glow around it. */
void UI_TextReveal(const char *text, float cx, float y, float size, float letters, Color c)
{
    float spacing = size * 0.03f, x = cx - Measure(true, text, size).x * 0.5f;
    int i, n = (int)strlen(text);
    char one[2] = { 0, 0 };
    for (i = 0; i < n; i++) {
        float a = letters - i;
        Vector2 w;
        one[0] = text[i];
        w = MeasureTextEx(titleFont, one, size, spacing);
        if (a > 0.0f) {
            float k = a > 1.0f ? 1.0f : a;
            Color glow = { c.r, (unsigned char)(c.g * 0.7f), (unsigned char)(c.b * 0.4f), (unsigned char)(50 * k) };
            DrawTextEx(titleFont, one, (Vector2){ x - 2, y - 2 }, size, spacing, glow);
            DrawTextEx(titleFont, one, (Vector2){ x + 2, y + 2 }, size, spacing, glow);
            DrawTextEx(titleFont, one, (Vector2){ x + 3, y + 4 }, size, spacing, (Color){ 0, 0, 0, (unsigned char)(200 * k) });
            DrawTextEx(titleFont, one, (Vector2){ x, y }, size, spacing, (Color){ c.r, c.g, c.b, (unsigned char)(c.a * k) });
        }
        x += w.x + spacing;
    }
}

static Color Alpha(Color c, float a)
{
    c.a = (unsigned char)(c.a * Clamp01(a));
    return c;
}

static const char *TimeText(float seconds)
{
    int s = (int)seconds;
    return TextFormat("%d:%02d", s / 60, s % 60);
}

/* ============================================================ small widgets */

/* Pixel-art heart: 7x6 grid of squares. */
static void DrawHeart(float x, float y, float px, bool full)
{
    static const char *rows[6] = { ".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..." };
    Color fill = full ? (Color){ 196, 24, 36, 255 } : (Color){ 40, 14, 18, 230 };
    int r, c;
    for (r = 0; r < 6; r++) {
        for (c = 0; c < 7; c++) {
            if (rows[r][c] != 'X') continue;
            DrawRectangle((int)(x + c * px - 1), (int)(y + r * px - 1), (int)px + 2, (int)px + 2, (Color){ 0, 0, 0, 200 });
        }
    }
    for (r = 0; r < 6; r++) {
        for (c = 0; c < 7; c++) {
            Color k = fill;
            if (rows[r][c] != 'X') continue;
            if (full && r == 1 && c == 1) k = (Color){ 255, 150, 150, 255 };    /* shine */
            DrawRectangle((int)(x + c * px), (int)(y + r * px), (int)px, (int)px, k);
        }
    }
}

static void DrawCheck(float x, float y, Color c)
{
    DrawLineEx((Vector2){ x, y + 9 }, (Vector2){ x + 6, y + 15 }, 3.0f, c);
    DrawLineEx((Vector2){ x + 6, y + 15 }, (Vector2){ x + 16, y + 2 }, 3.0f, c);
}

static void DrawBanner(const Game *g)
{
    const Banner *b;
    float a;
    if (g->bannerCount == 0) return;
    b = &g->banners[0];
    a = fminf(b->time / 0.4f, (b->duration - b->time) / 0.7f);
    a = Clamp01(a);
    DrawRectangleGradientV(0, 200, SCREEN_W, 60, (Color){ 0, 0, 0, 0 }, Alpha((Color){ 0, 0, 0, 150 }, a));
    DrawRectangleGradientV(0, 260, SCREEN_W, 60, Alpha((Color){ 0, 0, 0, 150 }, a), (Color){ 0, 0, 0, 0 });
    UI_TextCentered(true, b->title, SCREEN_W * 0.5f, 210, 56, Alpha(b->color, a));
    if (b->sub[0]) UI_TextCentered(false, b->sub, SCREEN_W * 0.5f, 272, 28, Alpha(COL_BONE, a));
}

/* ============================================================ HUD */

static void DrawTreasureList(const Game *g)
{
    float x = SCREEN_W - 340.0f, y = 20.0f;
    int i;
    DrawRectangleRounded((Rectangle){ x - 16, y - 8, 336, 52.0f + g->world.chestCount * 30.0f }, 0.08f, 6,
                         (Color){ 0, 0, 0, 120 });
    UI_Text(true, TextFormat("Ward Seals %d/%d", g->chestsOpened, g->world.chestCount), x, y, 34, COL_GOLD);
    for (i = 0; i < g->world.chestCount; i++) {
        float ly = y + 44 + i * 30.0f;
        if (g->chestOpened[i]) {
            DrawCheck(x, ly + 4, COL_GOLD);
            UI_Text(false, TREASURES[g->wing][i] ? TREASURES[g->wing][i] : "?", x + 26, ly, 25, COL_GOLD);
        } else {
            UI_Text(false, "??? (not yet found)", x + 26, ly, 25, COL_FADED);
        }
    }
}

static void DrawChestPrompt(const Game *g)
{
    Vector2 c = { SCREEN_W * 0.5f, SCREEN_H * 0.5f };
    float p;
    if (g->useChest < 0) return;
    p = Clamp01(g->useHold / CHEST_HOLD_TIME);
    DrawRing(c, 26, 36, 0, 360, 48, (Color){ 0, 0, 0, 150 });
    if (p > 0.0f) DrawRing(c, 27, 35, -90.0f, -90.0f + 360.0f * p, 48, COL_GOLD);
    UI_TextCentered(false, p > 0.0f ? "Opening..." : "[E] Hold to open chest", c.x, c.y + 48, 30, COL_BONE);
}

static void DrawBossBar(const Game *g)
{
    const Enemy *queen = NULL;
    int i;
    float w = 640.0f, x = (SCREEN_W - w) * 0.5f, y = SCREEN_H - 64.0f;
    if (!g->abbotAlerted) return;
    for (i = 0; i < g->enemyCount; i++)
        if (g->enemies[i].alive && g->enemies[i].type == EN_ABBOT) queen = &g->enemies[i];
    if (!queen) return;
    UI_TextCentered(true, "THE RED ABBOT", SCREEN_W * 0.5f, y - 44, 38, COL_BLOOD);
    DrawRectangle((int)x - 3, (int)y - 3, (int)w + 6, 22, (Color){ 0, 0, 0, 200 });
    DrawRectangle((int)x, (int)y, (int)(w * queen->hp / (float)queen->maxHp), 16, COL_BLOOD);
    DrawRectangleLinesEx((Rectangle){ x - 3, y - 3, w + 6, 22 }, 2, COL_GOLD);
}

void UI_DrawHUD(const Game *g)
{
    const Player *p = &g->player;
    float dashReady = 1.0f - Clamp01(p->dashCooldown / DASH_COOLDOWN);
    int i;

    if (!Post_Enabled())        /* the post-process shader draws its own vignette */
        DrawTexturePro(vignette, (Rectangle){ 0, 0, (float)vignette.width, (float)vignette.height },
                       (Rectangle){ 0, 0, SCREEN_W, SCREEN_H }, (Vector2){ 0 }, 0.0f, WHITE);
    if (g->hurtFlash > 0.0f)
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Alpha((Color){ 150, 18, 28, 255 }, 0.45f * g->hurtFlash / HURT_FLASH_TIME));

    /* hearts + dash bar, top-left */
    for (i = 0; i < PLAYER_MAX_HEARTS; i++) DrawHeart(24.0f + i * 46.0f, 22.0f, 5.0f, i < p->hearts);
    DrawRectangle(24, 62, 222, 8, (Color){ 0, 0, 0, 170 });
    DrawRectangle(24, 62, (int)(222 * dashReady), 8, dashReady >= 1.0f ? COL_GOLD : (Color){ 120, 100, 70, 255 });
    UI_Text(false, dashReady >= 1.0f ? "Dash ready (Shift)" : "Dash...", 26, 72, 20, Alpha(COL_BONE, 0.8f));
    if (p->god) UI_Text(false, "GOD MODE", 26, 96, 20, COL_GOLD);

    /* wing name, top center */
    UI_TextCentered(true, g->world.name, SCREEN_W * 0.5f, 12, 40, Alpha(COL_BONE, 0.9f));

    if (!g->sanctum) DrawTreasureList(g);
    DrawCircle(SCREEN_W / 2, SCREEN_H / 2, 3.5f, (Color){ 0, 0, 0, 160 });           /* crosshair: red dot */
    DrawCircle(SCREEN_W / 2, SCREEN_H / 2, 2.5f, (Color){ 230, 30, 30, 230 });
    DrawChestPrompt(g);
    DrawBossBar(g);
    if (g->state != STATE_DIALOGUE) Minimap_Draw(g);
    if (g->sanctum && g->state == STATE_PLAYING) {
        if (g->talkNpc >= 0) {
            const Npc *n = &g->npcs[g->talkNpc];
            DrawRectangleRounded((Rectangle){ 90, SCREEN_H - 150, 900, 90 }, 0.15f, 6, (Color){ 20, 12, 6, 200 });
            UI_Text(true, n->name, 120, SCREEN_H - 145, 34, COL_GOLD);
            UI_Text(false, n->line, 120, SCREEN_H - 108, 30, COL_BONE);
        }
        if (g->nearOren) UI_TextCentered(false, "[E] Speak with Master Oren", SCREEN_W * 0.5f, SCREEN_H * 0.5f + 60, 32, COL_GOLD);
    }
    DrawBanner(g);

    /* controls hint during the first seconds of wing 1 */
    if (g->wing == 0 && g->wingTime < CONTROLS_HINT_TIME) {
        float a = Clamp01((CONTROLS_HINT_TIME - g->wingTime) / 2.0f);
        DrawRectangle(0, SCREEN_H - 50, (int)(SCREEN_W - MINIMAP_W - 30), 50, Alpha((Color){ 0, 0, 0, 150 }, a));
        UI_TextCentered(false, CONTROLS_TEXT, (SCREEN_W - MINIMAP_W - 30) * 0.5f, SCREEN_H - 40, 20, Alpha(COL_BONE, a));
    }
    if (g->debug) {
        UI_Text(false, TextFormat("DEBUG  pos %.1f %.1f  cell %d %d  enemies %d  F3 god  F4 skip wing",
                                  p->pos.x, p->pos.z, (int)p->pos.x, (int)p->pos.z, g->enemyCount),
                10, SCREEN_H - 80, 20, GREEN);
    }
}

/* ============================================================ menus */

Rectangle UI_PauseItemRect(int index, int count)
{
    (void)count;
    return (Rectangle){ SCREEN_W * 0.5f - 200, 320.0f + index * 60.0f, 400, 52 };
}

static void DrawMenuItem(Rectangle r, const char *label, bool selected)
{
    if (selected) {
        DrawRectangleRounded(r, 0.3f, 6, (Color){ 110, 8, 14, 190 });
        DrawRectangleLinesEx(r, 2, (Color){ 220, 40, 40, 200 });
    }
    UI_TextCentered(false, label, r.x + r.width * 0.5f, r.y + 8, 34, selected ? (Color){ 255, 214, 200, 255 } : (Color){ 180, 60, 60, 255 });
}

void UI_DrawPause(const Game *g)
{
    static const char *items[3] = { "Resume", "Restart wing", "Quit to title" };
    int i;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 0, 0, 0, 160 });
    UI_TextCentered(true, "Paused", SCREEN_W * 0.5f, 170, 90, COL_BONE);
    for (i = 0; i < 3; i++) DrawMenuItem(UI_PauseItemRect(i, 3), items[i], i == g->pauseSel);
    UI_TextCentered(false, "Esc to resume", SCREEN_W * 0.5f, 530, 26, COL_FADED);
}

void UI_DrawInventory(const Game *g)
{
    int w, i, total = 0, found = 0;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 6, 4, 8, 235 });
    UI_TextCentered(true, "Ward Seals of the Ashen Monastery", SCREEN_W * 0.5f, 26, 64, COL_GOLD);

    for (w = 0; w < WING_COUNT; w++) {
        float y = 118.0f + w * 92.0f;
        bool here = w == g->wing;
        UI_Text(true, TextFormat("%s \xE2\x80\x94 %s", Game_WingTitle(w), WING_NAMES[w]), 110, y, 32,
                here ? COL_BONE : COL_FADED);
        for (i = 0; i < WINGS[w].chests; i++) {
            const char *name = TREASURES[w][i] ? TREASURES[w][i] : "?";
            float x = 130.0f + i * 212.0f;
            total++;
            if (g->found[w][i]) {
                found++;
                DrawCheck(x, y + 46, COL_GOLD);
                UI_Text(false, name, x + 24, y + 42, 22, COL_GOLD);
            } else {
                UI_Text(false, "???", x + 24, y + 42, 22, (Color){ 90, 84, 80, 255 });
            }
        }
    }
    for (i = 0; i < PLAYER_MAX_HEARTS; i++) DrawHeart(110.0f + i * 40.0f, SCREEN_H - 80.0f, 4.0f, i < g->player.hearts);
    UI_Text(false, TextFormat("Ward Seals %d / %d     Enemies defeated %d     Time %s", found, total, g->enemiesSlain,
                              TimeText(g->playTime)), 330, SCREEN_H - 84, 28, COL_BONE);
    UI_TextCentered(false, "Press I to close", SCREEN_W * 0.5f, SCREEN_H - 40, 24, COL_FADED);
}

void UI_DrawDeath(const Game *g)
{
    (void)g;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 40, 0, 4, 190 });
    DrawTexturePro(vignette, (Rectangle){ 0, 0, (float)vignette.width, (float)vignette.height },
                   (Rectangle){ 0, 0, SCREEN_W, SCREEN_H }, (Vector2){ 0 }, 0.0f, WHITE);
    UI_TextCentered(true, "THE FIRE TAKES YOU", SCREEN_W * 0.5f, 220, 110, COL_BLOOD);
    UI_TextCentered(false, "[ ENTER ]  Rise from the ashes", SCREEN_W * 0.5f, 370, 38, COL_BONE);
    UI_TextCentered(false, "You will return to the last reliquary you opened. Your Ward Seals are safe.",
                    SCREEN_W * 0.5f, 425, 26, COL_FADED);
}

void UI_DrawVictory(const Game *g)
{
    int w, i, total = 0, found = 0;
    static const char *const credits[] = {
        "Created by: ___",
        "Fonts: Pirata One, Crimson Text (SIL Open Font License, Google Fonts)",
        "Sounds: Kenney RPG Audio + Impact Sounds (CC0);  qubodup, Fupi, JaggedStone (OpenGameArt, CC0)",
        "Textures: Poly Haven (CC0)",
        "Characters, enemies, particles, bells and lightning: made in code with raylib",
    };
    for (w = 0; w < WING_COUNT; w++)
        for (i = 0; i < WINGS[w].chests; i++) { total++; if (g->found[w][i]) found++; }
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 4, 3, 2, 240 });
    UI_TextCentered(true, "THE BELLS ARE SILENT", SCREEN_W * 0.5f, 60, 96, COL_GOLD);
    UI_TextCentered(false, TextFormat("Ward Seals found: %d / %d", found, total), SCREEN_W * 0.5f, 210, 32, COL_BONE);
    UI_TextCentered(false, TextFormat("Enemies defeated: %d", g->enemiesSlain), SCREEN_W * 0.5f, 252, 32, COL_BONE);
    UI_TextCentered(false, TextFormat("Total time: %s", TimeText(g->playTime)), SCREEN_W * 0.5f, 294, 32, COL_BONE);
    for (i = 0; i < (int)(sizeof(credits) / sizeof(credits[0])); i++)
        UI_TextCentered(false, credits[i], SCREEN_W * 0.5f, 390.0f + i * 40.0f, i == 0 ? 30 : 24,
                        i == 0 ? COL_GOLD : COL_FADED);
    UI_TextCentered(false, "Press Enter to return to the menu", SCREEN_W * 0.5f, 640, 28, COL_BONE);
}

/* Master Oren's dialogue box. */
void UI_DrawDialogue(const Game *g)
{
    if (g->dialogLine < OREN_LINE_COUNT) {
        DrawRectangleRounded((Rectangle){ 140, SCREEN_H - 200, 1000, 150 }, 0.12f, 6, (Color){ 18, 10, 4, 225 });
        DrawRectangleLinesEx((Rectangle){ 140, SCREEN_H - 200, 1000, 150 }, 2, Alpha(COL_GOLD, 0.6f));
        UI_Text(true, "Master Oren", 172, SCREEN_H - 192, 38, COL_GOLD);
        UI_Text(false, OREN_LINES[g->dialogLine], 172, SCREEN_H - 145, 30, COL_BONE);
        UI_Text(false, "[ ENTER ] continue", SCREEN_W - 340, SCREEN_H - 84, 22, COL_FADED);
    }
    if (g->endFade > 0.0f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Alpha(BLACK, g->endFade));
}

/* The story, one line at a time: white text fading in and out on black. */
void UI_DrawIntro(const Game *g)
{
    int line = (int)(g->introTime / INTRO_LINE_TIME);
    float t = g->introTime - line * INTRO_LINE_TIME, a;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, BLACK);
    if (line >= INTRO_LINE_COUNT) return;
    a = Clamp01(fminf(t / 1.0f, (INTRO_LINE_TIME - t) / 0.6f));
    UI_TextCentered(false, INTRO_LINES[line], SCREEN_W * 0.5f, SCREEN_H * 0.5f - 22, 38, Alpha(WHITE, a));
    UI_TextCentered(false, "[ ENTER ] skip", SCREEN_W * 0.5f, SCREEN_H - 60, 22, (Color){ 120, 110, 105, 255 });
}

void UI_DrawFade(const Game *g)
{
    if (g->fade > 0.0f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Alpha(BLACK, g->fade));
}
