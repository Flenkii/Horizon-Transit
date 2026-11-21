/* The career: the company's reputation, long-term statistics and the working day (shift).

   Reputation, 0..100, starts at 50. After each job: (stars - 3) * 2.5, so 5 stars +5 and 1 star -5; a special
   ride (VIP, business, wedding, trip) counts a bit more either way; cancelling with a passenger on board -4,
   before the pickup -1. It changes the pay (x0.9 at 0 up to x1.1 at 100) and which rides are offered
   (jobs.c: VIPs from 60, weddings from 45, business from 35; under 20 only two requests). */
#include "jobs_internal.h"
#include <stdio.h>
#include <string.h>

static const char *const group_de[TS_GROUPS] = {"Kleinwagen", "Kompaktsportler", "Luxuslimousine", "Sportwagen",
                                                "Supersportwagen", "Klassiker", "Geländewagen", "Bus"};
static const char *const group_en[TS_GROUPS] = {"small car", "hot hatch", "luxury saloon", "sports car",
                                                "supercar", "classic", "off-roader", "bus"};

const char *ts_group_name(int group, int german) {
    if (group < 0 || group >= TS_GROUPS) return german ? "Auto" : "car";
    return german ? group_de[group] : group_en[group];
}

const char *ts_rep_name(const TsCore *c) {
    float r = c->reputation;
    if (r < 25.0f) return T(c, "Schlecht", "Poor");
    if (r < 45.0f) return T(c, "Okay", "Okay");
    if (r < 65.0f) return T(c, "Gut", "Good");
    if (r < 82.0f) return T(c, "Sehr gut", "Very good");
    return "Top";
}

float career_pay_factor(const TsCore *c) {
    if (c->extras_off) return 1.0f;
    return (0.9f + 0.2f * clampf(c->reputation, 0.0f, 100.0f) / 100.0f) * market_tier_pay(c); /* 0.6: Gold +5 %, Platinum +10 % */
}

float career_job_done(TsCore *c, int stars, int pay, int base, int special, float trip_m, int bus_line) {
    float delta = (float)(stars - 3) * 2.5f;
    if (special) delta += stars >= 4 ? 1.5f : stars <= 2 ? -3.0f : 0.0f;
    if (delta > 0.0f && c->reputation > 80.0f) delta *= 0.6f; /* the top is hard to keep climbing */
    float before = c->reputation;
    c->reputation = clampf(c->reputation + delta, 0.0f, 100.0f);
    int tip = pay > base ? pay - base : 0;
    c->tips_total += tip;
    c->crashes_total += c->n_crash;
    if (stars >= 5) c->five_stars++;
    if (special) c->specials_done++;
    if (pay > c->best_pay) c->best_pay = pay;
    if (trip_m > c->longest_m) c->longest_m = trip_m;
    if (bus_line >= 0 && bus_line < 8) c->line_done[bus_line]++;
    if (c->shift.on) {
        if (bus_line >= 0) c->shift.lines++;
        else c->shift.jobs++;
        c->shift.earned += pay;
        c->shift.tips += tip;
        c->shift.stars_sum += stars;
        c->shift.rated++;
        if (pay > c->shift.best_pay) c->shift.best_pay = pay;
    }
    return c->reputation - before;
}

/* 0.5: the day's goal: 5 jobs with 4 stars on average in one shift pays a bonus once */
void career_goal_check(TsCore *c) {
    if (!c->shift.on || c->shift.goal_done || c->extras_off) return;
    int jobs = c->shift.jobs + c->shift.lines;
    if (jobs < TS_GOAL_JOBS || (float)c->shift.stars_sum < TS_GOAL_STARS * (float)c->shift.rated) return;
    c->shift.goal_done = 1;
    c->wallet += TS_GOAL_BONUS;
    c->shift.earned += TS_GOAL_BONUS;
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%d %s · %s %.0f+ · +%d €", TS_GOAL_JOBS, T(c, "Aufträge", "jobs"), T(c, "Schnitt", "average"),
             (double)TS_GOAL_STARS, TS_GOAL_BONUS);
    toast(c, TS_ICON_TROPHY, TS_SOUND_NOTIFY, T(c, "Tagesziel geschafft", "Day's goal reached"), text);
    event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
}

void career_cancel(TsCore *c, int passenger_on_board) {
    c->reputation = clampf(c->reputation - (passenger_on_board ? 4.0f : 1.0f), 0.0f, 100.0f);
}

/* 0.8: the shift also starts by itself with the first ride or line (like going online in a ride app), so push
   requests come without looking for "Start shift" first */
void career_auto_shift(TsCore *c) {
    if (!c->shift.on && c->mode == TS_IDLE) ts_shift_toggle(c);
}

