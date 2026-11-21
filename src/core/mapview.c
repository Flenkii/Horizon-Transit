/* 0.7: the phone's map, the core's part: what the map shows (pins, the route, the region) and its rows and keys.
   - the Map tab: you (the blue dot), the towns, the festival, the hotspots; Enter zooms (near, the region, everything)
   - the preview: Enter on a ride shows where you are, where the passenger waits and where they want to go;
     on a bus line all its stops in order. Then "take it" or "back" (Backspace); Left/Right: the route / everything
   - during a job: a second tab "Map" with the way to the next target and the rest of the route
   The ways along the roads are drawn by phone_draw.c (map.c finds them on the picture). */
#include "jobs_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int bus_direction(const TsCore *c, int line);

#define HOT_RADIUS_M 1500 /* as market.c */

static TsMapPin *pin(TsHud *h, int kind, int route, const float pos[3], const char *label) {
    if (h->map_pin_count >= TS_MAP_PINS) return NULL;
    TsMapPin *p = &h->map_pin[h->map_pin_count++];
    memset(p, 0, sizeof(*p));
    p->x = pos[0], p->z = pos[2];
    p->kind = (signed char)kind, p->route = (signed char)route;
    if (label) snprintf(p->label, sizeof(p->label), "%s", label);
    return p;
}

static void place_pin(const TsCore *c, TsHud *h, int kind, int route, int place, int with_label) {
    if (place < 0 || place >= (int)ts_place_count) return;
    pin(h, kind, route, ts_places[place].pos, with_label ? ts_place_name(c, place) : NULL);
}

/* the towns, the festival, the hotspots: the map's own names */
static void base_pins(const TsCore *c, TsHud *h, int hotspots) {
    for (unsigned i = 0; i < ts_place_count; ++i)
        if (ts_places[i].kind == TS_PLACE_FESTIVAL || ts_places[i].kind == TS_PLACE_OUTPOST)
            place_pin(c, h, ts_places[i].kind == TS_PLACE_FESTIVAL ? TS_PIN_FESTIVAL : TS_PIN_TOWN, TS_ROUTE_NONE, (int)i, 1);
    if (!hotspots || c->extras_off) return;
    for (int k = 0; k < 2; ++k) {
        if (c->hot[k].place < 0 || c->t >= c->hot[k].until) continue;
        char sx[16];
        market_surge_text(c, market_surge(c, c->hot[k].place), sx, sizeof(sx));
        TsMapPin *p = pin(h, TS_PIN_HOT, TS_ROUTE_NONE, ts_places[c->hot[k].place].pos, sx);
        if (p) p->num = HOT_RADIUS_M;
    }
}

static void car_pin(const TsCore *c, TsHud *h, int route) {
    if (!c->car_ok) return;
    float at[3] = {roundf(c->car.pos[0]), 0.0f, roundf(c->car.pos[2])};
    pin(h, TS_PIN_CAR, route, at, NULL);
    /* moving: the way the car goes; standing: the way it faces (the game's forward vector, turned round if it
       points backwards) */
    float vx = c->car.vel[0], vz = c->car.vel[2], fs = c->fwd_sign < -0.2f ? -1.0f : 1.0f;
    float hd = vx * vx + vz * vz > 9.0f ? atan2f(vx, vz) : atan2f(fs * c->car.fwd[0], fs * c->car.fwd[2]);
    h->map_heading = roundf(hd * 90.0f) / 90.0f; /* ~0.6 degrees: no redraw for nothing */
}

