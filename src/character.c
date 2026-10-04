/* character.c - blocky humanoid models (see character.h).
 * Models face +Z in their own space, feet at y = 0; the character's right side is -X.
 * A "frame" is a matrix that places a body part: Part() draws a box inside a frame, Joint()
 * makes a child frame rotated around a joint (shoulder, hip, neck). */
#include <math.h>
#include "character.h"
#include "meshgen.h"
#include "textures.h"
#include "render.h"
#include "raymath.h"

static Mesh     cube;       /* unit cube, centered at the origin, white atlas tile */
static Material material;   /* atlas texture; tint is set per part */
static const CharPose *pose; /* pose being drawn (for tint effects) */

void Character_Init(void)
{
    MeshBuilder mb;
    MB_Begin(&mb);
    MB_Box(&mb, (Vector3){ -0.5f, -0.5f, -0.5f }, (Vector3){ 0.5f, 0.5f, 0.5f }, TILE_WHITE, WHITE);
    cube = MB_End(&mb);
    material = LoadMaterialDefault();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Atlas();
}

void Character_Shutdown(void)
{
    UnloadMesh(cube);
    material.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* the atlas is owned by textures.c */
    material.shader = (Shader){ 0 };                                  /* the shader is owned by render.c */
    UnloadMaterial(material);
}

void Character_SetShader(Shader shader)
{
    material.shader = shader;
}

/* ------------------------------------------------------------ building blocks */

static Color Mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), a.a };
}

/* Draw a box of `size` centered at `center` inside `frame`. */
static void Part(Matrix frame, Vector3 center, Vector3 size, Color col)
{
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(size.x, size.y, size.z),
                                             MatrixTranslate(center.x, center.y, center.z)), frame);
    if (pose->windup > 0.0f) col = Mix(col, (Color){ 255, 40, 30, 255 }, 0.65f * pose->windup);
    if (pose->flash > 0.0f)  col = Mix(col, WHITE, pose->flash);
    if (pose->alpha > 0.0f)  col.a = (unsigned char)(255 * pose->alpha);
    material.maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawMesh(cube, material, m);
}

/* A part that glows (eyes, orbs): no lighting, only fog. */
static void GlowPart(Matrix frame, Vector3 center, Vector3 size, Color col)
{
    Render_SetEmissive(true);
    Part(frame, center, size, col);
    Render_SetEmissive(false);
}

/* Child frame: rotate around X (pitch, e.g. leg swing), then Y, then Z, about a joint at `at`. */
static Matrix Joint(Matrix parent, Vector3 at, float rx, float ry, float rz)
{
    Matrix r = MatrixMultiply(MatrixMultiply(MatrixRotateX(rx), MatrixRotateY(ry)), MatrixRotateZ(rz));
    return MatrixMultiply(MatrixMultiply(r, MatrixTranslate(at.x, at.y, at.z)), parent);
}

static Matrix RootFrame(const CharPose *p, float scale)
{
    return MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(p->yaw)),
                          MatrixTranslate(p->pos.x, p->pos.y, p->pos.z));
}

/* ------------------------------------------------------------ knight (player) */

