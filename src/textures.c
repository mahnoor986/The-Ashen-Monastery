/* textures.c - all textures: the small 16x16-tile atlas used by characters and props, and the
 * world materials (repeating 128x128 textures: Poly Haven photos from assets/textures/ or
 * generated in code). Generated textures use deterministic pseudo-random noise, so they look the
 * same on every run. Filtering is POINT for the crisp PS1 look. */
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "textures.h"

static Texture2D atlas;   /* module-private GPU resources */
static Texture2D materials[MAT_COUNT];

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

/* ------------------------------------------------------------ world materials */

static void Px(Image *img, int x, int y, Color c)
{
    ImageDrawPixel(img, x & (img->width - 1), y & (img->height - 1), c);
}

/* Fallback when a photo texture is missing: rough blocks in `base` with dark joints. */
static Image GenBlocks(Color base, int bw, int bh, float noise, int seed)
{
    Image img = GenImageColor(64, 64, base);
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            int row = y / bh, shift = (row & 1) * (bw / 2);
            int bx = (x + shift) % bw, block = (x + shift) / bw + row * 7;
            Color c = Shade(base, 0.8f + 0.3f * Rnd(block, row, seed));
            c = Noisy(c, x, y, seed + 1, noise);
            if (y % bh == bh - 1 || bx == bw - 1) c = Shade(base, 0.35f);
            Px(&img, x, y, c);
        }
    }
    return img;
}

static Image GenCarpet(void)
{
    const Color red = { 96, 8, 16, 255 }, gold = { 170, 120, 50, 255 };
    Image img = GenImageColor(64, 64, red);
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            int lx = x % 16, ly = y % 16;
            float d = fabsf(lx - 7.5f) + fabsf(ly - 7.5f);
            Color c = Noisy(red, x, y, 50, 0.10f);
            if (d > 5.5f && d < 6.6f) c = Shade(gold, 0.55f);             /* diamond lattice */
            else if (d < 1.6f) c = Shade(gold, 0.5f);
            if (Rnd(x, y, 51) > 0.97f) c = Shade(c, 0.7f);               /* wear */
            Px(&img, x, y, c);
        }
    }
    return img;
}

static Image GenGold(void)
{
    const Color gold = { 168, 124, 52, 255 };
    Image img = GenImageColor(64, 64, gold);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            Color c = Noisy(gold, x / 2, y / 2, 52, 0.18f);
            if ((x + y * 3) % 23 == 0) c = Shade(c, 1.35f);              /* worn highlights */
            Px(&img, x, y, c);
        }
    return img;
}

/* Stained glass: deep blue / violet panes, a little crimson and gold, lead lines, a small rose at
 * the top. Brightest in the centre. UV 0..1 covers one window light. */
static Image GenGlass(void)
{
    static const Color panes[6] = { { 40, 70, 170, 255 }, { 70, 50, 150, 255 }, { 30, 90, 190, 255 },
                                    { 90, 40, 130, 255 }, { 150, 30, 40, 255 }, { 190, 150, 60, 255 } };
    const Color lead = { 14, 14, 20, 255 };
    Image img = GenImageColor(64, 64, BLACK);
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            int cx = x / 8, cy = y / 8;
            float pick = Rnd(cx + (cy & 1) * 3, cy, 53);
            int k = pick < 0.3f ? 0 : pick < 0.55f ? 1 : pick < 0.78f ? 2 : pick < 0.9f ? 3 : pick < 0.96f ? 4 : 5;
            float dx = (x - 31.5f) / 32.0f, dy = (y - 31.5f) / 32.0f;
            float glow = 1.15f - 0.55f * (dx * dx + dy * dy);
            Color c = Shade(Noisy(panes[k], x, y, 54, 0.12f), glow);
            if ((x % 8 == 0) || (y % 8 == 0) || ((x + y) % 16 == 0 && y > 8)) c = lead;
            if (y < 22) {                                             /* round rose motif at the top */
                float rx = x - 31.5f, ry = y - 12.0f, r = sqrtf(rx * rx + ry * ry);
                if (r < 9.0f && r > 7.6f) c = lead;
                else if (r < 3.0f) c = (Color){ 210, 170, 70, 255 };
            }
            Px(&img, x, y, c);
        }
    }
    return img;
}

