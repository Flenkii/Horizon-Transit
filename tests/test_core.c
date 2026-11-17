/* Tests of the job system without the game: a simulated car drives to the targets.
   Build and run (Linux or Windows, any C compiler):
     cc -std=c11 -O1 -Isrc/core tests/test_core.c src/core/jobs.c src/core/stops_data.c -lm -o test_core && ./test_core */
#include "jobs.h"
#include "stops.h"
#include "lines.h"
#include "phone_draw.h"
#include "bus_sql.h"
#include "people.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond, ...)                                                                                               \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                                \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

static TsCar car;
static const float DT = 1.0f / 60.0f;

static void place_car(const float at[3]) {
    memset(&car, 0, sizeof(car));
    car.pos[0] = at[0], car.pos[1] = at[1], car.pos[2] = at[2];
    car.fwd[2] = 1.0f;
    car.right[0] = 1.0f;
}

static void events_print(TsCore *c, int verbose, int *toasts, int *feeds) {
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(c, ev, TS_MAX_EVENTS);
    for (int i = 0; i < n; ++i) {
        if (ev[i].kind == TS_EV_TOAST && toasts) (*toasts)++;
        if (ev[i].kind == TS_EV_FEED && feeds) (*feeds)++;
        if (verbose && (ev[i].kind == TS_EV_TOAST || ev[i].kind == TS_EV_FEED))
            printf("   [%s] %s | %s\n", ev[i].kind == TS_EV_TOAST ? "Popup" : "Chat", ev[i].title, ev[i].text);
    }
}

/* one frame: the car moves with its velocity; ts_step every 6 frames (100 ms) */
static void frame(TsCore *c, int index) {
    car.pos[0] += car.vel[0] * DT, car.pos[1] += car.vel[1] * DT, car.pos[2] += car.vel[2] * DT;
    ts_car_sample(c, DT, &car);
    if (index % 6 == 0) ts_step(c);
}

/* drives straight to `to` with `speed`, accelerating with `accel` and braking with `brake` (m/s^2), and stops */
static int drive_to(TsCore *c, const float to[3], float speed, float accel, float brake, int verbose) {
    float v = sqrtf(car.vel[0] * car.vel[0] + car.vel[2] * car.vel[2]);
    int frames = 0;
    for (; frames < 60 * 60 * 30; ++frames) {
        float dx = to[0] - car.pos[0], dz = to[2] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
        if (d < 8.0f && v < 0.3f) break;
        float want = speed;
        float stop_at = v * v / (2.0f * brake) + 6.0f;
        if (d < stop_at) want = 0.0f;
        if (v < want) v = fminf(want, v + accel * DT);
        else v = fmaxf(want, v - brake * DT);
        if (d > 0.1f) {
            car.fwd[0] = dx / d, car.fwd[2] = dz / d, car.fwd[1] = 0.0f;
            car.right[0] = car.fwd[2], car.right[2] = -car.fwd[0];
        }
        car.vel[0] = car.fwd[0] * v, car.vel[2] = car.fwd[2] * v, car.vel[1] = 0.0f;
        frame(c, frames);
        events_print(c, verbose, NULL, NULL);
    }
    car.vel[0] = car.vel[1] = car.vel[2] = 0.0f;
    return frames;
}

static void wait_s(TsCore *c, float seconds, int verbose) {
    for (int i = 0; i < (int)(seconds * 60.0f); ++i) {
        frame(c, i);
        events_print(c, verbose, NULL, NULL);
    }
}

static void test_stops_are_real(void) {
    CHECK(ts_place_count > 150, "places: %u", ts_place_count);
    CHECK(ts_line_count == 6, "lines: %u (the phone shows 6)", ts_line_count);
    for (unsigned l = 0; l < ts_line_count; ++l)
        for (unsigned s = 1; s < ts_lines[l].stop_count; ++s)
            CHECK(ts_distance_xz(ts_places[ts_lines[l].stops[s - 1]].pos, ts_places[ts_lines[l].stops[s]].pos) > 150.0f,
                  "line %s: stops %u and %u apart", ts_lines[l].number, s - 1, s);
    for (unsigned i = 0; i < ts_place_count; ++i) {
        const TsPlace *p = &ts_places[i];
        CHECK(p->pos[2] < 0.0f && p->pos[0] > -5000.0f && p->pos[0] < 11000.0f, "place %u out of the map", i);
    }
    for (unsigned l = 0; l < ts_line_count; ++l)
        for (unsigned s = 0; s < ts_lines[l].stop_count; ++s) CHECK(ts_lines[l].stops[s] < ts_place_count, "stop index");
}

static void test_uber_ride(int verbose, int rough) {
    TsCore c;
    ts_init(&c, 12345u + (unsigned)rough, 1);
    place_car(ts_places[0].pos); /* at the festival */
    wait_s(&c, 0.5f, 0);
    ts_phone_toggle(&c);
    CHECK(c.phone_open && c.offer_count == TS_PHONE_OFFERS, "offers: %d", c.offer_count);
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.panel && h.row_count == 8 && h.row_selected == 2 && h.tab_count == TS_TABS, "phone panel rows %d sel %d", h.row_count, h.row_selected);
    if (verbose) {
        printf("  Handy:\n");
        for (int i = 0; i < h.row_count; ++i)
            printf("   %s %-40s %s\n", i == h.row_selected ? ">" : " ", h.row_label[i], h.row_value[i]);
        printf("   %s\n", h.footer);
    }
    TsOffer offer = c.offers[0];
    for (int i = 0; i < c.offer_count; ++i) {
        float pickup = ts_distance_xz(ts_places[0].pos, ts_places[c.offers[i].pickup].pos);
        CHECK(pickup >= 100.0f && pickup <= 2200.0f, "pickup distance %f", pickup);
        CHECK(c.offers[i].pickup != c.offers[i].dest, "pickup == dest");
    }
    unsigned serial = c.nav_serial;
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER && c.stage == TS_UBER_TO_PICKUP, "started");
    CHECK(c.nav_on && c.nav_serial != serial, "GPS set to the pickup");
    {   /* the phone during the job: app heading, all 8 rows, cancel selected last */
        TsHud j;
        int was = c.phone_open;
        c.phone_open = 1;
        ts_hud(&c, &j);
        CHECK(j.row_count == 8 && j.row_heading[0] == 1 && j.row_selected == 7, "job phone rows %d sel %d", j.row_count, j.row_selected);
        c.phone_open = was;
        for (int t = 0; t < TS_TABS; ++t) { /* every tab fits the 8 rows and starts with the app's heading */
            TsCore k = c;
            k.mode = TS_IDLE, k.phone_open = 1, k.phone_tab = t, k.phone_sel = 0;
            ts_hud(&k, &j);
            CHECK(j.row_count <= 8 && j.row_heading[0] == 1 && j.row_selected < j.row_count, "tab %d rows %d sel %d", t, j.row_count, j.row_selected);
        }
    }
    CHECK(ts_distance_xz(c.nav_target, ts_places[offer.pickup].pos) < 0.1f, "GPS target = pickup");
    events_print(&c, verbose, NULL, NULL);

    drive_to(&c, ts_places[offer.pickup].pos, 20.0f, 2.5f, 3.0f, verbose);
    wait_s(&c, 0.5f, verbose);
    CHECK(c.stage == TS_UBER_BOARDING, "boarding at the pickup (stage %d)", c.stage);
    wait_s(&c, 3.5f, verbose);
    CHECK(c.stage == TS_UBER_TO_DEST, "passenger in (stage %d)", c.stage);
    CHECK(ts_distance_xz(c.nav_target, ts_places[offer.dest].pos) < 0.1f, "GPS target = destination");

    /* rough driving: hard acceleration and braking */
    drive_to(&c, ts_places[offer.dest].pos, rough ? 45.0f : 20.0f, rough ? 7.0f : 2.0f, rough ? 9.5f : 3.0f, verbose);
    float sat = c.sat;
    long long before = c.wallet;
    wait_s(&c, 2.5f, verbose);
    CHECK(c.mode == TS_IDLE, "ride finished (mode %d stage %d)", c.mode, c.stage);
    CHECK(!c.nav_on, "GPS cleared");
    CHECK(c.wallet > before, "paid");
    CHECK(c.jobs_done == 1, "jobs done");
    CHECK(c.met[offer.passenger] == 1, "%s counts as met", ts_passenger_name(offer.passenger));
    if (rough) CHECK(sat < 70.0f && (c.n_brake + c.n_accel + c.n_speeding) > 0, "rough ride is penalised: %.1f", sat);
    else CHECK(sat >= 85.0f && c.n_brake + c.n_accel + c.n_crash == 0, "smooth ride is rewarded: %.1f", sat);
    if (verbose) printf("  -> %s: Zufriedenheit %.0f%%, Konto %lld CR\n", rough ? "ruppig" : "ruhig", c.sat, c.wallet);
}

static void test_crash(void) {
    TsCore c;
    ts_init(&c, 7u, 0);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.2f, 0);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    c.stage = TS_UBER_TO_DEST; /* the passenger is in */
    c.sat = 80.0f;
    car.vel[2] = 20.0f;
    wait_s(&c, 1.0f, 0);
    car.vel[2] = 2.0f; /* 18 m/s gone in one frame: a crash */
    wait_s(&c, 0.1f, 0);
    CHECK(c.n_crash == 1 && c.sat <= 72.5f, "crash: %d %.1f", c.n_crash, c.sat);
    /* a rewind (a jump of 300 m) is not a crash */
    car.pos[2] += 300.0f;
    car.vel[2] = 25.0f;
    wait_s(&c, 0.1f, 0);
    CHECK(c.n_crash == 1, "rewind is no crash");
    ts_cancel(&c);
    CHECK(c.mode == TS_IDLE && c.rating_count == 1, "cancel with passenger: rated");
}

