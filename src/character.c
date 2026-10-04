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
static Mesh     emberCube;  /* unit cube with the charred ember-crack texture (Ashen Monks) */
static Material material;   /* atlas texture; tint is set per part */
static const CharPose *pose; /* pose being drawn (for tint effects) */

void Character_Init(void)
{
    MeshBuilder mb;
    MB_Begin(&mb);
    MB_Box(&mb, (Vector3){ -0.5f, -0.5f, -0.5f }, (Vector3){ 0.5f, 0.5f, 0.5f }, TILE_WHITE, WHITE);
    cube = MB_End(&mb);
    MB_Begin(&mb);
    MB_Box(&mb, (Vector3){ -0.5f, -0.5f, -0.5f }, (Vector3){ 0.5f, 0.5f, 0.5f }, TILE_EMBER, WHITE);
    emberCube = MB_End(&mb);
    material = LoadMaterialDefault();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Atlas();
}

void Character_Shutdown(void)
{
    UnloadMesh(cube);
    UnloadMesh(emberCube);
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

/* Draw a box of `size` centered at `center` inside `frame`, using mesh `m`. */
static void PartMesh(Mesh m3, Matrix frame, Vector3 center, Vector3 size, Color col)
{
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(size.x, size.y, size.z),
                                             MatrixTranslate(center.x, center.y, center.z)), frame);
    if (pose->windup > 0.0f) col = Mix(col, (Color){ 255, 40, 30, 255 }, 0.65f * pose->windup);
    if (pose->flash > 0.0f)  col = Mix(col, WHITE, pose->flash);
    if (pose->alpha > 0.0f)  col.a = (unsigned char)(255 * pose->alpha);
    material.maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawMesh(m3, material, m);
}

static void Part(Matrix frame, Vector3 center, Vector3 size, Color col)
{
    PartMesh(cube, frame, center, size, col);
}

/* Charred cloth with glowing ember cracks. */
static void EmberPart(Matrix frame, Vector3 center, Vector3 size, Color col)
{
    PartMesh(emberCube, frame, center, size, col);
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
    const Color visor = { 8, 8, 12, 255 }, wood = { 46, 28, 18, 255 };
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

    /* right arm holds the wand; casting snaps it straight forward, pointing at the target */
    if (p->swing >= 0.0f) {
        armRx = -1.55f;
        armRy = 0.0f;
    } else {
        armRx += swingLeg * 0.4f;
    }
    arm = Joint(body, (Vector3){ -0.38f, 1.36f, 0.0f }, armRx, armRy, 0.0f);
    Part(arm, (Vector3){ 0.0f, -0.28f, 0.0f }, (Vector3){ 0.19f, 0.58f, 0.21f }, steel);
    Part(arm, (Vector3){ 0.0f, -0.56f, 0.0f }, (Vector3){ 0.22f, 0.13f, 0.24f }, steelDark);
    /* wand, held at the wrist: forward-down at rest, in line with the arm while casting */
    hand = Joint(arm, (Vector3){ 0.0f, -0.58f, 0.0f }, p->swing >= 0.0f ? 1.57f : 0.6f, 0.0f, 0.0f);
    Part(hand, (Vector3){ 0.0f, 0.0f, 0.15f }, (Vector3){ 0.045f, 0.045f, 0.36f }, wood);     /* dark wood */
    Part(hand, (Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 0.06f, 0.06f, 0.08f }, gold);        /* grip band */
    GlowPart(hand, (Vector3){ 0.0f, 0.0f, 0.35f }, (Vector3){ 0.08f, 0.08f, 0.08f }, (Color){ 255, 40, 30, 255 });

    /* helmet with a dark visor slit (cross shape) and a crimson crest */
    head = Joint(body, (Vector3){ 0.0f, 1.42f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.20f, 0.0f }, (Vector3){ 0.42f, 0.40f, 0.42f }, steelLight);
    Part(head, (Vector3){ 0.0f, 0.23f, 0.212f }, (Vector3){ 0.32f, 0.05f, 0.02f }, visor);
    Part(head, (Vector3){ 0.0f, 0.13f, 0.212f }, (Vector3){ 0.05f, 0.13f, 0.02f }, visor);
    Part(head, (Vector3){ 0.0f, 0.44f, -0.02f }, (Vector3){ 0.07f, 0.10f, 0.40f }, crimson);
}

/* ------------------------------------------------------------ enemy helpers */

/* Enemies are ~15% taller and ~15% thinner than a normal person. */
static Matrix EnemyRoot(const CharPose *p, float scale)
{
    return MatrixMultiply(MatrixMultiply(MatrixScale(scale * 0.85f, scale * 1.15f, scale * 0.85f), MatrixRotateY(p->yaw)),
                          MatrixTranslate(p->pos.x, p->pos.y, p->pos.z));
}