/* Banner cloth: deep crimson, gold border, an original emblem of a key under a crescent moon. */
static Image GenBanner(void)
{
    const Color red = { 110, 12, 22, 255 }, gold = { 196, 150, 62, 255 };
    Image img = GenImageColor(64, 64, red);
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            Color c = Noisy(red, x, y, 55, 0.08f);
            float mx = x - 31.5f, my = y - 18.0f, r1 = sqrtf(mx * mx + my * my);
            float r2 = sqrtf((mx + 4.0f) * (mx + 4.0f) + (my + 3.0f) * (my + 3.0f));
            if (x < 3 || x > 60 || y < 3) c = gold;
            if (r1 < 9.0f && r2 > 8.0f) c = gold;                           /* crescent moon */
            if (x >= 30 && x <= 33 && y >= 32 && y <= 52) c = gold;          /* key shaft */
            if (y >= 44 && y <= 46 && x >= 33 && x <= 39) c = gold;          /* key teeth */
            if (y >= 49 && y <= 51 && x >= 33 && x <= 37) c = gold;
            if (y >= 27 && y <= 34 && x >= 27 && x <= 36 && !(y >= 29 && y <= 32 && x >= 29 && x <= 34)) c = gold;  /* bow */
            if (y > 58 && ((x / 4) & 1)) c = gold;                          /* fringe */
            Px(&img, x, y, c);
        }
    }
    return img;
}

/* Book spines: 4 shelves per texture (one texture repeat = 2 world units = 4 shelves of 0.5). */
static Image GenBooks(void)
{
    static const Color spines[7] = { { 100, 24, 24, 255 }, { 30, 60, 40, 255 }, { 34, 40, 88, 255 },
                                     { 96, 66, 30, 255 }, { 66, 30, 70, 255 }, { 84, 76, 58, 255 }, { 40, 30, 22, 255 } };
    const Color shelf = { 48, 32, 20, 255 }, back = { 10, 7, 5, 255 };
    Image img = GenImageColor(64, 64, back);
    int row, x, y;
    for (row = 0; row < 4; row++) {
        int top = row * 16;
        x = 0;
        while (x < 64) {
            int w = 2 + (int)(Rnd(x, row, 56) * 3.0f), h = 9 + (int)(Rnd(x, row, 57) * 5.0f), bx, by;
            Color sp = spines[(int)(Rnd(x, row, 58) * 6.99f)];
            for (bx = x; bx < x + w && bx < 64; bx++)
                for (by = top + 14 - h; by < top + 14; by++) {
                    Color c = Shade(Noisy(sp, bx, by, 59, 0.1f), bx == x ? 0.6f : 1.0f);
                    if (by == top + 14 - h + 2 && Rnd(x, row, 60) > 0.5f) c = (Color){ 170, 130, 60, 255 };
                    Px(&img, bx, by, c);
                }
            x += w + (Rnd(x, row, 61) > 0.85f ? 2 : 0);
        }
        for (y = top + 14; y < top + 16; y++)
            for (x = 0; x < 64; x++) Px(&img, x, y, Noisy(shelf, x, y, 62, 0.15f));
    }
    return img;
}

static Image GenBone(void)
{
    const Color bone = { 200, 190, 160, 255 };
    Image img = GenImageColor(64, 64, bone);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            Color c = Noisy(bone, x, y, 63, 0.10f);
            if (Rnd(x / 2, y / 2, 64) > 0.92f) c = Shade(c, 0.65f);
            Px(&img, x, y, c);
        }
    return img;
}

