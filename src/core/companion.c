/* 0.10: what the phone app gets (companion.h). */
#include "companion.h"
#include "gamemap.h"
#include "jobs_internal.h"
#include "map.h"
#include "stops.h"
#include "phone_draw.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ the chat */

static struct {
    unsigned id;
    char name[64];
    char text[TS_TEXT];
    int style;
} feed[TS_APP_FEED];
static unsigned feed_next = 1; /* the next line's id (the app shows each id once) */

void ts_app_feed(const char *name, const char *text, int style) {
    unsigned id = feed_next++;
    int k = (int)(id % TS_APP_FEED);
    feed[k].id = id;
    snprintf(feed[k].name, sizeof(feed[k].name), "%s", name ? name : "");
    snprintf(feed[k].text, sizeof(feed[k].text), "%s", text ? text : "");
    feed[k].style = style;
}

void ts_app_feed_clear(void) {
    memset(feed, 0, sizeof(feed));
}

/* ------------------------------------------------------------------ JSON */

typedef struct {
    char *p;
    size_t n, cap;
    int bad;
} Out;

static void put(Out *o, const char *s, size_t len) {
    if (o->bad) return;
    if (o->n + len + 1 > o->cap) {
        o->bad = 1;
        return;
    }
    memcpy(o->p + o->n, s, len);
    o->n += len;
    o->p[o->n] = 0;
}
static void raw(Out *o, const char *s) { put(o, s, strlen(s)); }
static void fmt(Out *o, const char *f, ...) {
    char b[256];
    va_list a;
    va_start(a, f);
    int k = vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    if (k > 0) put(o, b, (size_t)k < sizeof(b) ? (size_t)k : sizeof(b) - 1);
}
static void str(Out *o, const char *s) {
    raw(o, "\"");
    if (s)
        for (; *s; ++s) {
            unsigned char ch = (unsigned char)*s;
            if (ch == '"') raw(o, "\\\"");
            else if (ch == '\\') raw(o, "\\\\");
            else if (ch == '\n') raw(o, "\\n");
            else if (ch < 0x20) fmt(o, "\\u%04x", ch);
            else put(o, (const char *)&ch, 1);
        }
    raw(o, "\"");
}
static void key(Out *o, const char *k) {
    if (o->n && o->p[o->n - 1] != '{' && o->p[o->n - 1] != '[') raw(o, ",");
    str(o, k);
    raw(o, ":");
}
static void kstr(Out *o, const char *k, const char *v) { key(o, k), str(o, v); }
static void kint(Out *o, const char *k, long long v) { key(o, k), fmt(o, "%lld", v); }
static void knum(Out *o, const char *k, double v) {
    key(o, k);
    if (!isfinite(v)) v = 0.0;
    fmt(o, "%.2f", v);
}
static void comma(Out *o) {
    if (o->n && o->p[o->n - 1] != '{' && o->p[o->n - 1] != '[') raw(o, ",");
}
/* a place: its name and where it is */
static void place(Out *o, const TsCore *c, const char *k, int i) {
    key(o, k);
    if (i < 0 || i >= (int)ts_place_count) {
        raw(o, "null");
        return;
    }
    raw(o, "{");
    kstr(o, "name", ts_place_name(c, i));
    knum(o, "x", ts_places[i].pos[0]);
    knum(o, "z", ts_places[i].pos[2]);
    raw(o, "}");
}

static float heading(const TsCore *c) {
    float vx = c->car.vel[0], vz = c->car.vel[2], fs = c->fwd_sign < -0.2f ? -1.0f : 1.0f;
    return vx * vx + vz * vz > 9.0f ? atan2f(vx, vz) : atan2f(fs * c->car.fwd[0], fs * c->car.fwd[2]);
}

/* ------------------------------------------------------------------ the way to the next target */

static struct {
    float from[2], to[2];
    int n;
    float pts[TS_APP_ROUTE * 2];
} way;

