/* game.h - the Game struct (all game state lives here) and the game flow: menu, wings,
 * chests and treasures, combat, checkpoints, death, victory, saving, and the --autotest run. */
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "config.h"
#include "world.h"
#include "player.h"
#include "enemy.h"
#include "input.h"
#include "atmos.h"

typedef enum {
    STATE_MENU = 0,       /* title screen over the slowly orbiting manor */
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_INVENTORY,
    STATE_DEAD,           /* "THE FIRE TAKES YOU" */
    STATE_VICTORY,        /* "THE BELLS ARE SILENT" */
    STATE_INTRO,          /* the story, line by line, before wing 1 */
    STATE_DIALOGUE,       /* Master Oren speaks (Sanctum) */
    STATE_MAP,            /* full-screen map (game paused) */
} GameState;

typedef struct {
    char  title[96];
    char  sub[128];
    float time;           /* seconds shown so far */
    float duration;
    Color color;
} Banner;

#define MAX_BANNERS 4

typedef struct {
    Vector3 pos, vel;
    float   life, maxLife, size;
    Color   color;
} Particle;

typedef struct {
    Vector3     pos;
    float       yaw;
    Color       robe;
    bool        oren;
    const char *name;     /* NULL for the freed monks */
    const char *line;     /* what a friend says when you come close */
} Npc;

typedef struct Game {
    GameState state;
    int       wing;                    /* 0-based index into WINGS */
    World     world;
    bool      worldLoaded;
    Player    player;
    CameraRig rig;

    Enemy     enemies[MAX_ENEMIES];
    int       enemyCount;
    Bolt      bolts[MAX_BOLTS];
    Particle  particles[MAX_PARTICLES];

    /* chests of the current wing (same order as world.chests) */
    bool      chestOpened[MAX_CHESTS];
    float     chestLid[MAX_CHESTS];    /* lid animation 0..1 */
    int       chestsOpened;
    int       useChest;                /* chest in reach and facing, or -1 */
    float     useHold;                 /* seconds E has been held on useChest */
    Vector3   checkpoint;              /* where you rise again */
    float     checkpointYaw;

    /* progress over the whole run */
    bool      found[WING_COUNT][MAX_CHESTS];
    int       enemiesSlain;
    float     playTime;
    int       savedWing;               /* highest unlocked wing from save.txt (1-based), 0 = none */
    bool      abbotDead;
    bool      abbotAlerted;
    bool      ghostHintShown;          /* "ghosts can only be hurt in the light" */

    /* map: cells the player has seen (kept on respawn, cleared when the wing is (re)loaded) */
    unsigned char discovered[WORLD_MAX_H][WORLD_MAX_W];
    float     revealTimer;

    /* the serpent's cage in the Bell Tower (relics.c) */
    bool      cageExists, cageBroken;
    Vector3   cagePos;

    /* presentation */
    Banner    banners[MAX_BANNERS];
    int       bannerCount;
    float     shake;                   /* camera shake 0..1 */
    float     hurtFlash;
    float     redPulse;                /* 0..1 full-screen red flash (bells) */
    Atmos     atmos;                   /* falling ash + rising embers */
    float     tollTimer;               /* seconds until the next distant bell toll */
    float     lightningTimer;          /* seconds until the next lightning strike */
    float     lightningAge;            /* seconds since the last strike (< 0: none) */
    bool      thunderPlayed;
    float     torchFlare;              /* 0..1 torches flare up when the bell tolls */
    float     beamTime;                /* > 0 while the red lightning is visible */
    Vector3   beamEnd;                 /* where the lightning struck */
    float     fade;                    /* black overlay 0..1 */
    bool      leaving;                 /* fading out toward the next wing / victory */
    float     time;                    /* seconds since start */
    float     wingTime;                /* seconds since entering the current wing */
    int       menuSel, pauseSel;
    float     introTime;               /* seconds into the intro text */

    /* the Sanctum ending */
    bool      sanctum;                 /* in the Sanctum (warm, peaceful, no enemies) */
    Npc       npcs[MAX_NPCS];
    int       npcCount;
    int       talkNpc;                 /* friend currently speaking (-1 none) */
    bool      nearOren;                /* show "[E] Speak with Master Oren" */
    int       dialogLine;              /* Master Oren's line being shown */
    float     endFade;                 /* fade to black after the last line */
    bool      mouseLook;               /* mouse captured for camera control */
    int       mouseSkip;               /* frames to ignore mouse movement after capturing */
    bool      autotest;
    bool      hideHud;                 /* --tour: clean screenshots */
    bool      debug;                   /* F1 overlay */
    bool      showFps;
    bool      quit;
} Game;

extern const char *const TREASURES[WING_COUNT][MAX_CHESTS];
extern const char *const WING_NAMES[WING_COUNT];
#define INTRO_LINE_COUNT 4
#define OREN_LINE_COUNT 4
extern const char *const OREN_LINES[OREN_LINE_COUNT];
extern const char *const INTRO_LINES[INTRO_LINE_COUNT];

void Game_Init(Game *g, int startWing, bool directStart, bool autotest);
void Game_NewGame(Game *g, int wing);       /* start a run at this wing */
bool Game_LoadWing(Game *g, int wing);      /* false if the wing file is missing/invalid */
bool Game_LoadSanctum(Game *g);             /* the ending scene after wing 5 */
void Game_Update(Game *g, const Input *in, float dt);
void Game_Draw(Game *g);                    /* draws into the virtual screen */
int  Game_Autotest(Game *g);                /* returns the process exit code */
int  Game_Tour(Game *g);                    /* --tour screenshots; returns the exit code */
void Game_Shutdown(Game *g);

const char *Game_WingTitle(int wing);       /* "THE SECOND BELL" */
/* Main menu entries: fills labels + actions (0 new game, 1 continue, 2 quit), returns the count. */
int Game_MenuOptions(const Game *g, int actions[4], const char *labels[4]);

#endif