/* the region: round the pins in the route (and the car), with a margin */
/* which: 0 every pin, 1 the route's pins and the car, 2 only the next target's and the car */
static void fit(TsHud *h, int which) {
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    int n = 0;
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (which == 1 && p->route == TS_ROUTE_NONE && p->kind != TS_PIN_CAR) continue;
        if (which == 2 && p->route != TS_ROUTE_MAIN && p->kind != TS_PIN_CAR) continue;
        if (p->x < x0) x0 = p->x;
        if (p->x > x1) x1 = p->x;
        if (p->z < z0) z0 = p->z;
        if (p->z > z1) z1 = p->z;
        ++n;
    }
    if (!n) {
        h->map_x = 3420.0f, h->map_z = -4230.0f, h->map_w = 22000.0f, h->map_h = 10400.0f;
        return;
    }
    h->map_x = (x0 + x1) * 0.5f, h->map_z = (z0 + z1) * 0.5f;
    h->map_w = (x1 - x0) * 1.25f + 500.0f, h->map_h = (z1 - z0) * 1.25f + 700.0f;
    if (h->map_w < 1400.0f) h->map_w = 1400.0f;
    if (h->map_h < 1400.0f) h->map_h = 1400.0f;
}

static void whole(TsHud *h) {
    h->map_x = 3600.0f, h->map_z = -4500.0f, h->map_w = 15000.0f, h->map_h = 7600.0f; /* the land you can drive */
}

/* 0.9.1: like the game's navigation: turned with the car, you in the lower part, the road ahead in view */
#define NAVI_M 600.0f
static void navi(const TsCore *c, TsHud *h) {
    if (!c->car_ok) {
        whole(h);
        return;
    }
    float a = h->map_heading, ahead = NAVI_M * 0.25f;
    h->map_navi = 1, h->map_rot = a, h->map_navi_t = 1.0f;
    h->map_x = roundf(c->car.pos[0]) + ahead * sinf(a), h->map_z = roundf(c->car.pos[2]) + ahead * cosf(a);
    h->map_w = h->map_h = NAVI_M;
}

static void around_car(const TsCore *c, TsHud *h, float metres) {
    float at[2] = {6150.0f, -5851.0f}; /* the festival when no car */
    if (c->car_ok) at[0] = roundf(c->car.pos[0]), at[1] = roundf(c->car.pos[2]);
    h->map_x = at[0], h->map_z = at[1], h->map_w = metres, h->map_h = metres;
}

/* the nearest town (the map tab's first row) */
static int nearest_town(const TsCore *c) {
    int best = -1;
    float bd = 1e9f;
    if (!c->car_ok) return -1;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        if (ts_places[i].kind > TS_PLACE_OUTPOST) continue;
        float d = ts_distance_xz(c->car.pos, ts_places[i].pos);
        if (d < bd) bd = d, best = (int)i;
    }
    return best;
}

/* a ride's pins: pickup, (the second's pickup, the first's destination), the destination */
static void ride_pins(const TsCore *c, TsHud *h, const TsOffer *o, int stage, int pool_stage) {
    /* stage: TS_UBER_TO_PICKUP before the passenger is in (also the preview), else on the way */
    int first = stage <= TS_UBER_TO_PICKUP;
    if (first) place_pin(c, h, TS_PIN_PICKUP, TS_ROUTE_MAIN, o->pickup, 1);
    if (o->pool) {
        if (pool_stage == 0) place_pin(c, h, TS_PIN_RIDER2, first ? TS_ROUTE_LATER : TS_ROUTE_MAIN, o->pool_pickup, 1);
        if (pool_stage <= 1) {
            place_pin(c, h, TS_PIN_DEST, pool_stage == 1 && !first ? TS_ROUTE_MAIN : TS_ROUTE_LATER, o->dest, 1);
            place_pin(c, h, TS_PIN_DEST, TS_ROUTE_LATER, o->pool_dest, 1);
        } else {
            place_pin(c, h, TS_PIN_DEST, TS_ROUTE_MAIN, o->dest, 1); /* the job is the second's now */
        }
    } else {
        place_pin(c, h, TS_PIN_DEST, first ? TS_ROUTE_LATER : TS_ROUTE_MAIN, o->dest, 1);
    }
}

