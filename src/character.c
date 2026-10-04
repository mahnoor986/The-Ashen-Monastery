/* character.c - code-built characters and their animations (see character.h), plus model slots.
 * Models face +Z in their own space, feet at y = 0; the character's right side is -X.
 * Bodies are made of a few low-poly primitives - flared robes, cylinders, spheres, cones - each a
 * unit mesh drawn with its own transform and tint (PS1-style faceted shading). A "frame" is a
 * matrix that places a body part: Joint() makes a child frame rotated around a joint.
 * Model slots: if assets/models/<role>.glb exists it is drawn instead (see Model_Slot). */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "character.h"
#include "meshgen.h"
#include "textures.h"
#include "render.h"
#include "config.h"
#include "raymath.h"

enum { SH_BOX = 0, SH_CYL, SH_ROBE, SH_CONE, SH_SPHERE, SH_EMBER_ROBE, SH_EMBER_CYL, SH_COUNT };

static Mesh     shapes[SH_COUNT]; /* unit meshes, centred on the origin, 1 unit tall */
static Material material;         /* atlas texture; tint is set per part */
static const CharPose *pose;      /* pose being drawn (for tint effects) */

/* ============================================================ unit meshes */

/* A turned shape: ring k has radius r[k] at height y[k]; UVs stretched over one atlas tile. */
static Mesh Lathe(int tile, const float *r, const float *y, int rings, int sides, bool capTop, bool capBottom)
{
    MeshBuilder mb;
    float u0, v0, u1, v1;
    Color white[4] = { WHITE, WHITE, WHITE, WHITE };
    int k, i;
    Textures_TileUV(tile, &u0, &v0, &u1, &v1);
    MB_Begin(&mb);
    for (k = 0; k + 1 < rings; k++) {
        for (i = 0; i < sides; i++) {
            float a0 = 2.0f * PI * i / sides, a1 = 2.0f * PI * (i + 1) / sides;
            Vector3 p[4] = { { cosf(a1) * r[k], y[k], sinf(a1) * r[k] }, { cosf(a0) * r[k], y[k], sinf(a0) * r[k] },
                             { cosf(a0) * r[k + 1], y[k + 1], sinf(a0) * r[k + 1] }, { cosf(a1) * r[k + 1], y[k + 1], sinf(a1) * r[k + 1] } };
            float ua = u0 + (u1 - u0) * (float)(i + 1) / sides, ub = u0 + (u1 - u0) * (float)i / sides;
            float va = v1 - (v1 - v0) * (float)k / (rings - 1), vb = v1 - (v1 - v0) * (float)(k + 1) / (rings - 1);
            Vector2 uv[4] = { { ua, va }, { ub, va }, { ub, vb }, { ua, vb } };
            MB_Quad(&mb, p, uv, white);
        }
    }
    for (i = 0; i < sides && (capTop || capBottom); i++) {
        float a0 = 2.0f * PI * i / sides, a1 = 2.0f * PI * (i + 1) / sides;
        Vector2 uv[4] = { { u0, v0 }, { u0, v0 }, { u0, v0 }, { u0, v0 } };
        if (capTop && r[rings - 1] > 0.0f) {
            float rr = r[rings - 1], yy = y[rings - 1];
            Vector3 p[4] = { { cosf(a1) * rr, yy, sinf(a1) * rr }, { cosf(a0) * rr, yy, sinf(a0) * rr }, { 0, yy, 0 }, { 0, yy, 0 } };
            MB_Quad(&mb, p, uv, white);
        }
        if (capBottom && r[0] > 0.0f) {
            Vector3 p[4] = { { cosf(a0) * r[0], y[0], sinf(a0) * r[0] }, { cosf(a1) * r[0], y[0], sinf(a1) * r[0] }, { 0, y[0], 0 }, { 0, y[0], 0 } };
            MB_Quad(&mb, p, uv, white);
        }
    }
    return MB_End(&mb);
}