/* Cobweb: radial threads + sagging rings on transparent black. UV (0,0) is the corner it hangs from. */
static Image GenCobweb(void)
{
    Image img = GenImageColor(64, 64, (Color){ 0, 0, 0, 0 });
    int x, y, k;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            float a = atan2f((float)y, (float)x), r = sqrtf((float)(x * x + y * y));
            bool spoke = false, ring = false;
            for (k = 0; k < 6; k++) if (fabsf(a - k * 0.29f - 0.06f) < 0.012f + 0.4f / (r + 4.0f)) spoke = true;
            if (fmodf(r + 2.0f * sinf(a * 9.0f), 9.0f) < 0.9f && r > 6.0f) ring = true;
            if ((spoke || ring) && r < 60.0f - 8.0f * Rnd(x / 6, y / 6, 65))
                Px(&img, x, y, (Color){ 200, 205, 215, (unsigned char)fmaxf(0.0f, 170.0f - r * 1.9f) });
        }
    }
    return img;
}

/* Flame: a teardrop of white-yellow fading to orange, transparent around it. */
static Image GenFlame(void)
{
    Image img = GenImageColor(64, 64, (Color){ 0, 0, 0, 0 });
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            float dx = (x - 31.5f) / 32.0f, t = y / 63.0f;                /* t: 0 top .. 1 bottom */
            float width = 0.12f + 0.8f * t * (1.0f - t * 0.4f);
            float f = 1.0f - fabsf(dx) / width;
            if (f <= 0.0f) continue;
            Px(&img, x, y, (Color){ 255, (unsigned char)(130 + 120 * f * t), (unsigned char)(40 + 150 * f * f * t),
                                    (unsigned char)(255 * fminf(1.0f, f * 2.0f)) });
        }
    }
    return img;
}

/* A dim old painting: dark sky, a hill with a lone tower, a pale moon; UV 0..1 = whole canvas. */
static Image GenPainting(void)
{
    const Color ink = { 16, 16, 18, 255 };
    Image img = GenImageColor(64, 64, BLACK);
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            float hill = 40.0f + 6.0f * sinf(x * 0.09f) + 3.0f * sinf(x * 0.23f + 1.0f);
            Color c = { (unsigned char)(26 + y / 3), (unsigned char)(30 + y / 3), (unsigned char)(44 + y / 4), 255 };
            float mx = x - 44.0f, my = y - 15.0f;
            if (mx * mx + my * my < 30.0f) c = (Color){ 170, 166, 140, 255 };
            if (y > hill) c = (Color){ 22, 26, 20, 255 };
            if (x >= 18 && x <= 22 && y > hill - 18 && y <= hill) c = ink;
            if (x >= 17 && x <= 23 && y > hill - 21 && y <= hill - 18) c = ink;
            if (x == 20 && y == (int)(hill - 12)) c = (Color){ 200, 150, 60, 255 };   /* lit window */
            c = Noisy(c, x, y, 66, 0.12f);
            if (x < 4 || x > 59 || y < 4 || y > 59) c = Noisy((Color){ 70, 48, 22, 255 }, x, y, 67, 0.2f);  /* frame */
            Px(&img, x, y, c);
        }
    }
    return img;
}

static Image GenCloth(void)
{
    const Color red = { 120, 16, 26, 255 };
    Image img = GenImageColor(64, 64, red);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            Px(&img, x, y, Shade(Noisy(red, x, y, 68, 0.06f), 0.8f + 0.25f * sinf(x * 0.8f)));   /* folds */
    return img;
}

