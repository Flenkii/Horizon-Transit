/* 0.13: the ride book (every job with its date, kept by the glue in a file) and a target picked on the app's map.

   The app shows the book without the game: earnings per day and week, and each ride. The glue tells the core the
   time (wall_time, unix seconds) and saves the book when book_serial moves. */
#include "jobs_internal.h"
#include <stdio.h>
#include <string.h>

void ts_book_add(TsCore *c, const TsBookEntry *e) {
    memmove(&c->book[1], &c->book[0], sizeof(TsBookEntry) * (TS_BOOK - 1));
    c->book[0] = *e;
    if (c->book_count < TS_BOOK) c->book_count++;
    c->book_serial++;
}

void service_book(TsCore *c, int bus, int who, int from, int to, int pay, int tip, int stars, int late) {
    TsBookEntry e;
    memset(&e, 0, sizeof(e));
    e.when = c->wall_time;
    e.bus = (short)bus, e.who = (short)who, e.from = (short)from, e.to = (short)to;
    e.car = (short)(c->car_index >= 0 ? c->car_index + 1 : 0);
    e.stars = (unsigned char)(stars < 0 ? 0 : stars > 5 ? 5 : stars), e.late = (unsigned char)(late != 0);
    e.pay = pay, e.tip = tip > 0 ? tip : 0;
    e.metres = (int)(c->trip_driven_m + 0.5f);
    e.seconds = (int)(c->t - c->trip_t + 0.5);
    ts_book_add(c, &e);
}

/* one line: when bus who from to car stars late pay tip metres seconds */
int ts_book_format(const TsBookEntry *e, char *out, size_t size) {
    int n = snprintf(out, size, "%lld %d %d %d %d %d %d %d %d %d %d %d", e->when, e->bus, e->who, e->from, e->to, e->car, e->stars,
                     e->late, e->pay, e->tip, e->metres, e->seconds);
    return n > 0 && (size_t)n < size;
}

int ts_book_parse(const char *line, TsBookEntry *e) {
    long long when;
    int v[11];
    if (!line || sscanf(line, "%lld %d %d %d %d %d %d %d %d %d %d %d", &when, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7],
                        &v[8], &v[9], &v[10]) != 12)
        return 0;
    int bus = v[0] != 0, who = v[1], from = v[2], to = v[3];
    if (bus ? who < 0 || who >= (int)ts_line_count : who < 0 || who >= TS_PEOPLE_MAX) return 0;
    if (from < 0 || from >= (int)ts_place_count || to < 0 || to >= (int)ts_place_count) return 0;
    if (v[4] < 0 || v[4] > (int)ts_car_count || v[5] < 0 || v[5] > 5 || v[7] < 0) return 0;
    memset(e, 0, sizeof(*e));
    e->when = when < 0 ? 0 : when;
    e->bus = (short)bus, e->who = (short)who, e->from = (short)from, e->to = (short)to, e->car = (short)v[4];
    e->stars = (unsigned char)v[5], e->late = (unsigned char)(v[6] != 0);
    e->pay = v[7], e->tip = v[8] > 0 ? v[8] : 0, e->metres = v[9] > 0 ? v[9] : 0, e->seconds = v[10] > 0 ? v[10] : 0;
    return 1;
}

/* ------------------------------------------------------------------ the target from the app's map */

void ts_set_target(TsCore *c, float x, float z) {
    c->target_on = 1;
    c->target[0] = x, c->target[1] = c->car_ok ? c->car.pos[1] : 0.0f, c->target[2] = z;
    if (c->mode != TS_IDLE) return; /* a job's GPS stays; the target waits for the job's end */
    set_nav(c, c->target);
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%s", T(c, "Das Navi führt dich hin.", "The GPS takes you there."));
    toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Ziel aus der App", "Target from the app"), text);
}

void ts_clear_target(TsCore *c) {
    if (!c->target_on) return;
    c->target_on = 0;
    if (c->mode == TS_IDLE && c->nav_on && !c->lost.state) {
        c->nav_on = 0;
        c->nav_serial++;
        c->gps_arrived = 0;
    }
}

/* while no job runs: the GPS to the target, and it's done there */
void service_target_step(TsCore *c) {
    if (!c->target_on || c->mode != TS_IDLE || c->lost.state) return;
    if (!c->nav_on || c->nav_target[0] != c->target[0] || c->nav_target[2] != c->target[2]) set_nav(c, c->target); /* back after a job */
    if (arrived_at(c, c->target)) {
        ts_clear_target(c);
        toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Ziel erreicht", "Target reached"), T(c, "Du bist da.", "You're there."));
    }
}