static void test_bus_line(int verbose) {
    for (unsigned seed = 1; seed <= 40; ++seed) {
        TsCore c;
        ts_init(&c, seed * 7919u, 1);
        c.bus_capacity = 40;
        int line = (int)(seed % ts_line_count);
        place_car(ts_places[ts_lines[line].stops[0]].pos);
        wait_s(&c, 0.2f, 0);
        ts_accept_row(&c, TS_PHONE_OFFERS + line);
        CHECK(c.mode == TS_BUS, "bus started");
        int show = verbose && seed == 1;
        int stops_seen = 0, max_onboard = 0;
        for (unsigned s = 0; s < ts_lines[line].stop_count && c.mode == TS_BUS; ++s) {
            const float *stop = ts_places[ts_lines[line].stops[c.stop_i]].pos;
            if (s) drive_to(&c, stop, 16.0f, 1.5f, 2.5f, show);
            wait_s(&c, 0.5f, show);
            CHECK(c.stage == TS_BUS_AT_STOP || c.mode == TS_IDLE, "at the stop (seed %u stop %u stage %d)", seed, s, c.stage);
            if (c.mode != TS_BUS) break;
            int on_before = c.onboard, out = c.to_alight, in = c.to_board;
            CHECK(out <= on_before, "never more out than on board (%d > %d)", out, on_before);
            CHECK(on_before - out + in <= c.bus_capacity, "never more than fit");
            if (show) printf("  Halt %u %-40s: %d im Bus, %d aus, %d ein\n", s + 1, ts_places[ts_lines[line].stops[s]].name, on_before, out, in);
            TsHud h;
            wait_s(&c, 1.0f, show);
            ts_hud(&c, &h);
            CHECK(h.countdown > 0 || c.stage == TS_BUS_TO_STOP || c.mode == TS_IDLE, "boarding countdown");
            wait_s(&c, c.stop_secs + 0.5f, show);
            CHECK(c.mode == TS_IDLE || c.onboard == on_before - out + in, "counts add up (%d != %d - %d + %d)", c.onboard, on_before, out, in);
            if (c.onboard > max_onboard) max_onboard = c.onboard;
            stops_seen++;
        }
        CHECK(c.mode == TS_IDLE && c.bus_lines_done == 1, "line done (seed %u, stops %d)", seed, stops_seen);
        CHECK(c.onboard == 0 && c.delivered == c.board_total, "everybody got off at the end (%d, %d/%d)", c.onboard, c.delivered, c.board_total);
        CHECK(c.wallet > 0, "fares paid");
        if (show) printf("  -> Linie fertig: %d Fahrgäste befördert, Konto %lld CR, Zufriedenheit %.0f%%\n", c.delivered, c.wallet, c.sat);
    }
}

static void test_bus_leave_early(void) {
    TsCore c;
    ts_init(&c, 99u, 1);
    place_car(ts_places[ts_lines[0].stops[0]].pos);
    wait_s(&c, 0.2f, 0);
    ts_accept_row(&c, TS_PHONE_OFFERS);
    wait_s(&c, 0.5f, 0);
    CHECK(c.stage == TS_BUS_AT_STOP && c.to_board >= 3, "first stop boards");
    wait_s(&c, 2.6f, 0); /* some are in */
    int in = c.boarded_now;
    car.vel[2] = 8.0f; /* drive off */
    wait_s(&c, 0.3f, 0);
    CHECK(c.stage == TS_BUS_TO_STOP, "left the stop");
    CHECK(c.onboard == in && c.onboard < c.board_total + 1, "only those who got in ride (%d, %d)", c.onboard, in);
    CHECK(c.sat < 80.0f, "left behind: less happy");
}

static void test_phone_cancel(void) {
    TsCore c;
    ts_init(&c, 5u, 1);
    place_car(ts_places[3].pos);
    wait_s(&c, 0.2f, 0);
    ts_phone_toggle(&c);
    ts_phone_next(&c);
    ts_phone_next(&c);
    ts_phone_next(&c); /* the first bus line */
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS && c.line == 0, "phone row 4 = line 1");
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS && c.cancel_armed, "first press only asks");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_IDLE && !c.nav_on, "second press cancels, GPS cleared");
}

/* arrow keys: up from the first row goes to the last, down back to the first */
static void test_phone_prev(void) {
    TsCore c;
    ts_init(&c, 6u, 1);
    place_car(ts_places[3].pos);
    wait_s(&c, 0.2f, 0);
    ts_phone_toggle(&c);
    ts_phone_prev(&c);
    CHECK(c.phone_tab == TS_TABS - 1, "up on the first ride = the last tab");
    ts_phone_next(&c);
    CHECK(c.phone_tab == TS_TAB_RIDES && c.phone_sel == 0, "down again = the first ride");
    ts_phone_tab(&c, 1);
    CHECK(c.phone_tab == TS_TAB_BUS && c.phone_sel == 0, "right = the bus tab");
    ts_phone_tab(&c, 1);
    ts_phone_prev(&c);
    CHECK(c.phone_tab == TS_TAB_BUS && c.phone_sel == (int)ts_line_count - 1, "up from the account = the last bus line");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS && c.line == (int)ts_line_count - 1, "the last row is the last bus line");
}

/* the driven car: the game's table, passenger kinds that depend on it */
static void test_cars_and_passengers(void) {
    CHECK(ts_car_count > 300, "cars in the table: %u", ts_car_count);
    const TsCarInfo *miura = ts_car_find("lam_miurap400_67");
    CHECK(miura && miura->kind == TS_CAR_DREAM && !strcmp(miura->make, "Lamborghini"), "the Miura is a dream car");
    const TsCarInfo *bus = ts_car_find("PG_BUS_14");
    CHECK(bus && bus->kind == TS_CAR_BUS, "the bus is the bus");
    TsCore c;
    ts_init(&c, 77u, 1);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    int fans = 0, total = 0, kinds[TS_P_KINDS] = {0};
    for (int i = 0; i < 60; ++i) {
        ts_refresh_offers(&c);
        for (int k = 0; k < c.offer_count; ++k) fans += c.offers[k].ptype == TS_P_FAN, total++;
    }
    CHECK(total > 0 && fans == 0, "no car fans without a known sporty car (%d of %d)", fans, total);
    ts_set_car(&c, "LAM_MiuraP400_67");
    CHECK(c.car_kind == TS_CAR_DREAM && !strcmp(c.car_name, "Lamborghini Miura P400"), "car name: %s", c.car_name);
    fans = total = 0;
    for (int i = 0; i < 60; ++i) {
        ts_refresh_offers(&c);
        for (int k = 0; k < c.offer_count; ++k) {
            const TsOffer *o = &c.offers[k];
            kinds[o->ptype]++, total++;
            if (o->ptype == TS_P_FAN) {
                fans++;
                CHECK(o->max_kmh == 180, "a fan in a dream car: 180 km/h");
            }
            if (o->ptype == TS_P_RELAXED) CHECK(o->max_kmh == 90, "relaxed: 90 km/h");
        }
    }
    CHECK(fans > total / 5 && kinds[TS_P_RELAXED] && kinds[TS_P_HURRY] && kinds[TS_P_NORMAL],
          "a mix of passengers in a dream car: fans %d, relaxed %d, hurry %d, normal %d of %d", fans, kinds[TS_P_RELAXED],
          kinds[TS_P_HURRY], kinds[TS_P_NORMAL], total);
    ts_set_car(&c, "FH2_SomeCarPack_20");
    CHECK(c.car_kind == TS_CAR_NORMAL, "a car the game's table doesn't have counts as a normal car");
}

/* bus lines need the bus (when the setting says so) */
static void test_bus_needs_bus(void) {
    TsCore c;
    ts_init(&c, 9u, 1);
    c.bus_needs_bus = 1;
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    ts_set_car(&c, "LAM_MiuraP400_67");
    ts_accept_row(&c, TS_PHONE_OFFERS);
    CHECK(c.mode == TS_IDLE, "no bus line in a Lamborghini");
    TsEvent e[TS_MAX_EVENTS];
    int n = ts_take_events(&c, e, TS_MAX_EVENTS);
    CHECK(n == 1 && e[0].kind == TS_EV_TOAST && strstr(e[0].title, "Bus"), "it says why");
    ts_set_car(&c, "PG_BUS_14");
    ts_accept_row(&c, TS_PHONE_OFFERS);
    CHECK(c.mode == TS_BUS, "a bus line in the bus");
}

/* the speed chip: the limit, and a warning over it */
static void test_speed_chip(void) {
    TsCore c;
    ts_init(&c, 4u, 1);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER, "ride taken");
    TsHud h;
    ts_hud(&c, &h);
    char want[32];
    snprintf(want, sizeof(want), "Max %d km/h", c.job.max_kmh);
    CHECK(!strcmp(h.chip2, want), "speed chip: %s", h.chip2);
    car.vel[0] = (float)(c.job.max_kmh + 20) / 3.6f;
    ts_car_sample(&c, DT, &car);
    ts_hud(&c, &h);
    CHECK(strstr(h.chip2, "Zu schnell") != NULL, "over the limit: %s", h.chip2);
    car.vel[0] = 0.0f;
}

