/* 0.6: cards in the ride panel's place by the speedometer, like a ride app's screens after a ride:
   - after a ride or a bus line: the stars light up one by one, the money counts up, the passenger's comment,
     along the bottom the fare, the tip and the streak's bonus (8 s)
   - at the end of a shift: the day's review: earned, rides and lines, average stars, distance, tips, the best job (14 s)
   - a new tier: the medal and what it brings (7 s)
   One at a time; the next waits (up to 3). A job starting hides them (the ride panel shows the job).
   Without the ride panel (its setting off, or no window) the cards aren't made and the pop-ups stay as before. */
#include "jobs_internal.h"
#include <stdio.h>
#include <string.h>

static void push(TsCore *c, const TsCard *k) {
    if (!c->cards_on) return;
    if (c->card.kind == TS_CARD_NONE) {
        c->card = *k;
        c->card_at = c->t;
        c->card_serial++;
        return;
    }
    if (c->card_queued < 3) c->card_queue[c->card_queued++] = *k;
}

void card_step(TsCore *c) {
    if (c->card.kind == TS_CARD_NONE) return;
    /* a job runs: the cards go (the panel shows the job) */
    if (c->mode != TS_IDLE) {
        memset(&c->card, 0, sizeof(c->card));
        c->card_queued = 0;
        return;
    }
    if (c->t - c->card_at < c->card.secs) return;
    memset(&c->card, 0, sizeof(c->card));
    if (c->card_queued > 0) {
        c->card = c->card_queue[0];
        memmove(c->card_queue, c->card_queue + 1, sizeof(c->card_queue[0]) * (size_t)(c->card_queued - 1));
        c->card_queued--;
        c->card_at = c->t;
        c->card_serial++;
    }
}

void card_hud(const TsCore *c, TsHud *h) {
    if (c->card.kind == TS_CARD_NONE || c->mode != TS_IDLE) return;
    h->card = c->card;
    h->card_serial = c->card_serial;
    h->card_anim = 9.0f; /* all shown; the window animates from 0 when a new card comes */
}

static void pair(TsCard *k, const char *label, const char *value) {
    if (k->pairs >= TS_CARD_PAIRS || !value || !value[0]) return;
    snprintf(k->label[k->pairs], sizeof(k->label[0]), "%s", label);
    snprintf(k->value[k->pairs], sizeof(k->value[0]), "%s", value);
    k->pairs++;
}

/* the passenger's comment in quotes, by the stars */
static void comment(TsCore *c, TsCard *k, int stars) {
    char line[90];
    line_text(c, stars >= 5 ? L_RATE_GREAT : stars == 4 ? L_RATE_GOOD : stars == 3 ? L_RATE_OK : L_RATE_BAD, line, sizeof(line));
    if (line[0]) snprintf(k->note, sizeof(k->note), "„%s“", line);
    if (line[0] && !c->german) snprintf(k->note, sizeof(k->note), "“%s”", line);
}

void card_ride(TsCore *c, int stars, int pay, int base, int bonus, int late) {
    TsCard k;
    memset(&k, 0, sizeof(k));
    k.kind = TS_CARD_RIDE;
    k.secs = 8.0f;
    k.tier = -1;
    snprintf(k.title, sizeof(k.title), "%s", T(c, "FAHRT BEENDET", "RIDE COMPLETE"));
    const char *name = ts_passenger_name(c->job.passenger);
    if (c->job.party > 1) snprintf(k.name, sizeof(k.name), "%s +%d · %s", name, c->job.party - 1, ts_class_name(c->job.ride_class, c->german));
    else snprintf(k.name, sizeof(k.name), "%s · %s", name, ts_class_name(c->job.ride_class, c->german));
    k.money = pay + bonus;
    k.stars = stars;
    comment(c, &k, stars);
    int fare = pay < base ? pay : base, tip = pay > base ? pay - base : 0;
    if (c->job.pool && c->pool_stage == 2) { /* 0.6: a shared ride: both together */
        snprintf(k.name, sizeof(k.name), "%s & %s", ts_passenger_name(c->pool_first), name);
        snprintf(k.title, sizeof(k.title), "%s", T(c, "SAMMELFAHRT BEENDET", "SHARED RIDE COMPLETE"));
        k.money += c->pool_paid;
        fare += c->pool_fare, tip += c->pool_tip;
    }
    char v[24];
    snprintf(v, sizeof(v), "%s", ts_cr(c->german, fare));
    pair(&k, T(c, "Fahrpreis", "Fare"), v);
    if (tip > 0) snprintf(v, sizeof(v), "+%s", ts_cr(c->german, tip)), pair(&k, T(c, "Trinkgeld", "Tip"), v);
    else if (pay < base) snprintf(v, sizeof(v), "-%s", ts_cr(c->german, base - pay)), pair(&k, T(c, "Abzug", "Deduction"), v);
    if (bonus > 0) snprintf(v, sizeof(v), "+%s", ts_cr(c->german, bonus)), pair(&k, T(c, "Serie", "Streak"), v);
    pair(&k, T(c, "Ankunft", "Arrival"), late ? T(c, "zu spät", "late") : T(c, "pünktlich", "on time"));
    push(c, &k);
}

