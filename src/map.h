#ifndef GOTHIC_MAP_H
#define GOTHIC_MAP_H

#include <stdbool.h>
#include "raylib.h"
#include "config.h"

typedef enum {
    TILE_FLOOR = 0,
    TILE_WALL,
    TILE_TORCH_WALL,
    TILE_DOOR,
    TILE_EXIT
} TileType;

typedef struct { char type; int tx, ty; } Spawn;

typedef struct {
    unsigned char tiles[MAP_H][MAP_W];
    Vector2 torches[MAX_TORCHES];
    int     torchCount;
    Spawn   spawns[MAX_SPAWNS];
    int     spawnCount;
    Vector2 playerStart;
} Tilemap;

#define MAP_TILE_CENTER(tx, ty) ((Vector2){ (tx) * TILE_SIZE + TILE_SIZE / 2.0f, (ty) * TILE_SIZE + TILE_SIZE / 2.0f })

int         Map_LevelCount(void);
const char *Map_LevelName(int level);
void        Map_Load(Tilemap *m, int level);

bool        Map_IsSolid(const Tilemap *m, int tx, int ty);
bool        Map_IsSolidAt(const Tilemap *m, Vector2 p);
TileType    Map_TileAt(const Tilemap *m, Vector2 p);
bool        Map_RectCollides(const Tilemap *m, Rectangle r);
bool        Map_HasLineOfSight(const Tilemap *m, Vector2 a, Vector2 b);
bool        Map_TryOpenDoor(Tilemap *m, Rectangle probe);

/* Move a centered box by delta, sliding along walls (axis-separated). */
void        Map_Move(const Tilemap *m, Vector2 *pos, Vector2 size, Vector2 delta);

/* view = visible world rectangle (used for culling) */
void        Map_Draw(const Tilemap *m, Rectangle view);

#endif
