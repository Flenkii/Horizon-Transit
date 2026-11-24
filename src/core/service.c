/* 0.5: the little things a real ride service has (ideas for 1.0, see docs/IDEAS-1.0.md):

   - Requests (0.13 like a driver app): after GO a request comes every few seconds, one at a time
     ("New request: Lena, 400 m away"). It waits TS_REQUEST_S seconds for a yes, then the next one comes.
   - Lost items: now and then a passenger leaves something in the car and calls you a little later. Bring it
     back to where you dropped them off (GPS is set) for a thank-you and a little reputation.
   - The streak: jobs in a row with 4 or 5 stars. From the third one on, each pays a bonus (5 % per job, up to 20 %).
   - The logbook: the last 5 jobs (the tablet shows it).
   - The taxi meter (rides) and the fares (bus) on the ride panel, and the tablet's detail column.
   Pure C like the rest of core/. */
#include "jobs_internal.h"
#include "people.h"
#include <stdio.h>
#include <string.h>

#define HAIL_FIRST_S 6.0  /* 0.13: the first request after GO or after a job: 6 to 14 s */
#define HAIL_GAP_S 12.0   /* 0.13: after a no or a missed one: 12 to 30 s (busy areas faster, quiet ones slower) */
#define LOST_CHANCE 0.15f /* 0.8: was 0.07, too rare to ever see */
#define LOST_REWARD 60

/* what they left, and how they say it */
static const char *const call_de[] = {
    "Meine Sonnenbrille liegt noch bei dir im Auto! Bringst du sie mir zurück?",
    "Mein Handy liegt noch bei dir im Auto! Ich rufe von einem anderen an. Bringst du es mir zurück?",
    "Meine Geldbörse ist noch bei dir im Auto! Bringst du sie mir bitte zurück?",
    "Ich hab meine Jacke bei dir im Auto vergessen. Bringst du sie mir zurück?",
    "Meine Schlüssel liegen noch bei dir im Auto! Ohne die komm ich nicht rein.",
    "Meine Kamera liegt noch auf dem Rücksitz! Bringst du sie mir zurück?",
    "Mein Regenschirm ist noch bei dir im Auto. Bringst du ihn mir zurück?",
    "Mein Rucksack liegt noch bei dir im Auto! Bringst du ihn mir zurück?"};
static const char *const call_en[] = {
    "I left my sunglasses in your car! Could you bring them back?",
    "My phone's still in your car! I'm calling from a friend's. Could you bring it back?",
    "My wallet's still in your car! Could you bring it back, please?",
    "I forgot my jacket in your car. Could you bring it back?",
    "My keys are still in your car! I can't get in without them.",
    "My camera's still on the back seat! Could you bring it back?",
    "My umbrella's still in your car. Could you bring it back?",
    "My backpack's still in your car! Could you bring it back?"};
#define ITEMS (int)(sizeof(call_de) / sizeof(call_de[0]))

/* ------------------------------------------------------------------ push requests */

void service_next_request(TsCore *c, int first) {
    double wait = first ? HAIL_FIRST_S + 8.0 * random_unit(c) : HAIL_GAP_S + 18.0 * random_unit(c);
    if (c->demand == 2) wait *= 0.6;
    else if (c->demand == 0) wait *= 1.5;
    c->hail_at = c->t + wait;
}

static void hail(TsCore *c) {
    ts_refresh_offers(c);
    if (!c->offer_count) return;
    /* the nearest request is the one that comes in, alone, like in a driver app */
    int best = 0;
    for (int i = 1; i < c->offer_count; ++i)
        if (c->offers[i].pickup_m < c->offers[best].pickup_m) best = i;
    c->offers[0] = c->offers[best];
    c->offer_count = 1;
    c->req_until = c->t + TS_REQUEST_S;
    c->phone_sel = 0;
    const TsOffer *o = &c->offers[0];
    char title[TS_TEXT], text[TS_TEXT], km[24];
    format_km(c, km, sizeof(km), o->pickup_m);
    snprintf(title, sizeof(title), "%s · %s", o->regular ? T(c, "Stammgast fragt nach dir", "A regular asks for you")
                                                           : T(c, "Neue Anfrage", "New request"), ts_cr(c->german, o->pay));
    snprintf(text, sizeof(text), "%s, %s %s · %s %s", ts_passenger_name(o->passenger), km, T(c, "entfernt", "away"),
             T(c, "nach", "to"), ts_place_name(c, o->dest));
    toast(c, TS_ICON_ROADS, TS_SOUND_REQUEST, title, text);
}

