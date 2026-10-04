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
    bool    alerted;      /* enemy has noticed the player (eyes glow) */
} CharPose;

void Character_Init(void);                 /* needs the texture atlas (Textures_Init first) */
void Character_Shutdown(void);
void Character_SetShader(Shader shader);   /* use the world shader so fog/light apply */
void Character_DrawKnight(const CharPose *p);
void Character_DrawSkeleton(const CharPose *p);
void Character_DrawGhost(const CharPose *p);      /* draw in the transparent pass */
void Character_DrawWitch(const CharPose *p);
void Character_DrawQueen(const CharPose *p);      /* 1.8x witch with a crown and two orbiting orbs */
/* A glowing hex bolt (spinning purple cube) at `pos`. */
void Character_DrawBolt(Vector3 pos, float spin);

#endif
