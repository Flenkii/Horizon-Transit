/* The bus timetable: when the bus is due at each stop of the line, counted from the first stop.

   Each leg's driving time is its road distance at the bus's average speed (TS_BUS_AVG_SPEED), times the
   time-limit setting, plus 30 s at the stop. At each stop the delay is the arrival against the plan:
   up to a minute late counts as on time (+1 satisfaction, now and then a happy remark), more is late.
   The clock is the mod's own (game seconds since the line started): the game's time of day is not read. */
#include "jobs_internal.h"
#include <math.h>
#include <stdio.h>

#define TT_DWELL 30.0f
#define TT_ON_TIME 60.0f

static float leg_seconds(const TsCore *c, const TsLine *line, unsigned i) {
    /* 0.5: the line may run backwards (only the running line; the legs are the same length both ways) */
    int a = line->stops[i - 1], b = line->stops[i];
    if (c->mode == TS_BUS && line == &ts_lines[c->line]) a = ts_bus_stop(c, c->line, (int)i - 1), b = ts_bus_stop(c, c->line, (int)i);
    float m = place_distance(a, b) * TS_ROAD_FACTOR;
    return m / TS_BUS_AVG_SPEED * c->time_factor + TT_DWELL;
}

void tt_start(TsCore *c) {
    const TsLine *line = &ts_lines[c->line];
    unsigned n = line->stop_count < 24 ? line->stop_count : 24;
    c->plan[0] = 0.0f;
    for (unsigned i = 1; i < n; ++i) c->plan[i] = c->plan[i - 1] + leg_seconds(c, line, i);
    c->plan_started = 0;
    c->stops_on_time = c->stops_late = 0;
    c->last_delay = 0.0f;
}

float tt_line_minutes(const TsCore *c, int index) {
    const TsLine *line = &ts_lines[index];
    float s = 0.0f;
    for (unsigned i = 1; i < line->stop_count; ++i) s += leg_seconds(c, line, i);
    return s / 60.0f;
}

float tt_arrived(TsCore *c) {
    if (c->stop_i == 0) {
        c->plan_t0 = c->t;
        c->plan_started = 1;
        return 0.0f;
    }
    if (!c->plan_started || c->stop_i >= 24) return 0.0f;
    float delay = (float)(c->t - c->plan_t0) - c->plan[c->stop_i];
    c->last_delay = delay;
    if (delay <= TT_ON_TIME) {
        c->stops_on_time++;
        c->on_time_total++;
        c->sat = clampf(c->sat + 1.0f, 0.0f, 100.0f);
        if (c->onboard > 0 && random_unit(c) < 0.25f) say(c, 1, L_BUS_ON_TIME, 0);
    } else {
        c->stops_late++;
        c->late_total++;
    }
    return delay;
}

void tt_chip(const TsCore *c, char *out, size_t size) {
    out[0] = 0;
    if (c->mode != TS_BUS || !c->plan_started || c->stop_i >= 24) return;
    char time[16];
    if (c->stage == TS_BUS_TO_STOP) {
        double due = c->plan_t0 + c->plan[c->stop_i] - c->t;
        format_time(time, sizeof(time), due);
        if (due >= -TT_ON_TIME) snprintf(out, size, "%s %s", T(c, "Fahrplan: in", "Due in"), due > 0 ? time : "0:00");
        else snprintf(out, size, "%s +%s", T(c, "Fahrplan:", "Timetable:"), time);
    } else {
        format_time(time, sizeof(time), fabsf(c->last_delay));
        if (c->stop_i == 0) snprintf(out, size, "%s", T(c, "Fahrplan läuft", "Timetable running"));
        else if (c->last_delay <= TT_ON_TIME) snprintf(out, size, "%s", T(c, "Pünktlich", "On time"));
        else snprintf(out, size, "%s +%s", T(c, "Verspätung", "Late"), time);
    }
}
