/* main.c - program entry point: window, main loop and command-line flags.
 *   --wing N     start directly in wing N (1-5, 6 = the Sanctum), skipping the menu
 *   --sanctum    start directly in the Sanctum (the ending hall)
 *   --tour       4 screenshots per wing in shots/tour_wN_K.png + average FPS, then exit
 *   --autotest   render every wing + screens, play wing 1 with scripted input, save
 *                screenshots to shots/, print a summary, exit 0 (or 1 on any error) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "config.h"
#include "screen.h"
#include "input.h"
#include "game.h"
#include "render.h"

int main(int argc, char **argv)
{
    static Game game;          /* static: keeps the big struct off the stack */
    bool autotest = false, directStart = false, tour = false;
    int startWing = 0, i, code = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) autotest = true;
        else if (strcmp(argv[i], "--tour") == 0) tour = true;
        else if (strcmp(argv[i], "--bright") == 0) Render_SetDebugBright(true);
        else if (strcmp(argv[i], "--sanctum") == 0) { startWing = WING_COUNT; directStart = true; }
        else if (strcmp(argv[i], "--wing") == 0 && i + 1 < argc) { startWing = atoi(argv[++i]) - 1; directStart = true; }
    }
    if (startWing < 0 || startWing > WING_COUNT) startWing = 0;

    if (autotest || tour) {
        SetTraceLogLevel(LOG_WARNING);              /* keep the summary readable */
        setvbuf(stdout, NULL, _IONBF, 0);           /* show progress immediately */
    }
    Screen_Init(WINDOW_TITLE, !tour);               /* --tour measures the real frame rate */
    ChangeDirectory(GetApplicationDirectory());     /* relative asset paths work when double-clicked */
    SetExitKey(KEY_NULL);                           /* Esc pauses instead of quitting */
    SetTargetFPS(tour ? 0 : TARGET_FPS);

    Game_Init(&game, startWing, directStart, autotest || tour);

    if (tour) {
        code = Game_Tour(&game);
    } else if (autotest) {
        code = Game_Autotest(&game);
    } else {
        while (!WindowShouldClose() && !game.quit) {
            Input in = Input_Read();
            float dt = GetFrameTime();
            if (dt > MAX_DT) dt = MAX_DT;
            Game_Update(&game, &in, dt);
            Game_Draw(&game);              /* renders the 3D scene, then the UI, then presents */
        }
    }

    Game_Shutdown(&game);
    Screen_Shutdown();
    return code;
}
