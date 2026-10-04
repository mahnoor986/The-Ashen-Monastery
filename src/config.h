/* config.h - every tunable value of Blackthorn Manor lives here.
 * Change numbers here to rebalance the game; no other file should hold magic numbers. */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------- window */
#define SCREEN_W        1280            /* virtual screen width (the window is scaled to fit) */
#define SCREEN_H        720             /* virtual screen height */
#define WINDOW_TITLE    "The Ashen Monastery"
#define TARGET_FPS      60
#define MAX_DT          0.05f           /* frame dt is clamped to this (avoids huge steps after a stall) */
#define AUTOTEST_FRAMES 90              /* frames simulated per wing in --autotest */

/* ------------------------------------------------------------ PS1 look */
#define POST_W              640         /* the 3D scene is rendered at this low resolution */
#define POST_H              360
#define POST_GRADE_STRENGTH 1.0f        /* 0 = original colours, 1 = full moonlit split-tone grade */
#define POST_GRAIN          0.04f       /* film grain strength */
#define POST_DITHER         1           /* 1 = PS1 5-bit colour with ordered dither */
#define PS1_WOBBLE          1           /* 1 = snap vertices to a coarse grid (PS1 wobble) */
#define PS1_WOBBLE_GRID_W   320.0f      /* grid the vertices snap to (coarser = more wobble) */
#define PS1_WOBBLE_GRID_H   180.0f

/* --------------------------------------------------------------- palette */
#define COL_BONE        (Color){ 226, 220, 200, 255 }   /* bone white: body text */
#define COL_BLOOD       (Color){ 150,  18,  28, 255 }   /* blood red: danger, titles */
#define COL_GOLD        (Color){ 232, 184,  88, 255 }   /* candle gold: highlights, treasures */
#define COL_NEARBLACK   (Color){   6,   8,  15, 255 }   /* background: deep indigo (matches FOG_COLOR) */

/* ----------------------------------------------------------------- fonts */
#define TEXTURE_DIR     "assets/textures"   /* optional Poly Haven photo textures */
#define TEXTURE_DARKEN  (-35)               /* brightness change applied to them (-255..255) */
#define FONT_DIR        "assets/fonts"            /* searched recursively for the files below */
#define FONT_TITLE_NAME "PirataOne-Regular.ttf"   /* titles (falls back to raylib's font) */
#define FONT_BODY_NAME  "CrimsonText-Regular.ttf" /* body text (falls back to raylib's font) */
#define FONT_TITLE_SIZE 96              /* pixel size the title font is rasterised at */
#define FONT_BODY_SIZE  36              /* pixel size the body font is rasterised at */

