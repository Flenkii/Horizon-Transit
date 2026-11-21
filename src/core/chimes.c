/* Horizon Transit 0.5.1: when the speed warning's tones play (the glue plays them, see TS_WARN_ in jobs.h).

   Yellow (a little too fast): one soft tone, once. It comes again only after the speed was back under the limit
   for TS_WARN_REARM_S, so hovering around the limit doesn't make it chime again and again.
   Red (clearly too fast): "ding-ding" every TS_WARN_RING_S for as long as the sign is red; red -> yellow stops it
   without the yellow tone (you're slowing down). */
#include "jobs.h"

void ts_speed_warn_init(TsSpeedWarn *w) {
    w->level = 0;
    w->armed = 1;
    w->below_t = -1e9;
    w->next_ring = 0.0;
}

int ts_speed_warn(TsSpeedWarn *w, int level, double t) {
    int was = w->level, out = TS_WARN_NONE;
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    w->level = level;
    if (level == 0) {
        if (was != 0) w->below_t = t;
        if (t - w->below_t >= TS_WARN_REARM_S) w->armed = 1;
        return was == 2 ? TS_WARN_STOP : TS_WARN_NONE;
    }
    if (level == 2) {
        w->armed = 0;
        if (was != 2 || t >= w->next_ring) {
            /* back in red right after leaving it: keep the rhythm instead of ringing twice in a row */
            if (was != 2 && t < w->next_ring) return TS_WARN_NONE;
            w->next_ring = t + TS_WARN_RING_S;
            return TS_WARN_RED;
        }
        return TS_WARN_NONE;
    }
    /* yellow */
    if (was == 2) out = TS_WARN_STOP;
    else if (w->armed) out = TS_WARN_YELLOW;
    w->armed = 0;
    return out;
}
