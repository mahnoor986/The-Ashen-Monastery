#include <math.h>
#include <string.h>
#include "map.h"
#include "draw.h"
#include "vec.h"
#include "levels.h"

int Map_LevelCount(void) { return LEVEL_COUNT; }

const char *Map_LevelName(int level)
{
    if (level < 0 || level >= LEVEL_COUNT) return "";
    return LEVEL_NAMES[level];
}

void Map_Load(Tilemap *m, int level)
{
    int x, y;
    const char *const *rows = LEVELS[level];

    memset(m, 0, sizeof(*m));
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            char c = rows[y][x];
            unsigned char t = TILE_FLOOR;
            switch (c) {
            case '#': t = TILE_WALL; break;
            case 'T':
                t = TILE_TORCH_WALL;
                if (m->torchCount < MAX_TORCHES) m->torches[m->torchCount++] = MAP_TILE_CENTER(x, y);
                break;
            case 'D': t = TILE_DOOR; break;
            case '>': t = TILE_EXIT; break;
            case '@': m->playerStart = MAP_TILE_CENTER(x, y); break;
            case '.': break;
            default:
                if (m->spawnCount < MAX_SPAWNS) {
                    Spawn *s = &m->spawns[m->spawnCount++];
                    s->type = c; s->tx = x; s->ty = y;
                }
                break;
            }
            m->tiles[y][x] = t;
        }
    }
}

bool Map_IsSolid(const Tilemap *m, int tx, int ty)
{
    unsigned char t;
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return true;
    t = m->tiles[ty][tx];
    return t == TILE_WALL || t == TILE_TORCH_WALL || t == TILE_DOOR;
}

bool Map_IsSolidAt(const Tilemap *m, Vector2 p)
{
    return Map_IsSolid(m, (int)floorf(p.x / TILE_SIZE), (int)floorf(p.y / TILE_SIZE));
}

TileType Map_TileAt(const Tilemap *m, Vector2 p)
{
    int tx = (int)floorf(p.x / TILE_SIZE), ty = (int)floorf(p.y / TILE_SIZE);
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return TILE_WALL;
    return (TileType)m->tiles[ty][tx];
}

bool Map_RectCollides(const Tilemap *m, Rectangle r)
{
    int x1 = (int)floorf(r.x / TILE_SIZE);
    int y1 = (int)floorf(r.y / TILE_SIZE);
    int x2 = (int)floorf((r.x + r.width  - 0.01f) / TILE_SIZE);
    int y2 = (int)floorf((r.y + r.height - 0.01f) / TILE_SIZE);
    int x, y;
    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            if (Map_IsSolid(m, x, y)) return true;
    return false;
}

bool Map_HasLineOfSight(const Tilemap *m, Vector2 a, Vector2 b)
{
    Vector2 d = V2Sub(b, a);
    int steps = (int)(V2Len(d) / 8.0f) + 1, i;
    for (i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        if (Map_IsSolidAt(m, V2Add(a, V2Scale(d, t)))) return false;
    }
    return true;
}

bool Map_TryOpenDoor(Tilemap *m, Rectangle probe)
{
    int x1 = (int)floorf(probe.x / TILE_SIZE);
    int y1 = (int)floorf(probe.y / TILE_SIZE);
    int x2 = (int)floorf((probe.x + probe.width)  / TILE_SIZE);
    int y2 = (int)floorf((probe.y + probe.height) / TILE_SIZE);
    int x, y;
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            if (m->tiles[y][x] == TILE_DOOR) { m->tiles[y][x] = TILE_FLOOR; return true; }
        }
    }
    return false;
}

static Rectangle CenteredRect(Vector2 c, Vector2 size)
{
    return (Rectangle){ c.x - size.x * 0.5f, c.y - size.y * 0.5f, size.x, size.y };
}

void Map_Move(const Tilemap *m, Vector2 *pos, Vector2 size, Vector2 delta)
{
    pos->x += delta.x;
    if (Map_RectCollides(m, CenteredRect(*pos, size))) pos->x -= delta.x;
    pos->y += delta.y;
    if (Map_RectCollides(m, CenteredRect(*pos, size))) pos->y -= delta.y;
}

void Map_Draw(const Tilemap *m, Rectangle view)
{
    int x1 = (int)floorf(view.x / TILE_SIZE);
    int y1 = (int)floorf(view.y / TILE_SIZE);
    int x2 = (int)floorf((view.x + view.width)  / TILE_SIZE);
    int y2 = (int)floorf((view.y + view.height) / TILE_SIZE);
    int x, y;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= MAP_W) x2 = MAP_W - 1;
    if (y2 >= MAP_H) y2 = MAP_H - 1;

    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            int px = x * TILE_SIZE, py = y * TILE_SIZE;
            unsigned char t = m->tiles[y][x];

            if (t == TILE_WALL || t == TILE_TORCH_WALL) {
                DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, COL_WALL);
                DrawRectangle(px, py + TILE_SIZE / 2 - 1, TILE_SIZE, 1, COL_WALL_EDGE);
                DrawRectangle(px + (((x + y) & 1) ? 10 : 22), py, 1, TILE_SIZE / 2 - 1, COL_WALL_EDGE);
                DrawRectangle(px + (((x + y) & 1) ? 22 : 10), py + TILE_SIZE / 2, 1, TILE_SIZE / 2, COL_WALL_EDGE);
                DrawRectangle(px, py + TILE_SIZE - 1, TILE_SIZE, 1, COL_WALL_EDGE);
                if (!Map_IsSolid(m, x, y - 1)) DrawRectangle(px, py, TILE_SIZE, 2, COL_WALL_LIGHT);
                if (t == TILE_TORCH_WALL) DrawEntity(ENT_TORCH, MAP_TILE_CENTER(x, y), V2(0, 1));
            } else {
                unsigned int hsh = (unsigned int)(x * 73856093u) ^ (unsigned int)(y * 19349663u);
                DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, ((x + y) & 1) ? COL_FLOOR_A : COL_FLOOR_B);
                if (hsh % 13u == 0u) {
                    DrawLine(px + 6, py + 8, px + 14, py + 12, (Color){ 26, 26, 36, 255 });
                    DrawLine(px + 14, py + 12, px + 17, py + 20, (Color){ 26, 26, 36, 255 });
                }
                if (t == TILE_DOOR) {
                    DrawRectangle(px + 3, py + 1, TILE_SIZE - 6, TILE_SIZE - 2, (Color){ 92, 60, 36, 255 });
                    DrawRectangle(px + 3, py + 10, TILE_SIZE - 6, 2, (Color){ 58, 36, 20, 255 });
                    DrawRectangle(px + 3, py + 21, TILE_SIZE - 6, 2, (Color){ 58, 36, 20, 255 });
                    DrawCircle(px + TILE_SIZE - 9, py + TILE_SIZE / 2, 3, COL_CANDLE);
                } else if (t == TILE_EXIT) {
                    int i;
                    for (i = 0; i < 4; i++) {
                        unsigned char sh = (unsigned char)(30 + i * 22);
                        DrawRectangle(px + 3 + i * 3, py + 3 + i * 3, TILE_SIZE - 6 - i * 6, TILE_SIZE - 6 - i * 6,
                                      (Color){ sh, (unsigned char)(sh - 6), (unsigned char)(sh - 18), 255 });
                    }
                }
            }
        }
    }
}
