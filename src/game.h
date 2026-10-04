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
#include "relics.h"

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
    float   gravity;      /* units/s^2 pulling down (negative = rises: smoke, embers) */
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

    /* Soul Relics, the serpent's cage and the serpent (relics.c) */
    bool      relicDestroyed[WING_COUNT];
    bool      relicActive;             /* this wing's relic is floating, waiting to be destroyed */
    Vector3   relicPos;                /* the chest it rose from */
    float     relicTime, relicShake;
    int       relicHits;
    bool      cageExists, cageBroken;
    Vector3   cagePos;
    int       cageHits;
    float     cageHitTime, cageBurst;
    bool      cageHint, shieldHint;
    Serpent   serpent;
    bool      showcase;                /* autotest: draw all five relics in a row */
    Vector3   showcasePos;
    float     showcaseYaw;

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
    int       menuPhase;               /* title: 0 reveal, 1 menu, 2 controls, 3 credits, 4 leaving */
    int       menuAction;              /* MENU_* chosen (acted on after the fade) */
    float     menuTime;                /* seconds in the current title phase */
    float     titleTime;               /* seconds since the title screen appeared */
    bool      titleBell;
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

/* Small services for relics.c */
void Game_Shake(Game *g, float amount);
void Game_Banner(Game *g, const char *title, const char *sub, Color color, float duration);
void Game_Particles(Game *g, Vector3 pos, int count, Color color, float speed, float size, float life, float gravity);
void Game_HurtPlayer(Game *g, Vector3 from);
void Game_CheckWingComplete(Game *g);
/* Title menu entries: fills labels + actions (MENU_*), returns the count. */
enum { MENU_NEW = 0, MENU_CONTINUE, MENU_QUIT, MENU_CONTROLS, MENU_CREDITS };
int Game_MenuOptions(const Game *g, int actions[6], const char *labels[6]);

#endif
