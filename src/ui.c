/* ui.c - fonts and 2D screens (see ui.h). */
#include <string.h>
#include "ui.h"
#include "game.h"
#include "config.h"

static Font titleFont, bodyFont;     /* module-private GPU resources */
static bool titleLoaded, bodyLoaded;

/* Search FONT_DIR recursively for a .ttf with this exact file name. */
static bool LoadFontByName(const char *name, int size, Font *out)
{
    FilePathList files;
    unsigned int i;
    bool ok = false;
    if (!DirectoryExists(FONT_DIR)) return false;
    files = LoadDirectoryFilesEx(FONT_DIR, ".ttf", true);
    for (i = 0; i < files.count && !ok; i++) {
        if (strcmp(GetFileName(files.paths[i]), name) == 0) {
            *out = LoadFontEx(files.paths[i], size, NULL, 0);
            ok = IsFontValid(*out);
            if (ok) SetTextureFilter(out->texture, TEXTURE_FILTER_BILINEAR);
        }
    }
    UnloadDirectoryFiles(files);
    return ok;
}

void UI_Init(void)
{
    titleLoaded = LoadFontByName(FONT_TITLE_NAME, FONT_TITLE_SIZE, &titleFont);
    bodyLoaded = LoadFontByName(FONT_BODY_NAME, FONT_BODY_SIZE, &bodyFont);
    if (!titleLoaded) titleFont = GetFontDefault();
    if (!bodyLoaded) bodyFont = GetFontDefault();
}

void UI_Shutdown(void)
{
    if (titleLoaded) UnloadFont(titleFont);
    if (bodyLoaded) UnloadFont(bodyFont);
}

void UI_Text(bool title, const char *text, float x, float y, float size, Color c)
{
    Font f = title ? titleFont : bodyFont;
    float spacing = title ? size * 0.03f : size * 0.02f;
    Color shadow = { 0, 0, 0, (unsigned char)(c.a * 0.8f) };
    DrawTextEx(f, text, (Vector2){ x + 2, y + 2 }, size, spacing, shadow);
    DrawTextEx(f, text, (Vector2){ x, y }, size, spacing, c);
}

void UI_TextCentered(bool title, const char *text, float cx, float y, float size, Color c)
{
    Font f = title ? titleFont : bodyFont;
    float spacing = title ? size * 0.03f : size * 0.02f;
    Vector2 m = MeasureTextEx(f, text, size, spacing);
    UI_Text(title, text, cx - m.x * 0.5f, y, size, c);
}

void UI_DrawTitleCard(void)
{
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){ 18, 10, 16, 255 }, COL_NEARBLACK);
    UI_TextCentered(true, "BLACKTHORN MANOR", SCREEN_W * 0.5f, 220, 110, COL_BLOOD);
    UI_TextCentered(false, "Collect the treasures. Survive the night.", SCREEN_W * 0.5f, 345, 34, COL_BONE);
}

void UI_DrawHUD(const Game *g)
{
    /* wing name, top center */
    UI_TextCentered(true, g->world.name, SCREEN_W * 0.5f, 14, 44, COL_BONE);

    /* controls hint for the first 20 seconds of wing 1 */
    if (g->wing == 0 && g->wingTime < 20.0f) {
        unsigned char a = (unsigned char)(g->wingTime > 17.0f ? 255 * (20.0f - g->wingTime) / 3.0f : 255);
        UI_TextCentered(false, "WASD move   -   Mouse look   -   Esc release mouse",
                        SCREEN_W * 0.5f, SCREEN_H - 60, 26, (Color){ 226, 220, 200, a });
    }
    if (g->showFps) DrawFPS(10, SCREEN_H - 24);
}
