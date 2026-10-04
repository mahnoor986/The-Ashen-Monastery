/* audio.h - sound effects and the ambience loop. Every file is optional: a missing sound
 * simply stays silent. */
#ifndef AUDIO_H
#define AUDIO_H

typedef enum {
    SND_ZAP = 0,     /* red lightning crack (generated in code) */
    SND_HIT,         /* sword hits an enemy */
    SND_HURT,        /* player takes a hit */
    SND_CHEST,       /* chest lid creaks open */
    SND_TREASURE,    /* treasure found (coins) */
    SND_DOOR,        /* exit door opens */
    SND_STEP,        /* footstep */
    SND_SCARE,       /* ghost shriek */
    SND_MOAN,        /* ghost moan */
    SND_BOLT,        /* witch fires a hex bolt */
    SND_BOLT_HIT,    /* hex bolt shatters */
    SND_DEATH,       /* enemy crumbles */
    SND_CLICK,       /* menu click */
    SND_DASH,        /* dash */
    SND_BELL,        /* distant bell toll (generated in code) */
    SND_BELL_BREAK,  /* a cursed bell shatters (generated in code) */
    SND_COUNT
} SoundId;

void Audio_Init(bool enabled);              /* enabled = false: stay silent (autotest) */
void Audio_Shutdown(void);
void Audio_Update(void);                    /* keeps the ambience stream fed; call every frame */
void Audio_Play(SoundId id, float volume);  /* random variant + small random pitch change */

#endif
