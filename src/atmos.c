/* atmos.c - falling ash and rising embers (see atmos.h). */
#include <math.h>
#include "atmos.h"
#include "rlgl.h"

static float Rand(float lo, float hi) { return lo + (hi - lo) * GetRandomValue(0, 10000) / 10000.0f; }

void Atmos_Reset(Atmos *a, Vector3 center)
{
    int i;
    for (i = 0; i < ASH_COUNT; i++) {
        a->ash[i].pos = (Vector3){ center.x + Rand(-ASH_RANGE, ASH_RANGE), Rand(0.0f, WALL_HEIGHT),
                                   center.z + Rand(-ASH_RANGE, ASH_RANGE) };
        a->ash[i].phase = Rand(0.0f, 6.28f);
    }
    for (i = 0; i < EMBER_COUNT; i++) a->embers[i].life = 0.0f;
}

/* Keep a coordinate within +-range of the center by wrapping it around. */
static float WrapAround(float v, float c, float range)
{
    if (v < c - range) return v + 2.0f * range;
    if (v > c + range) return v - 2.0f * range;
    return v;
}

void Atmos_Update(Atmos *a, Vector3 center, const World *w, float dt)
{
    int i;
    a->time += dt;
    for (i = 0; i < ASH_COUNT; i++) {
        AshFlake *f = &a->ash[i];
        f->pos.x += (ASH_WIND_X + sinf(a->time * 0.9f + f->phase) * 0.25f) * dt;
        f->pos.z += (ASH_WIND_Z + cosf(a->time * 0.7f + f->phase) * 0.20f) * dt;
        f->pos.y -= ASH_FALL_SPEED * (0.7f + 0.3f * sinf(f->phase)) * dt;
        if (f->pos.y < 0.0f) f->pos.y += WALL_HEIGHT;
        f->pos.x = WrapAround(f->pos.x, center.x, ASH_RANGE);
        f->pos.z = WrapAround(f->pos.z, center.z, ASH_RANGE);
    }

    for (i = 0; i < EMBER_COUNT; i++) {
        Ember *e = &a->embers[i];
        if (e->life > 0.0f) {
            e->life -= dt;
            e->pos.y += (0.5f + 0.4f * (e->life / e->maxLife)) * dt;
            e->pos.x += sinf(a->time * 3.0f + e->phase) * 0.25f * dt;
            e->pos.z += cosf(a->time * 2.3f + e->phase) * 0.25f * dt;
            continue;
        }
        /* respawn at a random torch near the camera */
        if (w->torchCount > 0) {
            const Torch *t = &w->torches[GetRandomValue(0, w->torchCount - 1)];
            float dx = t->pos.x - center.x, dz = t->pos.z - center.z;
            if (dx * dx + dz * dz > EMBER_RANGE * EMBER_RANGE) continue;
            e->pos = (Vector3){ t->pos.x + Rand(-0.08f, 0.08f), t->pos.y + 0.1f, t->pos.z + Rand(-0.08f, 0.08f) };
            e->maxLife = e->life = Rand(1.2f, 2.8f);
            e->phase = Rand(0.0f, 6.28f);
        }
    }
}

void Atmos_Draw(const Atmos *a)
{
    int i;
    for (i = 0; i < ASH_COUNT; i++)
        DrawCube(a->ash[i].pos, 0.035f, 0.035f, 0.035f, (Color){ 92, 86, 82, 255 });

    /* embers glow: additive blending */
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    for (i = 0; i < EMBER_COUNT; i++) {
        const Ember *e = &a->embers[i];
        float k;
        if (e->life <= 0.0f) continue;
        k = e->life / e->maxLife;
        DrawCube(e->pos, 0.035f, 0.035f, 0.035f, (Color){ 255, (unsigned char)(90 + 100 * k), 30, (unsigned char)(255 * k) });
    }
    rlDrawRenderBatchActive();
    EndBlendMode();
}
