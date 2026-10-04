/* audio.c - loads the sound effects / ambience if present and plays them (see audio.h).
 * Files come from the Kenney "RPG Audio" + "Impact Sounds" packs and OpenGameArt (CREDITS.md). */
#include <stddef.h>
#include <math.h>
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
    [SND_ZAP]      = { { NULL }, 0.75f, 0.08f },     /* generated: see MakeCrack() */
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
    [SND_BELL]     = { { NULL }, 0.55f, 0.03f },     /* generated: see MakeBell() */
    [SND_BELL_BREAK] = { { NULL }, 1.0f, 0.0f },
};

/* SND_BOLT plays pitched up so the knife whoosh sounds like a magic hiss */
static const float BASE_PITCH[SND_COUNT] = { [SND_ZAP] = 1.0f, [SND_BOLT] = 1.6f };

static Sound sounds[SND_COUNT][MAX_VARIANTS];   /* module-private audio resources */
static int   counts[SND_COUNT];
static Music ambience;
static bool  hasAmbience, ready;

/* Red Lightning crack, synthesised: a sharp noise burst + a falling electric whine + a low thump. */
static Sound MakeCrack(void)
{
    const int rate = 44100, n = (int)(44100 * 0.4f);
    short *data = MemAlloc((unsigned int)(n * sizeof(short)));
    unsigned int seed = 1234567u;
    float phase = 0.0f;
    Wave wave;
    Sound s;
    int i;
    for (i = 0; i < n; i++) {
        float t = (float)i / rate, noise, freq, v;
        seed = seed * 1664525u + 1013904223u;
        noise = ((seed >> 9) & 0xffff) / 32768.0f - 1.0f;
        freq = 200.0f + 2400.0f * expf(-t * 10.0f);
        phase += 2.0f * PI * freq / rate;
        v = noise * expf(-t * 18.0f) * 0.85f
          + sinf(phase) * expf(-t * 12.0f) * 0.35f
          + sinf(2.0f * PI * 65.0f * t) * expf(-t * 15.0f) * 0.6f;
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        data[i] = (short)(v * 30000.0f);
    }
    wave.frameCount = (unsigned int)n;
    wave.sampleRate = (unsigned int)rate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data;
    s = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return s;
}

/* A deep bell: a sum of decaying sine partials. `crack` adds a shattering noise burst. */
static Sound MakeBell(float seconds, float crack)
{
    static const float freq[5]  = { 110.0f, 220.0f, 277.0f, 330.0f, 440.0f };
    static const float decay[5] = { 0.9f, 1.3f, 1.8f, 1.1f, 2.6f };
    static const float amp[5]   = { 0.50f, 0.32f, 0.22f, 0.20f, 0.12f };
    const int rate = 22050, n = (int)(22050 * seconds);
    short *data = MemAlloc((unsigned int)(n * sizeof(short)));
    unsigned int seed = 777u;
    Wave wave;
    Sound s;
    int i, k;
    for (i = 0; i < n; i++) {
        float t = (float)i / rate, v = 0.0f, noise;
        for (k = 0; k < 5; k++) v += amp[k] * expf(-t * decay[k]) * sinf(2.0f * PI * freq[k] * t);
        v *= fminf(1.0f, t * 200.0f);                              /* soft strike */
        seed = seed * 1664525u + 1013904223u;
        noise = ((seed >> 9) & 0xffff) / 32768.0f - 1.0f;
        v += crack * noise * expf(-t * 6.0f);                      /* metal breaking */
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        data[i] = (short)(v * 28000.0f);
    }
    wave.frameCount = (unsigned int)n;
    wave.sampleRate = (unsigned int)rate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data;
    s = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return s;
}

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
    sounds[SND_ZAP][0] = MakeCrack();
    if (IsSoundValid(sounds[SND_ZAP][0])) counts[SND_ZAP] = 1;
    sounds[SND_BELL][0] = MakeBell(4.0f, 0.0f);
    if (IsSoundValid(sounds[SND_BELL][0])) counts[SND_BELL] = 1;
    sounds[SND_BELL_BREAK][0] = MakeBell(2.5f, 0.7f);
    if (IsSoundValid(sounds[SND_BELL_BREAK][0])) counts[SND_BELL_BREAK] = 1;

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

void Audio_SetCalm(bool calm)
{
    if (!ready || !hasAmbience) return;
    SetMusicVolume(ambience, calm ? AMBIENCE_VOLUME * 0.45f : AMBIENCE_VOLUME);
    SetMusicPitch(ambience, calm ? 0.75f : 1.0f);
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