/* Head joint with the random twitch applied. */
static Matrix TwitchHead(Matrix body, Vector3 at, const CharPose *p, float extraYaw)
{
    return Joint(body, at, 0.0f, extraYaw + p->headYaw, p->headRoll);
}

/* Glowing eyes: unlit and never darkened by fog, so they are the first thing seen in the dark. */
static void Eyes(Matrix head, float y, float z, float spread, Vector3 size, Color c)
{
    Render_SetEmissiveMode(2);
    Part(head, (Vector3){ -spread, y, z }, size, c);
    Part(head, (Vector3){  spread, y, z }, size, c);
    Render_SetEmissiveMode(0);
}

/* ------------------------------------------------------------ Ashen Monk */

void Character_DrawMonk(const CharPose *p)
{
    const Color robeDark = { 104, 96, 92, 255 }, hood = { 72, 64, 62, 255 }, shadow = { 4, 2, 2, 255 };
    const Color skin = { 58, 52, 48, 255 }, rope = { 96, 42, 18, 255 }, eye = { 255, 30, 20, 255 };
    float swingLeg = sinf(p->walkPhase) * 0.5f * p->walkAmount;
    float bob = fabsf(cosf(p->walkPhase)) * 0.04f * p->walkAmount + sinf(p->time * 2.0f) * 0.01f;
    Matrix root, body, limb, head;
    int side;

    pose = p;
    root = EnemyRoot(p, 1.0f);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.25f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    /* feet peek out under the robe */
    for (side = -1; side <= 1; side += 2) {
        limb = Joint(root, (Vector3){ 0.1f * side, 0.5f, 0.0f }, swingLeg * side, 0.0f, 0.0f);
        Part(limb, (Vector3){ 0.0f, -0.42f, 0.04f }, (Vector3){ 0.13f, 0.16f, 0.24f }, skin);
    }
    /* charred robe with ember cracks, rope belt */
    EmberPart(body, (Vector3){ 0.0f, 0.5f, 0.0f }, (Vector3){ 0.52f, 0.86f, 0.40f }, robeDark);
    EmberPart(body, (Vector3){ 0.0f, 1.14f, 0.0f }, (Vector3){ 0.46f, 0.50f, 0.32f }, robeDark);
    Part(body, (Vector3){ 0.0f, 0.88f, 0.0f }, (Vector3){ 0.50f, 0.06f, 0.36f }, rope);

    /* long arms hanging low, swaying; raised during the wind-up */
    for (side = -1; side <= 1; side += 2) {
        float rx = -0.2f - 1.2f * p->windup + sinf(p->time * 2.3f + side) * 0.06f - swingLeg * 0.3f * side;
        limb = Joint(body, (Vector3){ 0.29f * side, 1.36f, 0.0f }, rx, 0.0f, 0.06f * side);
        EmberPart(limb, (Vector3){ 0.0f, -0.36f, 0.0f }, (Vector3){ 0.16f, 0.72f, 0.18f }, robeDark);
        Part(limb, (Vector3){ 0.0f, -0.78f, 0.0f }, (Vector3){ 0.10f, 0.14f, 0.10f }, skin);       /* claw-like hand */
    }

    /* hood with a dark, faceless opening and burning red eyes */
    head = TwitchHead(body, (Vector3){ 0.0f, 1.40f, 0.0f }, p, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.22f, -0.02f }, (Vector3){ 0.42f, 0.44f, 0.42f }, hood);
    Part(head, (Vector3){ 0.0f, 0.46f, -0.08f }, (Vector3){ 0.22f, 0.12f, 0.24f }, hood);           /* hood peak */
    Part(head, (Vector3){ 0.0f, 0.18f, 0.195f }, (Vector3){ 0.28f, 0.28f, 0.02f }, shadow);         /* face in shadow */
    Eyes(head, 0.22f, 0.21f, 0.07f, (Vector3){ 0.07f, 0.04f, 0.02f }, eye);
}

/* ------------------------------------------------------------ Choir Wraith */