/* the way along the roads from the car to the GPS target; worked out again when the target changes or the car
   has left it (150 m) */
static int app_way(const TsCore *c, float *out, int max) {
    if (!c->car_ok || !c->nav_on || c->mode == TS_IDLE) return 0;
    float a[2] = {c->car.pos[0], c->car.pos[2]}, b[2] = {c->nav_target[0], c->nav_target[2]};
    if (!ts_map_ok()) {
        out[0] = a[0], out[1] = a[1], out[2] = b[0], out[3] = b[1];
        return 2;
    }
    int again = !way.n || way.to[0] != b[0] || way.to[1] != b[1];
    if (!again) { /* still on it? */
        float best = 1e18f;
        for (int i = 0; i < way.n; ++i) {
            float dx = way.pts[2 * i] - a[0], dz = way.pts[2 * i + 1] - a[1], d = dx * dx + dz * dz;
            if (d < best) best = d;
        }
        again = best > 150.0f * 150.0f;
    }
    if (again) {
        way.n = ts_map_route(a, b, way.pts, TS_APP_ROUTE);
        way.from[0] = a[0], way.from[1] = a[1], way.to[0] = b[0], way.to[1] = b[1];
    }
    int near = 0;
    float bd = 1e18f;
    for (int i = 0; i < way.n - 1; ++i) {
        float ex = way.pts[2 * i] - a[0], ez = way.pts[2 * i + 1] - a[1], d = ex * ex + ez * ez;
        if (d < bd) bd = d, near = i;
    }
    int m = 0;
    out[m * 2] = a[0], out[m * 2 + 1] = a[1], m++;
    for (int i = near + 1; i < way.n && m < max; ++i) out[m * 2] = way.pts[2 * i], out[m * 2 + 1] = way.pts[2 * i + 1], m++;
    return m;
}

/* ------------------------------------------------------------------ the state */

static void offer(Out *o, const TsCore *c, const TsOffer *f, int i) {
    raw(o, "{");
    kint(o, "i", i);
    kstr(o, "name", ts_passenger_name(f->passenger));
    kstr(o, "type", type_name(c, f->ptype));
    kint(o, "ptype", f->ptype);
    kstr(o, "cls", ts_class_name(f->ride_class, c->german));
    place(o, c, "from", f->pickup);
    place(o, c, "to", f->dest);
    knum(o, "pickupM", f->pickup_m);
    knum(o, "tripM", f->trip_m);
    knum(o, "limitS", f->limit_s);
    kint(o, "pay", f->pay);
    kint(o, "maxKmh", f->max_kmh);
    kint(o, "maxShown", ts_speed_shown(c, f->max_kmh));
    kint(o, "regular", f->regular);
    kint(o, "party", f->party);
    knum(o, "surge", f->surge);
    kint(o, "pool", f->pool);
    if (f->pool) {
        kstr(o, "poolName", ts_passenger_name(f->pool_passenger));
        place(o, c, "poolFrom", f->pool_pickup);
        place(o, c, "poolTo", f->pool_dest);
    }
    raw(o, "}");
}

static const char *stage_name(const TsCore *c) {
    switch (c->stage) {
    case TS_UBER_TO_PICKUP: return T(c, "Zum Fahrgast", "To the passenger");
    case TS_UBER_BOARDING: return T(c, "Fahrgast steigt ein", "Passenger getting in");
    case TS_UBER_TO_DEST: return T(c, "Zum Ziel", "To the destination");
    case TS_UBER_ALIGHTING: return T(c, "Fahrgast steigt aus", "Passenger getting out");
    case TS_BUS_TO_STOP: return T(c, "Zur nächsten Haltestelle", "To the next stop");
    case TS_BUS_AT_STOP: return T(c, "An der Haltestelle", "At the stop");
    default: return "";
    }
}

