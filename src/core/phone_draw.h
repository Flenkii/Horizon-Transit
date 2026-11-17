#ifndef TS_PHONE_DRAW_H
#define TS_PHONE_DRAW_H
/* The phone as a picture: a smartphone (frame, camera island, status bar) with the Horizon Transit app on its
   screen, drawn from what ts_hud says the phone shows. Plain C, no Windows: the shapes are drawn here into a
   BGRA buffer, the texts go to a callback (GDI in the game, a preview script in the tests). */
#include <stdint.h>
#include "jobs.h"

enum { TS_THEME_NIGHT = 0, TS_THEME_LIGHT, TS_THEME_FESTIVAL, TS_THEME_OCEAN, TS_THEME_CARBON, TS_THEME_RIVIERA, TS_THEME_GOLD, TS_THEMES };

enum { TS_TEXT_LEFT = 0, TS_TEXT_CENTER, TS_TEXT_RIGHT };

typedef struct {
    const char *utf8;
    int x, y, w, h;   /* the box, in pixels of the buffer; the text sits vertically centred in it */
    int size;         /* pixel height of the font */
    int weight;       /* 400 normal, 600 semibold, 700 bold */
    uint32_t color;   /* 0xRRGGBB */
    int align;        /* TS_TEXT_ */
    int wrap;         /* nonzero: word-wrapped from the box's top, up to its height */
} TsPhoneText;

typedef struct {
    uint32_t *px;     /* w*h pixels, 0xAARRGGBB, top row first; must start all zero */
    int w, h;
    void (*text)(void *user, const TsPhoneText *t);
    /* the width in pixels the text needs (t->w, t->x and t->align are ignored); NULL = estimated */
    int (*measure)(void *user, const TsPhoneText *t);
    void *user;
} TsPhoneCanvas;

/* the buffer's size at this scale (1 = a 1080-line picture), shadow margin included */
void ts_phone_size(float scale, int *w, int *h);
/* draws the phone; clock is "18:56" */
void ts_phone_draw(const TsHud *h, int theme, float scale, const char *clock, TsPhoneCanvas *cv);
/* after the texts: makes the buffer premultiplied alpha (the phone opaque, a soft shadow round it) */
void ts_phone_finish(int theme, float scale, TsPhoneCanvas *cv);

const char *ts_theme_name(int theme, int german);

/* The speed-limit sign (white disc, red ring, the number) shown at the top left while a passenger rides.
   `unit` ("km/h" / "mph") goes small under the number; `flash` = the passenger thinks it's too fast (yellow). */
void ts_sign_size(float scale, int *w, int *h);
void ts_sign_draw(int number, const char *unit, int flash, float scale, TsPhoneCanvas *cv);
void ts_sign_finish(float scale, TsPhoneCanvas *cv); /* after the texts: premultiplied alpha, a soft shadow */

/* The ride panel by the speedometer (0.4.2): the speed-limit sign, the G-meter (a circle: inside = the passenger is
   fine, the dot = how hard you brake, accelerate and corner), the time left and the passenger's mood, the wish. */
void ts_dash_size(float scale, int *w, int *h);
void ts_dash_draw(const TsHud *h, float scale, TsPhoneCanvas *cv);
void ts_dash_finish(float scale, TsPhoneCanvas *cv); /* after the texts */

/* The passengers' lines in an own window (0.4.2), for when the platform's HUD layer is off: the newest at the bottom. */
#define TS_FEED_LINES 3
typedef struct {
    char name[64];
    char text[TS_TEXT];
    int style; /* as TsEvent.style */
} TsFeedLine;
void ts_feed_size(float scale, int *w, int *h);
void ts_feed_draw(const TsFeedLine *lines, int n, float scale, TsPhoneCanvas *cv);
void ts_feed_finish(float scale, TsPhoneCanvas *cv); /* after the texts */

#endif