static void BuildShapes(void)
{
    const float cylR[2] = { 0.5f, 0.5f }, cylY[2] = { -0.5f, 0.5f };
    const float robeR[3] = { 0.5f, 0.38f, 0.25f }, robeY[3] = { -0.5f, 0.0f, 0.5f };   /* flares at the hem */
    const float coneR[2] = { 0.5f, 0.0f }, coneY[2] = { -0.5f, 0.5f };
    float sphR[7], sphY[7];
    MeshBuilder mb;
    int k;
    for (k = 0; k < 7; k++) { float a = PI * k / 6.0f; sphR[k] = 0.5f * sinf(a); sphY[k] = -0.5f * cosf(a); }
    MB_Begin(&mb);
    MB_Box(&mb, (Vector3){ -0.5f, -0.5f, -0.5f }, (Vector3){ 0.5f, 0.5f, 0.5f }, TILE_WHITE, WHITE);
    shapes[SH_BOX] = MB_End(&mb);
    shapes[SH_CYL] = Lathe(TILE_WHITE, cylR, cylY, 2, 8, true, true);
    shapes[SH_ROBE] = Lathe(TILE_WHITE, robeR, robeY, 3, 10, true, true);
    shapes[SH_CONE] = Lathe(TILE_WHITE, coneR, coneY, 2, 8, false, true);
    shapes[SH_SPHERE] = Lathe(TILE_WHITE, sphR, sphY, 7, 8, false, false);
    shapes[SH_EMBER_ROBE] = Lathe(TILE_EMBER, robeR, robeY, 3, 10, true, true);
    shapes[SH_EMBER_CYL] = Lathe(TILE_EMBER, cylR, cylY, 2, 8, true, true);
}

/* ============================================================ model slots */

typedef struct {
    bool            loaded;
    char            name[32];
    Model           model;
    ModelAnimation *anims;
    int             animCount;
    int             animFor[ANIM_COUNT];    /* animation index per CharAnim, -1 = none */
    float           scale, lift, yawOffset;
} ModelSlot;

static ModelSlot slots[MAX_MODEL_SLOTS];
static int       slotCount;
static Shader    worldShader;
static bool      hasWorldShader;

static bool NameHas(const char *name, const char *key)
{
    char low[32];
    int i;
    for (i = 0; i < 31 && name[i]; i++) low[i] = (char)((name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i]);
    low[i] = 0;
    return strstr(low, key) != NULL;
}

int Model_Slot(const char *role, float height, float yawOffset)
{
    const char *path = TextFormat("assets/models/%s.glb", role);
    ModelSlot *s;
    BoundingBox box;
    int i, a;
    for (i = 0; i < slotCount; i++) if (strcmp(slots[i].name, role) == 0) return slots[i].loaded ? i : -1;
    if (slotCount >= MAX_MODEL_SLOTS) return -1;
    s = &slots[slotCount++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, role, sizeof(s->name) - 1);
    if (!FileExists(path)) return -1;
    s->model = LoadModel(path);
    if (!IsModelValid(s->model)) { printf("warning: could not load %s (using the code-built look)\n", path); return -1; }
    box = GetModelBoundingBox(s->model);
    s->scale = (box.max.y - box.min.y) > 0.001f ? height / (box.max.y - box.min.y) : 1.0f;
    s->lift = -box.min.y * s->scale;                       /* feet on the floor */
    s->yawOffset = yawOffset;
    if (hasWorldShader) for (i = 0; i < s->model.materialCount; i++) s->model.materials[i].shader = worldShader;
    s->anims = LoadModelAnimations(path, &s->animCount);
    for (a = 0; a < ANIM_COUNT; a++) s->animFor[a] = -1;
    for (i = 0; i < s->animCount; i++) {
        const char *n = s->anims[i].name;
        if (!IsModelAnimationValid(s->model, s->anims[i])) continue;
        if (s->animFor[ANIM_IDLE] < 0 && NameHas(n, "idle")) s->animFor[ANIM_IDLE] = i;
        if (s->animFor[ANIM_WALK] < 0 && (NameHas(n, "walk") || NameHas(n, "run"))) s->animFor[ANIM_WALK] = i;
        if (s->animFor[ANIM_ATTACK] < 0 && (NameHas(n, "attack") || NameHas(n, "cast"))) s->animFor[ANIM_ATTACK] = i;
        if (s->animFor[ANIM_HIT] < 0 && NameHas(n, "hit")) s->animFor[ANIM_HIT] = i;
        if (s->animFor[ANIM_DEATH] < 0 && NameHas(n, "death")) s->animFor[ANIM_DEATH] = i;
    }
    for (a = 0; a < ANIM_COUNT; a++)                          /* missing ones fall back to idle */
        if (s->animFor[a] < 0) s->animFor[a] = s->animFor[ANIM_IDLE] >= 0 ? s->animFor[ANIM_IDLE] : (s->animCount > 0 ? 0 : -1);
    s->loaded = true;
    printf("model slot: %s loaded (%d animations)\n", path, s->animCount);
    return slotCount - 1;
}

