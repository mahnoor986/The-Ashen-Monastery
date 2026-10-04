/* character.c - blocky humanoid models (see character.h).
 * Models face +Z in their own space, feet at y = 0; the character's right side is -X.
 * A "frame" is a matrix that places a body part: Part() draws a box inside a frame, Joint()
 * makes a child frame rotated around a joint (shoulder, hip, neck). */
#include <math.h>
#include "character.h"
#include "meshgen.h"
#include "textures.h"
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
    material.maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawMesh(cube, material, m);
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
