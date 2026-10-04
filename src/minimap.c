/* minimap.c - corner minimap + full-screen map (see minimap.h). */
#include <math.h>
#include <string.h>
#include "minimap.h"
#include "game.h"
#include "ui.h"
#include "config.h"
#include "textures.h"

#define CELL_PX   8                      /* pixels per cell in the map texture */

static Texture2D mapTex;                 /* module-private GPU resource */
static bool      hasMap;

/* ------------------------------------------------------------ the map texture */

static Color CellColor(const World *w, int x, int z)
{
    static const int dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
    int d;
    if (w->area[z][x] == AREA_SOLID) {
        /* walls next to floor are a little lighter so the rooms' outlines read clearly */
        for (d = 0; d < 4; d++) {
            int nx = x + dx[d], nz = z + dz[d];
            if (nx >= 0 && nz >= 0 && nx < w->w && nz < w->h && w->area[nz][nx] != AREA_SOLID)
                return (Color){ 58, 60, 72, 255 };
        }
        return (Color){ 16, 17, 22, 255 };
    }
    if (w->grid[z][x] == 'P') return (Color){ 40, 42, 50, 255 };
    if (w->floorMat[z][x] == MAT_CARPET) return (Color){ 104, 30, 38, 255 };
    if (w->floorMat[z][x] == MAT_WOOD) return (Color){ 86, 72, 62, 255 };
    return w->area[z][x] == AREA_ROOM ? (Color){ 92, 100, 118, 255 } : (Color){ 74, 80, 96, 255 };
}