/* the tokens say() fills in */
static int token_ok(const char *x) {
    static const char *const known[] = {"{car}", "{max}", "{n}", "{lastcar}", "{group}", "{limit}", "{place}"};
    for (unsigned i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
        if (!strncmp(x, known[i], strlen(known[i]))) return 1;
    return 0;
}

/* every kind of line has lines, in both languages, with only known tokens to fill */
static void test_lines(void) {
    int count = 0;
    for (int k = 0; k < L_KINDS; ++k) {
        CHECK(ts_line_sets[k].count >= 3, "kind %d has %u lines", k, ts_line_sets[k].count);
        for (unsigned i = 0; i < ts_line_sets[k].count; ++i, ++count) {
            const TsLineText *t = &ts_line_sets[k].lines[i];
            CHECK(t->de && t->en && t->de[0] && t->en[0], "kind %d line %u: both languages", k, i);
            for (const char *x = t->de; x && (x = strchr(x, '{')); ++x)
                CHECK(token_ok(x), "kind %d line %u: unknown token", k, i);
            for (const char *x = t->en; x && (x = strchr(x, '{')); ++x)
                CHECK(token_ok(x), "kind %d line %u: unknown token (en)", k, i);
            CHECK(strlen(t->de) < TS_TEXT && strlen(t->en) < TS_TEXT, "kind %d line %u fits", k, i);
        }
    }
    CHECK(ts_people_count >= 30 && ts_people_count <= TS_PEOPLE_MAX, "people: %u", ts_people_count);
    for (unsigned p = 0; p < ts_people_count; ++p) {
        const TsPerson *who = &ts_people[p];
        CHECK(who->name && who->job_de && who->job_en && who->age > 10, "person %u", p);
        for (unsigned q = 0; q < p; ++q) CHECK(strcmp(who->name, ts_people[q].name) != 0, "%s twice", who->name);
        for (int i = 0; i < TS_PERSON_LINES; ++i, ++count) {
            const TsLineText *t = &who->story[i];
            CHECK(t->de && t->en && t->de[0] && t->en[0] && !strchr(t->de, '{') && !strchr(t->en, '{'), "%s line %d", who->name, i);
            CHECK(strlen(t->de) < TS_TEXT && strlen(t->en) < TS_TEXT, "%s line %d fits", who->name, i);
        }
    }
    CHECK(count >= 600, "lines in all: %d", count);
}


/* the phone picture: every tab and a job, in every theme; texts inside the phone, the phone opaque, the
   corners clear. With PHONE_PREVIEW=<dir> it also writes each picture (raw BGRA + its texts) for
   tools/phone_preview.py. */
typedef struct {
    FILE *json;
    int count, outside, w, h;
} TextLog;

static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static void record_text(void *user, const TsPhoneText *t) {
    TextLog *l = (TextLog *)user;
    if (t->x < 0 || t->y < 0 || t->x + t->w > l->w || t->y + t->h > l->h || t->w <= 0 || t->h <= 0) l->outside++;
    if (l->json) {
        fprintf(l->json, "%s{\"text\":", l->count ? ",\n" : "");
        json_str(l->json, t->utf8);
        fprintf(l->json, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"size\":%d,\"weight\":%d,\"color\":%u,\"align\":%d,\"wrap\":%d}", t->x,
                t->y, t->w, t->h, t->size, t->weight, t->color, t->align, t->wrap);
    }
    l->count++;
}

/* about as wide as the preview's DejaVu font (wider than Segoe UI), so the previews don't cut texts the game shows */
static int measure_like_preview(void *user, const TsPhoneText *t) {
    (void)user;
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)t->utf8; *p; ++p) n += (*p & 0xC0) != 0x80;
    return (int)(n * t->size * (t->weight >= 600 ? 0.72f : 0.62f) + 0.5f);
}

