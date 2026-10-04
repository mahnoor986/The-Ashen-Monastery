/* atmos.h - the burning monastery's air: grey ash flakes drifting down around the camera,
 * and orange embers rising from the torches. */
#ifndef ATMOS_H
#define ATMOS_H

#include "raylib.h"
#include "config.h"
#include "world.h"

typedef struct {
    Vector3 pos;
    float   phase;            /* sway offset */
} AshFlake;

typedef struct {
    Vector3 pos;
    float   life, maxLife, phase;
} Ember;

typedef struct {
    AshFlake ash[ASH_COUNT];
    Ember    embers[EMBER_COUNT];
    float    time;
} Atmos;

void Atmos_Reset(Atmos *a, Vector3 center);                                 /* scatter the ash around a point */
void Atmos_Update(Atmos *a, Vector3 center, const World *w, float dt);
void Atmos_Draw(const Atmos *a);                                            /* inside BeginMode3D */

#endif
