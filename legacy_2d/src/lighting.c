#include <math.h>
#include "lighting.h"
#include "config.h"
#include "vec.h"

static RenderTexture2D mask;
static Texture2D       glowTex;

void Lighting_Init(void)
{
    Image img = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLACK);
    glowTex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(glowTex, TEXTURE_FILTER_BILINEAR);
    mask = LoadRenderTexture(SCREEN_W, SCREEN_H);
}

void Lighting_Shutdown(void)
{
    UnloadTexture(glowTex);
    UnloadRenderTexture(mask);
}

void Lighting_Glow(Vector2 s, float r, Color tint)
{
    Rectangle src = { 0, 0, (float)glowTex.width, (float)glowTex.height };
    Rectangle dst = { s.x, s.y, r * 2.0f, r * 2.0f };
    DrawTexturePro(glowTex, src, dst, V2(r, r), 0.0f, tint);
}

static void AddLight(Vector2 world, float radius, Color tint, Camera2D cam)
{
    Vector2 s = GetWorldToScreen2D(world, cam);
    float r = radius * cam.zoom;
    if (s.x < -r || s.x > SCREEN_W + r || s.y < -r || s.y > SCREEN_H + r) return;
    Lighting_Glow(s, r, tint);
}

void Lighting_Draw(const Tilemap *map, const Player *pl, const Projectile *projs, Camera2D cam, float time)
{
    int i;

    BeginTextureMode(mask);
        ClearBackground(AMBIENT_LIGHT);
        BeginBlendMode(BLEND_ADDITIVE);
            AddLight(pl->pos, PLAYER_LIGHT_RADIUS, (Color){ 170, 160, 140, 255 }, cam);

            for (i = 0; i < map->torchCount; i++) {
                Vector2 tp = map->torches[i];
                float fl = sinf(time * 9.0f + tp.x * 0.7f) * 4.0f
                         + sinf(time * 5.3f + tp.y * 1.3f) * 3.0f
                         + sinf(time * 17.0f + tp.x) * 1.5f;
                AddLight(V2(tp.x, tp.y + 12.0f), TORCH_LIGHT_RADIUS + fl, (Color){ 255, 175, 90, 255 }, cam);
            }

            for (i = 0; i < MAX_PROJECTILES; i++)
                if (projs[i].active) AddLight(projs[i].pos, 45.0f, (Color){ 150, 70, 200, 255 }, cam);

            if (pl->attackTimer > 0.0f)
                AddLight(pl->pos, 70.0f, (Color){ 120, 120, 140, 255 }, cam);
        EndBlendMode();
    EndTextureMode();

    BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureRec(mask.texture, (Rectangle){ 0, 0, (float)mask.texture.width, -(float)mask.texture.height }, V2(0, 0), WHITE);
    EndBlendMode();
}