void Character_DrawKnight(const CharPose *p)
{
    const Color steelDark = { 66, 70, 82, 255 }, steel = { 104, 108, 122, 255 };
    const Color steelLight = { 152, 158, 172, 255 }, crimson = { 138, 18, 28, 255 };
    const Color leather = { 58, 38, 24, 255 }, gold = { 196, 156, 70, 255 };
    const Color visor = { 8, 8, 12, 255 }, blade = { 200, 206, 218, 255 };
    float swingLeg = sinf(p->walkPhase) * 0.6f * p->walkAmount;
    float bob = fabsf(cosf(p->walkPhase)) * 0.05f * p->walkAmount + sinf(p->time * 2.2f) * 0.012f;
    float armRx = -0.35f, armRy = 0.0f;
    Matrix root, body, leg, arm, hand, head;
    int side;

    if (p->blink > 0.5f) return;
    pose = p;
    root = RootFrame(p, 1.0f);
    body = MatrixMultiply(MatrixTranslate(0.0f, bob, 0.0f), root);

    /* legs + boots (hip joints) */
    for (side = -1; side <= 1; side += 2) {
        leg = Joint(root, (Vector3){ 0.13f * side, 0.78f, 0.0f }, swingLeg * side, 0.0f, 0.0f);
        Part(leg, (Vector3){ 0.0f, -0.34f, 0.0f }, (Vector3){ 0.24f, 0.68f, 0.27f }, steelDark);
        Part(leg, (Vector3){ 0.0f, -0.70f, 0.03f }, (Vector3){ 0.27f, 0.16f, 0.33f }, (Color){ 40, 30, 22, 255 });
    }

    /* torso, tabard stripe, belt */
    Part(body, (Vector3){ 0.0f, 1.10f, 0.0f }, (Vector3){ 0.56f, 0.66f, 0.32f }, steelDark);
    Part(body, (Vector3){ 0.0f, 1.06f, 0.0f }, (Vector3){ 0.22f, 0.60f, 0.345f }, crimson);
    Part(body, (Vector3){ 0.0f, 0.82f, 0.0f }, (Vector3){ 0.58f, 0.09f, 0.35f }, leather);
    Part(body, (Vector3){ 0.0f, 0.82f, 0.18f }, (Vector3){ 0.09f, 0.08f, 0.02f }, gold);

    /* pauldrons */
    Part(body, (Vector3){  0.37f, 1.39f, 0.0f }, (Vector3){ 0.30f, 0.15f, 0.35f }, steelLight);
    Part(body, (Vector3){ -0.37f, 1.39f, 0.0f }, (Vector3){ 0.30f, 0.15f, 0.35f }, steelLight);

    /* left arm swings opposite to the left leg */
    arm = Joint(body, (Vector3){ 0.38f, 1.36f, 0.0f }, -swingLeg * 0.8f, 0.0f, 0.0f);
    Part(arm, (Vector3){ 0.0f, -0.28f, 0.0f }, (Vector3){ 0.19f, 0.58f, 0.21f }, steel);
    Part(arm, (Vector3){ 0.0f, -0.56f, 0.0f }, (Vector3){ 0.22f, 0.13f, 0.24f }, steelDark);

    /* right arm holds the sword; during a swing it sweeps a 120 degree arc in front */
    if (p->swing >= 0.0f) {
        float s = p->swing;
        armRx = -1.45f;
        armRy = 1.05f - 2.1f * (s * s * (3.0f - 2.0f * s));    /* smoothstep from left to right */
    } else {
        armRx += swingLeg * 0.4f;
    }
    arm = Joint(body, (Vector3){ -0.38f, 1.36f, 0.0f }, armRx, armRy, 0.0f);
    Part(arm, (Vector3){ 0.0f, -0.28f, 0.0f }, (Vector3){ 0.19f, 0.58f, 0.21f }, steel);
    Part(arm, (Vector3){ 0.0f, -0.56f, 0.0f }, (Vector3){ 0.22f, 0.13f, 0.24f }, steelDark);
    /* sword, held at the wrist: points forward at rest, in line with the arm while swinging */
    hand = Joint(arm, (Vector3){ 0.0f, -0.58f, 0.0f }, p->swing >= 0.0f ? 1.3f : 0.0f, 0.0f, 0.0f);
    Part(hand, (Vector3){ 0.0f, 0.0f, -0.03f }, (Vector3){ 0.05f, 0.05f, 0.18f }, leather);   /* grip */
    Part(hand, (Vector3){ 0.0f, 0.0f, -0.14f }, (Vector3){ 0.08f, 0.08f, 0.06f }, gold);      /* pommel */
    Part(hand, (Vector3){ 0.0f, 0.0f, 0.09f }, (Vector3){ 0.32f, 0.05f, 0.06f }, gold);       /* crossguard */
    Part(hand, (Vector3){ 0.0f, 0.0f, 0.58f }, (Vector3){ 0.07f, 0.03f, 0.92f }, blade);      /* blade */

    /* helmet with a dark visor slit (cross shape) and a crimson crest */
    head = Joint(body, (Vector3){ 0.0f, 1.42f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.20f, 0.0f }, (Vector3){ 0.42f, 0.40f, 0.42f }, steelLight);
    Part(head, (Vector3){ 0.0f, 0.23f, 0.212f }, (Vector3){ 0.32f, 0.05f, 0.02f }, visor);
    Part(head, (Vector3){ 0.0f, 0.13f, 0.212f }, (Vector3){ 0.05f, 0.13f, 0.02f }, visor);
    Part(head, (Vector3){ 0.0f, 0.44f, -0.02f }, (Vector3){ 0.07f, 0.10f, 0.40f }, crimson);
}

/* ------------------------------------------------------------ skeleton */