/* ------------------------------------------------------------------ lost items */

void service_maybe_lost(TsCore *c, int person, int place) {
    if (c->extras_off || c->lost.state || random_unit(c) >= LOST_CHANCE) return;
    c->lost.state = 1;
    c->lost.person = person;
    c->lost.place = place;
    c->lost.item = random_below(c, ITEMS);
    c->lost.at = c->t + 25.0 + 20.0 * random_unit(c); /* they notice a little later */
}

static void lost_step(TsCore *c) {
    const float *at = ts_places[c->lost.place].pos;
    char text[TS_TEXT];
    if (c->lost.state == 1 && c->t >= c->lost.at) {
        float away = c->car_ok ? ts_distance_xz(c->car.pos, at) : 0.0f;
        if (away < 150.0f) { /* still there: they just come back and take it */
            c->lost.state = 0;
            return;
        }
        c->lost.state = 2;
        c->lost.until = c->t + away * TS_ROAD_FACTOR / TS_UBER_AVG_SPEED * c->time_factor + 90.0;
        snprintf(text, sizeof(text), "%s", c->german ? call_de[c->lost.item] : call_en[c->lost.item]);
        event(c, TS_EV_FEED, 0, 0, ts_passenger_name(c->lost.person), text);
        toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Fundsache im Auto", "Lost item in your car"), ts_place_name(c, c->lost.place));
        set_nav(c, at);
        return;
    }
    if (c->lost.state != 2) return;
    if (arrived_at(c, at) && c->speed < TS_STOPPED_SPEED) {
        c->lost.state = 0;
        c->wallet += LOST_REWARD * c->cr_scale; /* 0.13: CR */
        c->reputation = clampf(c->reputation + 1.0f, 0.0f, 100.0f);
        c->nav_on = 0;
        c->nav_serial++;
        snprintf(text, sizeof(text), "+%s · %s +1", ts_cr(c->german, LOST_REWARD * c->cr_scale), T(c, "Ruf", "reputation"));
        toast(c, TS_ICON_PAYOUT, TS_SOUND_PAYOUT, T(c, "Fundsache zurückgegeben", "Lost item returned"), text);
        event(c, TS_EV_FEED, 0, 0, ts_passenger_name(c->lost.person), T(c, "Du bist ein Schatz, danke!", "You're a star, thank you!"));
        event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
    } else if (c->t > c->lost.until) {
        c->lost.state = 0;
        c->nav_on = 0;
        c->nav_serial++;
        toast(c, TS_ICON_NONE, 0, T(c, "Fundsache", "Lost item"), T(c, "Zu spät: Sie geht per Post zurück.", "Too late: it goes back by post."));
    }
}

void service_job_starts(TsCore *c) {
    career_auto_shift(c);
    if (c->lost.state == 2) {
        toast(c, TS_ICON_NONE, 0, T(c, "Fundsache", "Lost item"), T(c, "Sie geht per Post zurück.", "It goes back by post."));
    }
    c->lost.state = 0;
    c->hail_at = 0.0;
    c->req_until = 0.0;
}

void service_idle_step(TsCore *c) {
    if (c->lost.state) {
        lost_step(c);
        return; /* no push requests while bringing something back */
    }
    if (!c->shift.on || !c->car_ok) {
        if (!c->shift.on) c->hail_at = 0.0, c->req_until = 0.0;
        return;
    }
    if (c->offer_count && c->req_until > 0.0) { /* 0.13: a request waits for a yes */
        if (c->t < c->req_until) return;
        c->offer_count = 0, c->req_until = 0.0, c->phone_sel = 0;
        toast(c, TS_ICON_NONE, 0, T(c, "Anfrage verpasst", "Request missed"), T(c, "Die nächste kommt gleich.", "The next one is on its way."));
        service_next_request(c, 0);
        return;
    }
    if (c->hail_at <= 0.0) {
        service_next_request(c, 1);
        return;
    }
    /* 0.13: also while waiting at the roadside and with the phone open (Marvin: GO, then just wait) */
    if (c->t >= c->hail_at) {
        hail(c);
        c->hail_at = 0.0;
    }
}

/* ------------------------------------------------------------------ the streak, the logbook */

int service_streak(TsCore *c, int stars, int pay) {
    if (stars < 4) {
        c->streak = 0;
        return 0;
    }
    c->streak++;
    if (c->extras_off || c->streak < 3) return 0;
    int pct = 5 * (c->streak - 2);
    if (pct > 20) pct = 20;
    int bonus = pay * pct / 100;
    c->wallet += bonus;
    if (c->shift.on) c->shift.earned += bonus;
    return bonus;
}

