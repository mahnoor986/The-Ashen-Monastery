/* character.h - blocky humanoid models built from boxes, plus their animations.
 * Every body part is the same unit cube mesh drawn with its own transform matrix
 * (scale x joint rotation x translation) and tint color. */
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