void Character_DrawSkeleton(const CharPose *p)
{
    const Color bone = { 222, 214, 192, 255 }, boneDark = { 170, 160, 136, 255 };
    const Color socket = { 14, 6, 6, 255 }, glow = { 255, 60, 40, 255 };
    float swingLeg = sinf(p->walkPhase) * 0.55f * p->walkAmount;
    float bob = fabsf(cosf(p->walkPhase)) * 0.04f * p->walkAmount + sinf(p->time * 2.0f) * 0.01f;
    Matrix root, body, limb, head;
    int side;

    pose = p;
    root = RootFrame(p, 1.0f);
    /* wind-up: lean back, ready to strike */
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.25f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    for (side = -1; side <= 1; side += 2) {
        limb = Joint(root, (Vector3){ 0.11f * side, 0.80f, 0.0f }, swingLeg * side, 0.0f, 0.0f);
        Part(limb, (Vector3){ 0.0f, -0.40f, 0.0f }, (Vector3){ 0.11f, 0.78f, 0.12f }, bone);
        Part(limb, (Vector3){ 0.0f, -0.77f, 0.05f }, (Vector3){ 0.14f, 0.06f, 0.22f }, boneDark);   /* foot */
    }
    Part(body, (Vector3){ 0.0f, 0.82f, 0.0f }, (Vector3){ 0.34f, 0.10f, 0.16f }, boneDark);        /* pelvis */
    Part(body, (Vector3){ 0.0f, 1.08f, -0.04f }, (Vector3){ 0.08f, 0.52f, 0.08f }, boneDark);      /* spine */
    Part(body, (Vector3){ 0.0f, 1.02f, 0.0f }, (Vector3){ 0.34f, 0.05f, 0.22f }, bone);            /* ribs */
    Part(body, (Vector3){ 0.0f, 1.13f, 0.0f }, (Vector3){ 0.40f, 0.05f, 0.24f }, bone);
    Part(body, (Vector3){ 0.0f, 1.24f, 0.0f }, (Vector3){ 0.42f, 0.05f, 0.24f }, bone);
    Part(body, (Vector3){ 0.0f, 1.37f, 0.0f }, (Vector3){ 0.52f, 0.07f, 0.14f }, bone);            /* collarbone */

    /* arms stretched forward, swaying; raised higher during the wind-up */
    for (side = -1; side <= 1; side += 2) {
        float reach = -1.05f - 0.9f * p->windup + sinf(p->time * 3.0f + side) * 0.08f;
        limb = Joint(body, (Vector3){ 0.28f * side, 1.36f, 0.0f }, reach + swingLeg * 0.2f * side, 0.0f, 0.0f);
        Part(limb, (Vector3){ 0.0f, -0.30f, 0.0f }, (Vector3){ 0.08f, 0.60f, 0.08f }, bone);
        Part(limb, (Vector3){ 0.0f, -0.63f, 0.0f }, (Vector3){ 0.12f, 0.08f, 0.12f }, boneDark);   /* hand */
    }

    head = Joint(body, (Vector3){ 0.0f, 1.42f, 0.0f }, 0.0f, sinf(p->time * 1.3f) * 0.15f, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.20f, 0.0f }, (Vector3){ 0.34f, 0.32f, 0.34f }, bone);
    Part(head, (Vector3){ 0.0f, 0.03f, 0.02f }, (Vector3){ 0.24f, 0.08f, 0.28f }, boneDark);       /* jaw */
    if (p->alerted) {
        GlowPart(head, (Vector3){ -0.08f, 0.22f, 0.165f }, (Vector3){ 0.09f, 0.08f, 0.02f }, glow);
        GlowPart(head, (Vector3){  0.08f, 0.22f, 0.165f }, (Vector3){ 0.09f, 0.08f, 0.02f }, glow);
    } else {
        Part(head, (Vector3){ -0.08f, 0.22f, 0.165f }, (Vector3){ 0.09f, 0.08f, 0.02f }, socket);
        Part(head, (Vector3){  0.08f, 0.22f, 0.165f }, (Vector3){ 0.09f, 0.08f, 0.02f }, socket);
    }
    Part(head, (Vector3){ 0.0f, 0.12f, 0.17f }, (Vector3){ 0.05f, 0.06f, 0.02f }, socket);          /* nose hole */
}

/* ------------------------------------------------------------ ghost */

