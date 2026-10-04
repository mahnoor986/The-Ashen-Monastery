/* character.h - code-built low-poly characters (robes, cylinders, spheres, cones) and their
 * animations, plus "model slots": a .glb file in assets/models/ replaces the code-built look. */
#ifndef CHARACTER_H
#define CHARACTER_H

#include "raylib.h"

typedef struct {
    Vector3 pos;          /* feet position */
    float   yaw;          /* facing: forward = (sin yaw, 0, cos yaw) */
    float   walkPhase;    /* radians; advance with distance walked */
    float   walkAmount;   /* 0 = standing, 1 = full walk swing */
    float   time;         /* seconds, drives idle breathing / floating */
    float   swing;        /* sword swing progress 0..1, or < 0 when not swinging */
    float   windup;       /* enemy attack wind-up 0..1 (red tint + lean back) */
    float   flash;        /* hit flash 0..1 (white tint) */
    float   blink;        /* 1 = hide this frame (invulnerability blink) */
    float   alpha;        /* 0..1 opacity (ghosts); 0 is treated as 1 */
    bool    alerted;      /* enemy has noticed the player */
    float   headYaw;      /* twitch: sudden head turn */
    float   headRoll;     /* twitch: sudden head tilt */
} CharPose;

/* Roles that can be replaced by assets/models/<name>.glb (see assets/models/README.txt). */
enum { ROLE_PLAYER = 0, ROLE_MONK, ROLE_WRAITH, ROLE_PRIEST, ROLE_ABBOT, ROLE_SERPENT, ROLE_OREN, ROLE_APPRENTICE, ROLE_COUNT };
/* Animations looked up by keyword in a model's animation names; missing ones use idle. */
enum { ANIM_IDLE = 0, ANIM_WALK, ANIM_ATTACK, ANIM_HIT, ANIM_DEATH, ANIM_COUNT };

/* Load assets/models/<name>.glb once, scaled to `height` (feet on the floor). Returns a slot
 * index, or -1 if the file is missing/invalid (then use the code-built look). */
int  Model_Slot(const char *name, float height, float yawOffset);
/* Draw a loaded model slot at pos/yaw playing animation `anim` (ANIM_*). False if no model. */
bool Model_Draw(int slot, Vector3 pos, float yaw, int anim, float time, Color tint);

void Character_Init(void);                 /* needs the texture atlas (Textures_Init first) */
void Character_Shutdown(void);
void Character_SetShader(Shader shader);   /* use the world shader so fog/light apply */
void Character_DrawKnight(const CharPose *p);
void Character_DrawMonk(const CharPose *p);       /* Ashen Monk: charred hooded robe, ember cracks */
void Character_DrawWraith(const CharPose *p);     /* Choir Wraith: draw in the transparent pass */
void Character_DrawPriest(const CharPose *p);     /* Ember Priest: crimson robe, tall hood, censer */
void Character_DrawAbbot(const CharPose *p);      /* The Red Abbot: 1.8x, bell mitre, two fireballs */
/* A friendly robed person in the Sanctum; `oren` adds beard, white hair and a glowing staff. */
void Character_DrawRobedNpc(const CharPose *p, Color robe, bool oren);
/* A fireball (spinning orange cube) at `pos`. */
void Character_DrawBolt(Vector3 pos, float spin);

#endif
