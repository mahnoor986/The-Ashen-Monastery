/* relics.h - the five Soul Relics, the Ember Serpent's cage and the serpent itself (Bell Tower),
 * and the Red Abbot's shield (he is immortal until the serpent dies). CLAUDE.md section 9. */
#ifndef RELICS_H
#define RELICS_H

#include "raylib.h"
#include "config.h"

struct Game;

#define SERPENT_SEGMENTS 26

typedef struct {
    bool    exists;          /* this wing has the cage + serpent (the Bell Tower) */
    bool    freed;           /* the cage is broken */
    bool    alive;
    Vector3 seg[SERPENT_SEGMENTS];   /* seg[0] = head; the body follows like a rope */
    float   headY;           /* how high the head is raised */
    float   yaw, time;
    int     hp;
    float   flash;           /* hit flash */
    float   rear;            /* >= 0: rearing up before a lunge (seconds) */
    float   lunge;           /* > 0: striking forward (seconds left) */
    Vector3 lungeDir;
    bool    lungeHit;        /* the current strike already hit */
    float   lungeTimer;      /* time until the next strike */
    float   hissTimer;
    float   dying;           /* > 0: writhing before it bursts into embers */
    float   side;            /* which way it circles the player */
} Serpent;

/* Called after a wing is loaded / reset to its start (relic hidden, cage + serpent placed). */
void Relics_OnWingLoaded(struct Game *g);
/* The last chest of the wing was opened: the Soul Relic rises from it. */
void Relics_Rise(struct Game *g, Vector3 chestPos);
void Relics_Update(struct Game *g, float dt);
/* Targets for aim assist: fills `pos` with the best relic/cage/serpent target within range and
 * sight (relic first). Returns 0 if none, else the target kind. */
int  Relics_AimTarget(const struct Game *g, Vector3 from, Vector3 *pos);
/* Lightning ray step at point p: 0 = nothing here, 1 = hit (the ray stops; the hit is handled). */
int  Relics_RayHit(struct Game *g, Vector3 p);
/* Moving lights (relic glow, cage runes) for this frame; call before Render_BeginFrame. */
void Relics_Lights(const struct Game *g);
void Relics_Draw(const struct Game *g);              /* opaque pass, inside BeginMode3D */
void Relics_DrawTransparent(const struct Game *g);   /* the Abbot's shield */
/* F4 / tests: destroy this wing's relic, break the cage, kill the serpent. */
void Relics_Complete(struct Game *g);
/* Draw relic i floating at pos (title of the autotest's relic line-up). */
void Relics_DrawRelic(int index, Vector3 pos, float time, int hits);

void Relics_Init(void);       /* builds the relic meshes */
void Relics_Shutdown(void);

extern const char *const RELIC_NAMES[WING_COUNT];

#endif
