/* input.c - reads the keyboard and mouse into an Input struct (see input.h). */
#include "input.h"

Input Input_Read(void)
{
    Input in = { 0 };
    if (IsKeyDown(KEY_W)) in.move.y += 1.0f;
    if (IsKeyDown(KEY_S)) in.move.y -= 1.0f;
    if (IsKeyDown(KEY_D)) in.move.x += 1.0f;
    if (IsKeyDown(KEY_A)) in.move.x -= 1.0f;
    if (IsKeyDown(KEY_RIGHT)) in.turn.x += 1.0f;
    if (IsKeyDown(KEY_LEFT))  in.turn.x -= 1.0f;
    if (IsKeyDown(KEY_UP))    in.turn.y += 1.0f;
    if (IsKeyDown(KEY_DOWN))  in.turn.y -= 1.0f;
    in.look = GetMouseDelta();
    in.swing = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.click = in.swing;
    in.mouse = GetMousePosition();
    in.dash = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT);
    in.use = IsKeyDown(KEY_E);
    in.usePressed = IsKeyPressed(KEY_E);
    in.pause = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P);
    in.inventory = IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB);
    in.map = IsKeyPressed(KEY_M);
    in.anyKey = GetKeyPressed() != 0 || in.click || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    in.up = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    in.down = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
    in.debug = IsKeyPressed(KEY_F1);
    in.showFps = IsKeyPressed(KEY_F6);
    in.togglePost = IsKeyPressed(KEY_F2);
    in.god = IsKeyPressed(KEY_F3);
    in.skipWing = IsKeyPressed(KEY_F4);
    return in;
}
