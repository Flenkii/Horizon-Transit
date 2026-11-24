/* 0.6: what the market offers: ride classes by the driven car, hotspots with a surcharge, the driver's tier.

   Ride classes (like a ride app's): the car decides which kinds of request come in.
   - Saver   (small car, hot hatch): pays x0.85, the passengers are easy-going (a little more time, softer).
   - Comfort (everything else): as before.
   - XL      (off-roader): 2 to 4 people, pays x1.3, a jolt bothers them a bit more.
   - Black   (luxury saloon, supercar; from the Silver tier): pays x1.4, strict about smooth driving.
   An XL or Black car gets Comfort requests as well (60 % XL, 50 % Black).

   Hotspots: one or two places (the festival, a town) where many people want a ride for 4 to 7 minutes:
   requests picked up within 1.5 km of one pay 1.2x to 1.5x (near one, one request
   of the list comes from inside it). They change on their own; on shift a pop-up says where.

   Tiers (never go down; saved): Bronze at the start, Silver (10 jobs, 4.0 stars on average, reputation 55),
   Gold (30 jobs, 4.3, 68), Platinum (75 jobs, 4.6, 82). Silver: Black rides. Gold: 5 % more pay, more VIPs.
   Platinum: 10 % more pay, hotspots pay 0.1x more. Off with "career extras" off (then no Black, no hotspots). */
#include "jobs_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float class_pay[TS_CLASSES] = {0.85f, 1.0f, 1.3f, 1.4f};
static const float class_soft[TS_CLASSES] = {0.9f, 1.0f, 1.1f, 1.35f};
static const float class_time[TS_CLASSES] = {1.1f, 1.0f, 1.0f, 1.0f};

const char *ts_class_name(int ride_class, int german) {
    switch (ride_class) {
    case TS_CLASS_SAVER: return german ? "Spar" : "Saver";
    case TS_CLASS_XL: return "XL";
    case TS_CLASS_BLACK: return "Black";
    default: return german ? "Komfort" : "Comfort";
    }
}

float market_class_pay(int ride_class) { return ride_class >= 0 && ride_class < TS_CLASSES ? class_pay[ride_class] : 1.0f; }
float market_class_soft(int ride_class) { return ride_class >= 0 && ride_class < TS_CLASSES ? class_soft[ride_class] : 1.0f; }
float market_class_time(int ride_class) { return ride_class >= 0 && ride_class < TS_CLASSES ? class_time[ride_class] : 1.0f; }

/* the class of one request for the driven car (type: the passenger's kind, for the special rides) */
int market_pick_class(TsCore *c, int type) {
    int g = c->car_group;
    int black_car = g == TS_GROUP_SALOON || g == TS_GROUP_SUPER;
    int black_ok = black_car && !c->extras_off && c->tier >= TS_TIER_SILVER;
    if (type == TS_P_VIP || type == TS_P_BUSINESS) return black_ok ? TS_CLASS_BLACK : TS_CLASS_COMFORT;
    if (type == TS_P_WEDDING) return TS_CLASS_COMFORT;
    if (type == TS_P_TOURIST) return g == TS_GROUP_OFFROAD ? TS_CLASS_XL : TS_CLASS_COMFORT;
    if (g == TS_GROUP_SMALL || g == TS_GROUP_HATCH) return TS_CLASS_SAVER;
    if (g == TS_GROUP_OFFROAD) return random_unit(c) < 0.6f ? TS_CLASS_XL : TS_CLASS_COMFORT;
    if (black_ok) return random_unit(c) < 0.5f ? TS_CLASS_BLACK : TS_CLASS_COMFORT;
    return TS_CLASS_COMFORT;
}

/* ------------------------------------------------------------------ tiers */

typedef struct {
    int jobs;
    float stars, rep;
} TierNeed;
static const TierNeed tier_need[TS_TIERS] = {{0, 0.0f, 0.0f}, {10, 4.0f, 55.0f}, {30, 4.3f, 68.0f}, {75, 4.6f, 82.0f}};

const char *ts_tier_name(int tier, int german) {
    switch (tier) {
    case TS_TIER_SILVER: return german ? "Silber" : "Silver";
    case TS_TIER_GOLD: return "Gold";
    case TS_TIER_PLATINUM: return german ? "Platin" : "Platinum";
    default: return "Bronze";
    }
}

void ts_tier_need(int tier, int *jobs, float *stars, float *rep) {
    if (tier < 0 || tier >= TS_TIERS) tier = 0;
    *jobs = tier_need[tier].jobs, *stars = tier_need[tier].stars, *rep = tier_need[tier].rep;
}

