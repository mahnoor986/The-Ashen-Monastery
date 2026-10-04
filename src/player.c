/* player.c - player movement and the third-person camera (see player.h). */
#include <math.h>
#include "player.h"
#include "config.h"
#include "raymath.h"

/* Shortest signed difference between two angles, in -PI..PI. */
static float AngleDiff(float from, float to)
{
    return Wrap(to - from, -PI, PI);
}

/* ------------------------------------------------------------ player */

void Player_Init(Player *p, Vector3 start, float yaw)
{
    p->pos = start;
    p->vel = (Vector3){ 0 };
    p->yaw = yaw;
    p->walkPhase = 0.0f;
    p->time = 0.0f;
}

void Player_Update(Player *p, const CameraRig *rig, const World *w, float dt, bool input)
{
    Vector3 fwd = { sinf(rig->yaw), 0.0f, cosf(rig->yaw) };
    Vector3 right = { -fwd.z, 0.0f, fwd.x };
    Vector3 wish = { 0 }, target;
    float speed, k;

    p->time += dt;
    if (input) {
        if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, fwd);
        if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, fwd);
        if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
        if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    }
    if (Vector3Length(wish) > 0.01f) wish = Vector3Normalize(wish);
    target = Vector3Scale(wish, PLAYER_SPEED);

    /* ease velocity toward the target so starts/stops feel smooth but responsive */
    k = fminf(1.0f, PLAYER_ACCEL * dt);
    p->vel = Vector3Lerp(p->vel, target, k);
    World_Move(w, &p->pos, PLAYER_SIZE, Vector3Scale(p->vel, dt));

    speed = Vector3Length(p->vel);
    p->walkPhase += speed * dt * 2.4f;

    /* turn the body smoothly toward the movement direction */
    if (Vector3Length(wish) > 0.01f) {
        float want = atan2f(wish.x, wish.z);
        float d = AngleDiff(p->yaw, want);
        float step = PLAYER_TURN_SPEED * dt;
        p->yaw += (fabsf(d) < step) ? d : (d > 0 ? step : -step);
        p->yaw = Wrap(p->yaw, -PI, PI);
    }
}

void Player_Draw(const Player *p)
{
    CharPose pose = { 0 };
    pose.pos = p->pos;
    pose.yaw = p->yaw;
    pose.walkPhase = p->walkPhase;
    pose.walkAmount = fminf(1.0f, Vector3Length(p->vel) / PLAYER_SPEED);
    pose.time = p->time;
    pose.swing = -1.0f;
    Character_DrawKnight(&pose);
}

/* ------------------------------------------------------------ camera */

void CameraRig_Init(CameraRig *rig, float yaw)
{
    rig->yaw = yaw;
    rig->pitch = CAM_PITCH_DEFAULT * DEG2RAD;
    rig->dist = CAM_DISTANCE;
    rig->cam = (Camera3D){ 0 };
    rig->cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    rig->cam.fovy = CAM_FOVY;
    rig->cam.projection = CAMERA_PERSPECTIVE;
}

/* Is a camera at point q too close to a wall, the floor or the ceiling? */
static bool CameraBlocked(const World *w, Vector3 q)
{
    const float m = CAM_WALL_MARGIN;
    if (q.y < CAM_MIN_Y || q.y > CAM_MAX_Y) return true;
    return World_IsWallCell(w, (int)floorf(q.x - m), (int)floorf(q.z - m)) ||
           World_IsWallCell(w, (int)floorf(q.x + m), (int)floorf(q.z - m)) ||
           World_IsWallCell(w, (int)floorf(q.x - m), (int)floorf(q.z + m)) ||
           World_IsWallCell(w, (int)floorf(q.x + m), (int)floorf(q.z + m));
}

void CameraRig_Update(CameraRig *rig, const Player *p, const World *w, float dt, bool mouse)
{
    Vector3 head = { p->pos.x, p->pos.y + CAM_HEAD_HEIGHT, p->pos.z };
    Vector3 look, right, desired, dir;
    float maxDist, allowed, t;

    if (mouse) {
        Vector2 md = GetMouseDelta();
        rig->yaw -= md.x * MOUSE_SENSITIVITY;
        rig->pitch -= md.y * MOUSE_SENSITIVITY;
        /* arrow keys also turn the camera (handy without a mouse) */
        if (IsKeyDown(KEY_LEFT))  rig->yaw += KEY_TURN_SPEED * dt;
        if (IsKeyDown(KEY_RIGHT)) rig->yaw -= KEY_TURN_SPEED * dt;
        if (IsKeyDown(KEY_UP))    rig->pitch += KEY_TURN_SPEED * 0.5f * dt;
        if (IsKeyDown(KEY_DOWN))  rig->pitch -= KEY_TURN_SPEED * 0.5f * dt;
    }
    rig->yaw = Wrap(rig->yaw, -PI, PI);
    rig->pitch = Clamp(rig->pitch, CAM_PITCH_MIN * DEG2RAD, CAM_PITCH_MAX * DEG2RAD);

    look = (Vector3){ cosf(rig->pitch) * sinf(rig->yaw), sinf(rig->pitch), cosf(rig->pitch) * cosf(rig->yaw) };
    right = (Vector3){ -cosf(rig->yaw), 0.0f, sinf(rig->yaw) };

    /* where the camera wants to be: behind the head, over the right shoulder */
    desired = Vector3Subtract(Vector3Add(head, Vector3Scale(right, CAM_SHOULDER)), Vector3Scale(look, CAM_DISTANCE));
    dir = Vector3Subtract(desired, head);
    maxDist = Vector3Length(dir);
    dir = Vector3Scale(dir, 1.0f / maxDist);

    /* walk from the head toward that spot and stop just before anything solid */
    allowed = maxDist;
    for (t = 0.05f; t <= maxDist; t += 0.05f) {
        if (CameraBlocked(w, Vector3Add(head, Vector3Scale(dir, t)))) { allowed = fmaxf(0.0f, t - 0.05f); break; }
    }

    /* snap in instantly (never clip), ease back out slowly (no popping) */
    if (allowed < rig->dist) rig->dist = allowed;
    else rig->dist += (allowed - rig->dist) * fminf(1.0f, CAM_RETURN_SPEED * dt);

    rig->cam.position = Vector3Add(head, Vector3Scale(dir, rig->dist));
    rig->cam.target = Vector3Add(rig->cam.position, look);
}
