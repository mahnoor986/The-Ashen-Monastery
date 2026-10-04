/* textures.c - generates the 16x16 block texture atlas in code.
 * Each tile is painted pixel by pixel with deterministic pseudo-random noise, so the
 * textures look exactly the same on every run. Filtering is POINT for a crisp blocky look. */
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "textures.h"

static Texture2D atlas;   /* module-private GPU resource */

/* ------------------------------------------------------------ helpers */

/* Integer hash -> pseudo-random 0..1. Same inputs always give the same output. */
static float Rnd(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u
                   + (unsigned int)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xffffu) / 65535.0f;
}

static unsigned char Clamp255(float v)
{
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}

/* Multiply a color's brightness. */
static Color Shade(Color c, float m)
{
    return (Color){ Clamp255(c.r * m), Clamp255(c.g * m), Clamp255(c.b * m), 255 };
}

/* Small per-pixel brightness noise (+-amount). */
static Color Noisy(Color c, int x, int y, int seed, float amount)
{
    return Shade(c, 1.0f - amount + 2.0f * amount * Rnd(x, y, seed));
}

static void Put(Image *img, int tile, int x, int y, Color c)
{
    int ox = (tile % ATLAS_TILES) * TILE_PIXELS, oy = (tile / ATLAS_TILES) * TILE_PIXELS;
    ImageDrawPixel(img, ox + x, oy + y, c);
}

/* ------------------------------------------------------------ tiles */

static void PaintStoneWall(Image *img)
{
    const Color base = { 84, 88, 106, 255 }, mortar = { 26, 26, 34, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int row = y / 4, shift = (row & 1) * 4;
            int bx = (x + shift) % 8, brick = (x + shift) / 8 + row * 3;
            Color c;
            if (y % 4 == 3 || bx == 7) c = Noisy(mortar, x, y, 1, 0.15f);
            else {
                c = Shade(base, 0.78f + 0.3f * Rnd(brick, row, 2));
                c = Noisy(c, x, y, 3, 0.08f);
                if (y % 4 == 0) c = Shade(c, 1.12f);           /* top edge catches light */
                if (Rnd(x, y, 4) > 0.96f) c = Shade(c, 0.6f);   /* pits */
            }
            Put(img, TILE_STONE_WALL, x, y, c);
        }
    }
}

static void PaintWoodWall(Image *img)
{
    const Color base = { 66, 42, 28, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int plank = x / 4;
            Color c = Shade(base, 0.82f + 0.25f * Rnd(plank, 0, 5));
            c = Shade(c, 0.88f + 0.2f * Rnd(x, y / 3, 6));       /* vertical grain */
            if (x % 4 == 3) c = Shade(c, 0.45f);                   /* gap between planks */
            if (y == 0 || y == 15) c = (Color){ 34, 22, 15, 255 }; /* rail */
            if (y == 1) c = Shade(c, 1.2f);
            Put(img, TILE_WOOD_WALL, x, y, c);
        }
    }
}

static void PaintObsidian(Image *img)
{
    const Color base = { 20, 16, 28, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            Color c = Noisy(base, x, y, 7, 0.25f);
            if ((x + y) % 11 == 0) c = (Color){ 42, 34, 60, 255 };      /* sheen streak */
            if (Rnd(x, y, 8) > 0.95f) c = (Color){ 120, 72, 180, 255 }; /* purple glint */
            if (x == 0 || x == 15) c = Shade(c, 0.6f);
            Put(img, TILE_OBSIDIAN, x, y, c);
        }
    }
}

static void PaintBookshelf(Image *img)
{
    static const Color spines[6] = {
        { 112, 26, 26, 255 }, { 30, 66, 42, 255 }, { 36, 44, 96, 255 },
        { 104, 72, 30, 255 }, { 74, 32, 76, 255 }, { 92, 82, 62, 255 },
    };
    const Color frame = { 52, 34, 22, 255 }, back = { 14, 9, 7, 255 };
    int shelf, x, y;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            Put(img, TILE_BOOKSHELF, x, y, back);

    for (shelf = 0; shelf < 2; shelf++) {
        int top = 1 + shelf * 8;            /* book rows: y 1..6 and 9..14 */
        x = 1;
        while (x < 15) {
            int w = 1 + (int)(Rnd(x, shelf, 9) * 2.0f);
            int h = 4 + (int)(Rnd(x, shelf, 10) * 2.99f);
            Color sp = spines[(int)(Rnd(x, shelf, 11) * 5.99f)];
            int bx, by;
            if (x + w > 15) w = 15 - x;
            for (bx = x; bx < x + w; bx++) {
                for (by = top + 6 - h; by < top + 6; by++) {
                    Color c = Noisy(sp, bx, by, 12, 0.1f);
                    if (by == top + 6 - h + 1 && Rnd(x, shelf, 13) > 0.5f) c = (Color){ 190, 150, 70, 255 };
                    Put(img, TILE_BOOKSHELF, bx, by, c);
                }
            }
            x += w + (Rnd(x, shelf, 14) > 0.8f ? 1 : 0);
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            if (x == 0 || x == 15 || y == 0 || y == 7 || y == 8 || y == 15)
                Put(img, TILE_BOOKSHELF, x, y, Noisy(frame, x, y, 15, 0.12f));
        }
    }
}