static void draw_one(const TsHud *h, int theme, const char *name) {
    float scale = 1.0f;
    int w, hh;
    ts_phone_size(scale, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    TextLog log = {0};
    log.w = w, log.h = hh;
    if (dir) {
        snprintf(path, sizeof(path), "%s/%s_%d.json", dir, name, theme);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, measure_like_preview, &log};
    ts_phone_draw(h, theme, scale, "18:56", &cv);
    ts_phone_finish(theme, scale, &cv);
    CHECK(log.count >= 8 && log.outside == 0, "%s theme %d: %d texts, %d outside", name, theme, log.count, log.outside);
    CHECK((px[0] >> 24) == 0 && (px[w * hh - 1] >> 24) == 0, "%s: corners clear", name);
    CHECK((px[(hh / 2) * w + w / 2] >> 24) == 255, "%s: phone opaque", name);
    int bad = 0;
    for (int i = 0; i < w * hh; ++i) {
        uint32_t a = px[i] >> 24, r = (px[i] >> 16) & 255, g = (px[i] >> 8) & 255, b = px[i] & 255;
        bad += r > a || g > a || b > a;
    }
    CHECK(bad == 0, "%s: %d pixels not premultiplied", name, bad);
    if (log.json) {
        fprintf(log.json, "]}\n");
        fclose(log.json);
        snprintf(path, sizeof(path), "%s/%s_%d.bgra", dir, name, theme);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

/* a regular (met before) is greeted as one, and tells the story further */
static void test_regular(void) {
    int regular = 0, stories = 0;
    for (unsigned seed = 1; seed <= 12; ++seed) {
        TsCore c;
        ts_init(&c, 4242u * seed, 1);
        c.german = 1;
        for (int i = 0; i < TS_PEOPLE_MAX; ++i) c.met[i] = 2;
        place_car(ts_places[0].pos);
        wait_s(&c, 0.5f, 0);
        ts_phone_toggle(&c);
        TsOffer offer = c.offers[0];
        CHECK(c.offers[0].passenger != c.offers[1].passenger && c.offers[1].passenger != c.offers[2].passenger &&
                  c.offers[0].passenger != c.offers[2].passenger, "three different people asking");
        ts_phone_accept(&c);
        drive_to(&c, ts_places[offer.pickup].pos, 20.0f, 2.5f, 3.0f, 0);
        for (int f = 0; f < 60 * 4; ++f) {
            frame(&c, f);
            TsEvent ev[TS_MAX_EVENTS];
            int n = ts_take_events(&c, ev, TS_MAX_EVENTS);
            for (int i = 0; i < n; ++i) {
                if (ev[i].kind != TS_EV_FEED) continue;
                const TsLineSet *set = &ts_line_sets[L_GREET_REGULAR];
                for (unsigned k = 0; k < set->count; ++k) regular += !strncmp(ev[i].text, set->lines[k].de, 8);
            }
        }
        /* on the way: the person's own lines, from where the story stopped (met 2 -> line 4) */
        float to[3] = {ts_places[offer.dest].pos[0], ts_places[offer.dest].pos[1], ts_places[offer.dest].pos[2]};
        for (int f = 0; f < 60 * 240 && c.mode == TS_UBER; ++f) {
            float dx = to[0] - car.pos[0], dz = to[2] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
            float v = d > 50.0f ? 12.0f : 0.0f;
            car.vel[0] = d > 1 ? dx / d * v : 0, car.vel[2] = d > 1 ? dz / d * v : 0;
            frame(&c, f);
            TsEvent ev[TS_MAX_EVENTS];
            int n = ts_take_events(&c, ev, TS_MAX_EVENTS);
            for (int i = 0; i < n; ++i)
                if (ev[i].kind == TS_EV_FEED)
                    for (int k = 0; k < TS_PERSON_LINES; ++k)
                        if (!strcmp(ev[i].text, ts_people[offer.passenger].story[k].de)) {
                            stories++;
                            CHECK(k == 4 || k == 5 || k == 0, "story goes on at line 4, not %d", k);
                        }
        }
    }
    CHECK(regular >= 6, "regulars greeted as such: %d of 12", regular);
    CHECK(stories >= 6, "own stories told: %d", stories);
}


/* ------------------------------------------------------------------ 0.4 */

/* drives a whole ride: to the pickup, then wherever the GPS points (a wished stop first), calmly */
static int ride_through(TsCore *c, int offer, int collect_feed, char feed[][TS_TEXT], int *feed_n, int feed_max) {
    TsOffer o = c->offers[offer];
    ts_accept_row(c, offer);
    if (c->mode != TS_UBER) return 0;
    for (int legs = 0; legs < 8 && c->mode == TS_UBER; ++legs) {
        float to[3] = {c->nav_target[0], c->nav_target[1], c->nav_target[2]};
        int frames = 0;
        float v = sqrtf(car.vel[0] * car.vel[0] + car.vel[2] * car.vel[2]);
        for (; frames < 60 * 60 * 30; ++frames) {
            float dx = to[0] - car.pos[0], dz = to[2] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
            if (d < 8.0f && v < 0.3f) break;
            float want = d < v * v / 5.0f + 6.0f ? 0.0f : 15.0f;
            if (v < want) v = fminf(want, v + 2.0f * DT);
            else v = fmaxf(want, v - 2.5f * DT);
            if (d > 0.1f) car.fwd[0] = dx / d, car.fwd[2] = dz / d, car.right[0] = car.fwd[2], car.right[2] = -car.fwd[0];
            car.vel[0] = car.fwd[0] * v, car.vel[2] = car.fwd[2] * v;
            frame(c, frames);
            TsEvent ev[TS_MAX_EVENTS];
            int n = ts_take_events(c, ev, TS_MAX_EVENTS);
            for (int i = 0; i < n && collect_feed; ++i)
                if (ev[i].kind == TS_EV_FEED && *feed_n < feed_max) snprintf(feed[(*feed_n)++], TS_TEXT, "%s", ev[i].text);
            if (c->nav_target[0] != to[0] || c->nav_target[2] != to[2]) break; /* the GPS changed (a wished stop) */
        }
        car.vel[0] = car.vel[2] = 0.0f;
        for (int f = 0; f < 60 * 6 && c->mode == TS_UBER; ++f) {
            frame(c, f);
            TsEvent ev[TS_MAX_EVENTS];
            int n = ts_take_events(c, ev, TS_MAX_EVENTS);
            for (int i = 0; i < n && collect_feed; ++i)
                if (ev[i].kind == TS_EV_FEED && *feed_n < feed_max) snprintf(feed[(*feed_n)++], TS_TEXT, "%s", ev[i].text);
            if (c->nav_target[0] != to[0] || c->nav_target[2] != to[2]) break;
        }
    }
    (void)o;
    return c->mode == TS_IDLE;
}

static void test_reputation_and_shift(void) {
    TsCore c;
    ts_init(&c, 31337u, 1);
    CHECK(c.reputation == 50.0f, "reputation starts at 50");
    ts_set_car(&c, "AUD_RS5_11");
    CHECK(c.car_group == TS_GROUP_SALOON && c.car_index >= 0, "the car's group: %d", c.car_group);
    ts_set_car(&c, "SOME_CarPack_99");
    CHECK(c.car_group == -1 && c.car_index == -1, "a car pack's car has no group");
    ts_set_car(&c, "AUD_RS5_11");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    /* the shift: from the Account tab */
    ts_phone_toggle(&c);
    c.phone_tab = TS_TAB_ACCOUNT, c.phone_sel = 0;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.row_selected == 1 && strstr(h.row_label[1], "Schicht beginnen"), "account: shift row %d '%s'", h.row_selected, h.row_label[1]);
    CHECK(h.row_count <= 8, "account rows fit");
    ts_phone_accept(&c);
    CHECK(c.shift.on, "shift on");
    ts_hud(&c, &h);
    CHECK(strstr(h.row_label[1], "Feierabend"), "account: end shift '%s'", h.row_label[1]);
    c.phone_open = 0;
    ts_take_events(&c, (TsEvent[TS_MAX_EVENTS]){0}, TS_MAX_EVENTS);
    int done = 0;
    for (int r = 0; r < 3; ++r) {
        ts_phone_toggle(&c);
        if (!c.offer_count) break;
        done += ride_through(&c, 0, 0, NULL, NULL, 0);
    }
    CHECK(done == 3, "three rides: %d", done);
    CHECK(c.reputation > 50.0f, "calm rides raise the reputation: %.1f", (double)c.reputation);
    CHECK(c.shift.jobs == 3 && c.shift.rated == 3 && c.shift.earned > 0, "shift counts %d %d %lld", c.shift.jobs, c.shift.rated, c.shift.earned);
    CHECK(c.on_time_total + c.late_total == 3, "punctuality counted");
    CHECK(c.best_pay > 0 && c.longest_m > 0.0f, "best values");
    int p = c.job.passenger;
    CHECK(c.met_car[p] == c.car_index + 1 && c.met_stars[p] >= 1, "remembered: car %d stars %d", c.met_car[p], c.met_stars[p]);
    float before = c.reputation;
    ts_phone_toggle(&c);
    ts_accept_row(&c, 0);
    ts_cancel(&c);
    CHECK(c.reputation < before, "cancelling costs reputation");
    ts_phone_toggle(&c);
    c.phone_tab = TS_TAB_ACCOUNT, c.phone_sel = 0;
    ts_phone_accept(&c);
    CHECK(!c.shift.on, "shift ended");
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(&c, ev, TS_MAX_EVENTS), statement = 0;
    for (int i = 0; i < n; ++i) statement += ev[i].kind == TS_EV_TOAST && strstr(ev[i].title, "Feierabend") != NULL;
    CHECK(statement == 1, "the day's statement");
    /* a bad name: fewer requests */
    c.reputation = 10.0f;
    c.phone_open = 0;
    ts_phone_toggle(&c);
    CHECK(c.offer_count == 2, "reputation 10: two requests (%d)", c.offer_count);
}

static void test_specials(void) {
    int counts[TS_P_KINDS] = {0}, low[TS_P_KINDS] = {0}, offroad[TS_P_KINDS] = {0};
    for (unsigned seed = 1; seed <= 200; ++seed) {
        TsCore c;
        ts_init(&c, seed * 104729u, 1);
        place_car(ts_places[0].pos);
        wait_s(&c, 0.1f, 0);
        ts_set_car(&c, seed % 2 ? "AUD_RS5_11" : "AST_DB5Vantage_64");
        c.reputation = 85.0f;
        ts_refresh_offers(&c);
        int specials = 0;
        for (int i = 0; i < c.offer_count; ++i) {
            counts[c.offers[i].ptype]++;
            specials += c.offers[i].ptype >= TS_P_VIP;
            if (c.offers[i].ptype == TS_P_VIP)
                CHECK(c.offers[i].pay > 0 && c.offers[i].max_kmh == 130, "VIP offer");
        }
        CHECK(specials <= 1, "one special per list");
        c.reputation = 15.0f;
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) low[c.offers[i].ptype]++;
        ts_set_car(&c, "BMW_X5M_11");
        c.reputation = 50.0f;
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) offroad[c.offers[i].ptype]++;
    }
    CHECK(counts[TS_P_VIP] > 5 && counts[TS_P_BUSINESS] > 5 && counts[TS_P_WEDDING] > 3, "specials: VIP %d business %d wedding %d",
          counts[TS_P_VIP], counts[TS_P_BUSINESS], counts[TS_P_WEDDING]);
    CHECK(!low[TS_P_VIP] && !low[TS_P_BUSINESS] && !low[TS_P_WEDDING], "no specials with a bad name");
    CHECK(offroad[TS_P_TOURIST] > 10 && !offroad[TS_P_VIP], "off-roader: trips %d", offroad[TS_P_TOURIST]);
    /* the reputation changes the pay */
    TsCore a, b;
    ts_init(&a, 99u, 1), ts_init(&b, 99u, 1);
    place_car(ts_places[0].pos);
    ts_car_sample(&a, DT, &car), ts_car_sample(&b, DT, &car);
    a.reputation = 100.0f, b.reputation = 0.0f;
    ts_refresh_offers(&a), ts_refresh_offers(&b);
    TsCore off;
    ts_init(&off, 99u, 1);
    off.extras_off = 1;
    off.reputation = 10.0f;
    ts_car_sample(&off, DT, &car);
    ts_set_car(&off, "AUD_RS5_11");
    ts_refresh_offers(&off);
    CHECK(off.offer_count == TS_PHONE_OFFERS && off.offers[0].pay_factor > 0.99f, "switched off: like 0.3");
    CHECK(a.offers[0].pay_factor > b.offers[0].pay_factor * 1.2f, "good name pays more: x%.2f vs x%.2f", (double)a.offers[0].pay_factor,
          (double)b.offers[0].pay_factor);
}

static void test_wishes_and_memory(void) {
    int wishes = 0, stops = 0, memory = 0, rides = 0;
    static char feed[400][TS_TEXT];
    for (unsigned seed = 1; seed <= 16; ++seed) {
        TsCore c;
        ts_init(&c, seed * 6151u, 1);
        ts_set_car(&c, "AUD_RS5_11");
        for (int i = 0; i < TS_PEOPLE_MAX; ++i) c.met[i] = 1, c.met_car[i] = 1; /* last time: the first car of the list */
        place_car(ts_places[seed * 7 % ts_place_count].pos);
        wait_s(&c, 0.5f, 0);
        ts_phone_toggle(&c);
        if (c.offer_count < 2) continue;
        int n = 0;
        int ok = ride_through(&c, c.offer_count - 1, 1, feed, &n, 400); /* the long one */
        CHECK(ok, "ride finished (seed %u)", seed);
        rides += ok;
        wishes += c.wishes_ok + c.wishes_failed;
        stops += c.wish_stop_stage == 0 && c.wishes_ok > 0;
        for (int i = 0; i < n; ++i) {
            const TsLineSet *set = &ts_line_sets[L_REG_OTHER_CAR];
            for (unsigned k = 0; k < set->count; ++k) {
                const char *de = set->lines[k].de, *cut = strchr(de, '{');
                size_t len = cut ? (size_t)(cut - de) : strlen(de);
                if (len >= 6 && !strncmp(feed[i], de, len)) memory++;
            }
            CHECK(!strchr(feed[i], '{'), "a token left: %s", feed[i]);
        }
        CHECK(c.wish == TS_WISH_NONE, "no wish left open");
    }
    CHECK(rides >= 12, "rides %d", rides);
    CHECK(wishes >= 4, "wishes on long rides: %d", wishes);
    CHECK(memory >= 3, "regulars remember the other car: %d", memory);
    (void)stops;
}