void service_streak_feed(TsCore *c, int bonus) {
    if (bonus <= 0) return;
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%s %d · +%s %s", T(c, "Serie", "Streak"), c->streak, ts_cr(c->german, bonus), T(c, "Bonus", "bonus"));
    event(c, TS_EV_FEED, 0, 0, "Horizon Transit", text);
}

void service_log(TsCore *c, int bus, int who, int place, int pay, int stars) {
    memmove(&c->log[1], &c->log[0], sizeof(TsLogEntry) * (TS_LOG - 1));
    c->log[0].bus = bus, c->log[0].who = who, c->log[0].place = place, c->log[0].pay = pay, c->log[0].stars = stars;
    if (c->log_count < TS_LOG) c->log_count++;
}

/* ------------------------------------------------------------------ the HUD: meter and the tablet's column */

static void det(TsHud *h, int kind, const char *label, const char *value) {
    if (h->detail_count >= TS_DETAIL_ROWS) return;
    int i = h->detail_count++;
    snprintf(h->detail_label[i], sizeof(h->detail_label[i]), "%s", label ? label : "");
    snprintf(h->detail_value[i], sizeof(h->detail_value[i]), "%s", value ? value : "");
    h->detail_kind[i] = kind;
}

static void person_rows(const TsCore *c, TsHud *h, int p) {
    const TsPerson *who = &ts_people[p];
    char a[64];
    snprintf(a, sizeof(a), "%d · %s", who->age, c->german ? who->job_de : who->job_en);
    det(h, TS_DET_PAIR, who->name, a);
    if (p >= 0 && p < TS_PEOPLE_MAX && c->met[p] > 0) {
        snprintf(a, sizeof(a), "%d×", c->met[p]);
        det(h, TS_DET_PAIR, T(c, "Schon gefahren", "Rides together"), a);
    } else {
        det(h, TS_DET_PAIR, T(c, "Schon gefahren", "Rides together"), T(c, "noch nie", "never"));
    }
    if (p < (int)ts_people_count && ts_people_taste[p][0] >= 0)
        det(h, TS_DET_PAIR, T(c, "Mag", "Likes"), ts_group_name(ts_people_taste[p][0], c->german));
}

static void route_rows(const TsCore *c, TsHud *h, int line, int rev, int now) {
    const TsLine *l = &ts_lines[line];
    int n = (int)l->stop_count, room = TS_DETAIL_ROWS - h->detail_count;
    /* too many stops: the ones around the bus, always with the last one */
    int first = 0;
    if (n > room) {
        first = now - 2;
        if (first < 0) first = 0;
        if (first > n - room) first = n - room;
    }
    for (int i = first; i < n && h->detail_count < TS_DETAIL_ROWS; ++i) {
        int place = l->stops[rev ? n - 1 - i : i];
        int kind = now < 0 ? TS_DET_STOP : i < now ? TS_DET_STOP_DONE : i == now ? TS_DET_STOP_NOW : TS_DET_STOP;
        char v[40] = "";
        if (i == 0) snprintf(v, sizeof(v), "%s", T(c, "Start", "Start"));
        else if (i == n - 1) snprintf(v, sizeof(v), "%s", T(c, "Ende", "End"));
        det(h, kind, ts_place_name(c, place), v);
    }
}

static void log_rows(const TsCore *c, TsHud *h) {
    if (!c->log_count) {
        det(h, TS_DET_TEXT, T(c, "Noch keine Aufträge. Die letzten fünf stehen hier.", "No jobs yet. The last five show here."), NULL);
        return;
    }
    for (int i = 0; i < c->log_count; ++i) {
        const TsLogEntry *e = &c->log[i];
        char a[64], b[40];
        if (e->bus) snprintf(a, sizeof(a), "%s %s · %s", T(c, "Linie", "Line"), ts_lines[e->who].number, ts_line_title(c, e->who));
        else snprintf(a, sizeof(a), "%s · %s", ts_passenger_name(e->who), ts_place_name(c, e->place));
        snprintf(b, sizeof(b), "%s · %d★", ts_cr(c->german, e->pay), e->stars);
        det(h, TS_DET_PAIR, a, b);
    }
}

