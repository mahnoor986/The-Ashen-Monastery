/* screen.c - fixed-size virtual screen (see screen.h). */
#include "screen.h"
#include "config.h"
#include "rlgl.h"

static RenderTexture2D target;   /* the SCREEN_W x SCREEN_H virtual screen */

/* Where the virtual screen lands inside the window (letterboxed, aspect kept). */
static Rectangle DestRect(void)
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float scale = sw / SCREEN_W;
    if (sh / SCREEN_H < scale) scale = sh / SCREEN_H;
    return (Rectangle){ (sw - SCREEN_W * scale) * 0.5f, (sh - SCREEN_H * scale) * 0.5f,
                        SCREEN_W * scale, SCREEN_H * scale };
}

void Screen_Init(const char *title)
{
    int mon, mw, mh, w = SCREEN_W, h = SCREEN_H;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, title);

    /* shrink the window if the monitor is too small (keep 16:9, leave room for the taskbar) */
    mon = GetCurrentMonitor();
    mw = GetMonitorWidth(mon);
    mh = GetMonitorHeight(mon);
    if (mw > 0 && mh > 0 && (w > mw * 0.95f || h > mh * 0.88f)) {
        float s = (mw * 0.95f) / SCREEN_W;
        if ((mh * 0.88f) / SCREEN_H < s) s = (mh * 0.88f) / SCREEN_H;
        w = (int)(SCREEN_W * s);
        h = (int)(SCREEN_H * s);
        SetWindowSize(w, h);
        SetWindowPosition((mw - w) / 2, (mh - h) / 2);
    }
    SetWindowMinSize(320, 180);

    target = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
}

void Screen_Begin(void)
{
    /* map real mouse coordinates back into virtual-screen coordinates */
    Rectangle d = DestRect();
    SetMouseOffset((int)-d.x, (int)-d.y);
    SetMouseScale(SCREEN_W / d.width, SCREEN_H / d.height);

    BeginTextureMode(target);
}

void Screen_End(void)
{
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    /* Copy without blending: translucent UI leaves alpha < 1 in the texture, which must not
     * make the final picture see-through. Render textures are stored upside down, hence the
     * negative source height. */
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    DrawTexturePro(target.texture, (Rectangle){ 0, 0, SCREEN_W, -SCREEN_H }, DestRect(),
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndDrawing();
}

bool Screen_Save(const char *path)
{
    Image img = LoadImageFromTexture(target.texture);
    bool ok;
    ImageFlipVertical(&img);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);   /* drop alpha (see Screen_End) */
    ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

void Screen_Shutdown(void)
{
    UnloadRenderTexture(target);
    CloseWindow();
}