static void PaintStoneFloor(Image *img)
{
    const Color base = { 76, 74, 80, 255 }, gap = { 30, 30, 36, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int slab = (x / 8) + (y / 8) * 2;
            Color c;
            if (x % 8 == 7 || y % 8 == 7) c = Noisy(gap, x, y, 16, 0.1f);
            else {
                c = Shade(base, 0.8f + 0.28f * Rnd(slab, 0, 17));
                c = Noisy(c, x, y, 18, 0.09f);
                if (Rnd(x, y, 19) > 0.97f) c = Shade(c, 0.65f);
            }
            Put(img, TILE_STONE_FLOOR, x, y, c);
        }
    }
}

static void PaintCarpet(Image *img)
{
    const Color red = { 92, 6, 14, 255 }, gold = { 196, 146, 62, 255 };       /* deep crimson */
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            float d = fabsf(x - 7.5f) + fabsf(y - 7.5f);
            Color c = Noisy(red, x, y, 20, 0.08f);
            if (x == 0 || x == 15) c = (Color){ 60, 8, 12, 255 };
            else if (x == 1 || x == 14) c = Noisy(gold, x, y, 21, 0.1f);
            else if (x == 2 || x == 13) c = ((y & 1) ? Shade(gold, 0.8f) : c);
            else if (d > 3.5f && d < 4.6f) c = Shade(gold, 0.7f);    /* diamond motif */
            else if (d < 1.2f) c = Shade(gold, 0.6f);
            Put(img, TILE_CARPET, x, y, c);
        }
    }
}

static void PaintPlankFloor(Image *img)
{
    const Color base = { 98, 66, 40, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            int row = y / 4;
            Color c = Shade(base, 0.8f + 0.25f * Rnd(row, 1, 22));
            c = Shade(c, 0.9f + 0.15f * Rnd(x / 3, y, 23));          /* horizontal grain */
            if (y % 4 == 3) c = Shade(c, 0.45f);
            if (x == (row * 5 + 3) % 16) c = Shade(c, 0.5f);          /* plank end */
            Put(img, TILE_PLANK_FLOOR, x, y, c);
        }
    }
}

static void PaintCeiling(Image *img)
{
    const Color base = { 24, 18, 15, 255 }, beam = { 44, 32, 24, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            Color c = Noisy(base, x, y, 24, 0.15f);
            if (x < 5) c = Noisy(beam, x, y / 2, 25, 0.12f);
            if (x == 5 || x == 0) c = (Color){ 10, 8, 7, 255 };
            Put(img, TILE_CEILING, x, y, c);
        }
    }
}

static void PaintChest(Image *img)
{
    const Color wood = { 104, 62, 30, 255 }, gold = { 206, 156, 62, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            Color c = Shade(wood, 0.85f + 0.2f * Rnd(y / 4, 0, 26));
            c = Noisy(c, x, y, 27, 0.08f);
            if (y % 4 == 3) c = Shade(c, 0.6f);
            if (x <= 1 || x >= 14 || y <= 1 || y >= 14) c = Noisy(gold, x, y, 28, 0.12f);
            if (x >= 6 && x <= 9 && y >= 5 && y <= 10) c = Noisy(gold, x, y, 29, 0.1f);  /* lock */
            if (x >= 7 && x <= 8 && y >= 7 && y <= 8) c = (Color){ 20, 12, 6, 255 };   /* keyhole */
            Put(img, TILE_CHEST, x, y, c);
        }
    }
}

static void PaintIronDoor(Image *img)
{
    const Color metal = { 54, 56, 64, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            Color c = Noisy(metal, x, y, 30, 0.12f);
            int rx = (x == 2 || x == 13), ry = (y == 2 || y == 8 || y == 13);
            if (x == 0 || x == 15 || y == 0 || y == 15 || x == 8) c = (Color){ 26, 26, 30, 255 };
            if (rx && ry) c = (Color){ 132, 134, 142, 255 };                 /* rivet */
            Put(img, TILE_IRON_DOOR, x, y, c);
        }
    }
}

static void PaintBone(Image *img)
{
    const Color bone = { 216, 206, 178, 255 };
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            Color c = Noisy(bone, x, y, 31, 0.08f);
            if (Rnd(x, y, 32) > 0.93f) c = (Color){ 150, 140, 114, 255 };
            Put(img, TILE_BONE, x, y, c);
        }
    }
}