static void job(Out *o, const TsCore *c) {
    key(o, "job");
    if (c->mode == TS_IDLE) {
        raw(o, "null");
        return;
    }
    raw(o, "{");
    kstr(o, "kind", c->mode == TS_UBER ? "ride" : "bus");
    kint(o, "stage", c->stage);
    kstr(o, "stageName", stage_name(c));
    kint(o, "sat", (long long)lroundf(c->sat));
    kint(o, "stars", ts_stars(c->sat));
    double left = c->deadline > 0.0 ? c->deadline - c->t : 0.0;
    knum(o, "timeLeft", c->deadline > 0.0 ? left : -1.0);
    knum(o, "driven", c->trip_driven_m);
    if (c->nav_on) {
        key(o, "target");
        fmt(o, "{\"x\":%.1f,\"z\":%.1f}", c->nav_target[0], c->nav_target[2]);
        if (c->car_ok) {
            float d = ts_distance_xz(c->car.pos, c->nav_target);
            knum(o, "targetM", d);
        }
    }
    if (c->mode == TS_UBER) {
        key(o, "ride");
        offer(o, c, &c->job, 0);
        kint(o, "poolStage", c->pool_stage);
    } else {
        const TsLine *l = &ts_lines[c->line];
        kint(o, "line", c->line);
        kstr(o, "lineNumber", l->number);
        kstr(o, "lineTitle", ts_line_title(c, c->line));
        kint(o, "stop", c->stop_i);
        kint(o, "onboard", c->onboard);
        kint(o, "seats", c->bus_capacity);
        kint(o, "delivered", c->delivered);
        kint(o, "fares", c->bus_pay);
        key(o, "stops");
        raw(o, "[");
        for (int i = 0; i < (int)l->stop_count; ++i) {
            int p = ts_bus_stop(c, c->line, i);
            comma(o);
            raw(o, "{");
            kstr(o, "name", ts_place_name(c, p));
            knum(o, "x", ts_places[p].pos[0]);
            knum(o, "z", ts_places[p].pos[2]);
            raw(o, "}");
        }
        raw(o, "]");
    }
    raw(o, "}");
}

static void card(Out *o, const TsHud *h) {
    key(o, "card");
    if (!h || !h->card.kind) {
        raw(o, "null");
        return;
    }
    const TsCard *k = &h->card;
    raw(o, "{");
    kint(o, "kind", k->kind);
    kint(o, "serial", h->card_serial);
    kstr(o, "title", k->title);
    kstr(o, "name", k->name);
    kstr(o, "note", k->note);
    kint(o, "money", k->money);
    kint(o, "stars", k->stars);
    kint(o, "tier", k->tier);
    key(o, "pairs");
    raw(o, "[");
    for (int i = 0; i < k->pairs && i < TS_CARD_PAIRS; ++i) {
        comma(o);
        raw(o, "[");
        str(o, k->label[i]);
        raw(o, ",");
        str(o, k->value[i]);
        raw(o, "]");
    }
    raw(o, "]}");
}

static void hud(Out *o, const TsHud *h) {
    key(o, "hud");
    if (!h) {
        raw(o, "null");
        return;
    }
    raw(o, "{");
    kint(o, "chips", h->chips);
    kstr(o, "chip0", h->chip0);
    kstr(o, "chip1", h->chip1);
    kstr(o, "chip2", h->chip2);
    kstr(o, "wish", h->chip3);
    kstr(o, "corner", h->corner);
    kint(o, "tripOn", h->trip_on);
    knum(o, "tripProgress", h->trip_progress);
    kstr(o, "tripLabel", h->trip_label);
    kstr(o, "tripTo", h->trip_to);
    kint(o, "meter", h->meter);
    kstr(o, "meterLabel", h->meter_label);
    kint(o, "signLimit", h->sign_limit);
    kint(o, "signOver", h->sign_over);
    kint(o, "signFree", h->sign_free);
    kstr(o, "signUnit", h->sign_unit);
    kint(o, "countdown", h->countdown);
    kstr(o, "countdownLabel", h->countdown_label);
    kint(o, "gOn", h->g_on);
    knum(o, "gLat", h->g_lat);
    knum(o, "gLong", h->g_long);
    kint(o, "gOver", h->g_over);
    raw(o, "}");
}

