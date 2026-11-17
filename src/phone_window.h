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
/* the speed-limit sign at the top left (number in the shown unit); want = 0 hides it. Shown only while the
   phone isn't. Any thread, cheap. */
void phone_window_sign(int number, const char *unit, int over, int want);
/* the ride panel by the speedometer (sign, G-meter, time, mood, wish: ts_dash_draw); want = 0 hides it.
   right/bottom: its distance from the picture's right and bottom edge, in pixels of a 1080p picture. */
void phone_window_dash(const TsHud *h, int want);
void phone_window_dash_place(int right, int bottom);
/* a passenger's line in the own feed window (bottom left, above the minimap), for when the HUD layer is off;
   it stays 8 s (longer for long lines), the newest 3 are shown */
void phone_window_feed(const char *name, const char *text, int style);
/* 1 while the window exists (else use the HUD list) */
int phone_window_ok(void);
/* 1 when the phone is on screen now */
int phone_window_visible(void);

#endif