void Character_DrawWraith(const CharPose *p)
{
    const Color pale = { 200, 198, 204, 255 }, hood = { 160, 158, 166, 255 }, hollow = { 4, 4, 6, 255 };
    const Color eye = { 255, 40, 30, 255 };
    float sway = sinf(p->time * 1.7f) * 0.06f;
    Matrix root, body, arm, head;
    int side;

    pose = p;
    root = EnemyRoot(p, 1.0f);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateZ(sway), MatrixRotateX(-0.3f * p->windup)), root);

    /* no legs: a tattered body tapering to a wisp */
    Part(body, (Vector3){ 0.0f, 0.10f, -0.16f }, (Vector3){ 0.16f, 0.22f, 0.14f }, pale);
    Part(body, (Vector3){ 0.0f, 0.34f, -0.10f }, (Vector3){ 0.28f, 0.28f, 0.22f }, pale);
    Part(body, (Vector3){ 0.0f, 0.66f, -0.04f }, (Vector3){ 0.40f, 0.36f, 0.30f }, pale);
    Part(body, (Vector3){ 0.0f, 1.04f, 0.0f }, (Vector3){ 0.50f, 0.42f, 0.34f }, pale);

    for (side = -1; side <= 1; side += 2) {
        float wave = sinf(p->time * 2.4f + side * 1.2f) * 0.18f;
        arm = Joint(body, (Vector3){ 0.28f * side, 1.20f, 0.0f }, -1.35f + wave - 0.4f * p->windup, 0.0f, 0.1f * side);
        Part(arm, (Vector3){ 0.0f, -0.30f, 0.0f }, (Vector3){ 0.10f, 0.60f, 0.10f }, pale);
    }

    /* hooded head, singing: the mouth hangs wide open */
    head = TwitchHead(body, (Vector3){ 0.0f, 1.26f, 0.0f }, p, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.22f, 0.0f }, (Vector3){ 0.36f, 0.42f, 0.36f }, pale);
    Part(head, (Vector3){ 0.0f, 0.26f, -0.05f }, (Vector3){ 0.44f, 0.50f, 0.40f }, hood);           /* hood */
    Part(head, (Vector3){ 0.0f, 0.06f, 0.185f }, (Vector3){ 0.12f, 0.20f, 0.02f }, hollow);          /* open mouth */
    Part(head, (Vector3){ 0.0f, 0.27f, 0.183f }, (Vector3){ 0.22f, 0.10f, 0.02f }, hollow);          /* eye hollows */
    Eyes(head, 0.27f, 0.195f, 0.06f, (Vector3){ 0.05f, 0.04f, 0.02f }, eye);
}

/* ------------------------------------------------------------ Ember Priest + Red Abbot */