void Character_DrawGhost(const CharPose *p)
{
    const Color pale = { 196, 218, 255, 255 }, hollow = { 6, 8, 18, 255 };
    float sway = sinf(p->time * 1.7f) * 0.06f;
    Matrix root, body, arm, head;
    int side;

    pose = p;
    root = RootFrame(p, 1.0f);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateZ(sway), MatrixRotateX(-0.3f * p->windup)), root);

    /* no legs: a body that tapers toward the bottom, trailing behind slightly */
    Part(body, (Vector3){ 0.0f, 0.12f, -0.14f }, (Vector3){ 0.22f, 0.24f, 0.18f }, pale);
    Part(body, (Vector3){ 0.0f, 0.38f, -0.08f }, (Vector3){ 0.34f, 0.30f, 0.26f }, pale);
    Part(body, (Vector3){ 0.0f, 0.70f, -0.03f }, (Vector3){ 0.46f, 0.36f, 0.32f }, pale);
    Part(body, (Vector3){ 0.0f, 1.06f, 0.0f }, (Vector3){ 0.56f, 0.40f, 0.36f }, pale);

    for (side = -1; side <= 1; side += 2) {
        float wave = sinf(p->time * 2.4f + side * 1.2f) * 0.18f;
        arm = Joint(body, (Vector3){ 0.32f * side, 1.20f, 0.0f }, -1.35f + wave - 0.4f * p->windup, 0.0f, 0.1f * side);
        Part(arm, (Vector3){ 0.0f, -0.28f, 0.0f }, (Vector3){ 0.12f, 0.56f, 0.12f }, pale);
    }

    head = Joint(body, (Vector3){ 0.0f, 1.28f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.22f, 0.0f }, (Vector3){ 0.40f, 0.42f, 0.40f }, pale);
    Part(head, (Vector3){ -0.09f, 0.27f, 0.205f }, (Vector3){ 0.10f, 0.12f, 0.02f }, hollow);
    Part(head, (Vector3){  0.09f, 0.27f, 0.205f }, (Vector3){ 0.10f, 0.12f, 0.02f }, hollow);
    Part(head, (Vector3){ 0.0f, 0.09f, 0.205f }, (Vector3){ 0.12f, 0.14f, 0.02f }, hollow);          /* wailing mouth */
}

/* ------------------------------------------------------------ witch + queen */