bool Model_Draw(int slot, Vector3 pos, float yaw, int anim, float time, Color tint)
{
    ModelSlot *s;
    int a;
    if (slot < 0 || slot >= slotCount || !slots[slot].loaded) return false;
    s = &slots[slot];
    a = s->animFor[anim >= 0 && anim < ANIM_COUNT ? anim : 0];
    if (a >= 0 && s->anims[a].keyframeCount > 0)
        UpdateModelAnimation(s->model, s->anims[a], fmodf(time * 30.0f, (float)s->anims[a].keyframeCount));
    s->model.transform = MatrixMultiply(MatrixMultiply(MatrixScale(s->scale, s->scale, s->scale), MatrixRotateY(yaw + s->yawOffset)),
                                        MatrixTranslate(pos.x, pos.y + s->lift, pos.z));
    DrawModel(s->model, (Vector3){ 0 }, 1.0f, tint);
    return true;
}

/* Which model animation fits a pose. */
static int PoseAnim(const CharPose *p)
{
    if (p->flash > 0.3f) return ANIM_HIT;
    if (p->swing >= 0.0f || p->windup > 0.0f) return ANIM_ATTACK;
    if (p->walkAmount > 0.3f) return ANIM_WALK;
    return ANIM_IDLE;
}

/* Draw the role's model if there is one (with hit flash / wind-up / transparency tint). */
static bool TryModel(int role, const CharPose *p)
{
    static const char *const names[ROLE_COUNT] = { "player", "monk", "wraith", "priest", "abbot", "serpent", "oren", "apprentice" };
    static const float heights[ROLE_COUNT] = { ROLE_H_PLAYER, ROLE_H_MONK, ROLE_H_WRAITH, ROLE_H_PRIEST, ROLE_H_ABBOT,
                                               ROLE_H_SERPENT, ROLE_H_OREN, ROLE_H_APPRENTICE };
    static int slotOf[ROLE_COUNT];
    static bool asked[ROLE_COUNT];
    Color tint = WHITE;
    if (!asked[role]) { asked[role] = true; slotOf[role] = Model_Slot(names[role], heights[role], MODEL_YAW_OFFSET); }
    if (slotOf[role] < 0) return false;
    if (p->windup > 0.0f) tint = (Color){ 255, (unsigned char)(255 - 180 * p->windup), (unsigned char)(255 - 190 * p->windup), 255 };
    if (p->flash > 0.0f) tint = WHITE;
    if (p->alpha > 0.0f) tint.a = (unsigned char)(255 * p->alpha);
    return Model_Draw(slotOf[role], p->pos, p->yaw, PoseAnim(p), p->time, tint);
}

/* ============================================================ setup */

void Character_Init(void)
{
    BuildShapes();
    material = LoadMaterialDefault();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Atlas();
}

void Character_Shutdown(void)
{
    int i;
    for (i = 0; i < SH_COUNT; i++) UnloadMesh(shapes[i]);
    for (i = 0; i < slotCount; i++) {
        if (!slots[i].loaded) continue;
        if (slots[i].anims) UnloadModelAnimations(slots[i].anims, slots[i].animCount);
        for (int m = 0; m < slots[i].model.materialCount; m++) slots[i].model.materials[m].shader = (Shader){ 0 };
        UnloadModel(slots[i].model);
    }
    slotCount = 0;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* the atlas is owned by textures.c */
    material.shader = (Shader){ 0 };                                  /* the shader is owned by render.c */
    UnloadMaterial(material);
}

void Character_SetShader(Shader shader)
{
    material.shader = shader;
    worldShader = shader;
    hasWorldShader = true;
}

/* ============================================================ building blocks */

static Color Mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), a.a };
}

/* Draw unit shape `sh` scaled to `size`, centred at `center` inside `frame`. */
static void Shape(int sh, Matrix frame, Vector3 center, Vector3 size, Color col)
{
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(size.x, size.y, size.z),
                                             MatrixTranslate(center.x, center.y, center.z)), frame);
    if (pose->windup > 0.0f) col = Mix(col, (Color){ 255, 40, 30, 255 }, 0.65f * pose->windup);
    if (pose->flash > 0.0f)  col = Mix(col, WHITE, pose->flash);
    if (pose->alpha > 0.0f)  col.a = (unsigned char)(255 * pose->alpha);
    material.maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawMesh(shapes[sh], material, m);
}

static void Part(Matrix f, Vector3 c, Vector3 s, Color col) { Shape(SH_BOX, f, c, s, col); }

