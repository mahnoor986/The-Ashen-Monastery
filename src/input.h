/* input.h - one frame of player input. The game reads only this struct, so the autotest
 * can drive the game with scripted input instead of the keyboard and mouse. */
#ifndef INPUT_H
#define INPUT_H

#include "raylib.h"

typedef struct {
    Vector2 move;        /* x = right, y = forward, each -1..1 (WASD) */
    Vector2 look;        /* mouse movement in pixels this frame */
    Vector2 turn;        /* arrow keys, -1..1 (camera turn without a mouse) */
    bool swing;          /* left click pressed */
    bool dash;           /* shift pressed */
    bool use;            /* E held */
    bool usePressed;     /* E pressed this frame */
    bool pause;          /* Esc or P pressed */
    bool inventory;      /* I or Tab pressed */
    bool confirm;        /* Enter pressed */
    bool up, down;       /* menu navigation pressed (W/S or arrows) */
    bool click;          /* left click pressed (menus) */
    Vector2 mouse;       /* mouse position in virtual-screen pixels */
    bool debug, god, skipWing;   /* F1, F3, F4 */
    bool showFps;        /* F6 */
    bool togglePost;     /* F2: post-process on/off */
} Input;

Input Input_Read(void);  /* from keyboard + mouse */

#endif