static float avg_stars(const TsCore *c) { return c->rating_count ? (float)c->rating_sum / (float)c->rating_count : 0.0f; }

int ts_tier_for(const TsCore *c) {
    int jobs = c->jobs_done + c->bus_lines_done, tier = TS_TIER_BRONZE;
    float stars = avg_stars(c);
    for (int t = 1; t < TS_TIERS; ++t)
        if (jobs >= tier_need[t].jobs && stars >= tier_need[t].stars - 0.001f && c->reputation >= tier_need[t].rep) tier = t;
    return tier;
}

/* how far to the next tier, 0..1: the furthest-behind of jobs, stars and reputation */
float market_tier_progress(const TsCore *c) {
    if (c->tier >= TS_TIERS - 1) return 1.0f;
    const TierNeed *n = &tier_need[c->tier + 1];
    float a = clampf((float)(c->jobs_done + c->bus_lines_done) / (float)n->jobs, 0.0f, 1.0f);
    float b = clampf(avg_stars(c) / n->stars, 0.0f, 1.0f);
    float r = clampf(c->reputation / n->rep, 0.0f, 1.0f);
    float m = a < b ? a : b;
    return m < r ? m : r;
}

/* what holds the driver back from the next tier, e.g. "noch 4 Fahrten" (empty at the top) */
void market_tier_hint(const TsCore *c, char *out, size_t size) {
    out[0] = 0;
    if (c->tier >= TS_TIERS - 1) return;
    const TierNeed *n = &tier_need[c->tier + 1];
    int jobs = c->jobs_done + c->bus_lines_done;
    if (jobs < n->jobs) snprintf(out, size, c->german ? "noch %d Fahrten" : "%d more jobs", n->jobs - jobs);
    else if (avg_stars(c) < n->stars - 0.001f) snprintf(out, size, c->german ? "Ø %.1f nötig" : "needs %.1f avg", (double)n->stars);
    else snprintf(out, size, c->german ? "Ruf %.0f nötig" : "needs rep %.0f", (double)n->rep);
    if (c->german)
        for (char *p = out; *p; ++p)
            if (*p == '.') *p = ',';
}

const char *market_tier_perks(const TsCore *c, int tier) {
    switch (tier) {
    case TS_TIER_SILVER: return T(c, "Black-Fahrten mit Luxuslimousine und Supersportwagen", "Black rides in a luxury saloon or a supercar");
    case TS_TIER_GOLD: return T(c, "+5 % Lohn · mehr VIP-Gäste", "+5 % pay · more VIP guests");
    case TS_TIER_PLATINUM: return T(c, "+10 % Lohn · Hotspots +0,1x", "+10 % pay · hotspots +0.1x");
    default: return "";
    }
}

float market_tier_pay(const TsCore *c) {
    if (c->extras_off) return 1.0f;
    return c->tier >= TS_TIER_PLATINUM ? 1.10f : c->tier >= TS_TIER_GOLD ? 1.05f : 1.0f;
}

/* after a job: a new tier? (the card and the save) */
void market_check_tier(TsCore *c) {
    int t = ts_tier_for(c);
    if (t <= c->tier) return;
    c->tier = t;
    event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
    if (c->extras_off) return;
    char title[TS_TEXT];
    snprintf(title, sizeof(title), "%s: %s", T(c, "Neue Stufe", "New tier"), ts_tier_name(t, c->german));
    card_tier(c, t);
    if (!c->cards_on) toast(c, TS_ICON_TROPHY, TS_SOUND_PAYOUT, title, market_tier_perks(c, t));
}

/* ------------------------------------------------------------------ hotspots */

#define HOT_RADIUS 1500.0f

static uint32_t hot_random(TsCore *c) { /* xorshift, the hotspots' own */
    uint32_t x = c->hot_rng ? c->hot_rng : 0x2545F491u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    c->hot_rng = x;
    return x;
}
static int hot_below(TsCore *c, int n) { return n > 0 ? (int)(hot_random(c) % (uint32_t)n) : 0; }
static float hot_unit(TsCore *c) { return (float)(hot_random(c) >> 8) / 16777216.0f; }

float market_surge(const TsCore *c, int place) {
    if (c->extras_off || place < 0 || place >= (int)ts_place_count) return 1.0f;
    float best = 1.0f;
    for (int i = 0; i < 2; ++i) {
        if (c->hot[i].place < 0 || c->t >= c->hot[i].until) continue;
        if (ts_distance_xz(ts_places[place].pos, ts_places[c->hot[i].place].pos) > HOT_RADIUS) continue;
        float f = c->hot[i].factor + (c->tier >= TS_TIER_PLATINUM ? 0.1f : 0.0f) + (ts_has(c, TS_SHOP_RADAR) ? 0.1f : 0.0f);
        if (f > best) best = f;
    }
    return best;
}