/* A part that glows (eyes, orbs, wand tip): no lighting, only fog. */
static void Glow(int sh, Matrix frame, Vector3 center, Vector3 size, Color col)
{
    Render_SetEmissive(true);
    Shape(sh, frame, center, size, col);
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

/* A limb hanging down from a joint frame: a cylinder of `len` (and a hand sphere). */
static void Limb(int sh, Matrix j, float len, float r, Color col)
{
    Shape(sh, j, (Vector3){ 0.0f, -len * 0.5f, 0.0f }, (Vector3){ r * 2.0f, len, r * 2.0f }, col);
}

/* ============================================================ Kael, the apprentice (player) */

void Character_DrawKnight(const CharPose *p)
{
    const Color robe = { 42, 42, 70, 255 }, robeDark = { 28, 28, 48, 255 }, crimson = { 138, 18, 28, 255 };
    const Color skin = { 196, 150, 120, 255 }, hair = { 52, 34, 24, 255 }, leather = { 58, 38, 24, 255 };
    const Color boots = { 34, 26, 20, 255 }, gold = { 196, 156, 70, 255 }, wood = { 46, 28, 18, 255 };
    float swingLeg = sinf(p->walkPhase) * 0.6f * p->walkAmount;
    float bob = fabsf(cosf(p->walkPhase)) * 0.05f * p->walkAmount + sinf(p->time * 2.2f) * 0.012f;
    float armRx = -0.35f;
    Matrix root, body, leg, arm, hand, head;
    int side;

    if (p->blink > 0.5f) return;
    pose = p;
    if (TryModel(ROLE_PLAYER, p)) return;
    root = RootFrame(p, 1.0f);
    body = MatrixMultiply(MatrixTranslate(0.0f, bob, 0.0f), root);

    /* legs + boots under the robe's hem */
    for (side = -1; side <= 1; side += 2) {
        leg = Joint(root, (Vector3){ 0.11f * side, 0.8f, 0.0f }, swingLeg * side, 0.0f, 0.0f);
        Limb(SH_CYL, leg, 0.72f, 0.09f, robeDark);
        Part(leg, (Vector3){ 0.0f, -0.74f, 0.04f }, (Vector3){ 0.17f, 0.12f, 0.28f }, boots);
    }
    /* long robe flaring to the shins, chest, crimson stole, belt */
    Shape(SH_ROBE, body, (Vector3){ 0.0f, 0.78f, 0.0f }, (Vector3){ 0.62f, 0.92f, 0.5f }, robe);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 1.28f, 0.0f }, (Vector3){ 0.42f, 0.34f, 0.3f }, robe);
    Part(body, (Vector3){ -0.08f, 1.05f, 0.15f }, (Vector3){ 0.07f, 0.62f, 0.03f }, crimson);
    Part(body, (Vector3){ 0.08f, 1.05f, 0.15f }, (Vector3){ 0.07f, 0.62f, 0.03f }, crimson);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 0.98f, 0.0f }, (Vector3){ 0.44f, 0.07f, 0.33f }, leather);
    Part(body, (Vector3){ 0.0f, 0.98f, 0.17f }, (Vector3){ 0.07f, 0.06f, 0.02f }, gold);
    /* hood lying on the shoulders */
    Shape(SH_SPHERE, body, (Vector3){ 0.0f, 1.42f, -0.1f }, (Vector3){ 0.38f, 0.22f, 0.26f }, robeDark);

    /* left arm swings opposite to the left leg */
    arm = Joint(body, (Vector3){ 0.25f, 1.40f, 0.0f }, -swingLeg * 0.8f, 0.0f, 0.08f);
    Limb(SH_ROBE, arm, 0.58f, 0.075f, robe);
    Shape(SH_SPHERE, arm, (Vector3){ 0.0f, -0.62f, 0.0f }, (Vector3){ 0.1f, 0.1f, 0.1f }, skin);

    /* right arm holds the wand; casting snaps it straight forward, pointing at the target */
    if (p->swing >= 0.0f) armRx = -1.55f;
    else armRx += swingLeg * 0.4f;
    arm = Joint(body, (Vector3){ -0.25f, 1.40f, 0.0f }, armRx, 0.0f, -0.08f);
    Limb(SH_ROBE, arm, 0.58f, 0.075f, robe);
    Shape(SH_SPHERE, arm, (Vector3){ 0.0f, -0.62f, 0.0f }, (Vector3){ 0.1f, 0.1f, 0.1f }, skin);
    /* wand, held at the wrist: forward-down at rest, in line with the arm while casting */
    hand = Joint(arm, (Vector3){ 0.0f, -0.6f, 0.0f }, p->swing >= 0.0f ? 1.57f : 0.6f, 0.0f, 0.0f);
    Shape(SH_CYL, hand, (Vector3){ 0.0f, 0.0f, 0.15f }, (Vector3){ 0.035f, 0.035f, 0.36f }, wood);
    Shape(SH_CYL, hand, (Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 0.05f, 0.05f, 0.07f }, gold);
    Glow(SH_SPHERE, hand, (Vector3){ 0.0f, 0.0f, 0.35f }, (Vector3){ 0.07f, 0.07f, 0.07f }, (Color){ 255, 40, 30, 255 });

    /* head: young face, dark hair */
    head = Joint(body, (Vector3){ 0.0f, 1.46f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.17f, 0.0f }, (Vector3){ 0.28f, 0.32f, 0.29f }, skin);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.24f, -0.03f }, (Vector3){ 0.31f, 0.27f, 0.31f }, hair);
    Part(head, (Vector3){ -0.06f, 0.17f, 0.14f }, (Vector3){ 0.04f, 0.03f, 0.02f }, (Color){ 30, 20, 18, 255 });
    Part(head, (Vector3){ 0.06f, 0.17f, 0.14f }, (Vector3){ 0.04f, 0.03f, 0.02f }, (Color){ 30, 20, 18, 255 });
}