/* Photo materials: file, saturation (1 = unchanged), colour multiplier, fallback colour + block size. */
static const struct {
    int mat; const char *file; float sat, r, g, b; Color fallback; int bw, bh;
} PHOTO_MAT[] = {
    { MAT_WALL_STONE, "wall_stone.png",  0.30f, 0.86f, 0.92f, 0.90f, {  86,  88,  90, 255 }, 16, 8 },
    { MAT_WOOD_PANEL, "wood_panel.png",  0.60f, 0.80f, 0.74f, 0.70f, {  60,  40,  28, 255 }, 8, 64 },
    { MAT_TRIM,       "trim_stone.png",  0.35f, 1.00f, 0.98f, 0.92f, { 150, 146, 134, 255 }, 32, 16 },
    { MAT_FLOOR,      "floor_stone.png", 0.30f, 0.80f, 0.84f, 0.86f, {  76,  76,  80, 255 }, 32, 32 },
    { MAT_WOOD,       "wood_dark.png",   0.50f, 0.62f, 0.52f, 0.46f, {  56,  38,  26, 255 }, 64, 8 },
    { MAT_CEILING,    "ceiling.png",     0.30f, 0.50f, 0.50f, 0.52f, {  40,  38,  38, 255 }, 64, 64 },
    { MAT_PILLAR,     "pillar.png",      0.25f, 0.70f, 0.72f, 0.74f, {  60,  60,  64, 255 }, 64, 32 },
    { MAT_METAL,      "metal.png",       0.25f, 0.45f, 0.45f, 0.48f, {  44,  44,  50, 255 }, 64, 64 },
};

static Image LoadPhotoMaterial(int i)
{
    const char *path = TextFormat("%s/%s", TEXTURE_DIR, PHOTO_MAT[i].file);
    Image img = { 0 };
    Color *px;
    int k;
    if (FileExists(path)) img = LoadImage(path);
    if (!IsImageValid(img)) {
        printf("note: %s missing - using a generated texture. To get the real one: open https://polyhaven.com/textures,\n"
               "      search a matching texture, download the 1k PNG diffuse map and save it as %s\n", path, path);
        return GenBlocks(PHOTO_MAT[i].fallback, PHOTO_MAT[i].bw, PHOTO_MAT[i].bh, 0.12f, 70 + i);
    }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageResize(&img, MAT_TEX_SIZE, MAT_TEX_SIZE);
    px = (Color *)img.data;
    for (k = 0; k < img.width * img.height; k++) {
        float l = px[k].r * 0.299f + px[k].g * 0.587f + px[k].b * 0.114f, s = PHOTO_MAT[i].sat;
        px[k].r = Clamp255((l + (px[k].r - l) * s) * PHOTO_MAT[i].r);
        px[k].g = Clamp255((l + (px[k].g - l) * s) * PHOTO_MAT[i].g);
        px[k].b = Clamp255((l + (px[k].b - l) * s) * PHOTO_MAT[i].b);
        px[k].a = 255;
    }
    return img;
}

static Image GenPotion(void);
static Image GenMirror(void);
static Image GenWindowLit(void);
static Image GenSnow(void);
static Image GenSolid(Color base, float noise, int seed, float stripes);

static void InitMaterials(void)
{
    Image gen[MAT_COUNT] = { 0 };
    unsigned int i;
    for (i = 0; i < sizeof(PHOTO_MAT) / sizeof(PHOTO_MAT[0]); i++) gen[PHOTO_MAT[i].mat] = LoadPhotoMaterial((int)i);
    gen[MAT_CARPET] = GenCarpet();
    gen[MAT_GOLD] = GenGold();
    gen[MAT_GLASS] = GenGlass();
    gen[MAT_BANNER] = GenBanner();
    gen[MAT_BOOKS] = GenBooks();
    gen[MAT_BONE] = GenBone();
    gen[MAT_COBWEB] = GenCobweb();
    gen[MAT_FLAME] = GenFlame();
    gen[MAT_PAINTING] = GenPainting();
    gen[MAT_CLOTH] = GenCloth();
    gen[MAT_POTION] = GenPotion();
    gen[MAT_MIRROR] = GenMirror();
    gen[MAT_WINDOW_LIT] = GenWindowLit();
    gen[MAT_WINDOW_FLICKER] = GenWindowLit();
    gen[MAT_SNOW] = GenSnow();
    gen[MAT_LEATHER] = GenSolid((Color){ 30, 24, 22, 255 }, 0.25f, 77, 0.0f);
    gen[MAT_RUBY] = GenSolid((Color){ 220, 24, 30, 255 }, 0.3f, 78, 0.5f);
    gen[MAT_SILVER] = GenSolid((Color){ 176, 184, 198, 255 }, 0.12f, 79, 0.0f);
    gen[MAT_EMBER_GLOW] = GenSolid((Color){ 255, 120, 30, 255 }, 0.45f, 80, 0.8f);
    for (i = 0; i < MAT_COUNT; i++) {
        if (gen[i].width != MAT_TEX_SIZE) ImageResizeNN(&gen[i], MAT_TEX_SIZE, MAT_TEX_SIZE);   /* chunky pixels */
        materials[i] = LoadTextureFromImage(gen[i]);
        SetTextureFilter(materials[i], TEXTURE_FILTER_POINT);
        SetTextureWrap(materials[i], TEXTURE_WRAP_REPEAT);
        UnloadImage(gen[i]);
    }
}

