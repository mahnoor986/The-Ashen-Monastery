/* textures.h - the block texture atlas, generated in code (no image files).
 * Every block texture is a tile packed into one small atlas texture. Where a real photo texture
 * exists in assets/textures/ (Poly Haven, CC0) it replaces the generated tile. */
#ifndef TEXTURES_H
#define TEXTURES_H

#include "raylib.h"

typedef enum {
    TILE_STONE_WALL = 0,   /* grey-blue bricks, dark mortar */
    TILE_WOOD_WALL,        /* dark wood panels */
    TILE_OBSIDIAN,         /* near-black pillar with purple glints */
    TILE_BOOKSHELF,        /* wood frame + colored book spines */
    TILE_STONE_FLOOR,      /* stone floor slabs */
    TILE_CARPET,           /* red carpet with gold edges */
    TILE_PLANK_FLOOR,      /* wood plank floor */
    TILE_CEILING,          /* very dark wood beams */
    TILE_CHEST,            /* chest wood + gold trim */
    TILE_IRON_DOOR,        /* dark metal with rivets */
    TILE_BONE,             /* off-white bone */
    TILE_WHITE,            /* plain white (tinted by material color) */
    TILE_FLAME,            /* glowing yellow-orange */
    TILE_IRON,             /* plain dark iron (torch brackets) */
    TILE_EMBER,            /* charred cloth with orange ember cracks */
    TILE_COUNT
} TileId;

#define TILE_PIXELS   16   /* generated tiles are painted at 16x16 pixels ... */
#define ATLAS_TILE    64   /* ... then scaled up (nearest) to 64x64 slots in the atlas */
#define ATLAS_TILES   4    /* atlas is 4x4 slots = 256x256 pixels */

/* World materials: one repeating 128x128 texture each (point filtered, no mipmaps).
 * Photo materials come from assets/textures/<file> (Poly Haven, CC0) and fall back to a
 * generated texture; the others are always generated in code. */
typedef enum {
    MAT_WALL_STONE = 0,    /* large rough ashlar blocks          wall_stone.png */
    MAT_WOOD_PANEL,        /* dark wood-paneled walls ('W')      wood_panel.png */
    MAT_TRIM,              /* smooth pale limestone trim         trim_stone.png */
    MAT_FLOOR,             /* big worn flagstones                floor_stone.png */
    MAT_WOOD,              /* dark wood: beams, doors, furniture wood_dark.png */
    MAT_CEILING,           /* dark plaster                       ceiling.png */
    MAT_PILLAR,            /* smooth dark stone                  pillar.png */
    MAT_METAL,             /* dark iron                          metal.png */
    MAT_CARPET,            /* deep crimson carpet (generated) */
    MAT_GOLD,              /* old gold (generated) */
    MAT_GLASS,             /* stained glass, emissive (generated) */
    MAT_BANNER,            /* crimson cloth with a key + crescent emblem (generated) */
    MAT_BOOKS,             /* book spines on shelves (generated) */
    MAT_BONE,              /* old bone (generated) */
    MAT_COBWEB,            /* cobweb with alpha (generated) */
    MAT_FLAME,             /* candle/torch flame, emissive (generated) */
    MAT_PAINTING,          /* dim old painting (generated) */
    MAT_CLOTH,             /* crimson curtain/rug cloth (generated) */
    MAT_POTION,            /* sickly green glow: cauldrons (generated, emissive) */
    MAT_MIRROR,            /* dark mirror glass with a faint sheen (generated) */
    MAT_WINDOW_LIT,        /* title castle: warm lit window, emissive (generated) */
    MAT_WINDOW_FLICKER,    /* the same, for windows that flicker */
    MAT_SNOW,              /* title cliff: snow (generated) */
    MAT_LEATHER,           /* black leather (the Grimoire) */
    MAT_RUBY,              /* blood-red faceted stone, emissive */
    MAT_SILVER,            /* cool polished silver */
    MAT_EMBER_GLOW,        /* swirling embers, emissive (the Chalice) */
    MAT_COUNT
} MaterialId;

#define MAT_TEX_SIZE  128      /* every material texture is resized to this */
#define MAT_UV_SCALE  0.5f     /* world units -> texture repeats (one repeat per 2 units) */

Texture2D Textures_Material(int mat);

void      Textures_Init(void);
void      Textures_Shutdown(void);
Texture2D Textures_Atlas(void);
/* UV rectangle of a tile, inset by half a texel so neighbouring tiles never bleed in */
void      Textures_TileUV(int tile, float *u0, float *v0, float *u1, float *v1);

#endif
