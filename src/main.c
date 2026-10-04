/* main.c - program entry point: window, main loop and command-line flags.
 *   --wing N     start directly in wing N (1-5), skipping the menu
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

int main(int argc, char **argv)
{
    static Game game;          /* static: keeps the big struct off the stack */
    bool autotest = false, directStart = false;
    int startWing = 0, i, code = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) autotest = true;
        else if (strcmp(argv[i], "--wing") == 0 && i + 1 < argc) { startWing = atoi(argv[++i]) - 1; directStart = true; }
    }
    if (startWing < 0 || startWing >= WING_COUNT) startWing = 0;

    if (autotest) {
        SetTraceLogLevel(LOG_WARNING);              /* keep the summary readable */
        setvbuf(stdout, NULL, _IONBF, 0);           /* show progress immediately */
    }
    Screen_Init(WINDOW_TITLE);
    ChangeDirectory(GetApplicationDirectory());     /* relative asset paths work when double-clicked */
    SetExitKey(KEY_NULL);                           /* Esc pauses instead of quitting */
    SetTargetFPS(TARGET_FPS);

    Game_Init(&game, startWing, directStart, autotest);

    if (autotest) {
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
