/* Wishes: now and then a passenger asks for something on the way, and the mod checks whether it happens.
   Only what the mod can see: the speed, the driving (jolts), a stop at a place. (Music and windows are not
   something the mod can check, so there are no wishes about them.)

   0.5.1: a wish lasts until the destination, as a real passenger means it (no timers any more):
   - slower:  under a speed for the rest of the ride; the speed sign shows it. Failed after 2 s clearly over it
              (10 km/h; 8 s to slow down first)
   - smooth:  no jolts for the rest of the ride; the first slip gets a remark, the second fails it
   - faster:  reach a speed before the destination (fans, the hurried, trips)
   - floor it: in a supercar or hypercar: no speed limit for the rest of the ride (the end-of-restrictions sign);
              done when you reach the speed they want to feel (180 km/h, 110 mph)
   - stop:    stop at a place near the way for 4 s; the GPS goes there and then on to the destination
   Done: satisfaction +6 and thanks; not done: -5. One wish per ride, two for VIPs and weddings. */
#include "jobs_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cars.h"

#define WISH_REWARD 6.0f
#define WISH_PENALTY 5.0f

void wish_ride_start(TsCore *c) {
    c->wish = TS_WISH_NONE;
    c->wish_count = 0;
    c->wish_stop_stage = 0;
    c->next_wish_t = c->t + 35.0 + 40.0 * random_unit(c);
    c->floorit = 0;
    c->wish_slips = 0;
    c->wish_over_since = 0.0;
}

static void wish_end(TsCore *c, int done) {
    if (done) {
        c->sat = clampf(c->sat + WISH_REWARD, 0.0f, 100.0f);
        c->wishes_ok++;
        say(c, 1, c->wish == TS_WISH_STOP ? L_STOP_DONE : c->wish == TS_WISH_FLOORIT ? L_FLOORIT_DONE : L_WISH_THANKS, 1);
    } else {
        c->sat = clampf(c->sat - WISH_PENALTY, 0.0f, 100.0f);
        c->wishes_failed++;
        say(c, 3, L_WISH_FAILED, 1);
    }
    if (c->wish == TS_WISH_STOP) set_nav(c, ts_places[c->job.dest].pos);
    c->wish = TS_WISH_NONE;
    c->wish_stop_stage = 0;
    c->wish_count++;
    c->next_wish_t = c->t + 60.0 + 40.0 * random_unit(c);
}

/* a place for a quick stop: close to the car, not much of a detour */
static int stop_place(TsCore *c) {
    const float *dest = ts_places[c->job.dest].pos;
    float to_dest = ts_distance_xz(c->car.pos, dest);
    int pick[64], n = 0;
    for (unsigned i = 0; i < ts_place_count && n < 64; ++i) {
        if ((int)i == c->job.dest || (int)i == c->job.pickup) continue;
        float d = ts_distance_xz(c->car.pos, ts_places[i].pos);
        if (d < 300.0f || d > 1500.0f) continue;
        if (ts_distance_xz(ts_places[i].pos, dest) > to_dest + 300.0f) continue; /* not backwards */
        pick[n++] = (int)i;
    }
    return n ? pick[random_below(c, n)] : -1;
}

/* 0.5.1: a modern supercar or hypercar (the Autoshow's "super" group without the old racers and the Gurkha) */
static int supercar(const TsCore *c) {
    if (c->car_group != TS_GROUP_SUPER || c->car_index < 0 || c->car_index >= (int)ts_car_count) return 0;
    const TsCarInfo *car = &ts_cars[c->car_index];
    return car->year >= 1985 && strcmp(car->make, "Terradyne") != 0;
}

static void wish_begin(TsCore *c) {
    int type = c->job.ptype, max = c->job.max_kmh > 0 ? c->job.max_kmh : 120;
    int options[8], n = 0;
    options[n++] = TS_WISH_SMOOTH;
    if (max >= 100 && c->speed > 16.0f && !c->floorit) options[n++] = TS_WISH_SLOWER;
    if ((type == TS_P_FAN || type == TS_P_HURRY || type == TS_P_TOURIST) && !c->floorit) options[n++] = TS_WISH_FASTER;
    if (type != TS_P_HURRY && type != TS_P_VIP) options[n++] = TS_WISH_STOP;
    /* 0.5.1: a supercar makes people want to feel it (car fans most of all); not the calm ones, not a wedding */
    if (supercar(c) && !c->floorit && type != TS_P_RELAXED && type != TS_P_WEDDING && type != TS_P_BUSINESS) {
        options[n++] = TS_WISH_FLOORIT;
        if (type == TS_P_FAN || type == TS_P_TOURIST) options[n++] = TS_WISH_FLOORIT, options[n++] = TS_WISH_FLOORIT;
    }
    int wish = options[random_below(c, n)];
    if (wish == TS_WISH_STOP) {
        c->wish_place = stop_place(c);
        if (c->wish_place < 0) wish = TS_WISH_SMOOTH;
    }
    c->wish = wish;
    c->wish_start = c->t;
    switch (wish) {
    case TS_WISH_SLOWER:
        c->wish_limit = max <= 100 ? 60 : 70;
        c->job.max_kmh = c->wish_limit; /* the sign shows it from now on, and it counts like the passenger's limit */
        c->wish_over_since = 0.0;
        say(c, 0, L_WISH_SLOWER, 1);
        break;
    case TS_WISH_SMOOTH:
        c->wish_slips = 0;
        c->wish_was_harsh = 1; /* a jolt going on right now doesn't count */
        say(c, 0, L_WISH_SMOOTH, 1);
        break;
    case TS_WISH_FASTER:
        c->wish_limit = max >= 140 ? 120 : 100;
        say(c, 1, L_WISH_FASTER, 1);
        break;
    case TS_WISH_FLOORIT:
        c->wish_limit = 180;
        c->floorit = 1; /* no limit for the rest of the ride, even if the speed isn't reached */
        say(c, 1, L_WISH_FLOORIT, 1);
        break;
    case TS_WISH_STOP: {
        c->wish_stop_stage = 1;
        set_nav(c, ts_places[c->wish_place].pos);
        /* the detour's time on top of the limit */
        float there = ts_distance_xz(c->car.pos, ts_places[c->wish_place].pos);
        c->job.limit_s += (there * 2.0f * TS_ROAD_FACTOR) / TS_UBER_AVG_SPEED + 20.0f;
        say(c, 0, L_WISH_STOP, 1);
        break;
    }
    }
}

