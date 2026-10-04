/* main.c - program entry point.
 * Phase 0 placeholder: opens the window, loads the fonts and shows a title card.
 * `--autotest` saves shots/menu.png and exits, proving the build + screenshot pipeline works. */
#include <string.h>
#include "raylib.h"
#include "config.h"
#include "screen.h"

int main(int argc, char **argv)
{
    bool autotest = false;
    int frame = 0;
    Font title, body;
    int i;

    for (i = 1; i < argc; i++)
        if (strcmp(argv[i], "--autotest") == 0) autotest = true;

    Screen_Init(WINDOW_TITLE);
    ChangeDirectory(GetApplicationDirectory());     /* relative asset paths work when double-clicked */
    SetExitKey(KEY_NULL);
    SetTargetFPS(TARGET_FPS);

    title = FileExists(FONT_TITLE_FILE) ? LoadFontEx(FONT_TITLE_FILE, 96, NULL, 0) : GetFontDefault();
    body  = FileExists(FONT_BODY_FILE)  ? LoadFontEx(FONT_BODY_FILE, 32, NULL, 0)  : GetFontDefault();

    while (!WindowShouldClose()) {
        const char *t = "BLACKTHORN MANOR";
        const char *s = "Collect the treasures. Survive the night.";
        Vector2 ts = MeasureTextEx(title, t, 96, 2);
        Vector2 ss = MeasureTextEx(body, s, 32, 1);

        Screen_Begin();
        ClearBackground(COL_NEARBLACK);
        DrawTextEx(title, t, (Vector2){ (SCREEN_W - ts.x) / 2, 240 }, 96, 2, COL_BLOOD);
        DrawTextEx(body, s, (Vector2){ (SCREEN_W - ss.x) / 2, 350 }, 32, 1, COL_BONE);
        Screen_End();

        if (autotest && ++frame == 3) {
            MakeDirectory("shots");
            Screen_Save("shots/menu.png");
            break;
        }
    }

    if (title.texture.id != GetFontDefault().texture.id) UnloadFont(title);
    if (body.texture.id != GetFontDefault().texture.id) UnloadFont(body);
    Screen_Shutdown();
    return 0;
}