static void theme_obj(Out *o, int theme, int de);

size_t ts_app_state_json(const TsCore *c, const TsHud *h, char *out, size_t cap) {
    Out o = {out, 0, cap, 0};
    if (cap) out[0] = 0;
    raw(&o, "{");
    kint(&o, "v", 1);
    kint(&o, "de", c->german);
    kint(&o, "book", c->book_serial); /* 0.13: the ride book changed: the app fetches /api/history */
    if (c->target_on) fmt(&o, ",\"target\":{\"x\":%.0f,\"z\":%.0f}", c->target[0], c->target[2]);
    kint(&o, "mapv", TS_MAP_VERSION); /* 0.13: the phone keeps map pieces; a new map version makes it fetch them again */
    kstr(&o, "unit", ts_speed_unit(c));
    kint(&o, "imperial", c->imperial);
    key(&o, "car");
    raw(&o, "{");
    kint(&o, "ok", c->car_ok);
    if (c->car_ok) {
        knum(&o, "x", c->car.pos[0]);
        knum(&o, "z", c->car.pos[2]);
        knum(&o, "h", heading(c));
        int kmh = (int)lroundf(c->speed * 3.6f);
        kint(&o, "speed", c->imperial ? (long long)lroundf(kmh / 1.609344f) : kmh);
    }
    kstr(&o, "name", c->car_kind == TS_CAR_UNKNOWN ? "" : c->car_name);
    kint(&o, "bus", c->car_kind == TS_CAR_BUS);
    raw(&o, "}");
    kint(&o, "money", c->game_credits); /* 0.13: the game account (-1 = not read yet) */
    knum(&o, "rep", c->reputation);
    kstr(&o, "repName", ts_rep_name(c));
    kstr(&o, "tier", ts_tier_name(c->tier, c->german));
    kint(&o, "tierNo", c->tier);
    knum(&o, "rating", c->rating_count ? (double)c->rating_sum / c->rating_count : 0.0);
    kint(&o, "ratings", c->rating_count);
    kint(&o, "jobs", c->jobs_done);
    kint(&o, "lines", c->bus_lines_done);
    knum(&o, "km", c->km_total);
    key(&o, "shift");
    raw(&o, "{");
    kint(&o, "on", c->shift.on);
    if (c->shift.on) {
        kint(&o, "jobs", c->shift.jobs + c->shift.lines);
        kint(&o, "earned", c->shift.earned);
        knum(&o, "minutes", (c->t - c->shift.start_t) / 60.0);
        kint(&o, "goal", TS_GOAL_JOBS);
        kint(&o, "goalDone", c->shift.goal_done);
    }
    raw(&o, "}");
    kstr(&o, "mode", c->mode == TS_UBER ? "ride" : c->mode == TS_BUS ? "bus" : "idle");
    kint(&o, "needsBus", c->bus_needs_bus && c->car_kind != TS_CAR_BUS);
    key(&o, "offers");
    raw(&o, "[");
    if (c->mode == TS_IDLE)
        for (int i = 0; i < c->offer_count; ++i) {
            comma(&o);
            offer(&o, c, &c->offers[i], i);
        }
    raw(&o, "]");
    /* 0.13: the request's countdown (seconds, 0 = none) and how long it had */
    knum(&o, "reqLeft", c->mode == TS_IDLE && c->offer_count && c->req_until > c->t ? c->req_until - c->t : 0.0);
    knum(&o, "reqTotal", TS_REQUEST_S);
    job(&o, c);
    key(&o, "reply");
    if (ts_talk_question(c)) {
        raw(&o, "{");
        kstr(&o, "q", ts_talk_question(c));
        kstr(&o, "a", ts_talk_answer(c));
        knum(&o, "left", c->ask_until - c->t);
        raw(&o, "}");
    } else raw(&o, "null");
    hud(&o, h);
    card(&o, h);
    /* 0.10.1: the phone's design, its options, the shop, the account */
    key(&o, "theme");
    theme_obj(&o, c->app_theme, c->german);
    key(&o, "settings");
    raw(&o, "[");
    for (int i = 0; i < c->setting_count && i < TS_SETTING_ROWS; ++i) {
        comma(&o);
        raw(&o, "{");
        kint(&o, "i", i);
        kstr(&o, "label", c->setting_label[i]);
        kstr(&o, "value", c->setting_value[i]);
        kstr(&o, "note", c->setting_note[i]);
        if (c->setting_choices[i] > 0) { /* 0.10.4 */
            kint(&o, "idx", c->setting_index[i]);
            key(&o, "choices");
            raw(&o, "[");
            for (int k = 0; k < c->setting_choices[i] && k < 12; ++k) {
                comma(&o);
                str(&o, c->setting_choice[i][k]);
            }
            raw(&o, "]");
        }
        raw(&o, "}");
    }
    raw(&o, "]");
    key(&o, "shop");
    raw(&o, "[");
    for (int i = 0; i < TS_SHOP_ITEMS; ++i) {
        comma(&o);
        raw(&o, "{");
        kint(&o, "i", i);
        kstr(&o, "name", ts_shop_name(c, i));
        kstr(&o, "what", ts_shop_what(c, i));
        kint(&o, "price", ts_shop_cost(c, i)); /* 0.13: in CR when the shop takes the game's credits */
        kint(&o, "bought", ts_has(c, i));
        raw(&o, "}");
    }
    raw(&o, "]");
    key(&o, "account");
    raw(&o, "{");
    {
        char hint[48];
        market_tier_hint(c, hint, sizeof(hint));
        kstr(&o, "tierHint", hint);
        knum(&o, "tierProgress", market_tier_progress(c));
        key(&o, "tiers"); /* 0.13: what each tier needs */
        raw(&o, "[");
        for (int t = 0; t < TS_TIERS; ++t) {
            int jobs;
            float stars, rep;
            ts_tier_need(t, &jobs, &stars, &rep);
            comma(&o);
            raw(&o, "{");
            kstr(&o, "name", ts_tier_name(t, c->german));
            kint(&o, "jobs", jobs);
            knum(&o, "stars", stars);
            knum(&o, "rep", rep);
            raw(&o, "}");
        }
        raw(&o, "]");
        kint(&o, "tips", c->tips_total);
        kint(&o, "fiveStars", c->five_stars);
        kint(&o, "bestPay", c->best_pay);
        knum(&o, "longest", c->longest_m);
        kint(&o, "onTime", c->on_time_total);
        kint(&o, "late", c->late_total);
        kint(&o, "specials", c->specials_done);
        kint(&o, "wishesOk", c->wishes_ok);
        kint(&o, "wishesFailed", c->wishes_failed);
        kint(&o, "crashes", c->crashes_total);
        kint(&o, "passengers", c->passengers_total);
        kint(&o, "streak", c->streak);
        int regulars = 0;
        for (int k = 0; k < TS_PEOPLE_MAX; ++k) regulars += c->met[k] >= 2;
        kint(&o, "regulars", regulars);
        if (c->shift.on) {
            kint(&o, "shiftTips", c->shift.tips);
            knum(&o, "shiftStars", c->shift.rated ? (double)c->shift.stars_sum / c->shift.rated : 0.0);
        }
    }
    raw(&o, "}");
    /* the log: the last jobs */
    key(&o, "log");
    raw(&o, "[");
    for (int i = 0; i < c->log_count && i < TS_LOG; ++i) {
        const TsLogEntry *e = &c->log[i];
        comma(&o);
        raw(&o, "{");
        kint(&o, "bus", e->bus);
        if (e->bus) kstr(&o, "who", e->who >= 0 && e->who < (int)ts_line_count ? ts_line_title(c, e->who) : "");
        else kstr(&o, "who", ts_passenger_name(e->who));
        kstr(&o, "place", e->place >= 0 && e->place < (int)ts_place_count ? ts_place_name(c, e->place) : "");
        kint(&o, "pay", e->pay);
        kint(&o, "stars", e->stars);
        raw(&o, "}");
    }
    raw(&o, "]");
    /* the hotspots (more pay there) */
    key(&o, "hot");
    raw(&o, "[");
    for (int k = 0; k < 2; ++k) {
        if (c->hot[k].place < 0 || c->hot[k].place >= (int)ts_place_count || c->hot[k].until <= c->t) continue;
        comma(&o);
        raw(&o, "{");
        kstr(&o, "name", ts_place_name(c, c->hot[k].place));
        knum(&o, "x", ts_places[c->hot[k].place].pos[0]);
        knum(&o, "z", ts_places[c->hot[k].place].pos[2]);
        knum(&o, "factor", c->hot[k].factor);
        raw(&o, "}");
    }
    raw(&o, "]");
    /* the way */
    static float pts[TS_APP_ROUTE * 2];
    int n = app_way(c, pts, TS_APP_ROUTE);
    key(&o, "route");
    raw(&o, "[");
    for (int i = 0; i < n; ++i) fmt(&o, i ? ",%.0f,%.0f" : "%.0f,%.0f", pts[2 * i], pts[2 * i + 1]);
    raw(&o, "]");
    /* the chat, oldest first */
    key(&o, "feed");
    raw(&o, "[");
    unsigned first = feed_next > TS_APP_FEED ? feed_next - TS_APP_FEED : 1;
    for (unsigned id = first; id < feed_next; ++id) {
        int k = (int)(id % TS_APP_FEED);
        if (feed[k].id != id) continue;
        comma(&o);
        raw(&o, "{");
        kint(&o, "id", id);
        kstr(&o, "name", feed[k].name);
        kstr(&o, "text", feed[k].text);
        kint(&o, "style", feed[k].style);
        raw(&o, "}");
    }
    raw(&o, "]");
    raw(&o, "}");
    if (o.bad) {
        if (cap) out[0] = 0;
        return 0;
    }
    return o.n;
}