void card_bus(TsCore *c, int stars, int pay, int base, int bonus) {
    TsCard k;
    memset(&k, 0, sizeof(k));
    k.kind = TS_CARD_RIDE;
    k.secs = 8.0f;
    k.tier = -1;
    snprintf(k.title, sizeof(k.title), "%s", T(c, "LINIE BEENDET", "LINE COMPLETE"));
    snprintf(k.name, sizeof(k.name), "%s %s · %s", T(c, "Linie", "Line"), ts_lines[c->line].number, ts_line_title(c, c->line));
    k.money = pay + bonus;
    k.stars = stars;
    int timed = c->stops_on_time + c->stops_late;
    if (timed) snprintf(k.note, sizeof(k.note), "%s %d %s %d %s", T(c, "Pünktlich an", "On time at"), c->stops_on_time, T(c, "von", "of"),
                        timed, T(c, "Halten", "stops"));
    char v[24];
    snprintf(v, sizeof(v), "%d", c->delivered);
    pair(&k, T(c, "Fahrgäste", "Passengers"), v);
    snprintf(v, sizeof(v), "%s", ts_cr(c->german, pay < base ? pay : base));
    pair(&k, T(c, "Fahrgeld", "Fares"), v);
    if (pay > base) snprintf(v, sizeof(v), "+%s", ts_cr(c->german, pay - base)), pair(&k, T(c, "Trinkgeld", "Tips"), v);
    if (bonus > 0) snprintf(v, sizeof(v), "+%s", ts_cr(c->german, bonus)), pair(&k, T(c, "Serie", "Streak"), v);
    push(c, &k);
}

void card_shift(TsCore *c) {
    TsCard k;
    memset(&k, 0, sizeof(k));
    k.kind = TS_CARD_SHIFT;
    k.secs = 14.0f;
    k.tier = c->tier;
    char time[16], km[24], v[24];
    format_time(time, sizeof(time), (c->t - c->shift.start_t) / 60.0); /* h:mm */
    snprintf(k.title, sizeof(k.title), "%s", T(c, "FEIERABEND", "END OF SHIFT"));
    snprintf(k.name, sizeof(k.name), c->german ? "%s h · %s-Fahrer" : "%s h · %s driver", time, ts_tier_name(c->tier, c->german));
    k.money = (int)c->shift.earned;
    k.stars = c->shift.rated ? (int)((float)c->shift.stars_sum / (float)c->shift.rated + 0.5f) : -1;
    snprintf(v, sizeof(v), "%d", c->shift.jobs);
    if (c->shift.lines) snprintf(v, sizeof(v), "%d + %d Bus", c->shift.jobs, c->shift.lines);
    pair(&k, T(c, "Fahrten", "Rides"), v);
    if (c->shift.rated) {
        snprintf(v, sizeof(v), "%.1f", (double)c->shift.stars_sum / c->shift.rated);
        if (c->german) for (char *p = v; *p; ++p) if (*p == '.') *p = ',';
        pair(&k, T(c, "Sterne", "Stars"), v);
    }
    format_km(c, km, sizeof(km), (float)((c->km_total - c->shift.km_start) * 1000.0));
    pair(&k, T(c, "Strecke", "Distance"), km);
    snprintf(v, sizeof(v), "%s", ts_cr(c->german, c->shift.tips));
    pair(&k, T(c, "Trinkgeld", "Tips"), v);
    if (c->shift.rated) {
        float rep = c->reputation - c->shift.rep_start;
        snprintf(k.note, sizeof(k.note), "%s %s · %s %+.0f%s", T(c, "Beste Fahrt", "Best job"), ts_cr(c->german, c->shift.best_pay), T(c, "Ruf", "Reputation"),
                 (double)rep, c->shift.goal_done ? T(c, " · Tagesziel geschafft", " · day's goal reached") : "");
    } else {
        snprintf(k.note, sizeof(k.note), "%s", T(c, "Heute keine Aufträge. Morgen wieder!", "No jobs today. See you tomorrow!"));
    }
    push(c, &k);
}

void card_tier(TsCore *c, int tier) {
    TsCard k;
    memset(&k, 0, sizeof(k));
    k.kind = TS_CARD_TIER;
    k.secs = 7.0f;
    k.tier = tier;
    k.money = -1;
    k.stars = -1;
    snprintf(k.title, sizeof(k.title), "%s", T(c, "NEUE STUFE", "NEW TIER"));
    snprintf(k.name, sizeof(k.name), c->german ? "%s-Fahrer" : "%s driver", ts_tier_name(tier, c->german));
    snprintf(k.note, sizeof(k.note), "%s", market_tier_perks(c, tier));
    push(c, &k);
}
