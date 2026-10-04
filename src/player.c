/* player.c - player movement, dash, sword timing, hearts and the third-person camera (see player.h). */
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
    Player fresh = { 0 };
    *p = fresh;
    p->pos = start;
    p->yaw = yaw;
    p->hearts = PLAYER_MAX_HEARTS;
    p->swingTime = -1.0f;
}

int Player_Update(Player *p, const CameraRig *rig, const World *w, const Input *in, float dt)
{
    Vector3 fwd = { sinf(rig->yaw), 0.0f, cosf(rig->yaw) };
    Vector3 right = { -fwd.z, 0.0f, fwd.x };
    Vector3 wish = Vector3Add(Vector3Scale(fwd, in->move.y), Vector3Scale(right, in->move.x));
    Vector3 target;
    float speed, k;
    int events = 0;

    p->time += dt;
    if (p->invuln > 0.0f) p->invuln -= dt;
    if (p->dashCooldown > 0.0f) p->dashCooldown -= dt;
    if (p->swingTime >= 0.0f) {
        p->swingTime += dt;
        if (p->swingTime >= SWORD_COOLDOWN) p->swingTime = -1.0f;
    }
    if (Vector3Length(wish) > 1.0f) wish = Vector3Normalize(wish);

    /* dash: a short burst in the movement direction (or forward), invulnerable while it lasts */
    if (in->dash && p->dashCooldown <= 0.0f && p->dashTime <= 0.0f) {
        p->dashDir = Vector3Length(wish) > 0.1f ? Vector3Normalize(wish)
                                                : (Vector3){ sinf(p->yaw), 0.0f, cosf(p->yaw) };
        p->dashTime = DASH_TIME;
        p->dashCooldown = DASH_COOLDOWN;
        events |= PLAYER_EV_DASH;
    }

    if (p->dashTime > 0.0f) {
        p->dashTime -= dt;
        p->vel = Vector3Scale(p->dashDir, DASH_SPEED);
    } else {
        /* ease velocity toward the target so starts/stops feel smooth but responsive */
        target = Vector3Scale(wish, PLAYER_SPEED * (p->swingTime >= 0.0f ? 0.55f : 1.0f));
        k = fminf(1.0f, PLAYER_ACCEL * dt);
        p->vel = Vector3Lerp(p->vel, target, k);
    }
    World_Move(w, &p->pos, PLAYER_SIZE, Vector3Scale(Vector3Add(p->vel, p->knock), dt));
    p->knock = Vector3Scale(p->knock, fmaxf(0.0f, 1.0f - 8.0f * dt));

    speed = Vector3Length(p->vel);
    p->walkPhase += speed * dt * 2.4f;

    /* footsteps while walking */
    if (speed > 1.0f && p->dashTime <= 0.0f) {
        p->stepTimer -= dt * (speed / PLAYER_SPEED);
        if (p->stepTimer <= 0.0f) { p->stepTimer = FOOTSTEP_INTERVAL; events |= PLAYER_EV_STEP; }
    }

    /* turn the body smoothly toward the movement direction (not while swinging) */
    if (Vector3Length(wish) > 0.1f && p->swingTime < 0.0f) {
        float want = atan2f(wish.x, wish.z);
        float d = AngleDiff(p->yaw, want);
        float step = PLAYER_TURN_SPEED * dt;
        p->yaw += (fabsf(d) < step) ? d : (d > 0 ? step : -step);
        p->yaw = Wrap(p->yaw, -PI, PI);
    }
    return events;
}

bool Player_CanSwing(const Player *p)
{
    return p->swingTime < 0.0f;
}

void Player_StartSwing(Player *p, float yaw)
{
    p->yaw = yaw;
    p->swingTime = 0.0f;
    p->swingId++;
}

bool Player_SwingActive(const Player *p)
{
    return p->swingTime >= 0.0f && p->swingTime < SWORD_ACTIVE;
}

bool Player_Hurt(Player *p, Vector3 from)
{
    Vector3 away;
    if (p->invuln > 0.0f || p->dashTime > 0.0f || p->god || p->hearts <= 0) return false;
    p->hearts--;
    p->invuln = PLAYER_INVULN;
    away = Vector3Subtract(p->pos, from);
    away.y = 0.0f;
    if (Vector3Length(away) > 0.01f) p->knock = Vector3Scale(Vector3Normalize(away), 6.0f);
    return true;
}

void Player_Draw(const Player *p)
{
    CharPose pose = { 0 };
    pose.pos = p->pos;
    pose.yaw = p->yaw;
    pose.walkPhase = p->walkPhase;
    pose.walkAmount = p->dashTime > 0.0f ? 0.3f : fminf(1.0f, Vector3Length(p->vel) / PLAYER_SPEED);
    pose.time = p->time;
    pose.swing = p->swingTime >= 0.0f ? fminf(1.0f, p->swingTime / SWORD_ACTIVE) : -1.0f;
    pose.blink = (p->invuln > 0.0f && fmodf(p->invuln, 0.16f) > 0.09f) ? 1.0f : 0.0f;
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

void CameraRig_Update(CameraRig *rig, const Player *p, const World *w, const Input *in, float dt)
{
    Vector3 head = { p->pos.x, p->pos.y + CAM_HEAD_HEIGHT, p->pos.z };
    Vector3 look, right, desired, dir;
    float maxDist, allowed, t;

    if (in) {
        rig->yaw -= in->look.x * MOUSE_SENSITIVITY + in->turn.x * KEY_TURN_SPEED * dt;
        rig->pitch -= in->look.y * MOUSE_SENSITIVITY - in->turn.y * KEY_TURN_SPEED * 0.5f * dt;
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
