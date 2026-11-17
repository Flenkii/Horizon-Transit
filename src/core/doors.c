/* 0.4.3: the passenger's door when getting in and out (Uber rides).

   When the car stands at the pickup, a door opens (rear left, rear right or the front passenger door, picked at
   random from the doors the car has), the open sound plays, a moment later the door closes with its sound, and
   then the ride starts. At the destination the same door does the same again before the ride is paid.

   This file only decides which door and when; the glue (src/mod.c) does each step in the game: the car's own
   door animation (the one the game's showroom plays) and the sound. Pure C, tested in tests/test_core.c. */
#include "jobs_internal.h"

/* seconds from the start of the sequence */
#define DOOR_PREPARE_S 0.0  /* the glue switches the car's animation to its showroom detail */
#define DOOR_OPEN_S 0.5     /* the showroom branch is ready after ~0.25 s (Extra Options' notes: 0.35 s is safe) */
#define DOOR_CLOSE_S 2.3    /* the door stays open ~1.8 s: the passenger gets in or out */
#define DOOR_SHUT_DEFAULT_S 0.6 /* close -> the close sound (its slam is 0.2 s into the file) */
#define DOOR_AFTER_S 0.6    /* after the close sound, before the car's animation goes back and the ride goes on */

static const char *const door_names[TS_DOORS] = {"doorLF", "doorRF", "doorLR", "doorRR"};

const char *ts_door_name(int door) {
    return door >= 0 && door < TS_DOORS ? door_names[door] : "";
}

int ts_door_front_passenger(int driver_right) {
    return driver_right ? TS_DOOR_LF : TS_DOOR_RF;
}

static double shut_s(const TsCore *c) {
    return c->door_shut_s > 0.05f && c->door_shut_s < 3.0f ? (double)c->door_shut_s : DOOR_SHUT_DEFAULT_S;
}

double ts_door_total_s(const TsCore *c) {
    return c->doors_on ? DOOR_CLOSE_S + shut_s(c) + DOOR_AFTER_S : 0.0;
}

/* A four-door car: rear left, rear right or the front passenger door, as the passenger likes. A two-door car (no rear
   doors): the front passenger door. Doors not known (the car's animations weren't read): as a four-door car, for the
   sound. */
int ts_door_pick(TsCore *c) {
    int front = ts_door_front_passenger(c->driver_right);
    int mask = c->door_mask;
    int choices[3], n = 0;
    if (!mask || (mask >> TS_DOOR_LR & 1)) choices[n++] = TS_DOOR_LR;
    if (!mask || (mask >> TS_DOOR_RR & 1)) choices[n++] = TS_DOOR_RR;
    if (!mask || (mask >> front & 1)) choices[n++] = front;
    if (!n) return front; /* only the driver's door (or none): the sound still plays */
    return choices[random_below(c, n)];
}

static void send(TsCore *c, int step) {
    c->door_step = step;
    event(c, TS_EV_DOOR, 0, step, NULL, NULL);
    c->events[c->event_count - 1].style = c->door;
}

void ts_door_start(TsCore *c) {
    if (!c->doors_on) return;
    if (c->door < 0 || c->door >= TS_DOORS) c->door = ts_door_pick(c);
    c->door_t0 = c->t;
    c->door_step = 0;
    send(c, TS_DOOR_PREPARE);
}

void ts_door_step(TsCore *c) {
    if (!c->door_step || c->door_step == TS_DOOR_DONE) return;
    double t = c->t - c->door_t0, shut = DOOR_CLOSE_S + shut_s(c);
    /* one step per call at most, in order (an update can come late: the next one follows on the next call) */
    if (c->door_step == TS_DOOR_PREPARE && t >= DOOR_OPEN_S) send(c, TS_DOOR_OPEN);
    else if (c->door_step == TS_DOOR_OPEN && t >= DOOR_CLOSE_S) send(c, TS_DOOR_CLOSE);
    else if (c->door_step == TS_DOOR_CLOSE && t >= shut) send(c, TS_DOOR_SHUT);
    else if (c->door_step == TS_DOOR_SHUT && t >= shut + DOOR_AFTER_S) send(c, TS_DOOR_DONE);
}

void ts_door_abort(TsCore *c) {
    if (!c->door_step || c->door_step == TS_DOOR_DONE) return;
    if (c->door_step == TS_DOOR_OPEN) send(c, TS_DOOR_CLOSE);
    if (c->door_step == TS_DOOR_CLOSE) send(c, TS_DOOR_SHUT);
    send(c, TS_DOOR_DONE);
}