void Minimap_Build(const Game *g)
{
    const World *w = &g->world;
    Image img;
    int x, z;
    Minimap_Unload();
    img = GenImageColor(w->w * CELL_PX, w->h * CELL_PX, BLANK);
    for (z = 0; z < w->h; z++)
        for (x = 0; x < w->w; x++)
            ImageDrawRectangle(&img, x * CELL_PX, z * CELL_PX, CELL_PX, CELL_PX, CellColor(w, x, z));
    mapTex = LoadTextureFromImage(img);
    SetTextureFilter(mapTex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    hasMap = true;
}

void Minimap_Unload(void)
{
    if (hasMap) UnloadTexture(mapTex);
    hasMap = false;
}

/* ------------------------------------------------------------ exploration */

void Minimap_Reveal(Game *g)
{
    const World *w = &g->world;
    Vector3 eye = { g->player.pos.x, 1.5f, g->player.pos.z };
    int px = (int)g->player.pos.x, pz = (int)g->player.pos.z, r = (int)MAP_REVEAL_RANGE + 1, x, z;
    for (z = pz - r; z <= pz + r; z++) {
        for (x = px - r; x <= px + r; x++) {
            float dx = x + 0.5f - g->player.pos.x, dz = z + 0.5f - g->player.pos.z;
            if (x < 0 || z < 0 || x >= w->w || z >= w->h || g->discovered[z][x]) continue;
            if (dx * dx + dz * dz > MAP_REVEAL_RANGE * MAP_REVEAL_RANGE) continue;
            if (w->area[z][x] == AREA_SOLID) {
                /* a wall shows once any open neighbour is seen */
                if ((x > 0 && g->discovered[z][x - 1] && w->area[z][x - 1]) || (x + 1 < w->w && g->discovered[z][x + 1] && w->area[z][x + 1]) ||
                    (z > 0 && g->discovered[z - 1][x] && w->area[z - 1][x]) || (z + 1 < w->h && g->discovered[z + 1][x] && w->area[z + 1][x]))
                    g->discovered[z][x] = 1;
                continue;
            }
            if (World_LineOfSight(w, eye, (Vector3){ x + 0.5f, 1.0f, z + 0.5f })) g->discovered[z][x] = 1;
        }
    }
}

/* ------------------------------------------------------------ drawing */

typedef struct {
    Rectangle area;      /* where on screen */
    float     cx, cz;    /* world point at the area's centre */
    float     scale;     /* pixels per cell */
} MapView;

static Vector2 ToScreen(const MapView *v, float x, float z)
{
    return (Vector2){ v->area.x + v->area.width * 0.5f + (x - v->cx) * v->scale,
                      v->area.y + v->area.height * 0.5f + (z - v->cz) * v->scale };
}

static void DrawChestIcon(Vector2 p, float s, bool opened)
{
    if (opened) {
        DrawRectangleV((Vector2){ p.x - s, p.y - s * 0.7f }, (Vector2){ 2 * s, 1.4f * s }, (Color){ 90, 90, 92, 255 });
        DrawLineEx((Vector2){ p.x - s * 0.6f, p.y }, (Vector2){ p.x - s * 0.1f, p.y + s * 0.5f }, 2.0f, (Color){ 200, 200, 200, 255 });
        DrawLineEx((Vector2){ p.x - s * 0.1f, p.y + s * 0.5f }, (Vector2){ p.x + s * 0.7f, p.y - s * 0.5f }, 2.0f, (Color){ 200, 200, 200, 255 });
    } else {
        DrawRectangleV((Vector2){ p.x - s - 1, p.y - s * 0.7f - 1 }, (Vector2){ 2 * s + 2, 1.4f * s + 2 }, BLACK);
        DrawRectangleV((Vector2){ p.x - s, p.y - s * 0.7f }, (Vector2){ 2 * s, 1.4f * s }, COL_GOLD);
        DrawRectangleV((Vector2){ p.x - s, p.y - s * 0.15f }, (Vector2){ 2 * s, 1.0f }, (Color){ 90, 60, 20, 255 });
    }
}

static void DrawDoorIcon(Vector2 p, float s, bool open)
{
    Color c = open ? (Color){ 70, 200, 90, 255 } : (Color){ 200, 40, 40, 255 };
    DrawRectangleV((Vector2){ p.x - s * 0.7f - 1, p.y - s - 1 }, (Vector2){ 1.4f * s + 2, 2 * s + 2 }, BLACK);
    DrawRectangleV((Vector2){ p.x - s * 0.7f, p.y - s * 0.4f }, (Vector2){ 1.4f * s, 1.4f * s }, c);
    DrawCircleV((Vector2){ p.x, p.y - s * 0.4f }, s * 0.7f, c);
}

static void DrawPlayerArrow(Vector2 p, float yaw, float s)
{
    Vector2 f = { sinf(yaw), cosf(yaw) }, r = { -f.y, f.x };
    Vector2 tip = { p.x + f.x * s * 1.3f, p.y + f.y * s * 1.3f };
    Vector2 l = { p.x - f.x * s * 0.8f + r.x * s * 0.8f, p.y - f.y * s * 0.8f + r.y * s * 0.8f };
    Vector2 rr = { p.x - f.x * s * 0.8f - r.x * s * 0.8f, p.y - f.y * s * 0.8f - r.y * s * 0.8f };
    DrawTriangle(tip, rr, l, BLACK);
    DrawTriangle(tip, l, rr, BLACK);
    DrawTriangle((Vector2){ tip.x - f.x, tip.y - f.y }, (Vector2){ rr.x + f.x * 0.6f, rr.y + f.y * 0.6f },
                 (Vector2){ l.x + f.x * 0.6f, l.y + f.y * 0.6f }, WHITE);
    DrawTriangle((Vector2){ tip.x - f.x, tip.y - f.y }, (Vector2){ l.x + f.x * 0.6f, l.y + f.y * 0.6f },
                 (Vector2){ rr.x + f.x * 0.6f, rr.y + f.y * 0.6f }, WHITE);
}

/* The map inside v.area: discovered cells from the texture, the rest dark; then the guide icons. */
static void DrawMap(const Game *g, const MapView *v, float iconSize)
{
    const World *w = &g->world;
    Color hidden = { 9, 10, 14, 255 };
    int x, z, i;
    BeginScissorMode((int)v->area.x, (int)v->area.y, (int)v->area.width, (int)v->area.height);
    DrawRectangleRec(v->area, hidden);
    {
        Vector2 o = ToScreen(v, 0, 0);
        DrawTexturePro(mapTex, (Rectangle){ 0, 0, (float)mapTex.width, (float)mapTex.height },
                       (Rectangle){ o.x, o.y, w->w * v->scale, w->h * v->scale }, (Vector2){ 0 }, 0.0f, WHITE);
    }
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            Vector2 p;
            if (g->discovered[z][x]) continue;
            p = ToScreen(v, (float)x, (float)z);
            if (p.x > v->area.x + v->area.width || p.y > v->area.y + v->area.height ||
                p.x + v->scale < v->area.x || p.y + v->scale < v->area.y) continue;
            DrawRectangleV(p, (Vector2){ v->scale + 1, v->scale + 1 }, hidden);
        }
    }
    /* guides: the exit, chests (even unseen), the cage, an alerted boss, the player */
    for (i = 0; i < w->exitCount; i++)
        DrawDoorIcon(ToScreen(v, w->exits[i].x + 0.5f, w->exits[i].z + 0.5f), iconSize, w->exitOpen);
    for (i = 0; i < w->chestCount; i++)
        DrawChestIcon(ToScreen(v, w->chests[i].x + 0.5f, w->chests[i].z + 0.5f), iconSize, g->chestOpened[i]);
    if (g->cageExists) {
        Vector2 p = ToScreen(v, g->cagePos.x, g->cagePos.z);
        DrawCircleLinesV(p, iconSize * 1.3f, g->cageBroken ? (Color){ 120, 120, 120, 255 } : (Color){ 230, 90, 40, 255 });
        DrawLineV((Vector2){ p.x - iconSize * 0.5f, p.y - iconSize }, (Vector2){ p.x - iconSize * 0.5f, p.y + iconSize }, (Color){ 230, 90, 40, 255 });
        DrawLineV((Vector2){ p.x + iconSize * 0.5f, p.y - iconSize }, (Vector2){ p.x + iconSize * 0.5f, p.y + iconSize }, (Color){ 230, 90, 40, 255 });
    }
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        if (e->alive && e->type == EN_ABBOT && g->abbotAlerted) {
            float pulse = 0.6f + 0.4f * sinf(g->time * 6.0f);
            DrawCircleV(ToScreen(v, e->pos.x, e->pos.z), iconSize * (0.9f + 0.4f * pulse), (Color){ 230, 20, 20, (unsigned char)(255 * pulse) });
        }
    }
    DrawPlayerArrow(ToScreen(v, g->player.pos.x, g->player.pos.z), g->player.yaw, iconSize * 1.1f);
    EndScissorMode();
}

