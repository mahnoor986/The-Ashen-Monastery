/* audio.c - loads the sound effects / ambience if present and plays them (see audio.h).
 * Files come from the Kenney "RPG Audio" + "Impact Sounds" packs and OpenGameArt (CREDITS.md). */
#include <stddef.h>
#include "raylib.h"
#include "audio.h"
#include "config.h"

#define MAX_VARIANTS 10
#define RPG     "assets/audio/kenney_rpg/Audio/"
#define IMPACT  "assets/audio/kenney_impact/Audio/"
#define MOANS   "assets/audio/qubodup-GhostMoans/wav/"

typedef struct {
    const char *files[MAX_VARIANTS];
    float volume;                    /* base volume of this role */
    float pitchVar;                  /* +- random pitch variation */
} SoundRole;

/* Best matching file(s) for every sound role (see CLAUDE.md section 9). */
static const SoundRole ROLES[SND_COUNT] = {
    [SND_SWING]    = { { RPG "drawKnife1.ogg", RPG "drawKnife2.ogg", RPG "drawKnife3.ogg" }, 0.55f, 0.10f },
    [SND_HIT]      = { { IMPACT "impactPlate_medium_000.ogg", IMPACT "impactPlate_medium_001.ogg",
                         IMPACT "impactPlate_medium_002.ogg" }, 0.70f, 0.08f },
    [SND_HURT]     = { { IMPACT "impactPunch_heavy_000.ogg", IMPACT "impactPunch_heavy_001.ogg",
                         IMPACT "impactPunch_heavy_002.ogg" }, 0.90f, 0.06f },
    [SND_CHEST]    = { { RPG "creak1.ogg", RPG "creak2.ogg", RPG "creak3.ogg" }, 0.80f, 0.05f },
    [SND_TREASURE] = { { RPG "handleCoins.ogg", RPG "handleCoins2.ogg" }, 0.90f, 0.03f },
    [SND_DOOR]     = { { RPG "doorOpen_1.ogg", RPG "doorOpen_2.ogg" }, 1.00f, 0.0f },
    [SND_STEP]     = { { RPG "footstep00.ogg", RPG "footstep01.ogg", RPG "footstep02.ogg", RPG "footstep03.ogg",
                         RPG "footstep04.ogg", RPG "footstep05.ogg", RPG "footstep06.ogg", RPG "footstep07.ogg",
                         RPG "footstep08.ogg", RPG "footstep09.ogg" }, 0.25f, 0.08f },
    [SND_SCARE]    = { { "assets/audio/scaryhighpitchedghost.wav", "assets/audio/scaryhighpitchedghost.ogg" }, 0.55f, 0.0f },
    [SND_MOAN]     = { { MOANS "qubodup-GhostMoan01.wav", MOANS "qubodup-GhostMoan02.wav", MOANS "qubodup-GhostMoan03.wav",
                         MOANS "qubodup-GhostMoan04.wav", MOANS "qubodup-GhostMoan05.wav" }, 0.45f, 0.05f },
    [SND_BOLT]     = { { RPG "knifeSlice.ogg", RPG "knifeSlice2.ogg" }, 0.60f, 0.10f },
    [SND_BOLT_HIT] = { { IMPACT "impactGlass_light_000.ogg", IMPACT "impactGlass_light_001.ogg",
                         IMPACT "impactGlass_light_002.ogg" }, 0.55f, 0.10f },
    [SND_DEATH]    = { { IMPACT "impactWood_heavy_000.ogg", IMPACT "impactWood_heavy_001.ogg",
                         IMPACT "impactWood_heavy_002.ogg" }, 0.80f, 0.08f },
    [SND_CLICK]    = { { RPG "metalClick.ogg" }, 0.60f, 0.0f },
    [SND_DASH]     = { { RPG "cloth1.ogg", RPG "cloth2.ogg", RPG "cloth3.ogg" }, 0.60f, 0.10f },
};

/* SND_BOLT plays pitched up so the knife whoosh sounds like a magic hiss */
static const float BASE_PITCH[SND_COUNT] = { [SND_SWING] = 1.0f, [SND_BOLT] = 1.6f };

static Sound sounds[SND_COUNT][MAX_VARIANTS];   /* module-private audio resources */
static int   counts[SND_COUNT];
static Music ambience;
static bool  hasAmbience, ready;

void Audio_Init(bool enabled)
{
    int r, v;
    ready = false;
    hasAmbience = false;
    if (!enabled) return;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    ready = true;
    SetMasterVolume(MASTER_VOLUME);

    for (r = 0; r < SND_COUNT; r++) {
        counts[r] = 0;
        for (v = 0; v < MAX_VARIANTS && ROLES[r].files[v]; v++) {
            Sound s;
            if (!FileExists(ROLES[r].files[v])) continue;
            s = LoadSound(ROLES[r].files[v]);
            if (IsSoundValid(s)) sounds[r][counts[r]++] = s;
        }
    }
    if (FileExists(AMBIENCE_FILE)) {
        ambience = LoadMusicStream(AMBIENCE_FILE);
        hasAmbience = IsMusicValid(ambience);
        if (hasAmbience) {
            ambience.looping = true;
            SetMusicVolume(ambience, AMBIENCE_VOLUME);
            PlayMusicStream(ambience);
        }
    }
}

void Audio_Shutdown(void)
{
    int r, v;
    if (!ready) return;
    for (r = 0; r < SND_COUNT; r++)
        for (v = 0; v < counts[r]; v++) UnloadSound(sounds[r][v]);
    if (hasAmbience) UnloadMusicStream(ambience);
    CloseAudioDevice();
    ready = false;
}

void Audio_Update(void)
{
    if (ready && hasAmbience) UpdateMusicStream(ambience);
}

void Audio_Play(SoundId id, float volume)
{
    Sound s;
    float pitch;
    if (!ready || (int)id < 0 || id >= SND_COUNT || counts[id] == 0) return;
    s = sounds[id][GetRandomValue(0, counts[id] - 1)];
    pitch = (BASE_PITCH[id] > 0.0f ? BASE_PITCH[id] : 1.0f)
          + ROLES[id].pitchVar * (GetRandomValue(-100, 100) / 100.0f);
    SetSoundPitch(s, pitch);
    SetSoundVolume(s, ROLES[id].volume * volume);
    PlaySound(s);
}
