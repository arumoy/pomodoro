// sound.h - Audio: the end-of-phase ding chime and the looping work sound.
#ifndef TOMATO_SOUND_H
#define TOMATO_SOUND_H

#include <windows.h>

// Play the short end-of-phase "ding bell" chime (non-blocking).
void PlayDingTune(void);

// Start / stop looping the configured work-sound .wav (async).
void StartWorkSound(void);
void StopWorkSound(void);

// Reconcile work-sound playback with the current timer state: it should loop
// only while the timer is running AND in the WORK phase. Call at every state
// change (start, stop, phase transition).
void UpdateWorkSound(void);

#endif // TOMATO_SOUND_H