/* ============================================================ enemy helpers */

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
static void Eyes(Matrix head, float y, float z, float spread, float size, Color c)
{
    Render_SetEmissiveMode(2);
    Shape(SH_SPHERE, head, (Vector3){ -spread, y, z }, (Vector3){ size, size * 0.6f, size * 0.5f }, c);
    Shape(SH_SPHERE, head, (Vector3){ spread, y, z }, (Vector3){ size, size * 0.6f, size * 0.5f }, c);
    Render_SetEmissiveMode(0);
}

/* A deep hood: rounded cowl, a peak falling back, and a face lost in shadow. */
static void Hood(Matrix head, float r, Color hood, bool peaked)
{
    const Color shadow = { 4, 2, 2, 255 };
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.2f, -0.03f }, (Vector3){ r * 2.0f, r * 2.2f, r * 2.1f }, hood);
    if (peaked) {
        Matrix tip = Joint(head, (Vector3){ 0.0f, 0.38f, -0.12f }, -0.7f, 0.0f, 0.0f);
        Shape(SH_CONE, tip, (Vector3){ 0.0f, 0.1f, 0.0f }, (Vector3){ r * 1.1f, 0.24f, r * 1.1f }, hood);
    }
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.17f, r * 0.72f }, (Vector3){ r * 1.3f, r * 1.5f, r * 0.6f }, shadow);
}

/* ============================================================ Ashen Monk */

void Character_DrawMonk(const CharPose *p)
{
    const Color robe = { 104, 96, 92, 255 }, hood = { 72, 64, 62, 255 }, skin = { 58, 52, 48, 255 };
    const Color rope = { 96, 42, 18, 255 }, eye = { 255, 30, 20, 255 };
    float swingLeg = sinf(p->walkPhase) * 0.5f * p->walkAmount;
    float bob = fabsf(cosf(p->walkPhase)) * 0.04f * p->walkAmount + sinf(p->time * 2.0f) * 0.01f;
    Matrix root, body, limb, head;
    int side;

    pose = p;
    if (TryModel(ROLE_MONK, p)) return;
    root = EnemyRoot(p, 1.0f);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.25f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    /* bare feet peek out under the robe */
    for (side = -1; side <= 1; side += 2) {
        limb = Joint(root, (Vector3){ 0.1f * side, 0.5f, 0.0f }, swingLeg * side, 0.0f, 0.0f);
        Shape(SH_SPHERE, limb, (Vector3){ 0.0f, -0.45f, 0.06f }, (Vector3){ 0.13f, 0.1f, 0.24f }, skin);
    }
    /* charred robe flaring to the floor, glowing ember cracks; hunched chest; rope belt */
    Shape(SH_EMBER_ROBE, body, (Vector3){ 0.0f, 0.55f, 0.0f }, (Vector3){ 0.66f, 1.1f, 0.54f }, robe);
    Shape(SH_EMBER_CYL, body, (Vector3){ 0.0f, 1.2f, -0.02f }, (Vector3){ 0.44f, 0.36f, 0.34f }, robe);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 0.92f, 0.0f }, (Vector3){ 0.4f, 0.05f, 0.33f }, rope);

    /* long arms hanging low, swaying; raised during the wind-up */
    for (side = -1; side <= 1; side += 2) {
        float rx = -0.2f - 1.2f * p->windup + sinf(p->time * 2.3f + side) * 0.06f - swingLeg * 0.3f * side;
        limb = Joint(body, (Vector3){ 0.25f * side, 1.34f, 0.0f }, rx, 0.0f, 0.06f * side);
        Limb(SH_EMBER_ROBE, limb, 0.74f, 0.085f, robe);
        Shape(SH_SPHERE, limb, (Vector3){ 0.0f, -0.8f, 0.0f }, (Vector3){ 0.1f, 0.15f, 0.09f }, skin);   /* claw-like hand */
    }

    /* hood with a dark, faceless opening and burning red eyes */
    head = TwitchHead(body, (Vector3){ 0.0f, 1.36f, 0.0f }, p, 0.0f);
    Hood(head, 0.2f, hood, true);
    Eyes(head, 0.21f, 0.2f, 0.065f, 0.06f, eye);
}