void ts_shift_toggle(TsCore *c) {
    if (c->mode != TS_IDLE) return;
    if (!c->shift.on) {
        memset(&c->shift, 0, sizeof(c->shift));
        c->shift.on = 1;
        c->shift.start_t = c->t;
        c->shift.km_start = c->km_total;
        c->shift.rep_start = c->reputation;
        char goal[TS_TEXT];
        snprintf(goal, sizeof(goal), "%s %d %s, %s %.0f %s. %s", T(c, "Tagesziel:", "Day's goal:"), TS_GOAL_JOBS, T(c, "Aufträge", "jobs"),
                 T(c, "Schnitt", "average"), (double)TS_GOAL_STARS, T(c, "Sterne", "stars"),
                 T(c, "Anfragen kommen jetzt von selbst.", "Requests now come to you."));
        toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Schicht beginnt", "Shift started"), goal);
        return;
    }
    /* the day's statement: 0.6 a card with the review (the pop-ups only without the ride panel) */
    if (c->cards_on) {
        card_shift(c);
        sound(c, TS_SOUND_PAYOUT);
        c->shift.on = 0;
        return;
    }
    char title[TS_TEXT], text[TS_TEXT], km[24], time[16];
    format_km(c, km, sizeof(km), (float)((c->km_total - c->shift.km_start) * 1000.0));
    format_time(time, sizeof(time), (c->t - c->shift.start_t) / 60.0); /* h:mm */
    snprintf(title, sizeof(title), "%s · %lld €", T(c, "Feierabend", "End of shift"), c->shift.earned);
    if (c->shift.rated)
        snprintf(text, sizeof(text), "%d %s · %d %s · %s · %s h · %.1f %s", c->shift.jobs, T(c, "Fahrten", "rides"), c->shift.lines,
                 T(c, "Linien", "lines"), km, time, (double)c->shift.stars_sum / c->shift.rated, T(c, "Sterne", "stars"));
    else snprintf(text, sizeof(text), "%s · %s · %s h", T(c, "Heute keine Aufträge", "No jobs today"), km, time);
    toast(c, TS_ICON_TROPHY, TS_SOUND_NOTIFY, title, text);
    if (c->shift.rated) {
        snprintf(text, sizeof(text), "%s %lld € · %s %d € · %s %+.0f", T(c, "Trinkgeld", "Tips"), c->shift.tips,
                 T(c, "Beste Fahrt", "Best job"), c->shift.best_pay, T(c, "Ruf", "Reputation"),
                 (double)(c->reputation - c->shift.rep_start));
        event(c, TS_EV_FEED, 0, 0, T(c, "Abrechnung", "Statement"), text);
    }
    c->shift.on = 0;
}

/* the Account tab: the shift switch first (the one thing to pick), then the numbers */
void career_account_rows(const TsCore *c, TsHud *h) {
    char a[64], b[48];
    if (c->shift.on) {
        char time[16];
        format_time(time, sizeof(time), (c->t - c->shift.start_t) / 60.0);
        snprintf(b, sizeof(b), "%s h · %lld €", time, c->shift.earned);
        row(h, T(c, "Feierabend machen", "End shift"), b, 0);
    } else {
        row(h, T(c, "Schicht beginnen", "Start shift"), "", 0);
    }
    snprintf(b, sizeof(b), "%d · %s", (int)(c->reputation + 0.5f), ts_rep_name(c));
    row(h, T(c, "Ruf", "Reputation"), b, 0);
    row_meter(h, c->reputation / 100.0f, 1);
    snprintf(b, sizeof(b), "%lld €", c->wallet);
    row(h, T(c, "Firmenkasse", "Company funds"), b, 0);
    {   /* 0.6: the driver's tier, the meter towards the next */
        char hint[40];
        market_tier_hint(c, hint, sizeof(hint));
        if (hint[0]) snprintf(b, sizeof(b), "%s · %s", ts_tier_name(c->tier, c->german), hint);
        else snprintf(b, sizeof(b), "%s", ts_tier_name(c->tier, c->german));
        row(h, T(c, "Fahrer-Stufe", "Driver tier"), b, 0);
        row_meter(h, market_tier_progress(c), 0);
    }
    snprintf(b, sizeof(b), "%d / %d", c->jobs_done, c->bus_lines_done);
    row(h, T(c, "Fahrten / Buslinien", "Rides / bus lines"), b, 0);
    if (c->rating_count) snprintf(b, sizeof(b), "%.1f / 5 (%d)", (double)c->rating_sum / c->rating_count, c->rating_count);
    else snprintf(b, sizeof(b), "-");
    row(h, T(c, "Bewertung", "Rating"), b, 0);
    char km[24];
    format_km(c, km, sizeof(km), (float)(c->km_total * 1000.0));
    snprintf(a, sizeof(a), "%s", T(c, "Gefahren", "Driven"));
    row(h, a, km, 0);
    int timed = c->on_time_total + c->late_total;
    int wishes = c->wishes_ok + c->wishes_failed;
    char p1[16] = "-", p2[16] = "-";
    if (timed) snprintf(p1, sizeof(p1), "%d%%", c->on_time_total * 100 / timed);
    if (wishes) snprintf(p2, sizeof(p2), "%d/%d", c->wishes_ok, wishes);
    if (c->shift.on && c->shift.rated)
        snprintf(h->footer, sizeof(h->footer), "%s: %d %s · %.1f %s · %s %lld €", T(c, "Heute", "Today"), c->shift.jobs + c->shift.lines,
                 T(c, "Aufträge", "jobs"), (double)c->shift.stars_sum / c->shift.rated, T(c, "Sterne", "stars"),
                 T(c, "Trinkgeld", "tips"), c->shift.tips);
    else
        snprintf(h->footer, sizeof(h->footer), "%s %lld € · %s %s · %s %s · %s %d €", T(c, "Trinkgeld", "Tips"), c->tips_total,
                 T(c, "Pünktlich", "On time"), p1, T(c, "Wünsche", "Wishes"), p2, T(c, "Rekord", "Best"), c->best_pay);
}