/* a line's stops in driving order; from = the next stop (the ones before it are done) */
static void line_pins(const TsCore *c, TsHud *h, int line, int rev, int from) {
    const TsLine *l = &ts_lines[line];
    int n = (int)l->stop_count;
    for (int k = 0; k < n; ++k) {
        int place = l->stops[rev ? n - 1 - k : k];
        /* a ring ends where it starts: the last stop isn't a pin of its own */
        if (k == n - 1 && l->stops[0] == l->stops[n - 1] && from < n - 1) {
            TsMapPin *p = pin(h, TS_PIN_STOP, TS_ROUTE_LATER, ts_places[place].pos, NULL);
            if (p) p->num = -1; /* only the line's end, no circle */
            continue;
        }
        int kind = k < from ? TS_PIN_STOP_DONE : k == from ? TS_PIN_STOP_NEXT : TS_PIN_STOP;
        int route = k < from ? (k == 0 ? TS_ROUTE_START : TS_ROUTE_DASHED) : k == from ? TS_ROUTE_MAIN : TS_ROUTE_LATER;
        if (k < from) route = TS_ROUTE_NONE;
        int named = k == 0 || k == n - 1 || k == from;
        TsMapPin *p = pin(h, kind, route, ts_places[place].pos, named ? ts_place_name(c, place) : NULL);
        if (p) p->num = (short)(k + 1);
    }
}

static int bus_locked(const TsCore *c) { return c->bus_needs_bus && c->car_kind != TS_CAR_BUS; }

/* the preview's rows: what it is, then the two buttons */
static void preview_rows(const TsCore *c, TsHud *h) {
    char a[64], b[72], km[24], pk[24];
    if (c->phone_tab == TS_TAB_RIDES) {
        const TsOffer *o = &c->offers[c->phone_sel];
        format_km(c, km, sizeof(km), o->trip_m);
        format_km(c, pk, sizeof(pk), o->pickup_m);
        if (o->pool) snprintf(a, sizeof(a), "%s & %s", ts_passenger_name(o->passenger), ts_passenger_name(o->pool_passenger));
        else if (o->party > 1) snprintf(a, sizeof(a), "%s +%d · %s", ts_passenger_name(o->passenger), o->party - 1, ts_class_name(o->ride_class, c->german));
        else snprintf(a, sizeof(a), "%s · %s", ts_passenger_name(o->passenger), ts_class_name(o->ride_class, c->german));
        snprintf(b, sizeof(b), "%s → %s", pk, km); /* to the pickup, then the ride */
        row(h, a, b, 2);
        snprintf(b, sizeof(b), "%d €", o->pay);
        row(h, T(c, "Fahrt annehmen", "Accept ride"), b, 0);
    } else {
        int li = c->phone_sel < (int)ts_line_count ? c->phone_sel : 0;
        const TsLine *l = &ts_lines[li];
        int n = (int)l->stop_count, rev = bus_direction(c, li);
        snprintf(a, sizeof(a), "%s %s · %s", T(c, "Linie", "Line"), l->number, ts_line_title(c, li));
        snprintf(b, sizeof(b), "%d %s · %d min", n - (l->stops[0] == l->stops[n - 1]), T(c, "Halte", "stops"), (int)(tt_line_minutes(c, li) + 0.5f));
        row(h, a, b, 2);
        snprintf(a, sizeof(a), "%s", ts_place_name(c, l->stops[rev ? n - 1 : 0]));
        row(h, bus_locked(c) ? T(c, "Linie starten (Bus nötig)", "Start line (needs the bus)") : T(c, "Linie starten", "Start line"), a, bus_locked(c) ? 2 : 0);
        if (bus_locked(c)) h->row_heading[h->row_count - 1] = 0; /* still a button: it says why it can't */
    }
    row(h, T(c, "Zurück", "Back"), "", 0);
    h->row_selected = h->row_count - 2 + (c->map_btn ? 1 : 0);
}

