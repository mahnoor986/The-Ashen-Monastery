/* main.c - program entry point: window, main loop and command-line flags.
 *   --wing N     start directly in wing N (1-5)
 *   --autotest   load every wing, render it, save screenshots to shots/, print a summary, exit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "config.h"
#include "screen.h"
#include "game.h"

int main(int argc, char **argv)
{
    static Game game;          /* static: keeps the big struct off the stack */
    bool autotest = false;
    int startWing = 0, i, code = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) autotest = true;
        else if (strcmp(argv[i], "--wing") == 0 && i + 1 < argc) startWing = atoi(argv[++i]) - 1;
    }
    if (startWing < 0 || startWing >= WING_COUNT) startWing = 0;

    if (autotest) SetTraceLogLevel(LOG_WARNING);    /* keep the summary readable */
    Screen_Init(WINDOW_TITLE);
    ChangeDirectory(GetApplicationDirectory());     /* relative asset paths work when double-clicked */
    SetExitKey(KEY_NULL);                           /* Esc is used by the game, not to quit */
    SetTargetFPS(TARGET_FPS);

    Game_Init(&game, startWing, autotest);

    if (autotest) {
        code = Game_Autotest(&game);
    } else {
        while (!WindowShouldClose() && !game.quit) {
            float dt = GetFrameTime();
            if (dt > MAX_DT) dt = MAX_DT;
            Game_Update(&game, dt);
            Screen_Begin();
            Game_Draw(&game);
            Screen_End();
        }
    }

    Game_Shutdown(&game);
    Screen_Shutdown();
    return code;
}