void service_hud(const TsCore *c, TsHud *h) {
    h->meter = -1;
    char b[40];
    if (c->mode == TS_UBER && c->stage >= TS_UBER_TO_DEST) {
        /* 0.13: a fixed price agreed before the ride, like in a ride app (it used to count up with the road driven, which
           looked like faster driving earns faster); what's paid at the end also counts the mood (the card shows it) */
        h->meter = c->job.pay;
        if (c->job.pool) /* 0.6: a shared ride: what the first paid, and the one in the car now */
            h->meter = c->pool_paid + uber_pay(c->job.trip_m, 80.0f, c->job.ptype, c->job.pay_factor);
        snprintf(h->meter_label, sizeof(h->meter_label), "%s", T(c, "FAHRPREIS", "FARE"));
    } else if (c->mode == TS_BUS) {
        h->meter = c->bus_pay;
        snprintf(h->meter_label, sizeof(h->meter_label), "%s", T(c, "KASSE", "FARES"));
    }

    /* the tablet's column */
    if (c->mode == TS_UBER) {
        snprintf(h->detail_title, sizeof(h->detail_title), "%s", T(c, "Fahrgast und Strecke", "Passenger and route"));
        person_rows(c, h, c->job.passenger % (int)ts_people_count);
        det(h, c->stage < TS_UBER_TO_DEST ? TS_DET_STOP_NOW : TS_DET_STOP_DONE, ts_place_name(c, c->job.pickup), T(c, "Abholen", "Pick up"));
        det(h, c->stage >= TS_UBER_TO_DEST ? TS_DET_STOP_NOW : TS_DET_STOP, ts_place_name(c, c->job.dest), T(c, "Ziel", "Destination"));
        return;
    }
    if (c->mode == TS_BUS) {
        snprintf(h->detail_title, sizeof(h->detail_title), "%s %s · %s", T(c, "Linie", "Line"), ts_lines[c->line].number, ts_line_title(c, c->line));
        route_rows(c, h, c->line, c->line_rev, c->stop_i);
        return;
    }
    switch (c->phone_tab) {
    case TS_TAB_RIDES:
        if (c->offer_count) {
            const TsOffer *o = &c->offers[c->phone_sel < c->offer_count ? c->phone_sel : 0];
            snprintf(h->detail_title, sizeof(h->detail_title), "%s", o->regular ? T(c, "Stammgast", "Regular") : T(c, "Anfrage", "Request"));
            person_rows(c, h, o->passenger % (int)ts_people_count);
            det(h, TS_DET_PAIR, T(c, "Art", "Kind"), type_name(c, o->ptype));
            format_km(c, b, sizeof(b), o->pickup_m);
            det(h, TS_DET_STOP, ts_place_name(c, o->pickup), b);
            format_km(c, b, sizeof(b), o->trip_m);
            det(h, TS_DET_STOP, ts_place_name(c, o->dest), b);
            char t[16];
            format_time(t, sizeof(t), o->limit_s);
            snprintf(b, sizeof(b), "%s · %s", t, ts_cr(c->german, o->pay));
            det(h, TS_DET_PAIR, T(c, "Zeitlimit · Lohn", "Time limit · pay"), b);
        } else {
            snprintf(h->detail_title, sizeof(h->detail_title), "%s", T(c, "Fahrtenbuch", "Logbook"));
            log_rows(c, h);
        }
        break;
    case TS_TAB_BUS: {
        int li = c->phone_sel < (int)ts_line_count ? c->phone_sel : 0;
        snprintf(h->detail_title, sizeof(h->detail_title), "%s %s · %s", T(c, "Linie", "Line"), ts_lines[li].number, ts_line_title(c, li));
        route_rows(c, h, li, bus_direction(c, li), -1);
        break;
    }
    case TS_TAB_ACCOUNT:
        snprintf(h->detail_title, sizeof(h->detail_title), "%s", T(c, "Fahrtenbuch", "Logbook"));
        log_rows(c, h);
        if (c->shift.on) {
            snprintf(b, sizeof(b), "%d / %d", c->shift.jobs + c->shift.lines < TS_GOAL_JOBS ? c->shift.jobs + c->shift.lines : TS_GOAL_JOBS, TS_GOAL_JOBS);
            det(h, TS_DET_PAIR, c->shift.goal_done ? T(c, "Tagesziel geschafft", "Day's goal reached") : T(c, "Tagesziel", "Day's goal"), b);
        }
        if (c->streak >= 2) {
            snprintf(b, sizeof(b), "%d", c->streak);
            det(h, TS_DET_PAIR, T(c, "Serie (4+ Sterne)", "Streak (4+ stars)"), b);
        }
        break;
    default:
        snprintf(h->detail_title, sizeof(h->detail_title), "%s", T(c, "Fahrtenbuch", "Logbook"));
        log_rows(c, h);
        break;
    }
}
