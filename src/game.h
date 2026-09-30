#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "raylib.h"
#include "config.h"
#include "map.h"
#include "player.h"
#include "enemy.h"

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_INVENTORY,
    STATE_GAME_OVER,
    STATE_VICTORY
} GameState;

typedef struct {
    bool       active;
    EntityType type;     /* ENT_KEY or ENT_POTION */
    Vector2    pos;
} Pickup;

typedef struct {
    GameState  state;
    bool       quit;
    bool       debugDraw;
    bool       lighting;

    Tilemap    map;
    Player     player;
    Enemy      enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Pickup     pickups[MAX_PICKUPS];

    int        level;
    int        kills;
    float      time;
    float      bannerTimer;
    float      hurtFlash;
    float      shake;
    Camera2D   camera;
} Game;

void Game_Init(Game *g);
void Game_Update(Game *g, float dt);
void Game_Draw(Game *g);
void Game_Shutdown(Game *g);

#endif