/* ----------------------------------------------------------------- world */
#define WORLD_MAX_W     56              /* max wing map width in cells */
#define WORLD_MAX_H     40              /* max wing map height in cells */
#define ROOM_HEIGHT     6.0f            /* rooms are this tall (vault apex / flat ceiling) */
#define CORRIDOR_HEIGHT 3.5f            /* corridors and doorways */
#define VAULT_SPRING    4.3f            /* ribbed vaults start curving at this height */
#define VAULT_MIN_ROOM  8               /* rooms of 8x8 cells or more get ribbed vaults */
#define PLINTH_HEIGHT   0.4f            /* stone base along the walls */
#define PLINTH_DEPTH    0.08f
#define CORNICE_HEIGHT  0.3f            /* moulding at the top of the walls */
#define CORNICE_DEPTH   0.15f
#define BOOKSHELF_HEIGHT 2.5f           /* 'B' walls: shelves up to here */
#define ARCH_SEGMENTS   7               /* segments per side of a pointed arch */
#define ARCH_SPRING_MIN 2.2f            /* arches never start curving lower than this */
#define ARCH_MAX_WIDTH  4.0f            /* wider openings get a straight lintel instead */
#define CHUNK_SIZE      16              /* cells per mesh chunk side */
#define MAX_CHUNKS      ((WORLD_MAX_W / CHUNK_SIZE + 1) * (WORLD_MAX_H / CHUNK_SIZE + 1))
#define MAX_WORLD_PARTS (MAX_CHUNKS * 20)   /* meshes: one per (chunk, material) */
#define MAX_TORCHES     256             /* light sources */
#define MAX_FLAMES      512
#define MAX_PROPS       512
#define MAX_COLLIDERS   384
#define MAX_WINDOWS     64
#define WINDOW_SPACING  5               /* a window every ~5 cells along exterior walls */
#define MOON_R          0.55f           /* cold moonlight through the windows */
#define MOON_G          0.65f
#define MOON_B          0.90f
#define WINDOW_LIGHT_RADIUS 6.5f
#define MAX_CHESTS      8
#define MAX_EXIT_CELLS  4
#define MAX_SPAWNS      64
#define MAX_ROOMS       32
#define ROOM_MIN_OPEN   4               /* a floor cell inside an open 4x4 area belongs to a room */
#define ISLAND_MAX_CELLS 16             /* free-standing wall groups up to this size don't split rooms */
#define TORCH_HEIGHT    2.3f            /* flame height above the floor */
#define TORCH_RADIUS    7.0f            /* how far baked torch light reaches */
#define TORCH_INTENSITY 1.7f            /* brightness of one torch right next to it */
#define TORCH_COLOR_R   1.00f           /* warm amber candle/torch light */
#define TORCH_COLOR_G   0.68f
#define TORCH_COLOR_B   0.35f
#define MAX_LIGHTS      16              /* per-pixel lights per frame (the nearest visible ones) */
#define MAX_DYN_LIGHTS  4               /* + moving lights (boss glow, relics...) */
#define LIGHT_VIEW_RANGE 22.0f          /* lights farther than this from the camera are ignored */
#define LIGHT_FADE_SPEED 4.0f           /* lights fade in/out over 1/4 s instead of popping */
#define CHAR_AMBIENT    0.06f           /* characters get a little extra light (readable silhouettes) */
#define FLOOR_SPECULAR  0.35f           /* polished flagstones catch the lights */
#define FLAME_LIGHT_SCALE 0.72f         /* overall brightness of candle/torch/fire light */
#define PLAYER_LIGHT_R  0.30f           /* soft, slightly cool light around the player */
#define PLAYER_LIGHT_G  0.32f
#define PLAYER_LIGHT_B  0.38f
#define PLAYER_LIGHT_HEIGHT 2.4f        /* light sits above, between the player and the camera */

/* ---------------------------------------------------------------- camera */
#define CAM_FOVY            62.0f       /* vertical field of view in degrees */
#define CAM_DISTANCE        3.6f        /* distance behind the player */
#define CAM_HEAD_HEIGHT     1.5f        /* camera looks at this height above the feet */
#define CAM_SHOULDER        0.4f        /* sideways offset to the right shoulder */
#define CAM_PITCH_MIN       (-55.0f)    /* degrees, most downward look */
#define CAM_PITCH_MAX       15.0f       /* degrees, most upward look */
#define CAM_PITCH_DEFAULT   (-14.0f)    /* degrees, starting pitch */
#define CAM_MIN_Y           0.2f        /* camera never goes below this */
#define PROP_CAMERA_HEIGHT  2.4f        /* solid props block the camera below this height */
#define CAM_CEILING_MARGIN  0.45f       /* camera stays this far below the local ceiling (and its beams) */
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

/* ------------------------------------------------------ wand: Red Lightning */
#define WAND_COOLDOWN       0.45f       /* seconds between casts */
#define WAND_RANGE          12.0f       /* how far the lightning reaches */
#define WAND_DAMAGE         1
#define WAND_KNOCKBACK      4.0f        /* units/s, decays */
#define AIM_ASSIST_RANGE    WAND_RANGE  /* casting snaps to the nearest visible enemy this close */
#define CAST_POSE_TIME      0.2f        /* the arm points at the target this long */
#define BEAM_TIME           0.15f       /* the lightning bolt stays visible this long */
#define BEAM_SEGMENTS       10          /* jagged segments per bolt */
#define FLASH_RADIUS        6.0f        /* red light at the hit point lights up the walls */

/* ---------------------------------------------------------------- chests */
#define CHEST_HOLD_TIME     1.5f        /* seconds to hold E */
#define CHEST_USE_RANGE     1.6f        /* how close you must be */

