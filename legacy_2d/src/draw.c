#include <math.h>
#include "draw.h"
#include "config.h"
#include "vec.h"

static Color Flash(Color c, float t)
{
    if (t <= 0.0f) return c;
    if (t > 1.0f) t = 1.0f;
    c.r = (unsigned char)(c.r + (255 - c.r) * t);
    c.g = (unsigned char)(c.g + (255 - c.g) * t);
    c.b = (unsigned char)(c.b + (255 - c.b) * t);
    return c;
}

static void Box(Vector2 pos, float w, float h, Color fill)
{
    DrawRectangleRec((Rectangle){ pos.x - w * 0.5f, pos.y - h * 0.5f, w, h }, fill);
}

void DrawTriFacing(Vector2 c, Vector2 dir, float len, float halfWidth, Color col)
{
    Vector2 perp = V2(-dir.y, dir.x);
    Vector2 tip  = V2(c.x + dir.x * len, c.y + dir.y * len);
    Vector2 base = V2(c.x - dir.x * len * 0.4f, c.y - dir.y * len * 0.4f);
    Vector2 b = V2(base.x + perp.x * halfWidth, base.y + perp.y * halfWidth);
    Vector2 d = V2(base.x - perp.x * halfWidth, base.y - perp.y * halfWidth);
    /* raylib wants a specific winding; fix it from the sign of the cross product */
    float cross = (b.x - tip.x) * (d.y - tip.y) - (b.y - tip.y) * (d.x - tip.x);
    if (cross > 0.0f) DrawTriangle(tip, d, b, col);
    else              DrawTriangle(tip, b, d, col);
}

void DrawEntity(EntityType type, Vector2 pos, Vector2 facing)
{
    DrawEntityFx(type, pos, facing, 0.0f);
}

void DrawEntityFx(EntityType type, Vector2 pos, Vector2 facing, float flash)
{
    float t = (float)GetTime();
    if (V2Len(facing) < 0.01f) facing = V2(0.0f, 1.0f);
    facing = V2Norm(facing);

    switch (type) {
    case ENT_PLAYER:
        Box(pos, 20, 20, Flash((Color){ 190, 192, 206, 255 }, flash));
        DrawRectangleLinesEx((Rectangle){ pos.x - 10, pos.y - 10, 20, 20 }, 1, (Color){ 60, 64, 84, 255 });
        DrawTriFacing(V2Add(pos, V2Scale(facing, 8)), facing, 10, 6, COL_BONE);
        break;

    case ENT_SKELETON:
        Box(pos, 20, 20, Flash((Color){ 222, 216, 196, 255 }, flash));
        DrawRectangleLinesEx((Rectangle){ pos.x - 10, pos.y - 10, 20, 20 }, 2, (Color){ 38, 34, 32, 255 });
        Box(V2(pos.x - 4, pos.y - 3), 3, 4, (Color){ 30, 26, 26, 255 });
        Box(V2(pos.x + 4, pos.y - 3), 3, 4, (Color){ 30, 26, 26, 255 });
        break;

    case ENT_GHOUL:
        DrawCircleV(pos, 12, Flash((Color){ 40, 84, 52, 255 }, flash));
        DrawCircleLinesV(pos, 12, (Color){ 16, 38, 24, 255 });
        DrawCircleV(V2(pos.x - 4, pos.y - 3), 2, (Color){ 210, 210, 90, 255 });
        DrawCircleV(V2(pos.x + 4, pos.y - 3), 2, (Color){ 210, 210, 90, 255 });
        break;

    case ENT_BAT: {
        float wob = sinf(t * 18.0f + pos.x * 0.1f) * 1.5f;
        DrawTriFacing(V2(pos.x, pos.y + wob), facing, 8, 9, Flash((Color){ 112, 60, 142, 255 }, flash));
        break;
    }

    case ENT_CULTIST:
        Box(pos, 18, 24, Flash(COL_BLOOD, flash));
        DrawCircleV(V2(pos.x, pos.y - 10), 6, Flash((Color){ 70, 10, 18, 255 }, flash));
        Box(V2(pos.x, pos.y - 9), 3, 3, COL_CANDLE);
        break;

    case ENT_BOSS: {
        float pulse = sinf(t * 3.0f) * 2.0f;
        DrawCircleV(pos, 28, Flash((Color){ 170, 20, 40, 255 }, flash));
        DrawCircleV(pos, 14, Flash((Color){ 92, 8, 22, 255 }, flash));
        DrawCircleLinesV(pos, 34 + pulse, COL_BLOOD);
        DrawCircleLinesV(pos, 38 + pulse, (Color){ 90, 10, 20, 255 });
        DrawCircleV(V2(pos.x - 8, pos.y - 6), 3, COL_CANDLE);
        DrawCircleV(V2(pos.x + 8, pos.y - 6), 3, COL_CANDLE);
        break;
    }

    case ENT_PROJECTILE:
        DrawCircleV(pos, 5, (Color){ 170, 60, 200, 255 });
        DrawCircleV(pos, 2.5f, (Color){ 240, 200, 255, 255 });
        break;

    case ENT_KEY:
        DrawCircleV(V2(pos.x, pos.y - 3), 5, COL_CANDLE);
        DrawCircleV(V2(pos.x, pos.y - 3), 2, (Color){ 60, 40, 10, 255 });
        Box(V2(pos.x, pos.y + 4), 3, 9, COL_CANDLE);
        Box(V2(pos.x + 3, pos.y + 6), 4, 2, COL_CANDLE);
        break;

    case ENT_POTION:
        DrawCircleV(V2(pos.x, pos.y + 1), 7, (Color){ 170, 24, 40, 255 });
        DrawCircleV(V2(pos.x - 2, pos.y - 1), 2, (Color){ 240, 140, 150, 255 });
        Box(V2(pos.x, pos.y - 8), 5, 5, COL_BONE);
        break;

    case ENT_TORCH: {
        float fl = sinf(t * 12.0f + pos.x) * 1.2f + sinf(t * 7.3f + pos.y) * 0.8f;
        Box(V2(pos.x, pos.y + 4), 4, 10, (Color){ 70, 46, 28, 255 });
        DrawCircleV(V2(pos.x, pos.y - 2), 5.0f + fl * 0.4f, (Color){ 240, 120, 30, 255 });
        DrawCircleV(V2(pos.x, pos.y - 2), 2.5f + fl * 0.3f, (Color){ 255, 220, 120, 255 });
        break;
    }

    default: break;
    }
}

void DrawAttackArc(Vector2 pos, Vector2 facing, float progress)
{
    float a = atan2f(facing.y, facing.x) * RAD2DEG;
    float half = PLAYER_ATTACK_ARC_DEG * 0.5f;
    float alpha = Clampf(1.0f - progress, 0.0f, 1.0f);
    float r = PLAYER_ATTACK_RANGE + 6.0f;
    DrawCircleSector(pos, r, a - half, a + half, 20, Fade(WHITE, 0.45f * alpha));
    DrawCircleSectorLines(pos, r, a - half, a + half, 20, Fade(COL_BONE, 0.9f * alpha));
}