/* 0.10.1: a design's colours for the app; 0.10.4: all of them go with the lines, so the app can switch offline */
static void theme_obj(Out *o, int theme, int de) {
    uint32_t col[11];
    int alpha = 255;
    static const char *names[11] = {"top", "bottom", "card", "line", "text", "dim", "selText", "accent", "accent2", "warn", "value"};
    ts_theme_colors(theme, col, &alpha);
    raw(o, "{");
    kint(o, "i", theme);
    kstr(o, "name", ts_theme_name(theme, de));
    for (int i = 0; i < 11; ++i) {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%06X", (unsigned)(col[i] & 0xFFFFFF));
        kstr(o, names[i], hex);
    }
    kint(o, "cardAlpha", alpha);
    raw(o, "}");
}

/* 0.13: a line's way over the roads, stop to stop, every ~60 m a point (made once the map is there) */
#define LINE_WAY 1500
static struct {
    int ready, n;
    short pts[LINE_WAY * 2]; /* metres / 2 + offset as in TS_APP_TARGET, small enough for the JSON */
} line_way[16];

static int way_budget;

static void line_way_json(Out *o, int l) {
    if (l < 0 || l >= 16) return;
    if (!line_way[l].ready && ts_map_ok() && way_budget > 0) { /* one line per call: no long stop of the game */
        way_budget--;
        static float pts[2 * 4000];
        int m = 0;
        const TsLine *line = &ts_lines[l];
        float lx = 1e9f, lz = 1e9f;
        for (unsigned i = 1; i < line->stop_count && m < LINE_WAY; ++i) {
            const float *a3 = ts_places[line->stops[i - 1]].pos, *b3 = ts_places[line->stops[i]].pos;
            float a[2] = {a3[0], a3[2]}, b[2] = {b3[0], b3[2]};
            int n = ts_map_route(a, b, pts, 4000);
            if (n < 2) pts[0] = a[0], pts[1] = a[1], pts[2] = b[0], pts[3] = b[1], n = 2;
            for (int k = 0; k < n && m < LINE_WAY; ++k) {
                float x = pts[2 * k], z = pts[2 * k + 1], dx = x - lx, dz = z - lz;
                if (k > 0 && k < n - 1 && dx * dx + dz * dz < 60.0f * 60.0f) continue;
                line_way[l].pts[2 * m] = (short)lroundf(x / 2.0f), line_way[l].pts[2 * m + 1] = (short)lroundf(z / 2.0f);
                lx = x, lz = z, m++;
            }
        }
        line_way[l].n = m, line_way[l].ready = 1;
    }
    if (!line_way[l].ready) return;
    key(o, "way");
    raw(o, "[");
    for (int k = 0; k < line_way[l].n; ++k) fmt(o, k ? ",%d,%d" : "%d,%d", line_way[l].pts[2 * k] * 2, line_way[l].pts[2 * k + 1] * 2);
    raw(o, "]");
}

