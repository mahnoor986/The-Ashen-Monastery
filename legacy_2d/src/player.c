#include "player.h"
#include "config.h"
#include "draw.h"
#include "vec.h"

void Player_Init(Player *p, Vector2 pos)
{
    p->pos = pos;
    p->facing = V2(0.0f, 1.0f);
    p->knock = V2(0.0f, 0.0f);
    p->dashDir = V2(0.0f, 1.0f);
    p->maxHp = PLAYER_MAX_HP;
    p->hp = PLAYER_MAX_HP;
    p->keys = 0;
    p->swingId = 0;
    p->attackTimer = p->attackCooldown = 0.0f;
    p->invuln = 0.0f;
    p->dashTimer = p->dashCooldown = 0.0f;
}

void Player_Update(Player *p, const Tilemap *map, float dt)
{
    Vector2 in = V2(0.0f, 0.0f), move;

    if (p->attackTimer    > 0.0f) p->attackTimer    -= dt;
    if (p->attackCooldown > 0.0f) p->attackCooldown -= dt;
    if (p->invuln         > 0.0f) p->invuln         -= dt;
    if (p->dashTimer      > 0.0f) p->dashTimer      -= dt;
    if (p->dashCooldown   > 0.0f) p->dashCooldown   -= dt;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    in.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  in.y += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  in.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.x += 1.0f;
    in = V2Norm(in);

    /* dash */
    if (IsKeyPressed(KEY_LEFT_SHIFT) && p->dashCooldown <= 0.0f) {
        p->dashTimer = PLAYER_DASH_TIME;
        p->dashCooldown = PLAYER_DASH_COOLDOWN;
        p->dashDir = (V2Len(in) > 0.0f) ? in : p->facing;
        if (p->invuln < PLAYER_DASH_TIME + 0.05f) p->invuln = PLAYER_DASH_TIME + 0.05f;
    }

    /* attack */
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_J) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        && p->attackCooldown <= 0.0f && p->dashTimer <= 0.0f) {
        p->attackTimer = PLAYER_ATTACK_TIME;
        p->attackCooldown = PLAYER_ATTACK_COOLDOWN;
        p->swingId++;
    }

    if (p->dashTimer > 0.0f) {
        move = V2Scale(p->dashDir, PLAYER_DASH_SPEED * dt);
    } else {
        move = V2Scale(in, PLAYER_SPEED * dt);
        if (V2Len(in) > 0.0f) p->facing = in;
    }

    move = V2Add(move, V2Scale(p->knock, dt));
    p->knock = V2Scale(p->knock, Maxf(0.0f, 1.0f - KNOCKBACK_DAMPING * dt));

    Map_Move(map, &p->pos, V2(PLAYER_SIZE, PLAYER_SIZE), move);
}

bool Player_Hurt(Player *p, int dmg, Vector2 from)
{
    if (p->invuln > 0.0f || p->hp <= 0) return false;
    p->hp -= dmg;
    if (p->hp < 0) p->hp = 0;
    p->invuln = PLAYER_INVULN_TIME;
    p->knock = V2Scale(V2Norm(V2Sub(p->pos, from)), PLAYER_KNOCKBACK);
    return true;
}

void Player_Draw(const Player *p)
{
    int blink = (p->invuln > 0.0f && p->dashTimer <= 0.0f) ? ((int)(GetTime() * 24.0) & 1) : 0;

    if (p->attackTimer > 0.0f)
        DrawAttackArc(p->pos, p->facing, 1.0f - p->attackTimer / PLAYER_ATTACK_TIME);

    if (!blink)
        DrawEntityFx(ENT_PLAYER, p->pos, p->facing, (p->dashTimer > 0.0f) ? 0.6f : 0.0f);
}