static Image GenPotion(void)
{
    Image img = GenImageColor(64, 64, BLACK);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float swirl = 0.5f + 0.5f * sinf(x * 0.35f + 2.0f * sinf(y * 0.21f));
            Px(&img, x, y, (Color){ (unsigned char)(40 + 50 * swirl), (unsigned char)(150 + 90 * swirl), (unsigned char)(60 + 40 * swirl), 255 });
        }
    return img;
}

static Image GenMirror(void)
{
    Image img = GenImageColor(64, 64, BLACK);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float sheen = fmaxf(0.0f, 1.0f - fabsf((x - y * 0.6f) - 10.0f) / 6.0f);       /* diagonal glint */
            Color c = Noisy((Color){ 26, 32, 44, 255 }, x / 2, y / 2, 69, 0.25f);
            c = Shade(c, 1.0f + 1.4f * sheen);
            Px(&img, x, y, c);
        }
    return img;
}

/* A warm lit window seen from outside: amber glow, dark mullion cross, brighter at the bottom. */
static Image GenWindowLit(void)
{
    Image img = GenImageColor(64, 64, BLACK);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float t = y / 63.0f;
            Color c = Noisy((Color){ (unsigned char)(200 + 50 * t), (unsigned char)(120 + 50 * t), (unsigned char)(40 + 30 * t), 255 }, x / 4, y / 4, 75, 0.15f);
            if (x < 5 || x > 58 || y < 5 || y > 58 || (x > 29 && x < 34) || (y > 27 && y < 32)) c = (Color){ 20, 14, 10, 255 };
            Px(&img, x, y, c);
        }
    return img;
}

/* A plain noisy colour; `stripes` adds bright facets/streaks (gems, embers). */
static Image GenSolid(Color base, float noise, int seed, float stripes)
{
    Image img = GenImageColor(64, 64, base);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            Color c = Noisy(base, x / 3, y / 3, seed, noise);
            if (stripes > 0.0f && ((x + y * 2) % 13) < 2) c = Shade(c, 1.0f + stripes);
            Px(&img, x, y, c);
        }
    return img;
}

static Image GenSnow(void)
{
    Image img = GenImageColor(64, 64, WHITE);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            Px(&img, x, y, Noisy((Color){ 190, 200, 214, 255 }, x / 2, y / 2, 76, 0.12f));
    return img;
}

Texture2D Textures_Material(int mat)
{
    return materials[mat >= 0 && mat < MAT_COUNT ? mat : 0];
}

/* ------------------------------------------------------------ API */

void Textures_Init(void)
{
    Image img = GenImageColor(TILE_PIXELS * ATLAS_TILES, TILE_PIXELS * ATLAS_TILES, MAGENTA);
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

    /* scale the 16 px tiles up to 64 px slots (nearest keeps them crisp) */
    ImageResizeNN(&img, ATLAS_TILE * ATLAS_TILES, ATLAS_TILE * ATLAS_TILES);

    atlas = LoadTextureFromImage(img);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    InitMaterials();
}

void Textures_Shutdown(void)
{
    int i;
    UnloadTexture(atlas);
    for (i = 0; i < MAT_COUNT; i++) UnloadTexture(materials[i]);
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
