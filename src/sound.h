#ifndef OPENPAIRS_SOUND_H
#define OPENPAIRS_SOUND_H

#include <stdbool.h>

// Procedural sound effects, synthesized at startup from square / swept / noise
// waveforms (no audio files). Sound is disabled by default.

typedef enum {
    SFX_DEAL = 0,    // a new board is dealt
    SFX_FLIP,        // a card turns face up
    SFX_MATCH,       // the second card matches
    SFX_MISS,        // the two cards differ and turn back
    SFX_WIN,         // the last pair is found
    SFX_MENU_MOVE,   // menu cursor moved
    SFX_MENU_SELECT, // menu item chosen
    SFX_COUNT,
} SfxId;

void sound_init(void);       // open the audio device and synthesize all effects
void sound_shutdown(void);   // free effects and close the audio device
bool sound_is_enabled(void);
void sound_toggle(void);
void sound_play(SfxId id);   // no-op when sound is disabled

#endif
