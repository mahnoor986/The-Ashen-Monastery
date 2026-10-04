/* player.h - the knight: movement relative to the camera, dash, sword swing timing, hearts,
 * and the third-person orbit camera with wall collision. */
#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "world.h"
#include "character.h"
#include "input.h"

typedef struct {
    Vector3 pos;          /* feet position */
    Vector3 vel;          /* current horizontal velocity */
    Vector3 knock;        /* knockback velocity (decays) */
    float   yaw;          /* body facing */
    float   walkPhase;    /* advances with distance walked (animation) */
    float   time;
    int     hearts;
    float   invuln;       /* > 0: can't be hurt (and blinks) */
    float   dashTime;     /* > 0 while dashing */
    float   dashCooldown;
    Vector3 dashDir;
    float   swingTime;    /* seconds since the swing started, < 0 when not swinging */
    int     swingId;      /* increases every swing, so each enemy is hit once per swing */
    float   stepTimer;    /* footstep sound timer */
    bool    god;          /* F3 debug: can't be hurt */
} Player;

/* things that happened during Player_Update (for sounds) */
enum { PLAYER_EV_STEP = 1, PLAYER_EV_DASH = 2 };

typedef struct {
    float    yaw, pitch;  /* orbit angles (radians); forward = (sin yaw, 0, cos yaw) */
    float    dist;        /* current (smoothed) distance after wall collision */
    Camera3D cam;
} CameraRig;

void Player_Init(Player *p, Vector3 start, float yaw);
/* Moves with wall sliding, handles dash. Returns PLAYER_EV_* flags. */
int  Player_Update(Player *p, const CameraRig *rig, const World *w, const Input *in, float dt);
bool Player_CanSwing(const Player *p);
void Player_StartSwing(Player *p, float yaw);
bool Player_SwingActive(const Player *p);      /* the sword can hit right now */
/* Take one heart of damage from `from`. Returns false if invulnerable (blinking, dashing, god). */
bool Player_Hurt(Player *p, Vector3 from);
void Player_Draw(const Player *p);

void CameraRig_Init(CameraRig *rig, float yaw);
/* Turn with the input (if any), then place the camera behind the player without entering walls. */
void CameraRig_Update(CameraRig *rig, const Player *p, const World *w, const Input *in, float dt);

#endif