void map_hud(const TsCore *c, TsHud *h) {
    if (!c->phone_open) return;
    if (c->mode == TS_IDLE && c->map_view && (c->phone_tab == TS_TAB_RIDES ? c->phone_sel < c->offer_count : c->phone_tab == TS_TAB_BUS)) {
        /* the preview */
        h->map_on = 1, h->map_keys = TS_MAPKEYS_PREVIEW;
        h->row_count = 1; /* keep the app's heading */
        h->row_selected = -1;
        h->row_meter_on = h->row_meter_quality = 0;
        h->footer[0] = 0;
        base_pins(c, h, 1);
        if (c->phone_tab == TS_TAB_RIDES) {
            car_pin(c, h, TS_ROUTE_START);
            ride_pins(c, h, &c->offers[c->phone_sel], TS_UBER_TO_PICKUP, 0);
        } else {
            int li = c->phone_sel < (int)ts_line_count ? c->phone_sel : 0;
            car_pin(c, h, TS_ROUTE_START);
            line_pins(c, h, li, bus_direction(c, li), 0);
        }
        if (c->map_zoom) whole(h);
        else fit(h, 1);
        preview_rows(c, h);
        return;
    }
    if (c->mode == TS_IDLE && c->phone_tab == TS_TAB_MAP) {
        h->map_on = 1, h->map_keys = TS_MAPKEYS_TAB;
        base_pins(c, h, 1);
        car_pin(c, h, TS_ROUTE_NONE);
        char a[64], b[48];
        int town = nearest_town(c);
        if (town >= 0) {
            char km[24];
            format_km(c, km, sizeof(km), ts_distance_xz(c->car.pos, ts_places[town].pos));
            snprintf(a, sizeof(a), "%s %s", T(c, "Bei", "Near"), ts_place_name(c, town));
            snprintf(b, sizeof(b), "%s", km);
        } else {
            snprintf(a, sizeof(a), "%s", T(c, "Position unbekannt: erst losfahren", "Position unknown: drive first"));
            b[0] = 0;
        }
        row(h, a, b, 2);
        static const char *const zde[4] = {"Nah", "Umgebung", "Ganze Karte", "Navi"}, *const zen[4] = {"Near", "Region", "Whole map", "Navi"};
        int z = c->map_zoom % 4;
        row(h, T(c, "Zoom", "Zoom"), c->german ? zde[z] : zen[z], 0);
        h->row_selected = h->row_count - 1;
        if (z == 3) navi(c, h);
        else if (z == 2 || !c->car_ok) whole(h);
        else around_car(c, h, z == 0 ? 3200.0f : 9000.0f);
        return;
    }
    if (c->mode != TS_IDLE && c->job_tab == 1) {
        /* the job's map: the way to the next target, the rest of the route */
        h->map_on = 1, h->map_keys = TS_MAPKEYS_JOB;
        h->row_count = 1;
        h->row_selected = -1;
        h->row_meter_on = h->row_meter_quality = 0;
        base_pins(c, h, 0);
        car_pin(c, h, TS_ROUTE_START);
        char a[64], b[48];
        if (c->mode == TS_UBER) {
            int stage = c->stage <= TS_UBER_TO_PICKUP ? TS_UBER_TO_PICKUP : TS_UBER_TO_DEST;
            ride_pins(c, h, &c->job, stage, c->job.pool ? c->pool_stage : 0);
        } else {
            line_pins(c, h, c->line, c->line_rev, c->stop_i);
        }
        snprintf(a, sizeof(a), "%s", h->trip_label[0] ? h->trip_label : T(c, "Auftrag", "Job"));
        b[0] = 0;
        if (h->chip1[0]) snprintf(b, sizeof(b), "%s", h->chip1);
        row(h, a, b, 2);
        row(h, h->trip_to, "", 0);
        int z = c->map_zoom % 3;
        row(h, T(c, "Zoom", "Zoom"), z == 2 ? "Navi" : z ? T(c, "Ganze Karte", "Whole map") : T(c, "Strecke", "Route"), 0);
        h->row_selected = h->row_count - 1;
        if (z == 2) navi(c, h);
        else if (z) whole(h);
        else fit(h, 2);
    }
}
