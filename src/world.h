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

typedef struct {
    Vector3 pos;         /* flame position */
    Vector3 normal;      /* direction the torch sticks out of the wall */
} Torch;

typedef struct {
    char  name[64];                          /* first line of the file */
    int   w, h;                              /* grid size in cells */
    char  grid[WORLD_MAX_H][WORLD_MAX_W];    /* raw map characters */
    unsigned char floorTile[WORLD_MAX_H][WORLD_MAX_W];  /* atlas tile of each floor cell */

    Vector3 start;                           /* player start (feet) */
    float   startYaw;                        /* player start facing */

    Cell  chests[MAX_CHESTS];    int chestCount;
    float chestYaw[MAX_CHESTS];              /* chests face the open floor */
    Cell  exits[MAX_EXIT_CELLS]; int exitCount;
    float exitYaw[MAX_EXIT_CELLS];           /* door slab rotation (0 = slab faces +-Z) */
    bool  exitOpen;                          /* door no longer blocks */
    float doorSlide;                         /* 0 = closed, 1 = fully sunk into the floor */
    Spawn spawns[MAX_SPAWNS];    int spawnCount;
    Torch torches[MAX_TORCHES];  int torchCount;

    Mesh  chunks[MAX_CHUNKS];    int chunkCount;
    int   vertexCount;                       /* total over all chunks (for the autotest summary) */
} World;

/* Load + validate a wing file. Prints errors (file:line:col) to stdout and returns false on any error. */
bool World_Load(World *w, const char *path, int expectedChests);
void World_BuildMeshes(World *w);            /* needs a GL context (after InitWindow) */
void World_Unload(World *w);

bool World_IsSolid(const World *w, int x, int z);          /* out of bounds counts as solid */
bool World_IsSolidAt(const World *w, float x, float z);
bool World_IsWallCell(const World *w, int x, int z);       /* full-height wall block (for the camera) */
/* Move an axis-aligned square of side `size` by delta on the XZ plane, sliding along walls. */
void World_Move(const World *w, Vector3 *pos, float size, Vector3 delta);
bool World_BoxBlocked(const World *w, float cx, float cz, float size);
bool World_LineOfSight(const World *w, Vector3 a, Vector3 b);
/* Baked torch light (0..1 rgb) arriving at point p; also used to light characters. */
Vector3 World_LightAt(const World *w, Vector3 p);

#endif