/* --------------------------------------------------------------- enemies */
#define ENEMY_WINDUP        0.5f        /* telegraph time before an attack lands */
#define ENEMY_HEAR_RANGE    3.0f        /* enemies notice you this close regardless of view cone */
#define ENEMY_FOV_DEG       110.0f      /* view cone width */
#define MONK_HP         2
#define MONK_SPEED      2.2f
#define MONK_SIGHT      9.0f
#define MONK_REACH      1.4f
#define WRAITH_HP            2
#define WRAITH_SPEED         1.6f
#define WRAITH_SIGHT         8.0f
#define WRAITH_SCARE_RANGE   7.0f
#define WRAITH_ALPHA         0.45f
#define PRIEST_HP            3
#define PRIEST_SPEED         2.0f
#define PRIEST_SIGHT         11.0f
#define PRIEST_MIN_DIST      5.0f
#define PRIEST_MAX_DIST      8.0f
#define PRIEST_FIRE_TIME     2.2f
#define BOLT_SPEED          5.0f
#define ABBOT_HP            20
#define ABBOT_SPEED         1.8f
#define ABBOT_WINDUP        0.6f
#define ABBOT_RING_TIME     3.5f
#define MAX_ENEMIES         64
#define MAX_BOLTS           128

/* -------------------------------------------------------------- enemies 2 */
#define ENEMY_SIZE          0.6f        /* collision box of normal enemies */
#define ENEMY_MIN_LIGHT     0.55f       /* enemies never get darker than this (readable silhouettes) */
#define MONK_COOLDOWN   1.1f        /* pause after a melee attack */
#define WRAITH_REACH         1.2f
#define WRAITH_COOLDOWN      1.3f
#define WRAITH_FAINT_ALPHA   0.10f       /* how a ghost looks while out of the light */
#define WRAITH_LIGHT_NEEDED  0.22f       /* torch light level that reveals a ghost */
#define WRAITH_MOAN_RANGE    10.0f       /* ghosts moan now and then within this distance */
#define PRIEST_WINDUP        0.5f        /* telegraph before a bolt */
#define ABBOT_SIGHT         14.0f
#define ABBOT_REACH         2.1f
#define ABBOT_SIZE          1.0f
#define ABBOT_COOLDOWN      1.4f
#define ABBOT_KNOCK_MUL     0.3f        /* the Queen barely flinches */
#define BOLT_LIFE           6.0f
#define BOLT_HIT_RADIUS     0.45f
#define BOLT_HEIGHT         1.2f

/* ------------------------------------------------------------------ feel */
#define SHAKE_HIT           0.7f        /* camera shake amounts (0..1) */
#define SHAKE_CHEST         0.35f
#define SHAKE_DOOR          0.6f
#define SHAKE_SCARE         0.9f
#define SHAKE_DECAY         2.2f        /* per second */
#define SHAKE_SIZE          0.12f       /* world units at shake 1.0 */
#define HURT_FLASH_TIME     0.5f
#define BANNER_TIME         3.2f        /* seconds a banner stays up */
#define FADE_SPEED          1.4f        /* screen fade per second */
#define RESPAWN_INVULN      2.0f        /* grace period after rising again */
#define DOOR_OPEN_TIME      2.0f        /* seconds for the exit door to sink */
#define CHEST_FACE_DEG      80.0f       /* how directly you must face a chest */
#define CONTROLS_HINT_TIME  20.0f       /* seconds the controls hint shows in wing 1 */
#define MAX_PARTICLES       256
#define INTRO_LINE_TIME     4.2f        /* seconds each intro line stays up */

/* ------------------------------------------------------------ atmosphere */
#define ASH_COUNT           300         /* grey ash flakes drifting around the camera */
#define ASH_RANGE           9.0f        /* half size of the volume they wrap around in */
#define ASH_FALL_SPEED      0.45f
#define ASH_WIND_X          0.30f       /* sideways drift */
#define ASH_WIND_Z          0.12f
#define EMBER_COUNT         60          /* orange embers rising from torches */
#define EMBER_RANGE         14.0f       /* only torches this close to the camera spark */
#define BELL_TOLL_MIN       25.0f       /* a distant bell tolls every 25-40 s (less often as bells break) */
#define BELL_TOLL_MAX       40.0f
#define LIGHTNING_ENABLED   1           /* 0 = no lightning storms */
#define LIGHTNING_MIN       20.0f       /* seconds between lightning strikes */
#define LIGHTNING_MAX       45.0f
#define LIGHTNING_STORMY    0.45f       /* wing 5: intervals x this (frequent lightning) */
#define DUST_PER_WINDOW     6           /* drifting dust motes in each moonlight shaft */

/* ----------------------------------------------------------------- audio */
#define MASTER_VOLUME       0.9f
#define AMBIENCE_FILE       "assets/audio/dungeon_ambient_1.ogg"
#define AMBIENCE_VOLUME     0.35f
#define FOOTSTEP_INTERVAL   0.4f        /* seconds between footsteps at full speed */

