#ifndef TS_VOICE_H
#define TS_VOICE_H
/* Passengers read out loud with the Windows speech voices (SAPI 5, the voices under Settings > Time & language >
   Speech that SAPI can use, e.g. Hedda for German, David and Zira for English). Optional ("Voices", off by default).
   Speaking runs on its own thread, so the game never waits for it. */

typedef void (*VoiceLogFn)(const char *format, ...);

/* starts the speech thread the first time it is needed; 0 = Windows speech could not be started */
int voice_start(VoiceLogFn log);
/* queues a line (UTF-8); german picks a German voice, female/pitch/rate shape it (pitch and rate -10..10) */
void voice_say(const char *utf8, int german, int female, int pitch, int rate);
/* stops what is being said and drops the queue */
void voice_stop(void);
/* 0..100 */
void voice_volume(int volume);

#endif