void Minimap_Draw(const Game *g)
{
    Rectangle panel = { SCREEN_W - MINIMAP_W - 18.0f, SCREEN_H - MINIMAP_H - 54.0f, MINIMAP_W, MINIMAP_H };
    MapView v;
    if (!hasMap) return;
    v.area = panel;
    v.cx = g->player.pos.x;
    v.cz = g->player.pos.z;
    v.scale = MINIMAP_W / MINIMAP_CELLS;
    DrawRectangleRounded((Rectangle){ panel.x - 6, panel.y - 6, panel.width + 12, panel.height + 46 }, 0.08f, 6, (Color){ 6, 6, 10, 216 });
    DrawMap(g, &v, 4.0f);
    DrawRectangleRoundedLinesEx((Rectangle){ panel.x - 6, panel.y - 6, panel.width + 12, panel.height + 46 }, 0.08f, 6, 1.5f,
                                (Color){ 196, 152, 70, 200 });
    UI_TextCentered(false, g->world.name, panel.x + panel.width * 0.5f, panel.y + panel.height + 2, 19, COL_BONE);
    if (!g->sanctum)
        UI_TextCentered(false, TextFormat("Ward Seals %d/%d   -   M: map", g->chestsOpened, g->world.chestCount),
                        panel.x + panel.width * 0.5f, panel.y + panel.height + 20, 17, COL_GOLD);
}

static void LegendRow(float x, float y, int kind, const char *text)
{
    Vector2 p = { x + 8, y + 12 };
    switch (kind) {
    case 0: DrawChestIcon(p, 7, false); break;
    case 1: DrawChestIcon(p, 7, true); break;
    case 2: DrawDoorIcon(p, 7, false); break;
    case 3: DrawDoorIcon(p, 7, true); break;
    case 4: DrawPlayerArrow(p, PI, 8); break;
    default: DrawCircleV(p, 7, (Color){ 230, 20, 20, 255 }); break;
    }
    UI_Text(false, text, x + 26, y, 22, COL_BONE);
}

void Minimap_DrawFull(const Game *g)
{
    const World *w = &g->world;
    Rectangle area = { 40, 80, 900, 600 };
    MapView v;
    float y = 120;
    if (!hasMap) return;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 4, 4, 8, 236 });
    v.scale = fminf(area.width / w->w, area.height / w->h);
    v.area = (Rectangle){ area.x, area.y, w->w * v.scale, w->h * v.scale };
    v.area.x += (area.width - v.area.width) * 0.5f;
    v.area.y += (area.height - v.area.height) * 0.5f;
    v.cx = w->w * 0.5f;
    v.cz = w->h * 0.5f;
    UI_TextCentered(true, TextFormat("%s", w->name), area.x + area.width * 0.5f, 20, 52, COL_GOLD);
    DrawRectangleLinesEx((Rectangle){ v.area.x - 4, v.area.y - 4, v.area.width + 8, v.area.height + 8 }, 2, (Color){ 196, 152, 70, 200 });
    DrawMap(g, &v, fmaxf(5.0f, v.scale * 0.45f));

    UI_Text(true, "Legend", 980, 80, 40, COL_GOLD);
    LegendRow(980, y += 20, 4, "You");
    LegendRow(980, y += 40, 0, "Unopened chest");
    LegendRow(980, y += 40, 1, "Opened chest");
    LegendRow(980, y += 40, 2, "Exit (locked)");
    LegendRow(980, y += 40, 3, "Exit (open)");
    if (w->theme == WING_COUNT - 1) LegendRow(980, y += 40, 5, "The Red Abbot");
    if (!g->sanctum)
        UI_Text(false, TextFormat("Ward Seals %d / %d", g->chestsOpened, w->chestCount), 980, y += 70, 26, COL_GOLD);
    UI_Text(false, "Dark areas: not explored yet", 980, y += 50, 20, (Color){ 150, 140, 128, 255 });
    UI_TextCentered(false, "M or Esc to close", SCREEN_W * 0.5f, SCREEN_H - 34, 24, (Color){ 150, 140, 128, 255 });
}