/* ============================================================ Choir Wraith */

void Character_DrawWraith(const CharPose *p)
{
    const Color pale = { 200, 198, 204, 255 }, hood = { 160, 158, 166, 255 }, hollow = { 4, 4, 6, 255 };
    const Color eye = { 255, 40, 30, 255 };
    float sway = sinf(p->time * 1.7f) * 0.06f;
    Matrix root, body, arm, head, wisp;
    int side;

    pose = p;
    if (TryModel(ROLE_WRAITH, p)) return;
    root = EnemyRoot(p, 1.0f);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateZ(sway), MatrixRotateX(-0.3f * p->windup)), root);

    /* no legs: the robe tapers into a trailing wisp */
    wisp = Joint(body, (Vector3){ 0.0f, 0.55f, -0.06f }, PI, 0.0f, 0.0f);         /* cone pointing down */
    Shape(SH_CONE, wisp, (Vector3){ 0.0f, 0.25f, 0.0f }, (Vector3){ 0.5f, 0.9f, 0.42f }, pale);
    Shape(SH_ROBE, body, (Vector3){ 0.0f, 0.95f, 0.0f }, (Vector3){ 0.6f, 0.6f, 0.46f }, pale);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 1.2f, 0.0f }, (Vector3){ 0.4f, 0.22f, 0.32f }, pale);

    for (side = -1; side <= 1; side += 2) {
        float wave = sinf(p->time * 2.4f + side * 1.2f) * 0.18f;
        arm = Joint(body, (Vector3){ 0.24f * side, 1.24f, 0.0f }, -1.35f + wave - 0.4f * p->windup, 0.0f, 0.1f * side);
        Limb(SH_ROBE, arm, 0.62f, 0.06f, pale);
        Shape(SH_SPHERE, arm, (Vector3){ 0.0f, -0.66f, 0.0f }, (Vector3){ 0.07f, 0.12f, 0.06f }, pale);
    }

    /* hooded head, singing: the mouth hangs wide open */
    head = TwitchHead(body, (Vector3){ 0.0f, 1.3f, 0.0f }, p, 0.0f);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.2f, 0.02f }, (Vector3){ 0.3f, 0.38f, 0.3f }, pale);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.24f, -0.06f }, (Vector3){ 0.4f, 0.46f, 0.38f }, hood);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.06f, 0.15f }, (Vector3){ 0.1f, 0.2f, 0.04f }, hollow);   /* open mouth */
    Shape(SH_SPHERE, head, (Vector3){ -0.06f, 0.25f, 0.14f }, (Vector3){ 0.08f, 0.07f, 0.04f }, hollow);
    Shape(SH_SPHERE, head, (Vector3){ 0.06f, 0.25f, 0.14f }, (Vector3){ 0.08f, 0.07f, 0.04f }, hollow);
    Eyes(head, 0.25f, 0.155f, 0.06f, 0.045f, eye);
}

/* ============================================================ Ember Priest + Red Abbot */