/* 0.13: the ride book, newest first */
size_t ts_app_history_json(const TsCore *c, char *out, size_t cap) {
    Out o = {out, 0, cap, 0};
    if (cap) out[0] = 0;
    raw(&o, "{");
    kint(&o, "serial", c->book_serial);
    key(&o, "rides");
    raw(&o, "[");
    for (int i = 0; i < c->book_count && i < TS_BOOK; ++i) {
        const TsBookEntry *e = &c->book[i];
        comma(&o);
        raw(&o, "{");
        kint(&o, "t", e->when);
        kint(&o, "bus", e->bus);
        if (e->bus) {
            char who[96];
            snprintf(who, sizeof(who), "%s %s", e->who < (int)ts_line_count ? ts_lines[e->who].number : "", e->who < (int)ts_line_count ? ts_line_title(c, e->who) : "");
            kstr(&o, "who", who);
        } else kstr(&o, "who", ts_passenger_name(e->who));
        kstr(&o, "from", e->from < (int)ts_place_count ? ts_place_name(c, e->from) : "");
        kstr(&o, "to", e->to < (int)ts_place_count ? ts_place_name(c, e->to) : "");
        if (e->car > 0 && e->car <= (int)ts_car_count) {
            const TsCarInfo *car = &ts_cars[e->car - 1];
            char name[96];
            if (car->make[0]) snprintf(name, sizeof(name), "%s %s", car->make, car->model);
            else snprintf(name, sizeof(name), "%s", car->model);
            kstr(&o, "car", name);
        }
        kint(&o, "stars", e->stars);
        kint(&o, "late", e->late);
        kint(&o, "pay", e->pay);
        kint(&o, "tip", e->tip);
        kint(&o, "m", e->metres);
        kint(&o, "s", e->seconds);
        raw(&o, "}");
    }
    raw(&o, "]}");
    if (o.bad) {
        if (cap) out[0] = 0;
        return 0;
    }
    return o.n;
}

