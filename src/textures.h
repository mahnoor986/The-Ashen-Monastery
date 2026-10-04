/* textures.h - the block texture atlas, generated in code (no image files).
 * Every block texture is a 16x16 pixel tile packed into one small atlas texture. */
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
    TILE_COUNT
} TileId;

#define TILE_PIXELS   16   /* each tile is 16x16 pixels */
#define ATLAS_TILES   4    /* atlas is 4x4 tiles = 64x64 pixels */

void      Textures_Init(void);
void      Textures_Shutdown(void);
Texture2D Textures_Atlas(void);
/* UV rectangle of a tile, inset by half a texel so neighbouring tiles never bleed in */
void      Textures_TileUV(int tile, float *u0, float *v0, float *u1, float *v1);

#endif
