/* config.h - every tunable value of Blackthorn Manor lives here.
 * Change numbers here to rebalance the game; no other file should hold magic numbers. */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------- window */
#define SCREEN_W        1280            /* virtual screen width (the window is scaled to fit) */
#define SCREEN_H        720             /* virtual screen height */
#define WINDOW_TITLE    "Blackthorn Manor"
#define TARGET_FPS      60
#define MAX_DT          0.05f           /* frame dt is clamped to this (avoids huge steps after a stall) */
#define AUTOTEST_FRAMES 90              /* frames simulated per wing in --autotest */

/* --------------------------------------------------------------- palette */
#define COL_BONE        (Color){ 226, 220, 200, 255 }   /* bone white: body text */
#define COL_BLOOD       (Color){ 150,  18,  28, 255 }   /* blood red: danger, titles */
#define COL_GOLD        (Color){ 232, 184,  88, 255 }   /* candle gold: highlights, treasures */
#define COL_NEARBLACK   (Color){   5,   5,   9, 255 }   /* background / fog (matches FOG_COLOR) */

/* ----------------------------------------------------------------- fonts */
#define FONT_DIR        "assets/fonts"            /* searched recursively for the files below */
#define FONT_TITLE_NAME "PirataOne-Regular.ttf"   /* titles (falls back to raylib's font) */
#define FONT_BODY_NAME  "CrimsonText-Regular.ttf" /* body text (falls back to raylib's font) */
#define FONT_TITLE_SIZE 96              /* pixel size the title font is rasterised at */
#define FONT_BODY_SIZE  36              /* pixel size the body font is rasterised at */

/* ----------------------------------------------------------------- world */
#define WORLD_MAX_W     56              /* max wing map width in cells */
#define WORLD_MAX_H     40              /* max wing map height in cells */
#define WALL_HEIGHT     4.0f            /* walls are 4 blocks tall; ceiling sits on top */
#define CHUNK_SIZE      16              /* cells per mesh chunk side */
#define MAX_CHUNKS      ((WORLD_MAX_W / CHUNK_SIZE + 1) * (WORLD_MAX_H / CHUNK_SIZE + 1))
#define MAX_TORCHES     128
#define MAX_CHESTS      8
#define MAX_EXIT_CELLS  4
#define MAX_SPAWNS      64
#define TORCH_HEIGHT    2.3f            /* flame height above the floor */
#define TORCH_RADIUS    7.0f            /* how far baked torch light reaches */
#define TORCH_INTENSITY 1.7f            /* brightness of one torch right next to it */
#define TORCH_COLOR_R   1.00f           /* warm orange torch light */
#define TORCH_COLOR_G   0.55f
#define TORCH_COLOR_B   0.30f
#define FOG_COLOR_R     0.02f           /* fog fades to this near-black */
#define FOG_COLOR_G     0.02f
#define FOG_COLOR_B     0.035f
#define PLAYER_LIGHT_R  0.55f           /* soft warm light carried by the player */
#define PLAYER_LIGHT_G  0.45f
#define PLAYER_LIGHT_B  0.36f
#define PLAYER_LIGHT_HEIGHT 1.7f        /* light sits above the player's head */

/* ---------------------------------------------------------------- camera */
#define CAM_FOVY            62.0f       /* vertical field of view in degrees */
#define CAM_DISTANCE        3.6f        /* distance behind the player */
#define CAM_HEAD_HEIGHT     1.5f        /* camera looks at this height above the feet */
#define CAM_SHOULDER        0.4f        /* sideways offset to the right shoulder */
#define CAM_PITCH_MIN       (-55.0f)    /* degrees, most downward look */
#define CAM_PITCH_MAX       15.0f       /* degrees, most upward look */
#define CAM_PITCH_DEFAULT   (-14.0f)    /* degrees, starting pitch */
#define CAM_MIN_Y           0.2f        /* camera never goes below this */
#define CAM_MAX_Y           3.8f        /* camera never goes above this (ceiling is 4.0) */
#define CAM_WALL_MARGIN     0.18f       /* keep this far from walls so the near plane doesn't clip */
#define CAM_RETURN_SPEED    4.0f        /* how fast the camera eases back out after a wall pushed it in */
#define MOUSE_SENSITIVITY   0.0035f     /* radians per pixel of mouse movement */
#define KEY_TURN_SPEED      2.6f        /* radians/s when turning the camera with arrow keys */