static void DrawRobed(const CharPose *p, float scale, Color robe, Color trim, bool abbot)
{
    const Color shadow = { 4, 2, 2, 255 }, iron = { 62, 58, 62, 255 }, ironDark = { 34, 32, 36, 255 };
    const Color fire = { 255, 120, 30, 255 }, fireCore = { 255, 220, 120, 255 };
    const Color eye = abbot ? (Color){ 255, 170, 40, 255 } : (Color){ 255, 40, 30, 255 };
    float bob = sinf(p->time * 2.0f) * 0.015f + fabsf(cosf(p->walkPhase)) * 0.03f * p->walkAmount;
    Matrix root, body, arm, head;
    int side, k;

    pose = p;
    root = EnemyRoot(p, scale);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.22f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    /* robe to the floor, narrower at the top, with a front panel */
    Part(body, (Vector3){ 0.0f, 0.34f, 0.0f }, (Vector3){ 0.66f, 0.68f, 0.52f }, robe);
    Part(body, (Vector3){ 0.0f, 0.92f, 0.0f }, (Vector3){ 0.50f, 0.52f, 0.38f }, robe);
    Part(body, (Vector3){ 0.0f, 0.64f, 0.0f }, (Vector3){ 0.18f, 1.26f, 0.535f }, trim);
    Part(body, (Vector3){ 0.0f, 1.16f, 0.0f }, (Vector3){ 0.60f, 0.10f, 0.42f }, trim);           /* stole */

    for (side = -1; side <= 1; side += 2) {
        bool censerHand = side < 0 && !abbot;
        float rx = censerHand ? -0.7f - 0.6f * p->windup : (side > 0 ? 0.1f + sinf(p->walkPhase) * 0.3f * p->walkAmount
                                                                     : -1.0f - 0.6f * p->windup);
        arm = Joint(body, (Vector3){ 0.31f * side, 1.12f, 0.0f }, rx, 0.0f, side > 0 ? 0.1f : 0.0f);
        Part(arm, (Vector3){ 0.0f, -0.27f, 0.0f }, (Vector3){ 0.16f, 0.54f, 0.18f }, robe);
        Part(arm, (Vector3){ 0.0f, -0.56f, 0.0f }, (Vector3){ 0.09f, 0.08f, 0.09f }, (Color){ 60, 46, 40, 255 });
        if (censerHand) {
            /* a burning censer swinging on its chain */
            Matrix chain = Joint(arm, (Vector3){ 0.0f, -0.58f, 0.0f }, 0.7f + 0.6f * p->windup + sinf(p->time * 3.0f) * 0.3f, 0.0f, 0.0f);
            Part(chain, (Vector3){ 0.0f, -0.18f, 0.0f }, (Vector3){ 0.02f, 0.36f, 0.02f }, ironDark);
            Part(chain, (Vector3){ 0.0f, -0.42f, 0.0f }, (Vector3){ 0.16f, 0.14f, 0.16f }, iron);
            GlowPart(chain, (Vector3){ 0.0f, -0.32f, 0.0f }, (Vector3){ 0.10f, 0.08f, 0.10f }, fire);
        }
    }

    head = TwitchHead(body, (Vector3){ 0.0f, 1.18f, 0.0f }, p, 0.0f);
    Part(head, (Vector3){ 0.0f, 0.18f, 0.02f }, (Vector3){ 0.32f, 0.32f, 0.30f }, shadow);           /* face lost in shadow */
    if (!abbot) {
        /* tall pointed hood */
        Part(head, (Vector3){ 0.0f, 0.20f, -0.03f }, (Vector3){ 0.40f, 0.42f, 0.38f }, trim);
        Part(head, (Vector3){ 0.0f, 0.46f, -0.06f }, (Vector3){ 0.30f, 0.20f, 0.30f }, trim);
        Part(head, (Vector3){ 0.0f, 0.62f, -0.10f }, (Vector3){ 0.18f, 0.18f, 0.18f }, trim);
        Part(head, (Vector3){ 0.0f, 0.75f, -0.14f }, (Vector3){ 0.08f, 0.12f, 0.08f }, trim);
        Part(head, (Vector3){ 0.0f, 0.17f, 0.172f }, (Vector3){ 0.24f, 0.26f, 0.02f }, shadow);
    } else {
        /* tall iron mitre shaped like a bell: knob, crown, flaring lip */
        Part(head, (Vector3){ 0.0f, 0.36f, 0.0f }, (Vector3){ 0.46f, 0.06f, 0.46f }, ironDark);     /* lip */
        Part(head, (Vector3){ 0.0f, 0.46f, 0.0f }, (Vector3){ 0.38f, 0.14f, 0.38f }, iron);
        Part(head, (Vector3){ 0.0f, 0.60f, 0.0f }, (Vector3){ 0.30f, 0.16f, 0.30f }, iron);
        Part(head, (Vector3){ 0.0f, 0.72f, 0.0f }, (Vector3){ 0.22f, 0.10f, 0.22f }, iron);
        Part(head, (Vector3){ 0.0f, 0.80f, 0.0f }, (Vector3){ 0.08f, 0.08f, 0.08f }, ironDark);     /* knob */
        Part(head, (Vector3){ 0.0f, 0.48f, 0.195f }, (Vector3){ 0.06f, 0.16f, 0.02f }, (Color){ 140, 16, 24, 255 });
        Part(head, (Vector3){ 0.0f, 0.18f, -0.12f }, (Vector3){ 0.36f, 0.34f, 0.14f }, trim);       /* cowl */
    }
    Eyes(head, 0.22f, abbot ? 0.18f : 0.185f, 0.07f, (Vector3){ 0.07f, 0.04f, 0.02f }, eye);

    if (abbot) {
        /* two fireballs circling him */
        for (k = 0; k < 2; k++) {
            float a = p->time * 2.2f + k * PI;
            Vector3 c = { sinf(a) * 0.75f, 1.05f + sinf(p->time * 3.0f + k) * 0.12f, cosf(a) * 0.75f };
            GlowPart(root, c, (Vector3){ 0.16f, 0.16f, 0.16f }, fire);
            GlowPart(root, c, (Vector3){ 0.09f, 0.20f, 0.09f }, fireCore);
        }
    }
}

void Character_DrawPriest(const CharPose *p)
{
    DrawRobed(p, 1.0f, (Color){ 120, 14, 24, 255 }, (Color){ 70, 8, 14, 255 }, false);
}

void Character_DrawAbbot(const CharPose *p)
{
    DrawRobed(p, 1.8f, (Color){ 150, 14, 24, 255 }, (Color){ 22, 16, 18, 255 }, true);
}

/* ------------------------------------------------------------ fireball */

void Character_DrawBolt(Vector3 pos, float spin)
{
    static const CharPose none = { 0 };
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateX(spin), MatrixRotateY(spin * 1.3f)),
                              MatrixTranslate(pos.x, pos.y, pos.z));
    pose = &none;
    GlowPart(m, (Vector3){ 0 }, (Vector3){ 0.28f, 0.28f, 0.28f }, (Color){ 255, 80, 20, 255 });
    GlowPart(m, (Vector3){ 0 }, (Vector3){ 0.16f, 0.36f, 0.16f }, (Color){ 255, 220, 120, 255 });
}