static void DrawWitchBody(const CharPose *p, float scale, Color robe, Color trim, bool queen)
{
    const Color face = { 122, 170, 104, 255 }, hat = { 30, 18, 40, 255 };
    const Color eye = { 10, 10, 10, 255 }, orb = { 190, 90, 255, 255 }, gold = { 222, 176, 64, 255 };
    const Color eyeGlow = { 210, 255, 90, 255 };
    float bob = sinf(p->time * 2.0f) * 0.015f + fabsf(cosf(p->walkPhase)) * 0.03f * p->walkAmount;
    Matrix root, body, arm, head;
    int side, k;

    pose = p;
    root = RootFrame(p, scale);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.22f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    /* robe: a wide box down to the floor, narrower at the top */
    Part(body, (Vector3){ 0.0f, 0.32f, 0.0f }, (Vector3){ 0.70f, 0.64f, 0.56f }, robe);
    Part(body, (Vector3){ 0.0f, 0.88f, 0.0f }, (Vector3){ 0.54f, 0.52f, 0.40f }, robe);
    Part(body, (Vector3){ 0.0f, 0.62f, 0.0f }, (Vector3){ 0.18f, 1.22f, 0.575f }, trim);                /* front panel */
    Part(body, (Vector3){ 0.0f, 0.70f, 0.0f }, (Vector3){ 0.58f, 0.06f, 0.44f }, trim);                 /* sash */

    /* left arm hangs, right arm holds the glowing orb forward */
    for (side = -1; side <= 1; side += 2) {
        float rx = side > 0 ? 0.15f + sinf(p->walkPhase) * 0.3f * p->walkAmount : -1.0f - 0.6f * p->windup;
        arm = Joint(body, (Vector3){ 0.33f * side, 1.10f, 0.0f }, rx, 0.0f, side > 0 ? 0.12f : 0.0f);
        Part(arm, (Vector3){ 0.0f, -0.26f, 0.0f }, (Vector3){ 0.16f, 0.52f, 0.18f }, robe);
        Part(arm, (Vector3){ 0.0f, -0.54f, 0.0f }, (Vector3){ 0.10f, 0.08f, 0.10f }, face);
        if (side < 0 && !queen) GlowPart(arm, (Vector3){ 0.0f, -0.68f, 0.0f }, (Vector3){ 0.18f, 0.18f, 0.18f }, orb);
    }

    head = Joint(body, (Vector3){ 0.0f, 1.14f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.18f, 0.0f }, (Vector3){ 0.34f, 0.34f, 0.34f }, face);
    Part(head, (Vector3){ 0.0f, 0.14f, 0.20f }, (Vector3){ 0.06f, 0.12f, 0.08f }, face);                /* long nose */
    for (side = -1; side <= 1; side += 2) {
        if (p->alerted) GlowPart(head, (Vector3){ 0.08f * side, 0.22f, 0.172f }, (Vector3){ 0.07f, 0.05f, 0.02f }, eyeGlow);
        else            Part(head, (Vector3){ 0.08f * side, 0.22f, 0.172f }, (Vector3){ 0.07f, 0.05f, 0.02f }, eye);
    }
    Part(head, (Vector3){ 0.0f, 0.18f, -0.12f }, (Vector3){ 0.38f, 0.36f, 0.14f }, hat);              /* hair */

    if (!queen) {
        /* tall pointed hat: brim + stacked shrinking boxes, tip bent back */
        Part(head, (Vector3){ 0.0f, 0.37f, 0.0f }, (Vector3){ 0.66f, 0.05f, 0.66f }, hat);
        Part(head, (Vector3){ 0.0f, 0.49f, 0.0f }, (Vector3){ 0.40f, 0.20f, 0.40f }, hat);
        Part(head, (Vector3){ 0.0f, 0.67f, -0.03f }, (Vector3){ 0.29f, 0.18f, 0.29f }, hat);
        Part(head, (Vector3){ 0.0f, 0.83f, -0.07f }, (Vector3){ 0.19f, 0.16f, 0.19f }, hat);
        Part(head, (Vector3){ 0.0f, 0.95f, -0.13f }, (Vector3){ 0.10f, 0.12f, 0.10f }, hat);
        Part(head, (Vector3){ 0.0f, 0.415f, 0.0f }, (Vector3){ 0.41f, 0.05f, 0.41f }, (Color){ 110, 40, 130, 255 }); /* band */
    } else {
        /* crown of small gold boxes with a red jewel */
        Part(head, (Vector3){ 0.0f, 0.39f, 0.0f }, (Vector3){ 0.40f, 0.08f, 0.40f }, gold);
        for (k = 0; k < 8; k++) {
            float a = k * PI / 4.0f;
            Part(head, (Vector3){ sinf(a) * 0.17f, 0.48f + (k % 2) * 0.04f, cosf(a) * 0.17f },
                 (Vector3){ 0.06f, 0.10f + (k % 2) * 0.08f, 0.06f }, gold);
        }
        GlowPart(head, (Vector3){ 0.0f, 0.40f, 0.205f }, (Vector3){ 0.07f, 0.06f, 0.02f }, (Color){ 255, 40, 50, 255 });
        /* two orbs circling her */
        for (k = 0; k < 2; k++) {
            float a = p->time * 2.2f + k * PI;
            GlowPart(root, (Vector3){ sinf(a) * 0.75f, 1.05f + sinf(p->time * 3.0f + k) * 0.12f, cosf(a) * 0.75f },
                     (Vector3){ 0.16f, 0.16f, 0.16f }, orb);
        }
    }
}

void Character_DrawWitch(const CharPose *p)
{
    DrawWitchBody(p, 1.0f, (Color){ 62, 26, 76, 255 }, (Color){ 40, 16, 50, 255 }, false);
}

void Character_DrawQueen(const CharPose *p)
{
    DrawWitchBody(p, 1.8f, (Color){ 22, 16, 24, 255 }, (Color){ 140, 16, 30, 255 }, true);
}

/* ------------------------------------------------------------ hex bolt */

void Character_DrawBolt(Vector3 pos, float spin)
{
    static const CharPose none = { 0 };
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateX(spin), MatrixRotateY(spin * 1.3f)),
                              MatrixTranslate(pos.x, pos.y, pos.z));
    pose = &none;
    GlowPart(m, (Vector3){ 0 }, (Vector3){ 0.26f, 0.26f, 0.26f }, (Color){ 170, 70, 255, 255 });
    GlowPart(m, (Vector3){ 0 }, (Vector3){ 0.14f, 0.34f, 0.14f }, (Color){ 240, 200, 255, 255 });
}