static void DrawRobed(const CharPose *p, float scale, Color robe, Color trim, bool abbot)
{
    const Color iron = { 62, 58, 62, 255 }, ironDark = { 34, 32, 36, 255 };
    const Color fire = { 255, 120, 30, 255 }, fireCore = { 255, 220, 120, 255 };
    const Color eye = abbot ? (Color){ 255, 170, 40, 255 } : (Color){ 255, 40, 30, 255 };
    float bob = sinf(p->time * 2.0f) * 0.015f + fabsf(cosf(p->walkPhase)) * 0.03f * p->walkAmount;
    Matrix root, body, arm, head;
    int side, k;

    pose = p;
    root = EnemyRoot(p, scale);
    body = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.22f * p->windup), MatrixTranslate(0.0f, bob, 0.0f)), root);

    /* robe flaring to the floor, a dark front panel, a stole over the shoulders */
    Shape(SH_ROBE, body, (Vector3){ 0.0f, 0.6f, 0.0f }, (Vector3){ 0.74f, 1.2f, 0.6f }, robe);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 1.18f, 0.0f }, (Vector3){ 0.42f, 0.2f, 0.32f }, robe);
    Part(body, (Vector3){ 0.0f, 0.62f, 0.2f }, (Vector3){ 0.14f, 1.16f, 0.06f }, trim);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 1.24f, 0.0f }, (Vector3){ 0.5f, 0.08f, 0.38f }, trim);

    for (side = -1; side <= 1; side += 2) {
        bool censerHand = side < 0 && !abbot;
        float rx = censerHand ? -0.7f - 0.6f * p->windup : (side > 0 ? 0.1f + sinf(p->walkPhase) * 0.3f * p->walkAmount
                                                                     : -1.0f - 0.6f * p->windup);
        arm = Joint(body, (Vector3){ 0.25f * side, 1.2f, 0.0f }, rx, 0.0f, side > 0 ? 0.1f : 0.0f);
        Limb(SH_ROBE, arm, 0.56f, 0.09f, robe);
        Shape(SH_SPHERE, arm, (Vector3){ 0.0f, -0.6f, 0.0f }, (Vector3){ 0.09f, 0.09f, 0.09f }, (Color){ 60, 46, 40, 255 });
        if (censerHand) {
            /* a burning censer swinging on its chain */
            Matrix chain = Joint(arm, (Vector3){ 0.0f, -0.6f, 0.0f }, 0.7f + 0.6f * p->windup + sinf(p->time * 3.0f) * 0.3f, 0.0f, 0.0f);
            Shape(SH_CYL, chain, (Vector3){ 0.0f, -0.18f, 0.0f }, (Vector3){ 0.02f, 0.36f, 0.02f }, ironDark);
            Shape(SH_SPHERE, chain, (Vector3){ 0.0f, -0.42f, 0.0f }, (Vector3){ 0.17f, 0.15f, 0.17f }, iron);
            Glow(SH_SPHERE, chain, (Vector3){ 0.0f, -0.34f, 0.0f }, (Vector3){ 0.1f, 0.07f, 0.1f }, fire);
        }
    }

    head = TwitchHead(body, (Vector3){ 0.0f, 1.24f, 0.0f }, p, 0.0f);
    if (!abbot) {
        /* tall pointed hood */
        Hood(head, 0.2f, trim, false);
        Shape(SH_CONE, head, (Vector3){ 0.0f, 0.62f, -0.06f }, (Vector3){ 0.32f, 0.55f, 0.32f }, trim);
    } else {
        /* tall iron mitre shaped like a bell: knob, waist, flaring lip */
        Hood(head, 0.2f, trim, false);
        Shape(SH_CYL, head, (Vector3){ 0.0f, 0.36f, 0.0f }, (Vector3){ 0.46f, 0.05f, 0.46f }, ironDark);     /* lip */
        Shape(SH_ROBE, head, (Vector3){ 0.0f, 0.56f, 0.0f }, (Vector3){ 0.4f, 0.38f, 0.4f }, iron);           /* bell */
        Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.8f, 0.0f }, (Vector3){ 0.1f, 0.1f, 0.1f }, ironDark);      /* knob */
        Part(head, (Vector3){ 0.0f, 0.52f, 0.19f }, (Vector3){ 0.05f, 0.18f, 0.02f }, (Color){ 140, 16, 24, 255 });
    }
    Eyes(head, 0.22f, 0.17f, 0.065f, abbot ? 0.07f : 0.06f, eye);

    if (abbot) {
        /* two fireballs circling him */
        for (k = 0; k < 2; k++) {
            float a = p->time * 2.2f + k * PI;
            Vector3 c = { sinf(a) * 0.75f, 1.05f + sinf(p->time * 3.0f + k) * 0.12f, cosf(a) * 0.75f };
            Glow(SH_SPHERE, root, c, (Vector3){ 0.18f, 0.18f, 0.18f }, fire);
            Glow(SH_SPHERE, root, c, (Vector3){ 0.1f, 0.12f, 0.1f }, fireCore);
        }
    }
}

void Character_DrawPriest(const CharPose *p)
{
    pose = p;
    if (TryModel(ROLE_PRIEST, p)) return;
    DrawRobed(p, 1.0f, (Color){ 120, 14, 24, 255 }, (Color){ 70, 8, 14, 255 }, false);
}

void Character_DrawAbbot(const CharPose *p)
{
    pose = p;
    if (TryModel(ROLE_ABBOT, p)) return;
    DrawRobed(p, 1.8f, (Color){ 150, 14, 24, 255 }, (Color){ 22, 16, 18, 255 }, true);
}