size_t ts_app_lines_json(const TsCore *c, char *out, size_t cap) {
    Out o = {out, 0, cap, 0};
    if (cap) out[0] = 0;
    way_budget = 1;
    raw(&o, "{\"lines\":[");
    for (int l = 0; l < (int)ts_line_count; ++l) {
        comma(&o);
        raw(&o, "{");
        kint(&o, "i", l);
        kstr(&o, "number", ts_lines[l].number);
        kstr(&o, "title", ts_line_title(c, l));
        kint(&o, "done", l < 8 ? c->line_done[l] : 0);
        key(&o, "stops");
        raw(&o, "[");
        for (int i = 0; i < (int)ts_lines[l].stop_count; ++i) {
            int p = ts_lines[l].stops[i];
            comma(&o);
            raw(&o, "{");
            kstr(&o, "name", ts_place_name(c, p));
            knum(&o, "x", ts_places[p].pos[0]);
            knum(&o, "z", ts_places[p].pos[2]);
            kint(&o, "min", (long long)lroundf(tt_stop_minutes(c, l, i))); /* 0.13: the timetable */
            raw(&o, "}");
        }
        raw(&o, "]");
        line_way_json(&o, l); /* 0.13: the line on the map */
        raw(&o, "}");
    }
    raw(&o, "],\"themes\":[");
    for (int t = 0; t < TS_THEMES; ++t) {
        comma(&o);
        theme_obj(&o, t, c->german);
    }
    raw(&o, "]}");
    if (o.bad) {
        if (cap) out[0] = 0;
        return 0;
    }
    return o.n;
}