static void PaintPlain(Image *img, int tile, Color c, float noise)
{
    int x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            Put(img, tile, x, y, noise > 0.0f ? Noisy(c, x, y, 33 + tile, noise) : c);
}

static void PaintEmber(Image *img)
{
    const Color char1 = { 46, 42, 40, 255 }, hot = { 255, 116, 30, 255 }, warm = { 190, 56, 12, 255 };
    int x, y, k;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            Put(img, TILE_EMBER, x, y, Noisy(char1, x, y, 40, 0.25f));
    /* three glowing cracks wandering down the cloth */
    for (k = 0; k < 3; k++) {
        int cx = 2 + k * 5;
        for (y = 0; y < 16; y++) {
            cx += (int)(Rnd(k, y, 41) * 3.0f) - 1;
            if (cx < 0) cx = 0;
            if (cx > 15) cx = 15;
            if (Rnd(k, y, 42) < 0.8f) Put(img, TILE_EMBER, cx, y, Rnd(cx, y, 43) > 0.5f ? hot : warm);
        }
    }
}

/* ------------------------------------------------------------ API */

/* Real textures (optional): file in assets/textures/ -> atlas slot. */
static const struct { const char *file; int tile; } PHOTO[] = {
    { "wall.png",      TILE_STONE_WALL },
    { "wood_wall.png", TILE_WOOD_WALL },
    { "floor.png",     TILE_STONE_FLOOR },
    { "pillar.png",    TILE_OBSIDIAN },
    { "door.png",      TILE_IRON_DOOR },
    { "ceiling.png",   TILE_CEILING },
};

/* Shrink a photo texture to one atlas slot, darken it, and paste it in. */
static void PastePhoto(Image *atlasImg, const char *file, int tile)
{
    const char *path = TextFormat("%s/%s", TEXTURE_DIR, file);
    Image img;
    if (!FileExists(path)) return;
    img = LoadImage(path);
    if (!IsImageValid(img)) { printf("warning: could not load %s (keeping the generated texture)\n", path); return; }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageResize(&img, ATLAS_TILE, ATLAS_TILE);
    ImageColorBrightness(&img, TEXTURE_DARKEN);
    ImageDraw(atlasImg, img, (Rectangle){ 0, 0, ATLAS_TILE, ATLAS_TILE },
              (Rectangle){ (float)((tile % ATLAS_TILES) * ATLAS_TILE), (float)((tile / ATLAS_TILES) * ATLAS_TILE),
                           ATLAS_TILE, ATLAS_TILE }, WHITE);
    UnloadImage(img);
}

void Textures_Init(void)
{
    Image img = GenImageColor(TILE_PIXELS * ATLAS_TILES, TILE_PIXELS * ATLAS_TILES, MAGENTA);
    unsigned int i;
    PaintStoneWall(&img);
    PaintWoodWall(&img);
    PaintObsidian(&img);
    PaintBookshelf(&img);
    PaintStoneFloor(&img);
    PaintCarpet(&img);
    PaintPlankFloor(&img);
    PaintCeiling(&img);
    PaintChest(&img);
    PaintIronDoor(&img);
    PaintBone(&img);
    PaintPlain(&img, TILE_WHITE, WHITE, 0.0f);
    PaintPlain(&img, TILE_FLAME, (Color){ 255, 196, 90, 255 }, 0.12f);
    PaintPlain(&img, TILE_IRON, (Color){ 44, 44, 50, 255 }, 0.2f);
    PaintEmber(&img);

    /* scale the 16 px tiles up to 64 px slots (nearest keeps them crisp), then add photo textures */
    ImageResizeNN(&img, ATLAS_TILE * ATLAS_TILES, ATLAS_TILE * ATLAS_TILES);
    for (i = 0; i < sizeof(PHOTO) / sizeof(PHOTO[0]); i++) PastePhoto(&img, PHOTO[i].file, PHOTO[i].tile);

    atlas = LoadTextureFromImage(img);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    UnloadImage(img);
}

void Textures_Shutdown(void)
{
    UnloadTexture(atlas);
}

Texture2D Textures_Atlas(void)
{
    return atlas;
}

void Textures_TileUV(int tile, float *u0, float *v0, float *u1, float *v1)
{
    const float size = (float)(ATLAS_TILE * ATLAS_TILES);
    float px = (float)((tile % ATLAS_TILES) * ATLAS_TILE);
    float py = (float)((tile / ATLAS_TILES) * ATLAS_TILE);
    *u0 = (px + 0.5f) / size;
    *v0 = (py + 0.5f) / size;
    *u1 = (px + ATLAS_TILE - 0.5f) / size;
    *v1 = (py + ATLAS_TILE - 0.5f) / size;
}
