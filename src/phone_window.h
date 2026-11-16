#ifndef TS_PHONE_WINDOW_H
#define TS_PHONE_WINDOW_H
/* The phone on screen: a see-through, click-through window laid over the left of the game's picture, showing
   the picture core/phone_draw.c draws. It never takes the focus, and shows only while the game is in front.
   (Own code; the idea of a layered window over the game is the one Gripline's cards use.) */
#include "core/jobs.h"

typedef void (*PhoneLogFn)(const char *format, ...);

/* starts the window's thread */
void phone_window_start(PhoneLogFn log);
/* what to show; want = 0 hides it. Any thread, cheap (the picture is drawn on the window's thread). */
void phone_window_show(const TsHud *h, int theme, int want);
/* 1 while the window exists (else use the HUD list) */
int phone_window_ok(void);
/* 1 when the phone is on screen now */
int phone_window_visible(void);

#endif