/* ------------------------------------------------------------------ what the app asks for */

int ts_app_action(const char *name) {
    static const char *names[] = {"", "accept", "line", "cancel", "reply", "refresh", "shift", "setting", "buy", "set", "target"};
    for (int i = 1; i < (int)(sizeof(names) / sizeof(names[0])); ++i)
        if (name && !strcmp(name, names[i])) return i;
    return TS_APP_NONE;
}

int ts_app_do(TsCore *c, int action, int arg) {
    switch (action) {
    case TS_APP_ACCEPT:
        if (c->mode != TS_IDLE || arg < 0 || arg >= c->offer_count) return 0;
        ts_accept_row(c, arg);
        return c->mode != TS_IDLE;
    case TS_APP_LINE:
        if (c->mode != TS_IDLE || arg < 0 || arg >= (int)ts_line_count) return 0;
        ts_accept_row(c, TS_PHONE_OFFERS + arg);
        return c->mode != TS_IDLE;
    case TS_APP_CANCEL:
        if (c->mode == TS_IDLE) return 0;
        ts_cancel(c);
        return 1;
    case TS_APP_REPLY:
        if (!ts_talk_question(c)) return 0;
        ts_reply(c);
        return 1;
    case TS_APP_REFRESH: /* 0.13: "decline": no to the request on the phone */
        if (c->mode != TS_IDLE || !c->offer_count) return 0;
        ts_decline(c);
        return 1;
    case TS_APP_SHIFT:
        if (c->mode != TS_IDLE) return 0;
        if (c->shift.on) ts_shift_toggle(c);
        else ts_go_online(c);
        return 1;
    case TS_APP_SETTING:
        if (arg < 0 || arg >= c->setting_count || arg >= TS_SETTING_ROWS) return 0;
        c->setting_pressed = arg; /* the glue changes it after the step, like Enter on the phone */
        return 1;
    case TS_APP_SET: { /* 0.10.4 */
        int row = arg / 16, v = arg % 16;
        if (arg < 0 || row >= c->setting_count || row >= TS_SETTING_ROWS) return 0;
        if (c->setting_choices[row] > 0 && v >= c->setting_choices[row]) return 0;
        c->setting_want[row] = v;
        return 1;
    }
    case TS_APP_TARGET: /* 0.13 */
        if (arg < 0) {
            ts_clear_target(c);
            return 1;
        }
        ts_set_target(c, (float)(arg / 32768) * 2.0f - 30000.0f, (float)(arg % 32768) * 2.0f - 30000.0f);
        return 1;
    case TS_APP_BUY: {
        if (arg < 0 || arg >= TS_SHOP_ITEMS || ts_has(c, arg) || ts_shop_balance(c) < ts_shop_cost(c, arg)) {
            ts_shop_buy(c, arg); /* says why not (a pop-up in the game) */
            return 0;
        }
        ts_shop_buy(c, arg);
        return ts_has(c, arg) || c->credit_buy_item == arg; /* 0.13: with credits it's paid a moment later */
    }
    default: return 0;
    }
}
