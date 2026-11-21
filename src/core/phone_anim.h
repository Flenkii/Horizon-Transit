#ifndef TS_PHONE_ANIM_H
#define TS_PHONE_ANIM_H
/* 0.9.1: the phone's animations, plain C (the window thread runs them on its clock, the tests check them):
   - the map glides from one zoom to the next (where it looks, how far, how it is turned) instead of jumping;
     in navi it turns smoothly with the car; the blue dot turns into the navi arrow and back
   - nothing pops up: when what the screen shows changes (a tab, a row, a pin), the old picture fades into the new one
   Times are seconds on any steady clock. */
#include <stdint.h>
#include "jobs.h"

typedef struct {
    int on, keys;           /* a map was shown last time, and which (map_keys) */
    float x, z, w, h;       /* what is shown */
    float tx, tz, tw, th;   /* last time's target */
    float fx, fz, fw, fh;   /* a glide's start */
    int gliding;
    double t0, last;
    float rot, head, navi;  /* the turn, the car's heading, the navi blend: eased */
    float tab;              /* the tabs' pill */
    int tabs, tab_on;
    double tab_last;
} TsMapAnim;

#define TS_MAP_GLIDE_S 0.6f

/* rewrites h's map view (map_x/z/w/h, map_rot, map_heading, map_navi_t) to the eased one; 1 while it still moves */
int ts_map_anim(TsMapAnim *a, TsHud *h, double now);

/* the open tab's pill slides to the new tab (h->tab_slide); 1 while it moves. off = the phone isn't shown */
int ts_tab_anim(TsMapAnim *a, TsHud *h, double now);

/* what the screen shows, as a number: a change of it fades (rows, tabs, the selection; not the ticking values) */
uint32_t ts_screen_key(const TsHud *h);
/* the map's pins (not where the car is): a change fades the map too */
uint32_t ts_pins_key(const TsHud *h);
/* the ride panel's content (not its ticking numbers) */
uint32_t ts_dash_key(const TsHud *h);

/* a crossfade from the picture shown before to the new one (premultiplied 0xAARRGGBB buffers of the same size) */
typedef struct {
    uint32_t *from, *raw; /* the old picture; the new one as drawn */
    int w, h, cap;
    int on;
    double t0;
    float secs;
    int keep[2][4];       /* x0, y0, x1, y1: these parts show the new picture at once (the map while it glides, the tabs
                             while their pill slides); x1 <= x0 = none */
} TsFader;

/* the picture `shown` (w x h) is about to change: keep it to fade from. 0 = no memory (then no fade) */
int ts_fader_start(TsFader *f, const uint32_t *shown, int w, int h, double now, float secs);
/* the new picture was drawn into px: keep a copy (call after each redraw while a fade may run) */
void ts_fader_drawn(TsFader *f, const uint32_t *px, int w, int h);
/* while it fades: writes the blend into px and returns 1; done (or none): px holds the new picture, returns 0 */
int ts_fader_apply(TsFader *f, uint32_t *px, int w, int h, double now);
void ts_fader_free(TsFader *f);

/* two premultiplied pixels mixed: t 0 = a, 256 = b */
static inline uint32_t ts_px_mix(uint32_t a, uint32_t b, int t) {
    uint32_t rb = ((a & 0x00FF00FFu) * (uint32_t)(256 - t) + (b & 0x00FF00FFu) * (uint32_t)t) >> 8 & 0x00FF00FFu;
    uint32_t ag = ((a >> 8 & 0x00FF00FFu) * (uint32_t)(256 - t) + (b >> 8 & 0x00FF00FFu) * (uint32_t)t) & 0xFF00FF00u;
    return rb | ag;
}

float ts_ease_inout(float t); /* 0..1, slow at both ends */
float ts_ease_out(float t);   /* 0..1, slows at the end */

#endif
