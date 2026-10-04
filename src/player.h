/* player.h - the knight: movement relative to the camera, body turning, and the
 * third-person orbit camera with wall collision. */
#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "world.h"
#include "character.h"

typedef struct {
    Vector3 pos;          /* feet position */
    Vector3 vel;          /* current horizontal velocity */
    float   yaw;          /* body facing */
    float   walkPhase;    /* advances with distance walked (animation) */
    float   time;
} Player;

typedef struct {
    float    yaw, pitch;  /* orbit angles (radians); forward = (sin yaw, 0, cos yaw) */
    float    dist;        /* current (smoothed) distance after wall collision */
    Camera3D cam;
} CameraRig;

void Player_Init(Player *p, Vector3 start, float yaw);
/* Reads WASD, moves with wall sliding. `input` = false freezes the controls (autotest, menus). */
void Player_Update(Player *p, const CameraRig *rig, const World *w, float dt, bool input);
void Player_Draw(const Player *p);

void CameraRig_Init(CameraRig *rig, float yaw);
/* Mouse look (if `mouse`), then place the camera behind the player without entering walls. */
void CameraRig_Update(CameraRig *rig, const Player *p, const World *w, float dt, bool mouse);

#endif