void wish_step(TsCore *c) {
    if (c->mode != TS_UBER || c->stage != TS_UBER_TO_DEST) return;
    if (c->job.pool && c->pool_stage < 2) return; /* 0.6: a shared ride: wishes only once the first got out */
    if (c->wish == TS_WISH_NONE) {
        if (c->extras_off) return;
        int most = c->job.ptype == TS_P_VIP || c->job.ptype == TS_P_WEDDING ? 2 : 1;
        if (c->wish_count >= most || c->t < c->next_wish_t || c->t < c->chat_cooldown) return;
        if (ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) < 900.0f) return; /* too close to the end */
        if (random_unit(c) < 0.6f) wish_begin(c);
        else c->next_wish_t = c->t + 60.0;
        return;
    }
    double since = c->t - c->wish_start;
    switch (c->wish) {
    case TS_WISH_SLOWER: { /* until the destination (wish_arrived_at_dest says thanks) */
        int clearly = c->speed * 3.6f > (float)unit_kmh(c, c->wish_limit) + 10.0f;
        if (since < 8.0 || !clearly) c->wish_over_since = 0.0;
        else if (c->wish_over_since <= 0.0) c->wish_over_since = c->t;
        else if (c->t - c->wish_over_since >= 2.0) {
            if (c->wish_slips++ == 0) say(c, 3, L_WISH_SLIP, 1), c->wish_over_since = 0.0; /* one reminder */
            else wish_end(c, 0);
        }
        break;
    }
    case TS_WISH_SMOOTH: /* until the destination; the first slip gets a remark */
        if (c->was_harsh && !c->wish_was_harsh && since > 3.0) {
            if (c->wish_slips++ == 0) say(c, 3, L_WISH_SLIP, 1);
            else wish_end(c, 0);
        }
        if (c->wish == TS_WISH_SMOOTH) c->wish_was_harsh = c->was_harsh;
        break;
    case TS_WISH_FASTER:
    case TS_WISH_FLOORIT:
        if (c->speed * 3.6f >= (float)unit_kmh(c, c->wish_limit)) wish_end(c, 1);
        break;
    case TS_WISH_STOP: {
        const float *at = ts_places[c->wish_place].pos;
        if (c->wish_stop_stage == 1) {
            if (arrived_at(c, at) && c->speed < TS_STOPPED_SPEED) c->wish_stop_stage = 2, c->wish_stopped_t = c->t;
        } else {
            if (c->speed > TS_LEAVE_SPEED) {
                c->wish_stop_stage = 1; /* drove on too early */
                say(c, 3, L_NOT_IN_YET, 1);
            } else if (c->t - c->wish_stopped_t >= 4.0) {
                wish_end(c, 1);
            }
        }
        break;
    }
    }
}

void wish_arrived_at_dest(TsCore *c) {
    if (c->wish != TS_WISH_NONE) wish_end(c, c->wish == TS_WISH_SLOWER || c->wish == TS_WISH_SMOOTH);
}

void wish_chip(const TsCore *c, char *out, size_t size) {
    out[0] = 0;
    if (c->mode != TS_UBER || c->wish == TS_WISH_NONE) return;
    /* 0.5.1: no timer: a wish holds until the destination */
    switch (c->wish) {
    case TS_WISH_SLOWER:
        snprintf(out, size, "%s %d", T(c, "Bis Ziel: unter", "Till end: under"), ts_speed_shown(c, c->wish_limit)); /* the sign shows the unit */
        break;
    case TS_WISH_SMOOTH: snprintf(out, size, "%s", T(c, "Bis Ziel: ganz sanft", "Till end: smooth")); break;
    case TS_WISH_FASTER:
        snprintf(out, size, "%s %d %s", T(c, "Wunsch: einmal", "Wish: hit"), ts_speed_shown(c, c->wish_limit), ts_speed_unit(c));
        break;
    case TS_WISH_FLOORIT:
        snprintf(out, size, "%s %d %s", T(c, "Vollgas!", "Floor it!"), ts_speed_shown(c, c->wish_limit), ts_speed_unit(c));
        break;
    case TS_WISH_STOP:
        snprintf(out, size, "%s", c->wish_stop_stage == 2 ? T(c, "Kurzer Halt ...", "Quick stop ...") : T(c, "Wunsch: kurz halten", "Wish: quick stop"));
        break;
    }
}