/* the hotspot nearest the car now (index into c->hot), -1 = none */
int market_hot_near(const TsCore *c) {
    int best = -1;
    float best_d = 1e9f;
    for (int i = 0; i < 2; ++i) {
        if (c->hot[i].place < 0 || c->t >= c->hot[i].until) continue;
        float d = c->car_ok ? ts_distance_xz(c->car.pos, ts_places[c->hot[i].place].pos) : 0.0f;
        if (d < best_d) best_d = d, best = i;
    }
    return best;
}

/* the town or the festival: hotspots are where people are */
static int hot_place(TsCore *c, int not_this) {
    int seen = 0, pick = -1;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        if (ts_places[i].kind != TS_PLACE_FESTIVAL && ts_places[i].kind != TS_PLACE_OUTPOST) continue;
        if ((int)i == not_this) continue;
        int around = 0; /* only where there are places to be picked up (not the lone mountain village) */
        for (unsigned j = 0; j < ts_place_count; ++j) around += j != i && ts_distance_xz(ts_places[i].pos, ts_places[j].pos) < HOT_RADIUS;
        if (around < 5) continue;
        if (hot_below(c, ++seen) == 0) pick = (int)i;
    }
    return pick;
}

void market_step(TsCore *c) {
    if (c->extras_off) return;
    if (c->hot_next <= 0.0) c->hot_next = c->t + 60.0; /* the first ones a minute into the game */
    if (c->t < c->hot_next) return;
    int said = 0;
    for (int i = 0; i < 2; ++i) {
        if (c->hot[i].place >= 0 && c->t < c->hot[i].until) continue;
        if (i == 1 && hot_unit(c) < 0.5f) { /* a second one only now and then */
            c->hot[1].place = -1;
            continue;
        }
        c->hot[i].place = hot_place(c, c->hot[1 - i].place);
        c->hot[i].factor = 1.2f + 0.1f * (float)hot_below(c, 4); /* 1.2 .. 1.5 */
        c->hot[i].until = c->t + 240.0 + 180.0 * hot_unit(c);
        /* on shift, without a job: where it is */
        if (!said && c->shift.on && c->mode == TS_IDLE && c->car_ok && c->hot[i].place >= 0 &&
            ts_distance_xz(c->car.pos, ts_places[c->hot[i].place].pos) < 6000.0f) {
            char title[TS_TEXT], text[TS_TEXT], km[24];
            format_km(c, km, sizeof(km), ts_distance_xz(c->car.pos, ts_places[c->hot[i].place].pos));
            snprintf(title, sizeof(title), "%s %s", T(c, "Viel los:", "Busy:"), ts_place_name(c, c->hot[i].place));
            snprintf(text, sizeof(text), "%.1fx %s · %s · %d min", (double)market_surge(c, c->hot[i].place),
                     T(c, "Aufpreis", "surcharge"), km, (int)((c->hot[i].until - c->t) / 60.0 + 0.5));
            if (c->german) for (char *p = text; *p && *p != 'x'; ++p) if (*p == '.') *p = ',';
            toast(c, TS_ICON_ROADS, TS_SOUND_REQUEST, title, text);
            said = 1; /* one pop-up at a time */
        }
    }
    double next = c->t + 600.0;
    for (int i = 0; i < 2; ++i)
        if (c->hot[i].place >= 0 && c->hot[i].until < next) next = c->hot[i].until;
    c->hot_next = next;
}

/* a pickup inside a hotspot near the car (within 3.5 km), -1 = none near */
int market_hot_pickup(TsCore *c) {
    int h = market_hot_near(c);
    if (h < 0 || !c->car_ok || ts_distance_xz(c->car.pos, ts_places[c->hot[h].place].pos) > 3500.0f) return -1;
    if (hot_unit(c) > 0.7f) return -1;
    return pick_place(c, ts_places[c->hot[h].place].pos, 0.0f, HOT_RADIUS * 0.8f, -1);
}

/* "1,6x" / "1.6x" */
void market_surge_text(const TsCore *c, float surge, char *out, size_t size) {
    snprintf(out, size, "%.1fx", (double)surge);
    if (c->german)
        for (char *p = out; *p; ++p)
            if (*p == '.') *p = ',';
}