/* --------------------------------------------------------------- minimap */
#define MINIMAP_W           230.0f      /* corner minimap size in pixels */
#define MINIMAP_H           170.0f
#define MINIMAP_CELLS       24.0f       /* cells visible across the minimap */
#define MAP_REVEAL_RANGE    7.0f        /* cells this close (and in sight) get discovered */

/* ------------------------------------------------------------------ save */
#define SAVE_FILE           "save.txt"  /* highest unlocked wing (1-5) */

/* ------------------------------------------------------------ wing names */
/* For the inventory (the in-game title comes from the first line of each wing file). */
#define WING_NAME_TABLE { "The Ash Gate", "The Hall of Prayer", "The Scriptorium", \
                          "The Ossuary", "The Bell Tower" }

/* ------------------------------------------------------------ ward seals */
/* Ward Seals: shown in the HUD list and the "Ward Seal found" banner, in map order (top row first). */
#define TREASURE_TABLE {                                                                             \
    { "Seal of Cinders", "Seal of the Iron Key", "Seal of the Pilgrim" },                            \
    { "Seal of Hymns", "Seal of the Kneeling Saint", "Seal of Candlewax", "Seal of Silence" },        \
    { "Seal of Ink", "Seal of the Burned Page", "Seal of the Quill", "Seal of Forgotten Names" },     \
    { "Seal of Bone", "Seal of the Nameless", "Seal of Marrow", "Seal of the Last Rite", "Seal of Dust" }, \
    { "Seal of Embers", "Seal of the Rope", "Seal of the Toll", "Seal of the Abbot's Ring", "Seal of the Red Flame" }, \
}

/* --------------------------------------------------------------- sanctum */
#define SANCTUM_FILE        "assets/wings/sanctum.txt"
#define MAX_NPCS            16
#define NPC_TALK_RANGE      2.0f        /* friends speak when you come this close */
/* lighting for the Sanctum (same fields as a wing; only the mood fields are used):
 * warm dark amber fog, golden ambient, bright candles */
#define SANCTUM_CONFIG { SANCTUM_FILE, 0, 1.0f, 1.0f, 1.0f, 0.045f, 9.0f, 0.30f, \
                         { 0.10f, 0.06f, 0.02f }, { 1.25f, 1.00f, 0.65f }, 1.3f }

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
    float fog[3];              /* fog / background colour */
    float tint[3];             /* ambient light colour */
    float lightMul;            /* brightness of candles, torches and fires */
} WingConfig;

/* Edit the numbers here. (The array itself is created in game.c from this macro, because a
 * static array in a header would be copied into every .c file.) */
#define WING_TABLE {                                                                                  \
    /* file                     chests speed  sight fire   fog    light ambient  fog colour              ambient tint            lights */ \
    /* 1: cold moonlit blue, many candles (easiest to see) */                                       \
    { "assets/wings/wing1.txt", 3,     1.00f, 0.8f, 1.0f,  0.050f, 7.0f, 0.30f, { 0.025f, 0.030f, 0.060f }, { 0.70f, 0.85f, 1.30f }, 1.15f }, \
    /* 2: warm gold candlelight, crimson banners */                                                 \
    { "assets/wings/wing2.txt", 4,     1.05f, 0.9f, 1.1f,  0.055f, 6.5f, 0.26f, { 0.045f, 0.030f, 0.035f }, { 0.95f, 0.82f, 0.88f }, 1.0f }, \
    /* 3: amber reading lamps, green-tinted shadows */                                              \
    { "assets/wings/wing3.txt", 4,     1.10f, 1.0f, 1.2f,  0.065f, 6.0f, 0.25f, { 0.025f, 0.040f, 0.032f }, { 0.70f, 0.95f, 0.80f }, 1.15f }, \
    /* 4: sickly pale green-cyan, few lights, dense fog */                                          \
    { "assets/wings/wing4.txt", 5,     1.20f, 1.1f, 1.3f,  0.090f, 5.5f, 0.21f, { 0.030f, 0.055f, 0.055f }, { 0.65f, 1.00f, 0.95f }, 0.95f }, \
    /* 5: stormy blue, frequent lightning, red light around the boss */                             \
    { "assets/wings/wing5.txt", 5,     1.25f, 1.2f, 1.4f,  0.075f, 5.0f, 0.19f, { 0.020f, 0.025f, 0.055f }, { 0.65f, 0.80f, 1.35f }, 1.00f }, \
}
extern const WingConfig WINGS[WING_COUNT];

#endif