static void test_timetable(void) {
    TsCore c;
    ts_init(&c, 4711u, 1);
    int line = 0;
    place_car(ts_places[ts_lines[line].stops[0]].pos);
    wait_s(&c, 0.2f, 0);
    ts_accept_row(&c, TS_PHONE_OFFERS + line);
    int chips = 0;
    for (unsigned s = 0; s < ts_lines[line].stop_count && c.mode == TS_BUS; ++s) {
        if (s) drive_to(&c, ts_places[ts_lines[line].stops[c.stop_i]].pos, 16.0f, 1.5f, 2.5f, 0);
        wait_s(&c, 0.5f, 0);
        TsHud h;
        ts_hud(&c, &h);
        chips += h.chip3[0] != 0;
        wait_s(&c, c.stop_secs + 1.5f, 0);
    }
    CHECK(c.mode == TS_IDLE, "line done");
    CHECK(c.stops_on_time + c.stops_late == (int)ts_lines[line].stop_count - 1, "every stop timed: %d + %d", c.stops_on_time, c.stops_late);
    CHECK(c.stops_on_time >= c.stops_late, "faster than the plan: on time %d late %d", c.stops_on_time, c.stops_late);
    CHECK(c.line_done[line] == 1, "line counted");
    CHECK(chips >= (int)ts_lines[line].stop_count - 1, "timetable chip shown (%d)", chips);
}

static void test_bus_sql(void) {
    char out[4096];
    CHECK(ts_bus_sql("SELECT Id FROM Data_Car WHERE Id=3", out, sizeof(out)) == 0, "other queries stay");
    CHECK(ts_bus_sql("CREATE VIEW Drivable_Data_Car AS SELECT Data_Car.* FROM Data_Car WHERE Id NOT IN (SELECT CarId FROM TrafficCars)", out,
                     sizeof(out)) == 0, "the view itself stays");
    CHECK(ts_bus_sql("INSERT INTO Data_Car_Buckets VALUES (1,2,3)", out, sizeof(out)) == 0, "writes stay");
    CHECK(ts_bus_sql("SELECT Id, MediaName FROM Drivable_Data_Car", out, sizeof(out)) == 1 &&
              strstr(out, "CarId<>414)) AS Drivable_Data_Car") != NULL, "drivable: %s", out);
    CHECK(ts_bus_sql("SELECT Garage.* FROM Drivable_Data_Car AS Garage LEFT JOIN Data_Car_Buckets ON Data_Car_Buckets.CarId=Garage.Id", out,
                     sizeof(out)) == 1 && strstr(out, ")) AS Garage") && strstr(out, "WHERE CarId=414)) AS Data_Car_Buckets ON Data_Car_Buckets.CarId"),
          "aliases: %s", out);
    CHECK(ts_bus_sql("SELECT 1 FROM CarBuckets cb INNER JOIN Data_Car_Buckets dcb ON dcb.CarBucket = cb.Id", out, sizeof(out)) == 1 &&
              strstr(out, "CarId=414)) dcb ON") && !strstr(out, "AS Data_Car_Buckets dcb"), "own alias: %s", out);
    CHECK(ts_bus_sql("SELECT COUNT(*) FROM TrafficCars WHERE CarId=414", out, sizeof(out)) == 1 &&
              !strcmp(out, "SELECT COUNT(*) FROM TrafficCars WHERE CarId<>414 AND CarId=414"), "traffic: %s", out);
    CHECK(ts_bus_sql("SELECT 'FROM Drivable_Data_Car' FROM Data_Car", out, sizeof(out)) == 0 || strstr(out, "'FROM Drivable_Data_Car'"),
          "texts in quotes stay");
    CHECK(ts_bus_sql("SELECT Id FROM Drivable_Data_Car", out, 40) == -1, "too small: the original is used");
}

static void test_shop_and_settings(void) {
    TsCore c;
    ts_init(&c, 2024u, 1);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    ts_phone_toggle(&c);
    c.phone_tab = TS_TAB_SHOP, c.phone_sel = TS_SHOP_BIGBUS;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.row_count == 2 + TS_SHOP_ITEMS && h.row_selected == 2 + TS_SHOP_BIGBUS, "shop rows %d sel %d", h.row_count, h.row_selected);
    c.wallet = 3000;
    ts_phone_accept(&c);
    CHECK(!ts_has(&c, TS_SHOP_BIGBUS) && c.wallet == 3000, "too little money: nothing bought");
    c.wallet = 12000;
    ts_phone_accept(&c);
    CHECK(ts_has(&c, TS_SHOP_BIGBUS) && c.wallet == 2000, "bought: %lld left", c.wallet);
    ts_phone_accept(&c);
    CHECK(c.wallet == 2000, "not bought twice");
    ts_step(&c);
    CHECK(c.bus_capacity == 60, "bigger bus: %d seats", c.bus_capacity);
    ts_hud(&c, &h);
    CHECK(strstr(h.row_value[2 + TS_SHOP_BIGBUS], "gekauft") != NULL, "shown as bought");
    /* the dispatch radio: more time */
    TsCore a, b;
    ts_init(&a, 55u, 1), ts_init(&b, 55u, 1);
    ts_car_sample(&a, DT, &car), ts_car_sample(&b, DT, &car);
    b.upgrades = 1u << TS_SHOP_DISPATCH;
    ts_refresh_offers(&a), ts_refresh_offers(&b);
    CHECK(b.offers[0].limit_s > a.offers[0].limit_s, "dispatch radio: %.0f > %.0f s", (double)b.offers[0].limit_s, (double)a.offers[0].limit_s);
    /* ads: three requests even with a poor name */
    a.reputation = b.reputation = 10.0f;
    b.upgrades = 1u << TS_SHOP_ADS;
    ts_refresh_offers(&a), ts_refresh_offers(&b);
    CHECK(a.offer_count == 2 && b.offer_count == 3, "ads: %d vs %d requests", a.offer_count, b.offer_count);
    /* settings: the glue's rows, Enter marks the pressed one */
    c.setting_count = 2;
    snprintf(c.setting_label[0], sizeof(c.setting_label[0]), "Sprache");
    snprintf(c.setting_value[0], sizeof(c.setting_value[0]), "Spiel");
    snprintf(c.setting_label[1], sizeof(c.setting_label[1]), "Zeitlimits");
    snprintf(c.setting_value[1], sizeof(c.setting_value[1]), "Normal");
    c.phone_tab = TS_TAB_SETTINGS, c.phone_sel = 0;
    ts_phone_next(&c);
    ts_hud(&c, &h);
    CHECK(h.row_count == 3 && h.row_selected == 2 && !strcmp(h.row_value[2], "Normal"), "settings rows %d sel %d", h.row_count, h.row_selected);
    ts_phone_accept(&c);
    CHECK(c.setting_pressed == 1, "pressed: %d", c.setting_pressed);
}

static void test_phone_draw(void) {
    TsCore c;
    ts_init(&c, 777u, 1);
    ts_set_car(&c, "LAM_MiuraP400_67");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    ts_phone_toggle(&c);
    static const char *const names[TS_TABS] = {"rides", "bus", "account", "shop", "settings"};
    static const char *const sl[8][2] = {{"Handy-Design", "Nacht"}, {"Sprache", "Spiel"}, {"Zeitlimits", "Normal"}, {"Fahrgäste meckern", "An"},
                                         {"Navigation automatisch", "An"}, {"Ruf, Sonderfahrten, Wünsche", "An"},
                                         {"Buslinien nur im Bus", "An"}, {"Stimmen (Versuch)", "Aus"}};
    static const char *const sl_en[8][2] = {{"Phone design", "Night"}, {"Language", "Game"}, {"Time limits", "Normal"}, {"Passenger complaints", "On"},
                                            {"Auto GPS", "On"}, {"Reputation, specials, wishes", "On"},
                                            {"Bus lines need the bus", "On"}, {"Voices (experimental)", "Off"}};
    c.setting_count = 8;
    c.wallet = 7400;
    c.upgrades = 1u << TS_SHOP_DISPATCH;
    for (int de = 0; de < 2; ++de) {
        c.german = de;
        for (int i = 0; i < 8; ++i) {
            snprintf(c.setting_label[i], sizeof(c.setting_label[i]), "%s", (de ? sl : sl_en)[i][0]);
            snprintf(c.setting_value[i], sizeof(c.setting_value[i]), "%s", (de ? sl : sl_en)[i][1]);
        }
        for (int tab = 0; tab < TS_TABS; ++tab) {
            c.phone_tab = tab, c.phone_sel = tab == TS_TAB_RIDES ? 1 : 0;
            TsHud h;
            ts_hud(&c, &h);
            char name[32];
            snprintf(name, sizeof(name), "%s_%s", names[tab], de ? "de" : "en");
            for (int t = 0; t < TS_THEMES; ++t) draw_one(&h, t, name);
        }
    }
    c.german = 1;
    c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    ts_phone_accept(&c);
    c.phone_open = 1;
    TsHud h;
    ts_hud(&c, &h);
    for (int t = 0; t < TS_THEMES; ++t) draw_one(&h, t, "job_de");
    c.german = 0, c.imperial = 1;
    ts_hud(&c, &h);
    for (int t = 0; t < TS_THEMES; ++t) draw_one(&h, t, "job_en");
}