/* ---------------------------------------------------------------- player */
#define PLAYER_SPEED        4.5f        /* units per second */
#define PLAYER_ACCEL        14.0f       /* how quickly velocity reaches target (1/s) */
#define PLAYER_TURN_SPEED   12.0f       /* how fast the body turns toward movement (rad/s) */
#define PLAYER_SIZE         0.6f        /* collision box width/depth */
#define PLAYER_MAX_HEARTS   5
#define PLAYER_INVULN       1.0f        /* seconds of invulnerability after a hit */
#define DASH_SPEED          12.0f
#define DASH_TIME           0.15f
#define DASH_COOLDOWN       0.8f

/* ----------------------------------------------------------------- sword */
#define SWORD_ACTIVE        0.25f       /* seconds the swing can hit */
#define SWORD_COOLDOWN      0.40f       /* seconds between swings */
#define SWORD_RANGE         2.0f        /* reach from the player center */
#define SWORD_ARC_DEG       130.0f      /* width of the hit arc in front */
#define SWORD_DAMAGE        1
#define SWORD_KNOCKBACK     4.0f        /* units/s, decays */
#define AIM_ASSIST_RANGE    3.0f        /* swinging snaps to the nearest enemy within this range */

/* ---------------------------------------------------------------- chests */
#define CHEST_HOLD_TIME     1.5f        /* seconds to hold E */
#define CHEST_USE_RANGE     1.6f        /* how close you must be */

/* --------------------------------------------------------------- enemies */
#define ENEMY_WINDUP        0.5f        /* telegraph time before an attack lands */
#define ENEMY_HEAR_RANGE    3.0f        /* enemies notice you this close regardless of view cone */
#define ENEMY_FOV_DEG       110.0f      /* view cone width */
#define SKELETON_HP         2
#define SKELETON_SPEED      2.2f
#define SKELETON_SIGHT      9.0f
#define SKELETON_REACH      1.4f
#define GHOST_HP            2
#define GHOST_SPEED         1.6f
#define GHOST_SIGHT         8.0f
#define GHOST_SCARE_RANGE   7.0f
#define GHOST_ALPHA         0.45f
#define WITCH_HP            3
#define WITCH_SPEED         2.0f
#define WITCH_SIGHT         11.0f
#define WITCH_MIN_DIST      5.0f
#define WITCH_MAX_DIST      8.0f
#define WITCH_FIRE_TIME     2.2f
#define BOLT_SPEED          5.0f
#define QUEEN_HP            20
#define QUEEN_SPEED         1.8f
#define QUEEN_WINDUP        0.6f
#define QUEEN_RING_TIME     3.5f
#define MAX_ENEMIES         64
#define MAX_BOLTS           128

/* ----------------------------------------------------------------- wings */
#define WING_COUNT 5

typedef struct {
    const char *file;          /* wing map file */
    int   chests;              /* number of chests the map must contain */
    float enemySpeedMul;       /* scales enemy speed */
    float sightMul;            /* scales enemy sight range */
    float witchFireMul;        /* scales witch fire rate */
    float fogDensity;          /* exponential-squared fog density */
    float playerLightRadius;   /* radius of the soft light around the player */
    float ambient;             /* base light level */
} WingConfig;

/* Edit the numbers here. (The array itself is created in game.c from this macro, because a
 * static array in a header would be copied into every .c file.) */
#define WING_TABLE {                                                                  \
    /* file                     chests speed  sight fire   fog    light ambient */      \
    { "assets/wings/wing1.txt", 3,     1.00f, 0.8f, 1.0f,  0.06f, 7.0f, 0.20f },        \
    { "assets/wings/wing2.txt", 4,     1.05f, 0.9f, 1.1f,  0.08f, 6.5f, 0.17f },        \
    { "assets/wings/wing3.txt", 4,     1.10f, 1.0f, 1.2f,  0.10f, 6.0f, 0.14f },        \
    { "assets/wings/wing4.txt", 5,     1.20f, 1.1f, 1.3f,  0.13f, 5.0f, 0.10f },        \
    { "assets/wings/wing5.txt", 5,     1.25f, 1.2f, 1.4f,  0.16f, 4.5f, 0.07f },        \
}
extern const WingConfig WINGS[WING_COUNT];

#endif
