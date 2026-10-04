/* game.h - the Game struct (all game state lives here) and the game flow:
 * loading wings, updating, drawing, and the --autotest screenshot run. */
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "config.h"
#include "world.h"
#include "player.h"

typedef enum {
    STATE_PLAYING = 0,
} GameState;

typedef struct Game {
    GameState state;
    int       wing;           /* 0-based index into WINGS */
    World     world;
    bool      worldLoaded;
    Player    player;
    CameraRig rig;
    float     time;           /* seconds since start */
    float     wingTime;       /* seconds since entering the current wing */
    bool      mouseLook;      /* mouse captured for camera control */
    int       mouseSkip;      /* frames to ignore mouse delta after capturing (avoids a jump) */
    bool      autotest;       /* no input, fixed dt */
    bool      showFps;
    bool      quit;
} Game;

void Game_Init(Game *g, int startWing, bool autotest);
bool Game_LoadWing(Game *g, int wing);      /* false if the wing file is missing/invalid */
void Game_Update(Game *g, float dt);
void Game_Draw(Game *g);                    /* draws into the virtual screen */
int  Game_Autotest(Game *g);                /* returns the process exit code */
void Game_Shutdown(Game *g);

#endif
