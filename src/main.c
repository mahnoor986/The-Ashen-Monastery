#include "raylib.h"
#include "config.h"
#include "game.h"

int main(void)
{
    static Game game;          /* static: keeps the big struct off the stack */

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, WINDOW_TITLE);
    SetExitKey(KEY_NULL);      /* ESC pauses instead of quitting */
    SetTargetFPS(TARGET_FPS);

    Game_Init(&game);

    while (!WindowShouldClose() && !game.quit) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;          /* avoid huge steps after a stall */
        Game_Update(&game, dt);
        Game_Draw(&game);
    }

    Game_Shutdown(&game);
    CloseWindow();
    return 0;
}