/* ============================================================ fireball */

void Character_DrawBolt(Vector3 pos, float spin)
{
    static const CharPose none = { 0 };
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateX(spin), MatrixRotateY(spin * 1.3f)),
                              MatrixTranslate(pos.x, pos.y, pos.z));
    pose = &none;
    Glow(SH_SPHERE, m, (Vector3){ 0 }, (Vector3){ 0.32f, 0.32f, 0.32f }, (Color){ 255, 80, 20, 255 });
    Glow(SH_SPHERE, m, (Vector3){ 0 }, (Vector3){ 0.18f, 0.24f, 0.18f }, (Color){ 255, 220, 120, 255 });
}

/* ============================================================ Sanctum NPCs */

/* A friendly robed person (normal proportions, no glowing eyes). `oren` adds a long white beard,
 * white hair and a tall staff with a glowing gold tip. */
void Character_DrawRobedNpc(const CharPose *p, Color robe, bool oren)
{
    const Color skin = { 206, 160, 128, 255 }, hair = { 58, 38, 26, 255 }, white = { 232, 230, 224, 255 };
    const Color eye = { 30, 20, 18, 255 }, wood = { 70, 46, 26, 255 }, gold = { 255, 210, 90, 255 };
    Color trim = { (unsigned char)(robe.r * 0.7f), (unsigned char)(robe.g * 0.7f), (unsigned char)(robe.b * 0.7f), 255 };
    float breathe = sinf(p->time * 1.8f) * 0.012f;
    Matrix root, body, arm, head;
    int side;

    pose = p;
    if (TryModel(oren ? ROLE_OREN : ROLE_APPRENTICE, p)) return;
    root = RootFrame(p, 1.0f);
    body = MatrixMultiply(MatrixTranslate(0.0f, breathe, 0.0f), root);

    Shape(SH_ROBE, body, (Vector3){ 0.0f, 0.6f, 0.0f }, (Vector3){ 0.7f, 1.2f, 0.56f }, robe);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 1.24f, 0.0f }, (Vector3){ 0.44f, 0.3f, 0.32f }, robe);
    Shape(SH_CYL, body, (Vector3){ 0.0f, 0.95f, 0.0f }, (Vector3){ 0.44f, 0.06f, 0.34f }, trim);    /* belt */

    for (side = -1; side <= 1; side += 2) {
        float rx = (oren && side < 0) ? -0.5f : 0.05f;
        arm = Joint(body, (Vector3){ 0.25f * side, 1.36f, 0.0f }, rx, 0.0f, 0.06f * side);
        Limb(SH_ROBE, arm, 0.58f, 0.075f, robe);
        Shape(SH_SPHERE, arm, (Vector3){ 0.0f, -0.62f, 0.0f }, (Vector3){ 0.1f, 0.1f, 0.1f }, skin);
        if (oren && side < 0) {
            /* tall staff held upright, glowing gold tip */
            Matrix hand = Joint(arm, (Vector3){ 0.0f, -0.62f, 0.0f }, 0.5f, 0.0f, 0.0f);
            Shape(SH_CYL, hand, (Vector3){ 0.0f, 0.35f, 0.0f }, (Vector3){ 0.05f, 2.0f, 0.05f }, wood);
            Glow(SH_SPHERE, hand, (Vector3){ 0.0f, 1.4f, 0.0f }, (Vector3){ 0.16f, 0.16f, 0.16f }, gold);
        }
    }

    head = Joint(body, (Vector3){ 0.0f, 1.42f, 0.0f }, 0.0f, 0.0f, 0.0f);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.19f, 0.0f }, (Vector3){ 0.3f, 0.34f, 0.3f }, skin);
    Part(head, (Vector3){ -0.06f, 0.21f, 0.145f }, (Vector3){ 0.045f, 0.035f, 0.02f }, eye);
    Part(head, (Vector3){ 0.06f, 0.21f, 0.145f }, (Vector3){ 0.045f, 0.035f, 0.02f }, eye);
    Shape(SH_SPHERE, head, (Vector3){ 0.0f, 0.27f, -0.03f }, (Vector3){ 0.33f, 0.28f, 0.33f }, oren ? white : hair);   /* hair */
    if (oren) {
        Matrix beard = Joint(head, (Vector3){ 0.0f, 0.06f, 0.12f }, PI, 0.0f, 0.0f);     /* long beard: cone down */
        Shape(SH_CONE, beard, (Vector3){ 0.0f, 0.22f, 0.0f }, (Vector3){ 0.24f, 0.5f, 0.12f }, white);
    }
}