/* an English game shows miles: the limit in mph (rounded to 5), the chip, the sign and the distances */
void format_km(const TsCore *c, char *out, size_t size, float metres);
void say(TsCore *c, int style, int kind, int force);
void complain(TsCore *c, int kind);
void toast(TsCore *c, int icon, int sound, const char *title, const char *text);
static void sign_one(int number, const char *unit, int flash, const char *name) {
    int w, hh;
    ts_sign_size(1.0f, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    TextLog log = {0};
    log.w = w, log.h = hh;
    if (dir) {
        snprintf(path, sizeof(path), "%s/%s_0.json", dir, name);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, NULL, &log};
    ts_sign_draw(number, unit, flash, 1.0f, &cv);
    ts_sign_finish(1.0f, &cv);
    CHECK(log.count >= 1 && log.outside == 0, "%s: %d texts, %d outside", name, log.count, log.outside);
    CHECK((px[0] >> 24) == 0, "%s: corner clear", name);
    uint32_t mid = px[(hh / 2) * w + w / 2];
    CHECK((mid >> 24) == 255, "%s: disc opaque", name);
    int bad = 0;
    for (int i = 0; i < w * hh; ++i) {
        uint32_t a = px[i] >> 24, r = (px[i] >> 16) & 255, g = (px[i] >> 8) & 255, b = px[i] & 255;
        bad += r > a || g > a || b > a;
    }
    CHECK(bad == 0, "%s: %d pixels not premultiplied", name, bad);
    if (log.json) {
        fprintf(log.json, "]}\n");
        fclose(log.json);
        snprintf(path, sizeof(path), "%s/%s_0.bgra", dir, name);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

static void test_imperial_and_sign(void) {
    TsCore c;
    ts_init(&c, 4u, 0);
    CHECK(ts_speed_shown(&c, 120) == 120 && !strcmp(ts_speed_unit(&c), "km/h"), "metric stays km/h");
    c.imperial = 1;
    CHECK(ts_speed_shown(&c, 120) == 75 && ts_speed_shown(&c, 50) == 30 && ts_speed_shown(&c, 80) == 50,
          "mph: %d %d %d", ts_speed_shown(&c, 120), ts_speed_shown(&c, 50), ts_speed_shown(&c, 80));
    CHECK(!strcmp(ts_speed_unit(&c), "mph"), "unit mph");
    char d[32];
    format_km(&c, d, sizeof(d), 3218.7f);
    CHECK(strstr(d, "mi") != NULL && strstr(d, "km") == NULL, "distance in miles: %s", d);
    format_km(&c, d, sizeof(d), 50.0f);
    CHECK(strstr(d, "ft") != NULL, "short distance in feet: %s", d);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER, "ride taken (imperial)");
    TsHud h;
    ts_hud(&c, &h);
    int lim = ts_speed_shown(&c, c.job.max_kmh);
    char want[32];
    snprintf(want, sizeof(want), "Max %d mph", lim);
    CHECK(!strcmp(h.chip2, want), "imperial chip: %s", h.chip2);
    CHECK(strstr(h.chip2, "km/h") == NULL, "no km/h in the chip");
    CHECK(h.sign_limit == 0, "no sign before the passenger is aboard");
    c.stage = TS_UBER_TO_DEST;
    ts_hud(&c, &h);
    CHECK(h.sign_limit == lim && !strcmp(h.sign_unit, "mph") && !h.sign_over, "sign %d %s %d", h.sign_limit, h.sign_unit, h.sign_over);
    /* the sign's limit is the shown mph value: over it (in km/h of that value) counts as too fast */
    car.vel[0] = ((float)lim * 1.609344f + 15.0f) / 3.6f;
    ts_car_sample(&c, DT, &car);
    ts_hud(&c, &h);
    CHECK(h.sign_over, "sign flashes when too fast");
    /* 0.4.3: yellow a little over, red from 6 mph over the sign (Marvin: "only yellow, never red") */
    for (int over = 1; over <= 12; ++over) {
        car.vel[0] = ((float)(lim + over) * 1.609344f) / 3.6f;
        for (int k = 0; k < 40; ++k) ts_car_sample(&c, DT, &car);
        ts_hud(&c, &h);
        int want_level = over > 6 ? 2 : over * 1.609344f > 2.0f ? 1 : 0;
        CHECK(h.sign_over == want_level, "%d mph over: level %d (want %d)", over, h.sign_over, want_level);
    }
    car.vel[0] = 0.0f;
    /* what passengers say about the limit is in mph too (Marvin's game: "75 km/h" in 0.4.1's first build) */
    TsEvent ev[TS_MAX_EVENTS];
    ts_take_events(&c, ev, TS_MAX_EVENTS);
    int mph_lines = 0, kmh_lines = 0;
    for (int i = 0; i < 40; ++i) {
        say(&c, 0, L_SPEEDING, 1);
        int k = ts_take_events(&c, ev, TS_MAX_EVENTS);
        for (int e = 0; e < k; ++e) mph_lines += strstr(ev[e].text, " mph") != NULL, kmh_lines += strstr(ev[e].text, "km/h") != NULL;
    }
    CHECK(mph_lines > 0 && kmh_lines == 0, "passenger lines in mph: %d mph, %d km/h", mph_lines, kmh_lines);
    sign_one(lim, "mph", 0, "sign_mph");
    sign_one(120, "km/h", 0, "sign_kmh");
    sign_one(lim, "mph", 1, "sign_over");
}

/* place and bus line names follow the language: no German words in an English game */
static void test_place_names(void) {
    TsCore c;
    ts_init(&c, 3u, 0);
    int bad = 0;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        const char *n = ts_place_name(&c, (int)i);
        bad += !n[0] || strstr(n, "Treffpunkt") || strstr(n, "Landstra") || strstr(n, "Nizza");
    }
    CHECK(bad == 0, "English place names: %d still German", bad);
    for (unsigned l = 0; l < ts_line_count; ++l) CHECK(strcmp(ts_line_title(&c, (int)l), ts_lines[l].title) != 0, "line %u has an English name", l + 1);
    CHECK(!strcmp(ts_place_name(&c, 5), "Nice"), "Nizza is Nice: %s", ts_place_name(&c, 5));
    c.german = 1;
    for (unsigned i = 0; i < ts_place_count; ++i) CHECK(ts_place_name(&c, (int)i) == ts_places[i].name, "German keeps the name");
    c.german = 0;
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    ts_phone_toggle(&c);
    TsHud h;
    ts_hud(&c, &h);
    int german_rows = 0;
    for (int r = 0; r < h.row_count; ++r) german_rows += strstr(h.row_label[r], "Treffpunkt") != NULL || strstr(h.row_value[r], "Treffpunkt") != NULL;
    german_rows += strstr(h.footer, "Treffpunkt") != NULL;
    CHECK(h.row_count > 0 && german_rows == 0, "the phone's ride list is English");
}

/* 0.4.1 (Marvin: "the croissant line three times in three rides", "long lines are cut off") */
static void dash_one(const TsHud *h, const char *name) {
    int w, hh;
    ts_dash_size(1.0f, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    TextLog log = {0};
    log.w = w, log.h = hh;
    if (dir) {
        snprintf(path, sizeof(path), "%s/%s_0.json", dir, name);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, measure_like_preview, &log};
    ts_dash_draw(h, 1.0f, &cv);
    ts_dash_finish(1.0f, &cv);
    CHECK(log.count >= 3 && log.outside == 0, "%s: %d texts, %d outside", name, log.count, log.outside);
    CHECK((px[0] >> 24) == 0, "%s: corner clear", name);
    int bad = 0;
    for (int i = 0; i < w * hh; ++i) {
        uint32_t a = px[i] >> 24, r = (px[i] >> 16) & 255, g = (px[i] >> 8) & 255, b = px[i] & 255;
        bad += r > a || g > a || b > a;
    }
    CHECK(bad == 0, "%s: %d pixels not premultiplied", name, bad);
    if (log.json) {
        fprintf(log.json, "]}\n");
        fclose(log.json);
        snprintf(path, sizeof(path), "%s/%s_0.bgra", dir, name);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

static void test_ride_variety(void) {
    TsCore c;
    ts_init(&c, 77u, 1);
    ts_set_car(&c, "LAM_MiuraP400_67");
    static int used[1024];
    memset(used, 0, sizeof(used));
    int rides = 0, repeats = 0;
    for (int r = 0; r < 12; ++r) {
        place_car(ts_places[0].pos); /* always from the festival, where it repeated most */
        wait_s(&c, 0.5f, 0);
        ts_phone_toggle(&c);
        ts_phone_accept(&c);
        if (c.mode != TS_UBER) { ts_phone_toggle(&c); continue; }
        rides++;
        repeats += used[c.job.dest]++ > 0;
        ts_cancel(&c);
        c.phone_open = 0;
    }
    CHECK(rides >= 10 && repeats == 0, "12 rides from one place: %d taken, %d destinations again", rides, repeats);
}

static void test_feed_window(void) {
    TsFeedLine l[3] = {{"Rosa", "Hi! I hope the air conditioning works.", 0},
                       {"Rosa (thinks)", "Hm. That was a bit rough.", 0},
                       {"Aylin", "It smells of tyres and candy floss here. Perfect. I could stay here all day long, honestly.", 2}};
    int w, hh;
    ts_feed_size(1.0f, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    TextLog log = {0};
    log.w = w, log.h = hh;
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    if (dir) {
        snprintf(path, sizeof(path), "%s/feed_0.json", dir);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, measure_like_preview, &log};
    ts_feed_draw(l, 3, 1.0f, &cv);
    ts_feed_finish(1.0f, &cv);
    CHECK(log.count >= 7 && log.outside == 0, "feed: %d texts, %d outside", log.count, log.outside);
    if (log.json) {
        fprintf(log.json, "]}\n");
        fclose(log.json);
        snprintf(path, sizeof(path), "%s/feed_0.bgra", dir);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

static void test_quiet_passengers(void) {
    TsCore c;
    ts_init(&c, 21u, 1);
    TsEvent ev[TS_MAX_EVENTS];
    int said[2] = {0, 0}, thought[2] = {0, 0};
    for (int type = 0; type < 2; ++type) {
        c.mode = TS_UBER, c.job.passenger = 3, c.job.ptype = type == 0 ? TS_P_RELAXED : TS_P_HURRY;
        for (int i = 0; i < 200; ++i) {
            c.t += 60.0;
            complain(&c, L_BRAKE);
            int k = ts_take_events(&c, ev, TS_MAX_EVENTS);
            for (int e = 0; e < k; ++e) {
                if (strstr(ev[e].title, "(denkt)")) {
                    thought[type]++;
                    CHECK(ev[e].speaker == -1, "a thought isn't spoken");
                } else {
                    said[type]++;
                }
            }
        }
    }
    CHECK(said[0] < 40 && thought[0] > 40, "relaxed: mostly quiet (said %d, thought %d of 200)", said[0], thought[0]);
    CHECK(said[1] > 130, "in a hurry: says it (said %d of 200)", said[1]);
}

static void test_dash(void) {
    TsCore c;
    ts_init(&c, 5u, 0);
    c.imperial = 1;
    ts_set_car(&c, "LAM_MiuraP400_67");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    c.stage = TS_UBER_TO_DEST;
    c.leg_t = c.t;
    c.car_ok = 1;
    c.acc_long = -3.0f, c.acc_lat = 2.0f;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.g_on && !h.g_over && h.g_long < 0.0f, "G-meter on with a passenger, inside the circle (%.2f, %.2f)", (double)h.g_lat, (double)h.g_long);
    dash_one(&h, "dash_ok");
    c.acc_long = -10.0f, c.acc_lat = -1.0f;
    snprintf(c.job.passenger >= 0 ? h.chip3 : h.chip3, sizeof(h.chip3), "%s", "");
    ts_hud(&c, &h);
    CHECK(h.g_over, "hard braking: outside the circle");
    snprintf(h.chip3, sizeof(h.chip3), "%s", "Wish: a quick stop at the café");
    dash_one(&h, "dash_over");
    h.sign_over = 2;
    dash_one(&h, "dash_red");
    c.stage = TS_UBER_TO_PICKUP;
    ts_hud(&c, &h);
    CHECK(!h.g_on, "no G-meter before the passenger is in");
}

static void test_complaints_switch(void) {
    TsCore c;
    ts_init(&c, 7u, 1);
    TsEvent ev[TS_MAX_EVENTS];
    c.complaints_off = 1;
    static const int quiet[] = {L_BRAKE, L_CORNER, L_SPEEDING, L_CRASH, L_HURRY_NAG, L_LATE, L_BUS_CRASH, L_BUS_LATE};
    int said = 0;
    for (unsigned i = 0; i < sizeof(quiet) / sizeof(quiet[0]); ++i) {
        c.t += 60.0;
        say(&c, 3, quiet[i], 1);
        said += ts_take_events(&c, ev, TS_MAX_EVENTS);
    }
    CHECK(said == 0, "complaints off: %d complaints still said", said);
    c.t += 60.0;
    say(&c, 0, L_SMALL_TALK, 1);
    CHECK(ts_take_events(&c, ev, TS_MAX_EVENTS) > 0, "complaints off: small talk still comes");
    c.t += 60.0;
    say(&c, 1, L_BYE_GOOD, 1);
    CHECK(ts_take_events(&c, ev, TS_MAX_EVENTS) > 0, "complaints off: goodbyes still come");
    c.complaints_off = 0;
    c.t += 60.0;
    say(&c, 3, L_CRASH, 1);
    CHECK(ts_take_events(&c, ev, TS_MAX_EVENTS) > 0, "complaints on: a crash gets a remark");
    /* voices: each passenger line says who speaks, with a voice that fits the person */
    CHECK(ts_people_female_count == ts_people_count, "a voice for every person: %u of %u", ts_people_female_count, ts_people_count);
    c.mode = TS_UBER, c.job.passenger = 0, c.job.ptype = TS_P_HURRY;
    c.t += 60.0;
    say(&c, 0, L_SMALL_TALK, 1);
    int k = ts_take_events(&c, ev, TS_MAX_EVENTS);
    CHECK(k == 1 && ev[0].speaker == 0 && ev[0].voice_female == 1 && ev[0].voice_rate == 2,
          "voice: Lena, female, in a hurry (speaker %d, female %d, rate %d)", k ? ev[0].speaker : -9, k ? ev[0].voice_female : -9, k ? ev[0].voice_rate : -9);
    CHECK(k == 1 && ev[0].voice_pitch >= -10 && ev[0].voice_pitch <= 10, "voice pitch in range");
    toast(&c, 0, 0, "Horizon Transit", "x");
    k = ts_take_events(&c, ev, TS_MAX_EVENTS);
    CHECK(k == 1 && ev[0].speaker == -1, "pop-ups aren't spoken");
}

static void test_no_repeats_and_feed(void) {
    TsCore c;
    ts_init(&c, 99u, 1);
    /* a kind's lines all come once before any comes back */
    int n = (int)ts_line_sets[L_SMALL_TALK].count;
    CHECK(n >= 80, "small talk has %d lines", n);
    static int seen[512];
    memset(seen, 0, sizeof(seen));
    TsEvent ev[TS_MAX_EVENTS];
    int repeats = 0;
    for (int i = 0; i < n; ++i) {
        c.t += 16.0;
        say(&c, 0, L_SMALL_TALK, 1);
        int k = ts_take_events(&c, ev, TS_MAX_EVENTS);
        for (int e = 0; e < k; ++e)
            for (int j = 0; j < n; ++j)
                if (!strcmp(ev[e].text, ts_line_sets[L_SMALL_TALK].lines[j].de)) repeats += seen[j]++ > 0;
    }
    CHECK(repeats == 0, "every small-talk line once before any repeats: %d repeats", repeats);
    /* no line of any kind again within 5 minutes, even in a kind with few lines */
    int early = 0;
    double last_at[16] = {0};
    for (int i = 0; i < 200; ++i) {
        c.t += 15.0;
        say(&c, 0, L_WISH_THANKS, 1);
        int k = ts_take_events(&c, ev, TS_MAX_EVENTS);
        for (int e = 0; e < k; ++e)
            for (int j = 0; j < (int)ts_line_sets[L_WISH_THANKS].count && j < 16; ++j)
                if (!strcmp(ev[e].text, ts_line_sets[L_WISH_THANKS].lines[j].de)) {
                    if (last_at[j] > 0.0 && c.t - last_at[j] < 300.0 && (int)ts_line_sets[L_WISH_THANKS].count * 15 >= 300) early++;
                    last_at[j] = c.t;
                }
    }
    CHECK(early == 0, "no line again within 5 minutes: %d", early);
    /* all lines together, people's stories included: over 1000 */
    int total = 0;
    for (int k = 0; k < L_KINDS; ++k) total += (int)ts_line_sets[k].count;
    CHECK(total + 40 * TS_PERSON_LINES >= 1000, "lines: %d + stories %d", total, 40 * TS_PERSON_LINES);
    CHECK(ts_line_slot(L_KINDS - 1, (int)ts_line_sets[L_KINDS - 1].count - 1) >= 0, "every line has a slot");
    /* the feed: long lines split at spaces, nothing lost, every part short enough */
    char parts[4][TS_TEXT];
    const char *longline = "Ich hab mir vorgenommen, mehr Französisch zu lernen, und bisher kann ich genau ein Wort: Croissant.";
    int np = ts_feed_split(longline, 52, parts, 4);
    CHECK(np == 2, "long line in 2 parts: %d", np);
    char joined[400] = "";
    for (int i = 0; i < np; ++i) {
        int chars = 0;
        for (const unsigned char *p = (const unsigned char *)parts[i]; *p; ++p) chars += (*p & 0xC0) != 0x80;
        CHECK(chars <= 52, "part %d has %d characters", i, chars);
        if (i) strcat(joined, " ");
        strcat(joined, parts[i]);
    }
    CHECK(!strcmp(joined, longline), "nothing lost: %s", joined);
    CHECK(ts_feed_split("Kurz.", 52, parts, 4) == 1 && !strcmp(parts[0], "Kurz."), "a short line stays one");
    /* every line of every kind fits in the event text */
    int too_long = 0;
    for (int k = 0; k < L_KINDS; ++k)
        for (unsigned i = 0; i < ts_line_sets[k].count; ++i)
            too_long += strlen(ts_line_sets[k].lines[i].de) > 150 || strlen(ts_line_sets[k].lines[i].en) > 150;
    CHECK(too_long == 0, "%d lines too long", too_long);
}


/* 0.4.3: the passenger's door: which door, the order of the steps, the same door at the destination, driving off */
typedef struct {
    int steps[64], doors[64];
    double at[64];
    int n;
} DoorLog;

static double door_clock;
static void door_events(TsCore *c, DoorLog *log) {
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(c, ev, TS_MAX_EVENTS);
    for (int i = 0; i < n; ++i)
        if (ev[i].kind == TS_EV_DOOR && log->n < 64) {
            log->steps[log->n] = ev[i].sound, log->doors[log->n] = ev[i].style, log->at[log->n] = c->t;
            log->n++;
        }
    (void)door_clock;
}

static void door_wait(TsCore *c, float seconds, DoorLog *log) {
    for (int i = 0; i < (int)(seconds * 60.0f); ++i) {
        frame(c, i);
        door_events(c, log);
    }
}

static int door_drive(TsCore *c, const float to[3], DoorLog *log) {
    float v = 0.0f;
    int frames = 0;
    for (; frames < 60 * 60 * 30; ++frames) {
        float dx = to[0] - car.pos[0], dz = to[2] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
        if (d < 8.0f && v < 0.3f) break;
        float want = d < v * v / 6.0f + 6.0f ? 0.0f : 20.0f;
        v = v < want ? fminf(want, v + 2.0f * DT) : fmaxf(want, v - 3.0f * DT);
        if (d > 0.1f) car.fwd[0] = dx / d, car.fwd[2] = dz / d, car.right[0] = car.fwd[2], car.right[2] = -car.fwd[0];
        car.vel[0] = car.fwd[0] * v, car.vel[2] = car.fwd[2] * v;
        frame(c, frames);
        door_events(c, log);
    }
    car.vel[0] = car.vel[1] = car.vel[2] = 0.0f;
    return frames;
}

static void test_doors(void) {
    /* the pick: four doors -> rear left, rear right, front passenger (by the wheel's side); two doors -> front passenger */
    TsCore c;
    ts_init(&c, 77u, 1);
    CHECK(c.door == -1 && !c.doors_on, "doors off by default, none chosen");
    CHECK(ts_uber_stop_s(&c, 1) == 3.0 && ts_uber_stop_s(&c, 0) == 2.0, "without doors: 3 s in, 2 s out as before");
    CHECK(!strcmp(ts_door_name(TS_DOOR_RR), "doorRR") && !strcmp(ts_door_name(TS_DOOR_LF), "doorLF") && !ts_door_name(9)[0], "door names");
    CHECK(ts_door_front_passenger(0) == TS_DOOR_RF && ts_door_front_passenger(1) == TS_DOOR_LF, "front passenger door by the wheel");
    for (int rhd = 0; rhd < 2; ++rhd) {
        int seen[TS_DOORS] = {0};
        c.driver_right = rhd;
        c.door_mask = 15;
        for (int i = 0; i < 600; ++i) seen[ts_door_pick(&c)]++;
        int driver = rhd ? TS_DOOR_RF : TS_DOOR_LF, front = rhd ? TS_DOOR_LF : TS_DOOR_RF;
        CHECK(seen[driver] == 0, "never the driver's door (rhd %d)", rhd);
        CHECK(seen[TS_DOOR_LR] > 120 && seen[TS_DOOR_RR] > 120 && seen[front] > 120, "all three passenger doors (rhd %d: %d %d %d)", rhd,
              seen[TS_DOOR_LR], seen[TS_DOOR_RR], seen[front]);
        c.door_mask = 1 << TS_DOOR_LF | 1 << TS_DOOR_RF; /* a coupé */
        for (int i = 0; i < 50; ++i) CHECK(ts_door_pick(&c) == front, "two doors: the front passenger door");
        c.door_mask = 0; /* not known: as four doors, for the sound */
        int any_rear = 0;
        for (int i = 0; i < 50; ++i) any_rear |= ts_door_pick(&c) >= TS_DOOR_LR;
        CHECK(any_rear, "unknown doors: rear doors too");
        c.door_mask = 1 << driver; /* only the driver's door animates: the passenger side for the sound */
        CHECK(ts_door_pick(&c) == front, "only the driver's door: still the passenger side");
    }

    /* a whole ride with doors on */
    ts_init(&c, 4242u, 0);
    c.doors_on = 1;
    c.door_mask = 15;
    place_car(ts_places[0].pos);
    DoorLog log = {0};
    door_wait(&c, 0.5f, &log);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER, "ride started");
    TsOffer offer = c.job;
    door_drive(&c, ts_places[offer.pickup].pos, &log);
    door_wait(&c, 0.2f, &log);
    CHECK(c.stage == TS_UBER_BOARDING && log.n >= 1 && log.steps[0] == TS_DOOR_PREPARE, "boarding: prepare first (stage %d, %d)", c.stage, log.n);
    int door = c.door;
    CHECK(door >= 0 && door < TS_DOORS && door != TS_DOOR_LF, "a passenger door: %d", door);
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.countdown >= 3 && h.countdown <= 4, "countdown covers the door: %d", h.countdown);
    while (c.stage == TS_UBER_BOARDING && c.t - log.at[0] < 10.0) door_wait(&c, 0.1f, &log);
    double in_s = c.t - log.at[0];
    CHECK(c.stage == TS_UBER_TO_DEST && in_s >= 3.4 && in_s < 3.8, "ride on 3.5 s after stopping, after the door (%.2f s)", in_s);
    CHECK(log.n == 5, "five steps at the pickup: %d", log.n);
    for (int i = 0; i < log.n && i < 5; ++i) CHECK(log.steps[i] == TS_DOOR_PREPARE + i && log.doors[i] == door, "step %d: %d door %d", i, log.steps[i], log.doors[i]);
    if (log.n == 5) {
        CHECK(log.at[1] - log.at[0] >= 0.45 && log.at[1] - log.at[0] < 0.7, "opens after 0.5 s: %.2f", log.at[1] - log.at[0]);
        CHECK(log.at[2] - log.at[1] >= 1.5 && log.at[2] - log.at[1] < 2.1, "open ~1.8 s: %.2f", log.at[2] - log.at[1]);
        CHECK(log.at[3] - log.at[2] >= 0.5 && log.at[3] - log.at[2] < 0.75, "close sound 0.6 s after closing: %.2f", log.at[3] - log.at[2]);
    }

    /* at the destination: the same door; driving off with the door open closes it at once */
    log.n = 0;
    door_drive(&c, ts_places[offer.dest].pos, &log);
    while (log.n < 2 && c.t < 1e6) door_wait(&c, 0.1f, &log);
    door_wait(&c, 0.3f, &log);
    CHECK(c.stage == TS_UBER_ALIGHTING && log.n == 2 && log.steps[1] == TS_DOOR_OPEN && log.doors[1] == door,
          "the same door opens at the destination (stage %d, %d steps)", c.stage, log.n);
    car.vel[2] = 8.0f; /* drives off */
    door_wait(&c, 0.3f, &log);
    car.vel[0] = car.vel[1] = car.vel[2] = 0.0f;
    CHECK(c.stage == TS_UBER_TO_DEST, "drove off: not out yet");
    CHECK(log.n == 5 && log.steps[2] == TS_DOOR_CLOSE && log.steps[3] == TS_DOOR_SHUT && log.steps[4] == TS_DOOR_DONE,
          "drove off: close, shut, done at once (%d)", log.n);
    log.n = 0;
    door_drive(&c, ts_places[offer.dest].pos, &log);
    long long before = c.wallet;
    door_wait(&c, 2.0f, &log);
    CHECK(c.mode == TS_UBER, "still getting out after 2 s (doors take longer)");
    door_wait(&c, 2.0f, &log);
    CHECK(c.mode == TS_IDLE && c.wallet > before, "paid after the door shut");
    CHECK(log.n == 5 && log.steps[4] == TS_DOOR_DONE && log.doors[0] == door, "a full sequence at the destination (%d)", log.n);

    /* cancelled with the door open: it closes and the car's animation goes back */
    ts_init(&c, 99u, 0);
    c.doors_on = 1;
    place_car(ts_places[0].pos);
    log.n = 0;
    door_wait(&c, 0.5f, &log);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    offer = c.job;
    door_drive(&c, ts_places[offer.pickup].pos, &log);
    door_wait(&c, 1.5f, &log);
    ts_cancel(&c);
    door_events(&c, &log);
    CHECK(log.n >= 4 && log.steps[log.n - 1] == TS_DOOR_DONE && log.steps[log.n - 2] == TS_DOOR_SHUT, "cancel: the door closes (%d)", log.n);
    door_wait(&c, 3.0f, &log);
    CHECK(log.steps[log.n - 1] == TS_DOOR_DONE, "nothing after the cancel");

    /* doors off: no door events, timings as before */
    ts_init(&c, 4242u, 0);
    place_car(ts_places[0].pos);
    log.n = 0;
    door_wait(&c, 0.5f, &log);
    ts_phone_toggle(&c);
    ts_phone_accept(&c);
    door_drive(&c, ts_places[c.job.pickup].pos, &log);
    door_wait(&c, 3.6f, &log);
    CHECK(c.stage == TS_UBER_TO_DEST && log.n == 0, "doors off: in after 3 s, no door (stage %d, %d)", c.stage, log.n);
}

int main(int argc, char **argv) {
    (void)argv;
    int verbose = argc > 1;
    test_stops_are_real();
    if (verbose) printf("Uber, ruhige Fahrt:\n");
    test_uber_ride(verbose, 0);
    if (verbose) printf("Uber, ruppige Fahrt:\n");
    test_uber_ride(verbose, 1);
    test_crash();
    if (verbose) printf("Bus:\n");
    test_bus_line(verbose);
    test_bus_leave_early();
    test_phone_cancel();
    test_phone_prev();
    test_cars_and_passengers();
    test_bus_needs_bus();
    test_speed_chip();
    test_lines();
    test_regular();
    test_reputation_and_shift();
    test_specials();
    test_wishes_and_memory();
    test_timetable();
    test_bus_sql();
    test_shop_and_settings();
    test_phone_draw();
    test_imperial_and_sign();
    test_place_names();
    test_no_repeats_and_feed();
    test_complaints_switch();
    test_dash();
    test_quiet_passengers();
    test_feed_window();
    test_ride_variety();
    test_doors();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
