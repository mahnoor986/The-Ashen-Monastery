/* world.h - one wing of the manor: the cell grid loaded from assets/wings/wingN.txt,
 * its validation, the chunked block meshes, collision and line of sight. */
#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "config.h"

typedef struct { int x, z; } Cell;

typedef struct {
    char    type;        /* 's' skeleton, 'g' ghost, 'w' witch, 'Q' Witch Queen */
    Vector3 pos;         /* feet position (cell center) */
    float   yaw;         /* starting facing */
} Spawn;

/* What kind of space a cell is (architecture: rooms are tall, corridors low). */
enum { AREA_SOLID = 0, AREA_CORRIDOR = 1, AREA_ROOM = 2 };

typedef struct {
    int x0, z0, x1, z1;  /* bounding box in cells (inclusive) */
    int cells;           /* number of room cells */
} Room;

typedef struct {
    Mesh mesh;
    int  mat;            /* MaterialId */
} WorldPart;

/* An exit doorway: the opening from a to b (floor points) seen from inside the wing; the two
 * door leaves hinge at a and b and swing toward `inward`. */
typedef struct {
    Vector3 a, b, inward;
    float   spring, top;  /* pointed arch: starts curving at spring, apex at top */
} Doorway;

/* A light source: wall sconce, candelabra, lantern, chandelier, fireplace, window... */
typedef struct {
    Vector3 pos;         /* light position */
    Vector3 normal;      /* sconces: direction the torch sticks out of the wall (else 0) */
    Vector3 color;       /* light colour (1 = full) */
    float   radius;      /* how far it reaches */
} Torch;

/* A visible flame (drawn every frame, flickering); size 1 = torch, ~0.35 = candle. */
typedef struct {
    Vector3 pos;
    float   size;
} Flame;

/* A placed prop (props.c decides what it looks like from `type`). */
typedef struct {
    int   type;
    float x, z, yaw;     /* floor position of the prop's origin, facing */
    int   cx, cz;        /* cell it stands in */
    float param;         /* type-specific (length of a table, ...) */
} Prop;

/* Axis-aligned collision box of a solid prop (floor plan). */
typedef struct { float x0, z0, x1, z1; } Collider;

typedef struct {
    char  name[64];                          /* first line of the file */
    int   theme;                             /* 0-4 = wing 1-5, 5 = the Sanctum (props, mood) */
    int   w, h;                              /* grid size in cells */
    char  grid[WORLD_MAX_H][WORLD_MAX_W];    /* raw map characters */
    unsigned char floorMat[WORLD_MAX_H][WORLD_MAX_W];   /* MaterialId of each floor cell */

    Vector3 start;                           /* player start (feet) */
    float   startYaw;                        /* player start facing */

    Cell  chests[MAX_CHESTS];    int chestCount;
    float chestYaw[MAX_CHESTS];              /* chests face the open floor */
    Cell  exits[MAX_EXIT_CELLS]; int exitCount;
    float exitYaw[MAX_EXIT_CELLS];           /* door slab rotation (0 = slab faces +-Z) */
    bool  exitOpen;                          /* door no longer blocks */
    float doorSlide;                         /* door leaves: 0 = closed, 1 = swung fully open */
    Doorway doorways[MAX_EXIT_CELLS]; int doorwayCount;
    Spawn spawns[MAX_SPAWNS];    int spawnCount;
    Spawn npcs[MAX_NPCS];        int npcCount;   /* 'O' Master Oren, 'a' apprentices/monks */
    Torch torches[MAX_TORCHES];  int torchCount;   /* light sources */
    Flame flames[MAX_FLAMES];    int flameCount;
    Prop  props[MAX_PROPS];      int propCount;
    Collider colliders[MAX_COLLIDERS]; int colliderCount;
    unsigned char window[WORLD_MAX_H][WORLD_MAX_W]; /* wall cell holds a window (bit per side) */
    unsigned char area[WORLD_MAX_H][WORLD_MAX_W];   /* AREA_* of every cell */
    unsigned char roomId[WORLD_MAX_H][WORLD_MAX_W]; /* 1-based room index, 0 = not a room */
    unsigned char island[WORLD_MAX_H][WORLD_MAX_W]; /* free-standing wall block inside a room */
    Room  rooms[MAX_ROOMS];      int roomCount;
    float ceiling[WORLD_MAX_H][WORLD_MAX_W];       /* lowest ceiling over each open cell */

    WorldPart parts[MAX_WORLD_PARTS]; int partCount;   /* one mesh per (chunk, material) */
    int   chunkCount;                        /* chunks that have any geometry */
    int   vertexCount;                       /* total over all parts (for the autotest summary) */
} World;

/* Load + validate a wing file. Prints errors (file:line:col) to stdout and returns false on any error.
 * needExit = false allows a map without an 'E' (the Sanctum). */
bool World_Load(World *w, const char *path, int expectedChests, bool needExit);
/* Builds the 3D architecture from the grid (architecture.c); needs a GL context. */
void World_BuildMeshes(World *w);
void World_Unload(World *w);

bool World_IsSolid(const World *w, int x, int z);          /* out of bounds counts as solid */
bool World_IsSolidAt(const World *w, float x, float z);
bool World_IsWallCell(const World *w, int x, int z);       /* full-height wall block (for the camera) */
/* Move an axis-aligned square of side `size` by delta on the XZ plane, sliding along walls. */
void World_Move(const World *w, Vector3 *pos, float size, Vector3 delta);
bool World_BoxBlocked(const World *w, float cx, float cz, float size);
bool World_LineOfSight(const World *w, Vector3 a, Vector3 b);
void World_AddFlame(World *w, Vector3 pos, float size);
void World_AddLight(World *w, Vector3 pos, Vector3 color, float radius);
/* Is the point inside a solid prop's collision box (grown by `margin`)? */
bool World_PropBlocked(const World *w, float x, float z, float margin);
/* Lowest ceiling height over the cell at (x, z) (corridor height for walls / outside). */
float World_CeilingAt(const World *w, float x, float z);
/* Baked torch light (0..1 rgb) arriving at point p; also used to light characters. */
Vector3 World_LightAt(const World *w, Vector3 p);

#endif
