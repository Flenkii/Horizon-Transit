/* Tests of the job system without the game: a simulated car drives to the targets.
   Build and run (Linux or Windows, any C compiler):
     cc -std=c11 -O1 -Isrc/core tests/test_core.c src/core/jobs.c src/core/stops_data.c -lm -o test_core && ./test_core */
#include "jobs.h"
#include "jobs_internal.h"
#include "stops.h"
#include "lines.h"
#include "phone_draw.h"
#include "phone_anim.h"
#include "bus_sql.h"
#include "people.h"
#include "map.h"
#include "gamemap.h"
#include "inflate.h"
#include "bc7.h"
#include "map_vectors.h"
#include "companion.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* 0.13: rides need GO (the shift) first; most tests are about what comes after, so they start online */
#define ts_init(c, seed, german) (ts_init((c), (seed), (german)), (c)->shift.on = 1)
/* and they look at the three requests nearby at once, without waiting for them one by one */
static void phone_with_offers(TsCore *c) {
    ts_phone_toggle(c);
    if (c->phone_open && c->mode == TS_IDLE && c->shift.on) ts_refresh_offers(c);
}

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
/* internal helpers (jobs_internal.h is not for the glue; the tests reach them by these names) */
int bus_direction(const TsCore *c, int line);
void service_maybe_lost(TsCore *c, int person, int place);
int service_streak(TsCore *c, int stars, int pay);
void career_goal_check(TsCore *c);
void wish_step(TsCore *c);
void wish_arrived_at_dest(TsCore *c);
int ride_max_kmh(const TsCore *c);
#define bus_dir bus_direction
#define service_maybe_lost_for_test(k) service_maybe_lost((k), 1, 2)
#define service_streak_for_test service_streak
#define career_goal_for_test career_goal_check
static int arrived_at_first(const TsCore *c) { return c->stage == TS_BUS_TO_STOP && c->stop_i == 0; }
static const float DT = 1.0f / 60.0f;

static void place_car(const float at[3]) {
    memset(&car, 0, sizeof(car));
    car.pos[0] = at[0], car.pos[1] = at[1], car.pos[2] = at[2];
    car.fwd[2] = 1.0f;
    car.right[0] = 1.0f;
}

static int announcements; /* 0.5: bus announcements seen */
static int sounds_seen[16], toast_sounds[16], gong_missing; /* 0.5.1: the own sounds (sound events and toasts' sounds) */
static void events_print(TsCore *c, int verbose, int *toasts, int *feeds) {
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(c, ev, TS_MAX_EVENTS);
    for (int i = 0; i < n; ++i) {
        announcements += ev[i].speaker == TS_SPEAKER_ANNOUNCE;
        if (ev[i].speaker == TS_SPEAKER_ANNOUNCE && ev[i].kind == TS_EV_FEED &&
            !(i > 0 && ev[i - 1].kind == TS_EV_SOUND && ev[i - 1].sound == TS_SOUND_GONG))
            gong_missing++;
        if (ev[i].kind == TS_EV_SOUND && ev[i].sound > 0 && ev[i].sound < 16) sounds_seen[ev[i].sound]++;
        if (ev[i].kind == TS_EV_TOAST && ev[i].sound > 0 && ev[i].sound < 16) toast_sounds[ev[i].sound]++;
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
    CHECK(ts_place_count > 120, "places: %u", ts_place_count);
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

/* 0.8: realistic places: nothing near the car meet, the festival's stop on the road, the real bus stops in Nice's
   town line, and in towns rides start and end at real places (bus stops, parking spots, outposts) */
static void test_places_realistic(void) {
    const float meet[3] = {7699.28f, 159.86f, -6783.0f}, garage[3] = {6150.65f, 117.0f, -5851.22f};
    unsigned stops = 0, parking = 0;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        CHECK(ts_distance_xz(ts_places[i].pos, meet) > 500.0f, "place %s near the car meet", ts_places[i].name);
        if (i) CHECK(ts_distance_xz(ts_places[i].pos, garage) > 450.0f, "place %s on the festival grounds", ts_places[i].name);
        stops += ts_places[i].kind == TS_PLACE_STOP, parking += ts_places[i].kind == TS_PLACE_PARKING;
    }
    CHECK(stops >= 10 && parking >= 40, "real places: %u stops, %u parking", stops, parking);
    CHECK(ts_places[0].kind == TS_PLACE_FESTIVAL && ts_distance_xz(ts_places[0].pos, garage) > 100.0f, "festival stop on the road");
    unsigned nice_stops = 0;
    for (unsigned s = 0; s < ts_lines[3].stop_count; ++s) nice_stops += ts_places[ts_lines[3].stops[s]].kind == TS_PLACE_STOP;
    CHECK(nice_stops >= 4, "Nice city bus: %u real stops", nice_stops);
    int real = 0, all = 0;
    for (int seed = 0; seed < 40; ++seed) {
        TsCore c;
        ts_init(&c, 777u + (unsigned)seed, 1);
        place_car(ts_places[seed % 2 ? 5 : 3].pos); /* in Nice, in San Giovanni */
        wait_s(&c, 0.5f, 0);
        phone_with_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) {
            real += ts_places[c.offers[i].pickup].kind != TS_PLACE_SPOT;
            all++;
        }
    }
    CHECK(all > 0 && real * 10 >= all * 8, "pickups in town at real places: %d of %d", real, all);
}

/* 0.13: like a driver app: offline only GO; online the requests come one by one, each waits 20 s for a yes */
static void test_go_flow(void) {
    TsCore c;
    ts_init(&c, 4242u, 1);
    c.shift.on = 0;
    place_car(ts_places[5].pos);
    wait_s(&c, 0.5f, 0);
    ts_phone_toggle(&c);
    TsHud h;
    ts_hud(&c, &h);
    CHECK(!c.offer_count && h.go_row == 2 && h.row_selected == 2 && !h.go_on, "offline: only GO (%d offers, go row %d)", c.offer_count, h.go_row);
    ts_phone_next(&c);
    CHECK(c.phone_tab == TS_TAB_RIDES && c.phone_sel == 0, "offline: down stays on GO");
    ts_phone_accept(&c);
    CHECK(c.shift.on && c.mode == TS_IDLE && !c.offer_count, "GO: online, no ride yet");
    ts_hud(&c, &h);
    CHECK(!strcmp(h.row_label[2], "Suche Fahrten …"), "online: looking for rides ('%s')", h.row_label[2]);
    wait_s(&c, 25.0f, 0);
    CHECK(c.offer_count == 1 && c.req_until > c.t, "a request comes by itself, standing still (%d)", c.offer_count);
    ts_hud(&c, &h);
    CHECK(!strncmp(h.footer, "Noch ", 5) && !strcmp(h.row_label[4], "Ablehnen") && h.row_selected == 2, "the request: countdown, accept, decline (%s)", h.footer);
    ts_phone_next(&c);
    ts_hud(&c, &h);
    CHECK(c.phone_sel == 1 && h.row_selected == 4, "down: decline");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_IDLE && !c.offer_count && c.hail_at > c.t, "declined: the next one comes later");
    wait_s(&c, 60.0f, 0);
    CHECK(c.offer_count == 1, "the next request");
    wait_s(&c, (float)TS_REQUEST_S + 1.0f, 0);
    CHECK(!c.offer_count || c.req_until > c.t - 1.0, "missed: it goes away");
    for (int k = 0; k < 80 && !c.offer_count; ++k) wait_s(&c, 1.0f, 0);
    c.map_preview = 0;
    c.phone_sel = 0;
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER && c.shift.on, "Enter: the ride starts");
}

/* 0.13: what the app has without the game: the ride book, the timetables and the lines' ways, a target from the map */
static void test_app_offline_things(void) {
    TsCore c;
    ts_init(&c, 99u, 1);
    c.shift.on = 0;
    TsBookEntry e = {1795500000, 0, 3, 5, 9, 1, 4, 1, 1250, 85, 4321, 300}, back;
    char line[256];
    char line2[256];
    CHECK(ts_book_format(&e, line, sizeof(line)) && ts_book_parse(line, &back) && ts_book_format(&back, line2, sizeof(line2)) && !strcmp(line, line2),
          "book line: %s", line);
    CHECK(!ts_book_parse("1 0 3 5 99999 1 4 1 10 0 1 1", &back) && !ts_book_parse("kaputt", &back) && !ts_book_parse("1 0 3 5 9 1 9 1 10 0 1 1", &back),
          "bad book lines are left out");
    for (int i = 0; i < TS_BOOK + 5; ++i) e.pay = i, ts_book_add(&c, &e);
    CHECK(c.book_count == TS_BOOK && c.book[0].pay == TS_BOOK + 4 && c.book[TS_BOOK - 1].pay == 5, "the book keeps the newest %d", TS_BOOK);
    static char j[256 * 1024];
    size_t n = ts_app_history_json(&c, j, sizeof(j));
    CHECK(n > 0 && strstr(j, "\"rides\":[{\"t\":1795500000,\"bus\":0") && strstr(j, "\"pay\":304"), "history JSON (%zu bytes)", n);
    CHECK(n < 120 * 1024, "history JSON stays small: %zu", n);
    n = ts_app_lines_json(&c, j, sizeof(j));
    CHECK(n > 0 && strstr(j, "\"min\":0") && strstr(j, "\"min\":"), "lines: stop minutes");
    /* a target from the app's map: the GPS goes there, and it's done on arrival */
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    float x = ts_places[7].pos[0], z = ts_places[7].pos[2];
    int arg = (int)lroundf((x + 30000.0f) / 2.0f) * 32768 + (int)lroundf((z + 30000.0f) / 2.0f);
    CHECK(ts_app_do(&c, ts_app_action("target"), arg) && c.target_on && c.nav_on && fabsf(c.nav_target[0] - x) < 2.5f && fabsf(c.nav_target[2] - z) < 2.5f,
          "target: GPS to %.0f %.0f (%.0f %.0f)", x, z, c.nav_target[0], c.nav_target[2]);
    TsHud h;
    ts_hud(&c, &h);
    n = ts_app_state_json(&c, &h, j, sizeof(j));
    CHECK(strstr(j, "\"target\":{") && strstr(j, "\"book\":"), "state: target and book serial");
    place_car(ts_places[7].pos);
    wait_s(&c, 1.0f, 0);
    CHECK(!c.target_on && !c.nav_on, "target reached: GPS off");
    ts_app_do(&c, ts_app_action("target"), arg);
    ts_app_do(&c, ts_app_action("target"), -1);
    CHECK(!c.target_on && !c.nav_on, "target cleared");
}

static void test_uber_ride(int verbose, int rough) {
    TsCore c;
    ts_init(&c, 12345u + (unsigned)rough, 1);
    c.wall_time = 1795500000;
    place_car(ts_places[0].pos); /* at the festival */
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
    CHECK(c.phone_open && c.offer_count == TS_PHONE_OFFERS, "offers: %d", c.offer_count);
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.panel && h.row_count == 5 && h.row_selected == 2 && h.tab_count == TS_TABS, "phone panel rows %d sel %d", h.row_count, h.row_selected);
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

    {   /* 0.5.1: the ride card shows the pickup, nothing done yet */
        TsHud k;
        ts_hud(&c, &k);
        CHECK(k.trip_on && strcmp(k.trip_to, ts_place_name(&c, offer.pickup)) == 0 && k.trip_progress < 0.05f,
              "ride card on the way: %s %.2f", k.trip_to, k.trip_progress);
    }
    drive_to(&c, ts_places[offer.pickup].pos, 20.0f, 2.5f, 3.0f, verbose);
    wait_s(&c, 0.5f, verbose);
    CHECK(c.stage == TS_UBER_BOARDING, "boarding at the pickup (stage %d)", c.stage);
    wait_s(&c, 3.5f, verbose);
    CHECK(c.stage == TS_UBER_TO_DEST, "passenger in (stage %d)", c.stage);
    CHECK(ts_distance_xz(c.nav_target, ts_places[offer.dest].pos) < 0.1f, "GPS target = destination");
    {   /* ... then the destination; halfway there the route is about half full */
        TsHud k;
        ts_hud(&c, &k);
        CHECK(strcmp(k.trip_to, ts_place_name(&c, offer.dest)) == 0 && k.trip_progress < 0.2f, "ride card to the destination: %s %.2f",
              k.trip_to, k.trip_progress);
        float mid[3] = {(ts_places[offer.pickup].pos[0] + ts_places[offer.dest].pos[0]) / 2, ts_places[offer.dest].pos[1],
                        (ts_places[offer.pickup].pos[2] + ts_places[offer.dest].pos[2]) / 2};
        TsCore half = c;
        place_car(mid);
        wait_s(&half, 0.1f, 0);
        ts_hud(&half, &k);
        CHECK(k.trip_progress > 0.3f && k.trip_progress < 0.75f, "ride card halfway: %.2f", k.trip_progress);
        place_car(ts_places[offer.pickup].pos);
        wait_s(&c, 0.1f, 0);
    }

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
    /* 0.13: the ride book */
    CHECK(c.book_count == 1 && c.book[0].bus == 0 && c.book[0].who == offer.passenger && c.book[0].from == offer.pickup &&
              c.book[0].to == offer.dest && c.book[0].pay == (int)(c.wallet - before) && c.book[0].when == 1795500000 && c.book[0].metres > 100,
          "ride book: %d %d %d %lld", c.book_count, c.book[0].pay, c.book[0].metres, c.book[0].when);
    if (rough) CHECK(sat < 70.0f && (c.n_brake + c.n_accel + c.n_speeding) > 0, "rough ride is penalised: %.1f", sat);
    else CHECK(sat >= 85.0f && c.n_brake + c.n_accel + c.n_crash == 0, "smooth ride is rewarded: %.1f", sat);
    if (verbose) printf("  -> %s: Zufriedenheit %.0f%%, Konto %lld CR\n", rough ? "ruppig" : "ruhig", c.sat, c.wallet);
}

static void test_crash(void) {
    TsCore c;
    ts_init(&c, 7u, 0);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.2f, 0);
    phone_with_offers(&c);
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
    int drive_through = 0;
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
        CHECK(c.line_rev == 0, "from the first stop: forwards");
        for (unsigned s = 0; s < ts_lines[line].stop_count && c.mode == TS_BUS; ++s) {
            const float *stop = ts_places[ts_bus_stop(&c, line, c.stop_i)].pos;
            int request = c.stop_request, at = c.stop_i, on = c.onboard;
            if (s) drive_to(&c, stop, 16.0f, 1.5f, 2.5f, show);
            wait_s(&c, 0.5f, show);
            if (!request) { /* 0.5: nobody wants off or on: the bus drove through */
                CHECK(c.stage == TS_BUS_TO_STOP && c.stop_i == at + 1 && c.onboard == on, "drove through (seed %u stop %u)", seed, s);
                CHECK(at > 0 && at < (int)ts_lines[line].stop_count - 1, "never through the first or last stop");
                drive_through++;
                stops_seen++;
                continue;
            }
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
    CHECK(drive_through > 0, "some stops without a stop request");
}

/* 0.5.1: wishes last until the destination; "floor it" in a supercar */
static void test_wishes_whole_ride(void) {
    /* statistically: in a supercar, some passengers want to floor it; then the sign has no limit */
    int floorit = 0, free_sign = 0, rides = 0, in_normal = 0;
    announcements = 0;
    for (unsigned seed = 1; seed <= 40; ++seed) {
        for (int super = 0; super < 2; ++super) {
            TsCore c;
            ts_init(&c, seed * 4099u + (unsigned)super, 1);
            ts_set_car(&c, super ? "LAM_Huracan_15" : "AUD_RS5_11");
            place_car(ts_places[seed * 11 % ts_place_count].pos);
            wait_s(&c, 0.5f, 0);
            phone_with_offers(&c);
            if (c.offer_count < 2) continue;
            int offer = c.offer_count - 1;
            ts_accept_row(&c, offer);
            if (c.mode != TS_UBER) continue;
            rides++;
            /* drive: teleport to the pickup, wait, then cruise toward the destination long enough for wishes */
            place_car(ts_places[c.job.pickup].pos);
            wait_s(&c, 8.0f, 0);
            if (c.stage != TS_UBER_TO_DEST) continue;
            for (int k = 0; k < 6 * 60 && c.mode == TS_UBER && c.stage == TS_UBER_TO_DEST; ++k) {
                float to[3] = {c.nav_target[0], c.nav_target[1], c.nav_target[2]};
                float dx = to[0] - car.pos[0], dz = to[2] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
                if (d < 1200.0f) break; /* keep out of the end: wishes come only far from it */
                car.fwd[0] = dx / d, car.fwd[2] = dz / d, car.right[0] = car.fwd[2], car.right[2] = -car.fwd[0];
                car.vel[0] = car.fwd[0] * 0.5f, car.vel[2] = car.fwd[2] * 0.5f; /* barely moving: no harsh moments */
                wait_s(&c, 1.0f, 0);
                if (c.floorit) {
                    TsHud h;
                    ts_hud(&c, &h);
                    free_sign += h.sign_free && h.sign_limit > 0 && !h.sign_over;
                    break;
                }
            }
            if (c.floorit) (super ? floorit++ : in_normal++);
        }
    }
    CHECK(rides > 30, "rides for the wish test: %d", rides);
    CHECK(!announcements, "no stop announcements on rides, only on bus lines (%d)", announcements);
    CHECK(floorit > 0, "supercar passengers ask to floor it (%d)", floorit);
    CHECK(!in_normal, "never in an everyday car (%d)", in_normal);
    CHECK(free_sign == floorit, "floor it: the end-of-restrictions sign, never too fast (%d of %d)", free_sign, floorit);

    /* the wishes by hand: "slower" holds for minutes, no timer; the chip says "till arrival" */
    TsCore c;
    ts_init(&c, 777u, 0);
    ts_set_car(&c, "AUD_RS5_11");
    place_car(ts_places[5].pos);
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
    ts_accept_row(&c, 0);
    place_car(ts_places[c.job.pickup].pos);
    wait_s(&c, 8.0f, 0);
    CHECK(c.mode == TS_UBER && c.stage == TS_UBER_TO_DEST, "on the way (stage %d)", c.stage);
    c.wish = TS_WISH_SLOWER, c.wish_start = c.t, c.wish_limit = 70, c.job.max_kmh = 70, c.wish_slips = 0, c.wish_over_since = 0.0;
    c.speed = 60.0f / 3.6f;
    for (int i = 0; i < 3000; ++i) c.t += 0.1, wish_step(&c); /* 5 minutes under it */
    CHECK(c.wish == TS_WISH_SLOWER, "slower: still on after 5 minutes (no timer)");
    TsHud h;
    ts_hud(&c, &h);
    CHECK(strstr(h.chip3, "Till end") != NULL, "chip: %s", h.chip3);
    CHECK(h.sign_limit == 70, "the sign shows the wish (%d)", h.sign_limit);
    c.speed = 76.0f / 3.6f; /* a little over (yellow): not failed */
    for (int i = 0; i < 100; ++i) c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_SLOWER && c.wish_slips == 0, "a little over doesn't fail it");
    c.speed = 95.0f / 3.6f; /* clearly over: a reminder, then failed */
    for (int i = 0; i < 30; ++i) c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_SLOWER && c.wish_slips == 1, "clearly over: one reminder first (%d)", c.wish_slips);
    for (int i = 0; i < 30; ++i) c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_NONE && c.wishes_failed == 1, "clearly over again: failed");

    int ok = c.wishes_ok;
    c.wish = TS_WISH_SMOOTH, c.wish_start = c.t, c.wish_slips = 0, c.wish_was_harsh = 0, c.was_harsh = 0;
    for (int i = 0; i < 2000; ++i) c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_SMOOTH, "smooth: still on after minutes");
    c.was_harsh = 1, c.t += 0.1, wish_step(&c);
    c.was_harsh = 0, c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_SMOOTH && c.wish_slips == 1, "one jolt: a remark, the wish goes on");
    wish_arrived_at_dest(&c);
    CHECK(c.wishes_ok == ok + 1, "smooth until the destination: done");

    c.wish = TS_WISH_FLOORIT, c.wish_start = c.t, c.wish_limit = 180, c.floorit = 1;
    c.speed = 150.0f / 3.6f;
    for (int i = 0; i < 1000; ++i) c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_FLOORIT, "floor it: waits for the speed, no timer");
    c.speed = 185.0f / 3.6f, c.t += 0.1, wish_step(&c);
    CHECK(c.wish == TS_WISH_NONE && c.wishes_ok == ok + 2 && c.floorit, "floor it: done at 180, the limit stays off");
    CHECK(ride_max_kmh(&c) >= 400, "no limit after floor it");
}

/* 0.5.1: talking back: questions come only with the reply key on; the answer is the driver's, then a reaction */

/* 0.10: the phone app's JSON and what it can ask for */
static int json_balanced(const char *j) {
    int depth = 0, in_str = 0;
    for (const char *p = j; *p; ++p) {
        if (in_str) {
            if (*p == '\\') ++p;
            else if (*p == '"') in_str = 0;
            continue;
        }
        if (*p == '"') in_str = 1;
        else if (*p == '{' || *p == '[') ++depth;
        else if (*p == '}' || *p == ']') { if (--depth < 0) return 0; }
        else if ((unsigned char)*p < 0x20) return 0;
    }
    return depth == 0 && !in_str;
}

static void test_phone_app(void) {
    static char j[1 << 18];
    for (int de = 0; de < 2; ++de) {
        TsCore c;
        ts_init(&c, 4242, de);
        c.bus_capacity = 40, c.time_factor = 1.0f, c.replies_on = 1;
        place_car(ts_places[0].pos);
        for (int i = 0; i < 10; ++i) ts_car_sample(&c, DT, &car);
        c.shift.on = 0;
        CHECK(!ts_app_do(&c, TS_APP_REFRESH, 0), "0.13 app: nothing to decline offline");
        CHECK(ts_app_do(&c, TS_APP_SHIFT, 0) && c.shift.on && !c.offer_count, "0.13 app: GO: online, waiting");
        ts_refresh_offers(&c);
        c.req_until = c.t + TS_REQUEST_S;
        CHECK(c.offer_count > 0, "app: a request");
        TsHud h;
        ts_hud(&c, &h);
        size_t n = ts_app_state_json(&c, &h, j, sizeof(j));
        CHECK(n > 0 && json_balanced(j), "app: state JSON is whole (%zu)", n);
        CHECK(strstr(j, "\"mode\":\"idle\"") && strstr(j, "\"offers\":[{"), "app: idle with offers");
        CHECK(strstr(j, "\"reqLeft\":20") != NULL, "0.13 app: the countdown");
        CHECK(!ts_app_state_json(&c, &h, j, 64) && j[0] == 0, "app: too small a buffer gives nothing");
        CHECK(!ts_app_do(&c, TS_APP_ACCEPT, 7) && c.mode == TS_IDLE, "app: no 8th offer");
        CHECK(ts_app_do(&c, TS_APP_ACCEPT, 0) && c.mode == TS_UBER, "app: accept the first ride");
        CHECK(!ts_app_do(&c, TS_APP_ACCEPT, 0), "app: no second job");
        ts_app_feed("Lena", "Hi \"there\"\n\\ ok", 0);
        ts_hud(&c, &h);
        n = ts_app_state_json(&c, &h, j, sizeof(j));
        CHECK(n > 0 && json_balanced(j) && strstr(j, "\"job\":{\"kind\":\"ride\"") && strstr(j, "Hi \\\"there\\\"\\n\\\\ ok"),
              "app: the ride and an escaped chat line");
        CHECK(strstr(j, "\"route\":[") && !strstr(j, "\"route\":[]"), "app: a way to the pickup");
        CHECK(ts_app_do(&c, TS_APP_CANCEL, 0) && c.mode == TS_IDLE, "app: cancel");
        c.bus_needs_bus = 0;
        CHECK(ts_app_do(&c, TS_APP_LINE, 0) && c.mode == TS_BUS, "app: start line 1");
        ts_hud(&c, &h);
        n = ts_app_state_json(&c, &h, j, sizeof(j));
        CHECK(n > 0 && json_balanced(j) && strstr(j, "\"kind\":\"bus\"") && strstr(j, "\"stops\":[{"), "app: the bus line");
        ts_cancel(&c);
        n = ts_app_lines_json(&c, j, sizeof(j));
        CHECK(n > 0 && json_balanced(j), "app: lines JSON");
        CHECK(ts_app_action("accept") == TS_APP_ACCEPT && ts_app_action("shift") == TS_APP_SHIFT && !ts_app_action("format"), "app: action names");
        CHECK(strstr(j, "\"themes\":[{") != NULL, "app: all designs go with the lines (0.10.4)");
        c.setting_count = 3, c.setting_choices[2] = 4, c.setting_index[2] = 1;
        snprintf(c.setting_choice[2][3], sizeof(c.setting_choice[2][3]), "Laut");
        CHECK(ts_app_action("set") == TS_APP_SET && ts_app_do(&c, TS_APP_SET, 2 * 16 + 3) && c.setting_want[2] == 3, "app: set a row to a value");
        CHECK(!ts_app_do(&c, TS_APP_SET, 2 * 16 + 4) && !ts_app_do(&c, TS_APP_SET, 5 * 16), "app: no value or row out of range");
        n = ts_app_state_json(&c, &h, j, sizeof(j));
        CHECK(n > 0 && json_balanced(j) && strstr(j, "\"idx\":1,\"choices\":[") && strstr(j, "\"Laut\"]"), "app: the choices of a row");
        c.setting_count = 0;
        CHECK(!ts_app_do(&c, TS_APP_REPLY, 0), "app: no question, no reply");
    }
    ts_app_feed_clear();
}

static void test_talk_back(void) {
    int asked = 0, answered = 0, reactions = 0, driver_lines = 0, asked_off = 0, chip_ok = 0, ups = 0;
    for (unsigned seed = 1; seed <= 30; ++seed) {
        for (int on = 0; on < 2; ++on) {
            TsCore c;
            ts_init(&c, seed * 2357u, seed % 2);
            ts_set_car(&c, "AUD_RS5_11");
            c.replies_on = on;
            snprintf(c.reply_key, sizeof(c.reply_key), "B");
            place_car(ts_places[seed * 13 % ts_place_count].pos);
            wait_s(&c, 0.5f, 0);
            phone_with_offers(&c);
            if (c.offer_count < 1) continue;
            ts_accept_row(&c, c.offer_count - 1);
            if (c.mode != TS_UBER) continue;
            place_car(ts_places[c.job.pickup].pos);
            wait_s(&c, 8.0f, 0);
            c.thinking = 0;
            for (int s = 0; s < 3000 && c.mode == TS_UBER; ++s) {
                for (int f = 0; f < 6; ++f) frame(&c, f);
                TsEvent ev[TS_MAX_EVENTS];
                int n = ts_take_events(&c, ev, TS_MAX_EVENTS);
                for (int i = 0; i < n; ++i) {
                    driver_lines += ev[i].speaker == TS_SPEAKER_DRIVER;
                    reactions += ev[i].kind == TS_EV_FEED && ev[i].style == 1 && c.replied < 0 && answered > 0 && ev[i].speaker >= 0;
                }
                if (c.ask >= 0) {
                    if (!on) asked_off++;
                    TsHud h;
                    ts_hud(&c, &h);
                    chip_ok += !strncmp(h.chip3, "[B]", 3);
                    asked++;
                    if (seed % 3) { /* answer most of them */
                        float before = c.sat;
                        ts_reply(&c);
                        ups += c.sat > before || before >= 99.9f;
                        answered++;
                    } else { /* let it lapse */
                        c.t += 10.0;
                    }
                }
            }
        }
    }
    CHECK(asked >= 5, "passengers ask questions (%d)", asked);
    CHECK(!asked_off, "no questions without the reply key (%d)", asked_off);
    CHECK(chip_ok == asked, "the chip offers [B] (%d of %d)", chip_ok, asked);
    CHECK(answered > 0 && driver_lines == answered, "each answer is the driver's line (%d of %d)", driver_lines, answered);
    CHECK(ups == answered, "answering pleases (%d of %d)", ups, answered);
    CHECK(reactions > 0, "they react to the answer (%d)", reactions);
}

/* 0.5.1: the own sounds: the speed warning's tones, the bus's gong and stop bell, the payout and request sounds */
static void test_sounds(void) {
    TsSpeedWarn w;
    ts_speed_warn_init(&w);
    double t = 0.0;
    int yellow = 0, red = 0, stop = 0;
#define WARN_RUN(level, secs)                                                                                          \
    for (double end_ = t + (secs); t < end_; t += 0.1) {                                                               \
        int r_ = ts_speed_warn(&w, (level), t);                                                                        \
        yellow += r_ == TS_WARN_YELLOW, red += r_ == TS_WARN_RED, stop += r_ == TS_WARN_STOP;                          \
    }
    WARN_RUN(0, 3.0);
    CHECK(!yellow && !red && !stop, "under the limit: quiet");
    WARN_RUN(1, 5.0);
    CHECK(yellow == 1 && !red, "yellow: one tone, not again while yellow (%d)", yellow);
    WARN_RUN(0, 0.5);
    WARN_RUN(1, 2.0);
    CHECK(yellow == 1, "a short dip under the limit doesn't chime again (%d)", yellow);
    WARN_RUN(0, 2.0);
    WARN_RUN(1, 1.0);
    CHECK(yellow == 2, "slower, then too fast again: the tone again (%d)", yellow);
    WARN_RUN(2, 5.05);
    CHECK(red >= 5 && red <= 6, "red: rings every second while red (%d)", red);
    CHECK(yellow == 2, "yellow -> red: no yellow tone");
    WARN_RUN(1, 1.0);
    CHECK(stop == 1 && yellow == 2, "red -> yellow: the ringing stops, no yellow tone (stop %d, yellow %d)", stop, yellow);
    int red_before = red;
    WARN_RUN(2, 0.2);
    CHECK(red == red_before + 1, "back to red: it rings again (%d)", red - red_before);
    red_before = red;
    WARN_RUN(1, 0.1);
    WARN_RUN(2, 0.1);
    CHECK(red == red_before, "red, yellow, red within a second: no double ring");
    WARN_RUN(0, 2.0);
    CHECK(stop == 3, "red -> under the limit: stop (%d)", stop);
    WARN_RUN(2, 0.5);
    CHECK(yellow == 2 && red > red_before, "straight into red: rings, no yellow tone");
#undef WARN_RUN
    CHECK(ts_speed_warn(&w, 7, t) == TS_WARN_NONE && w.level == 2, "levels past red count as red");

    /* the bus: a gong before every announcement, the stop bell a little later, no UI tick any more */
    memset(sounds_seen, 0, sizeof(sounds_seen));
    memset(toast_sounds, 0, sizeof(toast_sounds));
    gong_missing = 0;
    int bells_without_request = 0;
    for (unsigned seed = 3; seed <= 12; ++seed) {
        TsCore c;
        ts_init(&c, seed * 104729u, 0);
        c.shift.on = 0; /* as before GO existed: the line starts the shift */
        c.bus_capacity = 40;
        int line = (int)(seed % ts_line_count);
        place_car(ts_places[ts_lines[line].stops[0]].pos);
        wait_s(&c, 0.2f, 0);
        ts_accept_row(&c, TS_PHONE_OFFERS + line);
        for (unsigned s = 0; s < ts_lines[line].stop_count && c.mode == TS_BUS; ++s) {
            const float *stop_pos = ts_places[ts_bus_stop(&c, line, c.stop_i)].pos;
            int bells = sounds_seen[TS_SOUND_BELL], alight = c.to_alight;
            if (s) drive_to(&c, stop_pos, 16.0f, 1.5f, 2.5f, 0);
            wait_s(&c, 0.5f, 0);
            if (sounds_seen[TS_SOUND_BELL] > bells && alight == 0) bells_without_request++;
            if (c.stage == TS_BUS_AT_STOP) wait_s(&c, c.stop_secs + 1.5f, 0);
        }
    }
    CHECK(sounds_seen[TS_SOUND_GONG] > 10 && !gong_missing, "a gong before each announcement (%d, %d missing)",
          sounds_seen[TS_SOUND_GONG], gong_missing);
    CHECK(sounds_seen[TS_SOUND_BELL] > 0, "the stop bell rings (%d)", sounds_seen[TS_SOUND_BELL]);
    CHECK(!bells_without_request, "no bell when nobody gets off (%d)", bells_without_request);
    CHECK(!sounds_seen[TS_SOUND_TICK], "the stop button is the bell now, not the UI tick");
    CHECK(toast_sounds[TS_SOUND_PAYOUT] >= 10, "the line's payout has the payout sound (%d)", toast_sounds[TS_SOUND_PAYOUT]);
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
    phone_with_offers(&c);
    ts_phone_tab(&c, 1); /* 0.13: the arrows stay in the tab: the bus tab, its first line */
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS && c.line == 0, "phone row 4 = line 1");
    phone_with_offers(&c);
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS && c.cancel_armed, "first press only asks");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_IDLE && !c.nav_on, "second press cancels, GPS cleared");
}

/* 0.13: the shop with the game's credits: prices in CR, paid by the glue, nothing taken without the money */
static void test_credit_shop(void) {
    TsCore c;
    ts_init(&c, 7u, 1);
    c.credits_mode = 1, c.cr_scale = 5;
    CHECK(ts_shop_cost(&c, TS_SHOP_DISPATCH) == 25000, "credit shop: 5,000 x 5 = 25,000 CR");
    c.game_credits = -1;
    ts_shop_buy(&c, TS_SHOP_DISPATCH);
    CHECK(c.credit_buy_item < 0 && !ts_has(&c, TS_SHOP_DISPATCH), "credit shop: balance unknown, nothing happens");
    c.game_credits = 20000;
    ts_shop_buy(&c, TS_SHOP_DISPATCH);
    CHECK(c.credit_buy_item < 0 && !ts_has(&c, TS_SHOP_DISPATCH), "credit shop: too little, nothing happens");
    c.game_credits = 30000;
    long long w = c.wallet;
    ts_shop_buy(&c, TS_SHOP_DISPATCH);
    CHECK(c.credit_buy_item == TS_SHOP_DISPATCH && c.credit_buy_cost == 25000 && !ts_has(&c, TS_SHOP_DISPATCH), "credit shop: handed to the game");
    ts_shop_buy(&c, TS_SHOP_ADS);
    CHECK(c.credit_buy_item == TS_SHOP_DISPATCH, "credit shop: one at a time");
    ts_shop_credit_done(&c, TS_SHOP_DISPATCH, 1, 5000);
    CHECK(ts_has(&c, TS_SHOP_DISPATCH) && c.credit_buy_item < 0 && c.game_credits == 5000 && c.wallet == w, "credit shop: paid, bought, funds untouched");
    ts_shop_buy(&c, TS_SHOP_ADS);
    ts_shop_credit_done(&c, TS_SHOP_ADS, 0, 4000);
    CHECK(!ts_has(&c, TS_SHOP_ADS) && c.credit_buy_item < 0 && c.game_credits == 4000, "credit shop: refused by the game");
    c.local_hour = 23;
    c.upgrades |= 1u << TS_SHOP_NIGHT | 1u << TS_SHOP_ONTIME | 1u << TS_SHOP_AIRCON;
    float f = ts_shop_pay_factor(&c, 1, 0.0f, 1);
    CHECK(f > 1.349f && f < 1.351f, "night + on time + air con = +35 %% (%.3f)", f);
    c.local_hour = 12;
    f = ts_shop_pay_factor(&c, 0, 8000.0f, 0);
    CHECK(f > 0.999f && f < 1.001f, "by day, late, no licence: no surcharge");
}

/* arrow keys: up from the first row goes to the last, down back to the first */
static void test_phone_prev(void) {
    TsCore c;
    ts_init(&c, 6u, 1);
    place_car(ts_places[3].pos);
    wait_s(&c, 0.2f, 0);
    phone_with_offers(&c);
    ts_phone_prev(&c);
    CHECK(c.phone_tab == TS_TAB_RIDES && c.phone_sel == 1, "0.13: up on accept = decline (the last row)");
    ts_phone_next(&c);
    CHECK(c.phone_tab == TS_TAB_RIDES && c.phone_sel == 0, "down again = the first ride");
    ts_phone_tab(&c, 1);
    CHECK(c.phone_tab == TS_TAB_BUS && c.phone_sel == 0, "right = the bus tab");
    ts_phone_prev(&c);
    CHECK(c.phone_tab == TS_TAB_BUS && c.phone_sel == (int)ts_line_count - 1, "0.13: up on the first line = the last bus line");
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
    phone_with_offers(&c);
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

static void draw_dev(const TsHud *h, int device, int theme, const char *name) {
    float scale = 1.0f;
    int w, hh;
    ts_device_size(device, scale, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    TextLog log = {0};
    log.w = w, log.h = hh;
    if (dir) {
        snprintf(path, sizeof(path), "%s/%s%s_%d.json", dir, device == TS_DEVICE_EDGE ? "edge_" : device == TS_DEVICE_TABLET ? "tablet_" : "", name, theme);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, measure_like_preview, &log};
    ts_device_draw(h, device, theme, scale, "18:56", &cv);
    ts_device_finish(device, theme, scale, &cv);
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
        snprintf(path, sizeof(path), "%s/%s%s_%d.bgra", dir, device == TS_DEVICE_EDGE ? "edge_" : device == TS_DEVICE_TABLET ? "tablet_" : "", name, theme);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

static void draw_one(const TsHud *h, int theme, const char *name) { draw_dev(h, TS_DEVICE_PHONE, theme, name); }

/* a regular (met before) is greeted as one, and tells the story further */
static void test_regular(void) {
    int regular = 0, stories = 0;
    for (unsigned seed = 1; seed <= 12; ++seed) {
        TsCore c;
        ts_init(&c, 4242u * seed, 1);
        c.hail_at = 1e9; /* no request timer: the same random numbers as the seeds were picked with */
        c.german = 1;
        for (int i = 0; i < TS_PEOPLE_MAX; ++i) c.met[i] = 2;
        place_car(ts_places[0].pos);
        wait_s(&c, 0.5f, 0);
        phone_with_offers(&c);
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
    c.shift.on = 0;
    phone_with_offers(&c);
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
        phone_with_offers(&c);
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
    phone_with_offers(&c);
    ts_accept_row(&c, 0);
    ts_cancel(&c);
    CHECK(c.reputation < before, "cancelling costs reputation");
    phone_with_offers(&c);
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
    c.shift.on = 1;
    phone_with_offers(&c);
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
        phone_with_offers(&c);
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
    /* 0.13: engine swaps for the bus: the engine list gets four rows, in a FROM list too and in the parts view */
    CHECK(ts_bus_sql("SELECT EngineID, Id, Level FROM List_UpgradeEngine WHERE Ordinal=414", out, sizeof(out)) == 1 && strstr(out, "WHERE Id IN (444001, 1276001, 1496001, 1011001)") &&
              strstr(out, ") AS List_UpgradeEngine WHERE"), "engines: %s", out);
    CHECK(ts_bus_sql("SELECT MediaName FROM Data_Engine DE, List_UpgradeEngine LU WHERE DE.EngineId = LU.EngineId AND Ordinal = 414", out, sizeof(out)) == 1 &&
              strstr(out, "UNION ALL SELECT Id, 414") && strstr(out, ") LU WHERE"), "engines in a FROM list: %s", out);
    CHECK(ts_bus_sql("SELECT EngineID FROM List_UpgradeEngine WHERE Id=444001", out, sizeof(out)) == 0 &&
              ts_bus_sql("SELECT x FROM Garage INNER JOIN List_UpgradeEngine ON List_UpgradeEngine.Id=Garage.Engine WHERE Ordinal=1", out, sizeof(out)) == 0,
          "lookups by Id stay (no row twice)");
    CHECK(ts_bus_sql("SELECT x FROM Data_Engine WHERE List_UpgradeEngine.Id = 3", out, sizeof(out)) == 0, "a column name stays");
    CHECK(ts_bus_sql("CREATE VIEW View_UpgradeParts AS SELECT 0 AS PartEnum FROM List_UpgradeEngine WHERE Ordinal = 414", out, sizeof(out)) == 1,
          "the upgrade parts view");
    CHECK(ts_bus_sql("CREATE TABLE t AS SELECT * FROM List_UpgradeEngine", out, sizeof(out)) == 0 &&
              ts_bus_sql("UPDATE List_UpgradeEngine SET Price = 1", out, sizeof(out)) == 0, "other writes stay");
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
    phone_with_offers(&c);
    c.phone_tab = TS_TAB_SHOP, c.phone_sel = TS_SHOP_BIGBUS;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.row_count == 8 && h.row_selected == 2 + TS_SHOP_BIGBUS - ts_scroll_first(TS_SHOP_ITEMS, TS_SHOP_BIGBUS, 6) &&
          !strcmp(h.row_label[h.row_selected], "Großer Bus"), "shop rows %d sel %d (0.10.1: it scrolls)", h.row_count, h.row_selected);
    {   /* 0.10.1: the last item is reachable and selected on screen */
        TsCore k = c;
        k.phone_sel = TS_SHOP_ITEMS - 1;
        TsHud s;
        ts_hud(&k, &s);
        CHECK(s.row_count == 8 && s.row_selected == 7 && !strcmp(s.row_label[7], "Versicherung"), "shop scrolled to the end: %d %s", s.row_selected, s.row_label[7]);
        k.phone_tab = TS_TAB_SETTINGS, k.setting_count = TS_SETTING_ROWS;
        for (int i = 0; i < TS_SETTING_ROWS; ++i) snprintf(k.setting_label[i], sizeof(k.setting_label[i]), "S%d", i);
        snprintf(k.setting_note[13], sizeof(k.setting_note[13]), "note 13");
        k.phone_sel = 13;
        ts_hud(&k, &s);
        CHECK(s.row_count <= 8 && !strcmp(s.row_label[s.row_selected], "S13") && !strcmp(s.footer, "note 13"), "settings scroll: %s / %s", s.row_label[s.row_selected], s.footer);
        CHECK(ts_scroll_first(16, 0, 7) == 0 && ts_scroll_first(16, 15, 7) == 9 && ts_scroll_first(5, 4, 7) == 0, "scroll window");
    }
    c.credits_mode = 1; /* 0.13: the shop takes the game's credits, the glue pays */
    c.game_credits = 3000;
    ts_phone_accept(&c);
    CHECK(!ts_has(&c, TS_SHOP_BIGBUS) && c.credit_buy_item < 0, "too little money: nothing bought");
    c.game_credits = 60000;
    ts_phone_accept(&c);
    CHECK(c.credit_buy_item == TS_SHOP_BIGBUS && c.credit_buy_cost == 50000, "handed to the game: %lld CR", c.credit_buy_cost);
    ts_shop_credit_done(&c, TS_SHOP_BIGBUS, 1, 10000);
    CHECK(ts_has(&c, TS_SHOP_BIGBUS) && c.game_credits == 10000, "bought: %lld left", c.game_credits);
    ts_phone_accept(&c);
    CHECK(c.credit_buy_item < 0, "not bought twice");
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
    /* 0.13: a row with many values opens a list: pick one, Back closes it */
    c.setting_pressed = -1;
    c.setting_choices[1] = 9, c.setting_index[1] = 2;
    for (int k = 0; k < 9; ++k) snprintf(c.setting_choice[1][k], sizeof(c.setting_choice[1][k]), "Design %d", k);
    ts_phone_accept(&c);
    CHECK(c.pick_row == 1 && c.pick_sel == 2 && c.setting_pressed == -1, "list open: %d %d %d", c.pick_row, c.pick_sel, c.setting_pressed);
    ts_hud(&c, &h);
    CHECK(h.row_count == 8 && !strcmp(h.row_label[h.row_selected], "Design 2"), "list rows %d sel %s", h.row_count, h.row_label[h.row_selected]);
    for (int k = 0; k < 7; ++k) ts_phone_next(&c);
    ts_hud(&c, &h);
    CHECK(c.pick_sel == 0 && !strcmp(h.row_label[h.row_selected], "Design 0"), "list wraps: %d", c.pick_sel);
    ts_phone_prev(&c);
    ts_phone_accept(&c);
    CHECK(c.pick_row == -1 && c.setting_want[1] == 8, "picked: %d want %d", c.pick_row, c.setting_want[1]);
    c.setting_want[1] = -1;
    ts_phone_accept(&c);
    ts_phone_back(&c);
    CHECK(c.pick_row == -1 && c.phone_open && c.setting_want[1] == -1, "back closes the list only");
    c.setting_choices[1] = 0;
}

static void test_phone_draw(void) {
    TsCore c;
    ts_init(&c, 777u, 1);
    ts_set_car(&c, "LAM_MiuraP400_67");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
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
            for (int t = 0; t < TS_THEMES; ++t) draw_dev(&h, TS_DEVICE_EDGE, t, name);
            for (int t = 0; t < TS_THEMES; ++t) draw_dev(&h, TS_DEVICE_TABLET, t, name);
        }
    }
    {   /* 0.13: the rides tab like a driver app: offline GO, looking, one request */
        c.german = 1, c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
        TsOffer keep = c.offers[0];
        static const char *const scene[3] = {"go_offline", "go_seek", "go_request"};
        for (int k = 0; k < 3; ++k) {
            c.shift.on = k > 0, c.offer_count = k == 2, c.offers[0] = keep, c.req_until = k == 2 ? c.t + 14.2 : 0.0;
            TsHud h;
            ts_hud(&c, &h);
            for (int t = 0; t < TS_THEMES; ++t) draw_one(&h, t, scene[k]);
            draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_PURE_DARK, scene[k]);
        }
    }
    {   /* 0.13: the list of designs */
        static const char *const dz[9] = {"Nacht", "Hell", "Festival", "Ozean", "Carbon", "Riviera", "Gold", "Pur", "Pur Dunkel"};
        c.german = 1, c.phone_tab = TS_TAB_SETTINGS, c.phone_sel = 0;
        snprintf(c.setting_label[0], sizeof(c.setting_label[0]), "Handy-Design");
        c.setting_choices[0] = 9, c.setting_index[0] = 0;
        for (int k = 0; k < 9; ++k) snprintf(c.setting_choice[0][k], sizeof(c.setting_choice[0][k]), "%s", dz[k]);
        c.pick_row = 0, c.pick_sel = 8;
        TsHud h;
        ts_hud(&c, &h);
        for (int t = 0; t < TS_THEMES; ++t) draw_one(&h, t, "settings_pick");
        draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_PURE_DARK, "settings_pick");
        c.pick_row = -1, c.setting_choices[0] = 0;
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
    for (int t = 0; t < TS_THEMES; ++t) draw_dev(&h, TS_DEVICE_TABLET, t, "job_en");
    for (int t = 0; t < TS_THEMES; ++t) draw_dev(&h, TS_DEVICE_EDGE, t, "job_en");
    /* 0.5: a bus line on the tablet: the route with the stops done, the one now, the ones to come */
    {
        TsCore b;
        ts_init(&b, 5u, 1);
        b.bus_needs_bus = 0;
        place_car(ts_places[ts_lines[0].stops[0]].pos);
        wait_s(&b, 0.2f, 0);
        ts_accept_row(&b, TS_PHONE_OFFERS);
        for (int k = 0; k < 4 && b.mode == TS_BUS; ++k) {
            wait_s(&b, b.stop_secs + 1.0f, 0);
            drive_to(&b, ts_places[ts_bus_stop(&b, 0, b.stop_i)].pos, 16.0f, 1.5f, 2.5f, 0);
        }
        b.phone_open = 1;
        TsHud bh;
        ts_hud(&b, &bh);
        CHECK(bh.detail_count >= 8, "bus route rows %d", bh.detail_count);
        int now = 0, done = 0;
        for (int i = 0; i < bh.detail_count; ++i) now += bh.detail_kind[i] == TS_DET_STOP_NOW, done += bh.detail_kind[i] == TS_DET_STOP_DONE;
        CHECK(now == 1 && done >= 2, "the route: %d now, %d done", now, done);
        for (int t = 0; t < TS_THEMES; ++t) draw_dev(&bh, TS_DEVICE_TABLET, t, "bus_job_de");
    }
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
    phone_with_offers(&c);
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
    phone_with_offers(&c);
    TsHud h;
    ts_hud(&c, &h);
    int german_rows = 0;
    for (int r = 0; r < h.row_count; ++r) german_rows += strstr(h.row_label[r], "Treffpunkt") != NULL || strstr(h.row_value[r], "Treffpunkt") != NULL;
    german_rows += strstr(h.footer, "Treffpunkt") != NULL;
    CHECK(h.row_count > 0 && german_rows == 0, "the phone's ride list is English");
}

/* 0.4.1 (Marvin: "the croissant line three times in three rides", "long lines are cut off") */
static int dash_style; /* 0.5.1: the design the ride panel and the feed are drawn in */
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
    ts_dash_draw(h, dash_style, 1.0f, &cv);
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
        phone_with_offers(&c);
        ts_phone_accept(&c);
        if (c.mode != TS_UBER) { phone_with_offers(&c); continue; }
        rides++;
        repeats += used[c.job.dest]++ > 0;
        ts_cancel(&c);
        c.phone_open = 0;
    }
    CHECK(rides >= 10 && repeats <= 1, "12 rides from one place: %d taken, %d destinations again", rides, repeats);
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
    ts_feed_draw(l, 3, dash_style, 1.0f, &cv);
    ts_feed_finish(1.0f, &cv);
    if (getenv("GIFS") && dir) /* 0.9.1: one, two, three lines (the GIF fades between them) */
        for (int n = 1; n <= 2; ++n) {
            uint32_t *q = calloc((size_t)w * hh, 4);
            TextLog lg = {0};
            lg.w = w, lg.h = hh;
            snprintf(path, sizeof(path), "%s/giffeed%d_0.json", dir, n);
            lg.json = fopen(path, "w");
            if (lg.json) fprintf(lg.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
            TsPhoneCanvas qv = {q, w, hh, record_text, measure_like_preview, &lg};
            ts_feed_draw(l, n, dash_style, 1.0f, &qv);
            ts_feed_finish(1.0f, &qv);
            if (lg.json) fprintf(lg.json, "]}\n"), fclose(lg.json);
            snprintf(path, sizeof(path), "%s/giffeed%d_0.bgra", dir, n);
            FILE *f = fopen(path, "wb");
            if (f) fwrite(q, 4, (size_t)w * hh, f), fclose(f);
            free(q);
        }
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
    phone_with_offers(&c);
    ts_phone_accept(&c);
    c.stage = TS_UBER_TO_DEST;
    c.leg_t = c.t;
    c.car_ok = 1;
    c.acc_long = -3.0f, c.acc_lat = 2.0f;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.g_on && !h.g_over && h.g_long < 0.0f, "G-meter on with a passenger, inside the circle (%.2f, %.2f)", (double)h.g_lat, (double)h.g_long);
    dash_one(&h, "dash_ok");
    if (getenv("GIFS")) { /* 0.9.1: a wish comes: the panel fades to it */
        TsHud hb = h;
        snprintf(hb.chip3, sizeof(hb.chip3), "%s", "Wish: a quick stop at the café");
        dash_one(&h, "gifdash_a");
        dash_one(&hb, "gifdash_b");
    }
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
    phone_with_offers(&c);
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
    phone_with_offers(&c);
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
    phone_with_offers(&c);
    ts_phone_accept(&c);
    door_drive(&c, ts_places[c.job.pickup].pos, &log);
    door_wait(&c, 3.6f, &log);
    CHECK(c.stage == TS_UBER_TO_DEST && log.n == 0, "doors off: in after 3 s, no door (stage %d, %d)", c.stage, log.n);
}

/* 0.5: lines both ways, announcements, push requests, lost items, streak, day's goal, regulars, meter, the tablet's column */
static void test_service(void) {
    /* a line that isn't a ring, started at its far end: it runs backwards */
    int open_line = -1;
    for (unsigned i = 0; i < ts_line_count; ++i)
        if (ts_lines[i].stops[0] != ts_lines[i].stops[ts_lines[i].stop_count - 1]) open_line = (int)i;
    CHECK(open_line >= 0, "a line with two ends");
    {
        TsCore c;
        ts_init(&c, 4242u, 1);
        c.bus_needs_bus = 0;
        const TsLine *l = &ts_lines[open_line];
        place_car(ts_places[l->stops[l->stop_count - 1]].pos);
        wait_s(&c, 0.2f, 0);
        ts_accept_row(&c, TS_PHONE_OFFERS + open_line);
        CHECK(c.mode == TS_BUS && c.line_rev == 1, "from the far end: backwards");
        CHECK(ts_bus_stop(&c, open_line, 0) == l->stops[l->stop_count - 1] && ts_bus_stop(&c, open_line, (int)l->stop_count - 1) == l->stops[0],
              "stops in reverse");
        CHECK(c.stage == TS_BUS_AT_STOP || arrived_at_first(&c), "at the first stop");
        TsHud h;
        ts_hud(&c, &h);
        CHECK(h.meter >= 0 && h.detail_count >= 3 && h.detail_kind[0] == TS_DET_STOP_NOW, "bus: fares and the route (%d, %d)", h.meter, h.detail_count);
        /* the whole line backwards, with announcements */
        announcements = 0;
        for (int guard = 0; guard < 40 && c.mode == TS_BUS; ++guard) {
            wait_s(&c, c.stop_secs + 1.0f, 0);
            if (c.mode != TS_BUS) break;
            drive_to(&c, ts_places[ts_bus_stop(&c, open_line, c.stop_i)].pos, 16.0f, 1.5f, 2.5f, 0);
            wait_s(&c, 0.5f, 0);
        }
        CHECK(c.mode == TS_IDLE && c.bus_lines_done == 1, "backwards line done");
        int announced = announcements;
        CHECK(announced == (int)l->stop_count - 1, "every stop after the first announced (%d)", announced);
        CHECK(c.log_count == 1 && c.log[0].bus == 1 && c.log[0].place == l->stops[0], "logbook: the line, ending at its first stop");
    }
    {   /* a ring line: the other way round the second time */
        int ring = -1;
        for (unsigned i = 0; i < ts_line_count; ++i)
            if (ts_lines[i].stops[0] == ts_lines[i].stops[ts_lines[i].stop_count - 1]) ring = (int)i;
        TsCore c;
        ts_init(&c, 7u, 0);
        place_car(ts_places[ts_lines[ring].stops[0]].pos);
        wait_s(&c, 0.2f, 0);
        CHECK(bus_dir(&c, ring) == 0, "ring: first time forwards");
        c.line_done[ring] = 1;
        CHECK(bus_dir(&c, ring) == 1, "ring: then the other way");
    }
    {   /* push requests on shift */
        TsCore c;
        ts_init(&c, 99u, 1);
        c.shift.on = 0;
        place_car(ts_places[0].pos);
        car.vel[2] = 10.0f;
        wait_s(&c, 0.5f, 0);
        ts_take_events(&c, (TsEvent[TS_MAX_EVENTS]){0}, TS_MAX_EVENTS);
        int toasts = 0;
        for (int i = 0; i < 120 * 60; ++i) frame(&c, i);
        events_print(&c, 0, &toasts, NULL);
        CHECK(toasts == 0, "no push requests off shift");
        place_car(ts_places[0].pos);
        car.vel[2] = 10.0f;
        ts_shift_toggle(&c);
        events_print(&c, 0, NULL, NULL);
        int requests = 0, first_at = -1;
        TsOffer first;
        memset(&first, 0, sizeof(first));
        for (int i = 0; i < 300 * 60; ++i) {
            if (i % 600 == 0) car.vel[2] = -car.vel[2]; /* drive back and forth */
            frame(&c, i);
            TsEvent ev[TS_MAX_EVENTS];
            int n = ts_take_events(&c, ev, TS_MAX_EVENTS);
            for (int k = 0; k < n; ++k)
                if (ev[k].kind == TS_EV_TOAST && (!strncmp(ev[k].title, "Neue Anfrage", 12) || !strncmp(ev[k].title, "Stammgast", 9))) {
                    requests++;
                    if (first_at < 0) first_at = i, first = c.offers[0];
                }
            if (first_at >= 0 && i == first_at + 10 * 60) {
                ts_phone_toggle(&c);
                CHECK(c.offer_count == 1 && c.offers[0].pickup == first.pickup && c.offers[0].dest == first.dest, "0.13: the phone shows that one request");
                ts_phone_toggle(&c);
            }
        }
        CHECK(requests >= 5 && requests <= 14, "0.13: a request every 30 to 50 s when none is taken (%d)", requests);
    }
    {   /* a lost item: noticed later, brought back, paid */
        TsCore c;
        ts_init(&c, 5u, 1);
        place_car(ts_places[0].pos);
        wait_s(&c, 0.2f, 0);
        c.lost.state = 1, c.lost.person = 3, c.lost.place = 0, c.lost.item = 4, c.lost.at = c.t + 1.0;
        place_car(ts_places[2].pos);
        wait_s(&c, 1.5f, 0);
        CHECK(c.lost.state == 2 && c.nav_on && ts_distance_xz(c.nav_target, ts_places[0].pos) < 0.1f, "lost item: GPS back");
        long long before = c.wallet;
        float rep = c.reputation;
        drive_to(&c, ts_places[0].pos, 25.0f, 2.0f, 3.0f, 0);
        wait_s(&c, 0.5f, 0);
        CHECK(c.lost.state == 0 && c.wallet == before + 60 * c.cr_scale && c.reputation > rep && !c.nav_on, "lost item returned");
        c.lost.state = 1, c.lost.place = 0, c.lost.at = c.t + 0.5;
        wait_s(&c, 1.0f, 0);
        CHECK(c.lost.state == 0, "still there: they take it themselves");
        int dropped = 0;
        for (unsigned seed = 1; seed <= 300; ++seed) {
            TsCore k;
            ts_init(&k, seed * 2654435761u, 1);
            service_maybe_lost_for_test(&k);
            dropped += k.lost.state == 1;
        }
        CHECK(dropped > 5 && dropped < 50, "lost items now and then (%d of 300)", dropped);
    }
    {   /* streak and the day's goal */
        TsCore c;
        ts_init(&c, 11u, 1);
        c.shift.on = 0, ts_shift_toggle(&c); /* a fresh shift */
        long long w = c.wallet;
        int bonus = 0;
        for (int i = 0; i < 4; ++i) bonus += service_streak_for_test(&c, 5, 1000);
        CHECK(c.streak == 4 && bonus == 50 + 100 && c.wallet == w + bonus, "streak: +5 %% from the 3rd, +10 %% the 4th (%d)", bonus);
        service_streak_for_test(&c, 3, 1000);
        CHECK(c.streak == 0, "3 stars end the streak");
        c.shift.jobs = 5, c.shift.rated = 5, c.shift.stars_sum = 21;
        w = c.wallet;
        career_goal_for_test(&c);
        CHECK(c.shift.goal_done && c.wallet == w + TS_GOAL_BONUS * c.cr_scale, "day's goal paid");
        career_goal_for_test(&c);
        CHECK(c.wallet == w + TS_GOAL_BONUS * c.cr_scale, "only once");
    }
    {   /* a regular asks for you; the taxi meter runs */
        TsCore c;
        ts_init(&c, 31u, 1);
        for (int k = 0; k < 10; ++k) c.met[k] = 3;
        place_car(ts_places[0].pos);
        wait_s(&c, 0.2f, 0);
        int regular = 0;
        for (int i = 0; i < 60; ++i) {
            ts_refresh_offers(&c);
            for (int k = 0; k < c.offer_count; ++k)
                if (c.offers[k].regular) {
                    regular++;
                    CHECK(c.met[c.offers[k].passenger] >= 2, "a regular is someone you know");
                }
        }
        CHECK(regular > 5, "regulars ask now and then (%d)", regular);
        ts_refresh_offers(&c);
        TsOffer o = c.offers[0];
        ts_accept_row(&c, 0);
        TsHud h;
        ts_hud(&c, &h);
        CHECK(h.meter == -1 && h.detail_count >= 4, "no meter before the pickup; the tablet shows the passenger");
        drive_to(&c, ts_places[o.pickup].pos, 20.0f, 2.0f, 3.0f, 0);
        wait_s(&c, 4.5f, 0);
        ts_hud(&c, &h);
        CHECK(c.stage == TS_UBER_TO_DEST && h.meter == o.pay, "0.13: the fixed fare from the start (%d of %d)", h.meter, o.pay);
        drive_to(&c, ts_places[o.dest].pos, 20.0f, 2.0f, 3.0f, 0);
        ts_hud(&c, &h);
        CHECK(c.mode == TS_IDLE || h.meter == o.pay, "0.13: the fare stays fixed while driving");
    }
    {   /* every tab fills the tablet's column */
        TsCore c;
        ts_init(&c, 3u, 0);
        place_car(ts_places[0].pos);
        wait_s(&c, 0.2f, 0);
        phone_with_offers(&c);
        for (int t = 0; t < TS_TABS; ++t) {
            c.phone_tab = t, c.phone_sel = 0;
            TsHud h;
            ts_hud(&c, &h);
            CHECK(h.detail_title[0] && h.detail_count > 0, "tab %d: tablet column", t);
        }
    }
}

/* 0.5: pictures of the new things for the docs (SCREENS=1 PHONE_PREVIEW=<dir>; then tools/phone_preview.py <dir>) */
static void feed_shot(const TsFeedLine *l, int n, const char *name) {
    int w, hh;
    ts_feed_size(1.0f, &w, &hh);
    uint32_t *px = calloc((size_t)w * hh, 4);
    TextLog log = {0};
    log.w = w, log.h = hh;
    const char *dir = getenv("PHONE_PREVIEW");
    char path[512];
    if (dir) {
        snprintf(path, sizeof(path), "%s/%s.json", dir, name);
        log.json = fopen(path, "w");
        if (log.json) fprintf(log.json, "{\"w\":%d,\"h\":%d,\"texts\":[\n", w, hh);
    }
    TsPhoneCanvas cv = {px, w, hh, record_text, measure_like_preview, &log};
    ts_feed_draw(l, n, dash_style, 1.0f, &cv);
    ts_feed_finish(1.0f, &cv);
    if (log.json) {
        fprintf(log.json, "]}\n");
        fclose(log.json);
        snprintf(path, sizeof(path), "%s/%s.bgra", dir, name);
        FILE *f = fopen(path, "wb");
        if (f) fwrite(px, 4, (size_t)w * hh, f), fclose(f);
    }
    free(px);
}

static void screens(void) {
    TsCore c;
    ts_init(&c, 4711u, 1);
    ts_set_car(&c, "LAM_MiuraP400_67");
    for (int k = 0; k < 12; ++k) c.met[k] = (unsigned char)(2 + k % 4);
    c.wallet = 18450, c.reputation = 72.0f;
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
    for (int i = 0; i < 30; ++i) { /* requests with a regular in them */
        ts_refresh_offers(&c);
        int reg = 0;
        for (int k = 0; k < c.offer_count; ++k) reg |= c.offers[k].regular;
        if (reg && c.offer_count == 3) break;
    }
    for (int k = 0; k < c.offer_count; ++k) if (c.offers[k].regular) c.phone_sel = k;
    TsHud h;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_NIGHT, "shot_edge_rides");
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_NIGHT, "shot_tablet_rides");
    c.phone_tab = TS_TAB_BUS, c.phone_sel = 1;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_OCEAN, "shot_edge_bus");
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_LIGHT, "shot_tablet_bus");
    /* the account with a logbook, a running shift, the day's goal and a streak */
    c.shift.on = 1, c.shift.start_t = c.t - 3000, c.shift.jobs = 4, c.shift.lines = 1, c.shift.rated = 5, c.shift.stars_sum = 22;
    c.shift.earned = 5260, c.shift.goal_done = 1, c.streak = 4;
    int who[5] = {3, 7, 1, 0, 9}, dest[5] = {27, 5, 100, 4, 53}, pay[5] = {1180, 940, 1610, 0, 720}, st[5] = {5, 4, 5, 5, 4};
    for (int i = 4; i >= 0; --i) {
        TsLogEntry *e = &c.log[4 - i];
        e->bus = i == 3, e->who = i == 3 ? 1 : who[i], e->place = i == 3 ? 5 : dest[i], e->pay = i == 3 ? 820 : pay[i], e->stars = st[i];
    }
    c.log_count = 5;
    c.phone_tab = TS_TAB_ACCOUNT, c.phone_sel = 0;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_GOLD, "shot_tablet_account");
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_RIVIERA, "shot_edge_account");
    c.phone_tab = TS_TAB_SETTINGS, c.phone_sel = 0;
    static const char *const sl[8][2] = {{"Gerät", "Edge"}, {"Design", "Carbon"}, {"Sprache", "Spiel"}, {"Zeitlimits", "Normal"},
                                         {"Fahrgäste meckern", "An"}, {"Navigation automatisch", "An"},
                                         {"Ruf, Sonderfahrten, Wünsche", "An"}, {"Stimmen (Versuch)", "Normal"}};
    c.setting_count = 8;
    for (int i = 0; i < 8; ++i) {
        snprintf(c.setting_label[i], sizeof(c.setting_label[i]), "%s", sl[i][0]);
        snprintf(c.setting_value[i], sizeof(c.setting_value[i]), "%s", sl[i][1]);
    }
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_CARBON, "shot_edge_options");
    /* a ride: the tablet during the job, the taxi meter by the speedometer */
    c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    ts_refresh_offers(&c);
    TsOffer o = c.offers[0];
    ts_accept_row(&c, 0);
    drive_to(&c, ts_places[o.pickup].pos, 20.0f, 2.0f, 3.0f, 0);
    wait_s(&c, 4.5f, 0);
    car.vel[2] = 15.0f;
    for (int i = 0; i < 60 * 90; ++i) frame(&c, i); /* a minute and a half on the way */
    events_print(&c, 0, NULL, NULL);
    c.phone_open = 1;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_CARBON, "shot_tablet_ride");
    h.sign_over = 0, h.g_lat = 0.25f, h.g_long = -0.3f, h.g_over = 0;
    dash_one(&h, "shot_dash_taxameter");
    /* a bus line on the tablet, its fares by the speedometer */
    TsCore b;
    ts_init(&b, 5u, 1);
    b.bus_needs_bus = 0;
    place_car(ts_places[ts_lines[0].stops[0]].pos);
    wait_s(&b, 0.2f, 0);
    ts_accept_row(&b, TS_PHONE_OFFERS);
    for (int k = 0; k < 4 && b.mode == TS_BUS; ++k) {
        wait_s(&b, b.stop_secs + 1.0f, 0);
        drive_to(&b, ts_places[ts_bus_stop(&b, 0, b.stop_i)].pos, 16.0f, 1.5f, 2.5f, 0);
    }
    wait_s(&b, b.stop_secs + 1.0f, 0);
    b.phone_open = 1;
    ts_hud(&b, &h);
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_FESTIVAL, "shot_tablet_busline");
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_NIGHT, "shot_tablet_busline_night");
    car.vel[2] = 12.0f;
    for (int i = 0; i < 60 * 20; ++i) frame(&b, i);
    ts_hud(&b, &h);
    h.g_lat = -0.2f, h.g_long = 0.15f;
    dash_one(&h, "shot_dash_bus");
    /* the lines in the feed: announcements, driving through, a lost item, the streak */
    TsFeedLine bus[3] = {{"Ansage", "Nächster Halt: Montellino · Frantoio. Kein Haltewunsch.", 3},
                         {"Leitstelle", "Durchgefahren: Montellino · Frantoio", 0},
                         {"Ansage", "Nächster Halt: Horizon Festival. Endhaltestelle, bitte alle aussteigen.", 3}};
    feed_shot(bus, 3, "shot_feed_bus");
    TsFeedLine svc[3] = {{"Horizon Transit", "Serie 4 · +118 € Bonus", 0},
                         {"Lena", "Meine Schlüssel liegen noch bei dir im Auto! Ohne die komm ich nicht rein.", 0},
                         {"Lena", "Du bist ein Schatz, danke!", 1}};
    feed_shot(svc, 3, "shot_feed_service");

    /* 0.5.1: the job with its meters, a wish till arrival, the reply prompt, "floor it", talking back in the feed */
    c.phone_open = 1, c.wish = TS_WISH_SLOWER, c.wish_limit = 70, c.job.max_kmh = 70;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot051_phone_job");
    h.sign_over = 1, h.g_lat = 0.1f, h.g_long = 0.05f, h.g_over = 0;
    dash_one(&h, "shot051_dash_wish");
    c.ask = 3, c.ask_until = c.t + 7.2, c.replies_on = 1;
    snprintf(c.reply_key, sizeof(c.reply_key), "B");
    ts_hud(&c, &h);
    h.sign_over = 0, h.g_lat = -0.15f, h.g_long = 0.1f;
    dash_one(&h, "shot051_dash_reply");
    c.ask = -1, c.wish = TS_WISH_FLOORIT, c.wish_limit = 180, c.floorit = 1;
    ts_hud(&c, &h);
    h.g_lat = 0.05f, h.g_long = 0.6f;
    dash_one(&h, "shot051_dash_floorit");
    TsFeedLine talk[3] = {{"Mia", "Wie lange brauchen wir ungefähr noch?", 0},
                          {"Du", "Nicht mehr lange. Ich bring dich hin.", 1},
                          {"Mia", "Gut, dann lehn ich mich zurück.", 1}};
    feed_shot(talk, 3, "shot051_feed_reply");
    TsFeedLine fl[3] = {{"Mia", "Moment, das ist ein Lamborghini Huracán? Vergiss das Tempolimit, gib Vollgas!", 1},
                        {"Mia", "WAHNSINN! Mein Magen ist noch hinten!", 1},
                        {"Mia", "Hast du das gespürt? Unglaublich!", 1}};
    feed_shot(fl, 3, "shot051_feed_floorit");
    /* 0.5.1: the new design "Pure" */
    c.wish = TS_WISH_NONE, c.floorit = 0, c.job.max_kmh = 120;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_PURE, "shot051_pure_job");
    c.phone_open = 1, c.mode = TS_IDLE, c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    ts_refresh_offers(&c);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_PURE, "shot051_pure_rides");
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_PURE, "shot051_pure_tablet");
    c.phone_tab = TS_TAB_ACCOUNT, c.phone_sel = 0;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_PURE, "shot051_pure_account");
    /* 0.5.1: the ride panel and the feed in each design, as glass */
    c.phone_tab = TS_TAB_RIDES;
    ts_accept_row(&c, 0);
    place_car(ts_places[c.job.pickup].pos);
    wait_s(&c, 8.0f, 0);
    c.wish = TS_WISH_SLOWER, c.wish_limit = 70, c.job.max_kmh = 70;
    ts_hud(&c, &h);
    h.sign_over = 1, h.g_lat = 0.2f, h.g_long = -0.25f, h.g_over = 0, h.g_on = 1;
    CHECK(h.trip_on && h.trip_to[0] && h.trip_label[0], "ride card: where to (%s / %s)", h.trip_label, h.trip_to);
    CHECK(strcmp(h.trip_to, ts_place_name(&c, c.job.dest)) == 0 || c.stage < TS_UBER_TO_DEST, "ride card: the destination once the passenger is in");
    h.trip_progress = 0.42f;
    snprintf(h.corner, sizeof(h.corner), "%s", "4:12 · 88%");
    TsFeedLine glass[3] = {{"Mia", "Wie lange brauchen wir ungefähr noch?", 0},
                           {"Du", "Nicht mehr lange. Ich bring dich hin.", 1},
                           {"Mia", "Gut, dann lehn ich mich zurück.", 1}};
    for (int th = 0; th < TS_THEMES; ++th) {
        char name[48];
        dash_style = th;
        snprintf(name, sizeof(name), "shot051_glass_dash_%d", th);
        dash_one(&h, name);
        snprintf(name, sizeof(name), "shot051_glass_feed_%d", th);
        feed_shot(glass, 3, name);
    }
    dash_style = 0;
}

/* ------------------------------------------------------------------ 0.6 */

/* one ride from wherever the car is: the request in row `row`, driven gently; returns the stars (0 = no ride) */
static int ride_once(TsCore *c, int row) {
    wait_s(c, 0.3f, 0);
    ts_phone_toggle(c);
    if (!c->phone_open) ts_phone_toggle(c);
    if (c->mode == TS_IDLE && c->shift.on) ts_refresh_offers(c);
    if (c->offer_count <= row) { c->phone_open = 0; return 0; }
    ts_accept_row(c, row);
    if (c->mode != TS_UBER) return 0;
    long long before = c->jobs_done;
    c->next_wish_t = 1e9; /* no wished stop on the way */
    drive_to(c, ts_places[c->job.pickup].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(c, 6.0f, 0);
    if (c->job.pool) { /* a shared ride: the second's pickup, the first's destination, then on */
        drive_to(c, ts_places[c->job.pool_pickup].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(c, 6.0f, 0);
        drive_to(c, ts_places[c->job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(c, 5.0f, 0);
    }
    drive_to(c, ts_places[c->job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(c, 4.0f, 0);
    return c->jobs_done > before ? (int)c->met_stars[c->met[c->job.passenger] ? c->job.passenger : 0] : 0;
}

static int card_toasts; /* payout pop-ups seen while cards are on */
static void count_payout_toasts(TsCore *c) {
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(c, ev, TS_MAX_EVENTS);
    for (int i = 0; i < n; ++i) card_toasts += ev[i].kind == TS_EV_TOAST && ev[i].icon == TS_ICON_PAYOUT;
}

static void test_cards(void) {
    TsCore c;
    ts_init(&c, 4242u, 1);
    c.cards_on = 1;
    ts_set_car(&c, "AUD_RS5_11");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
    ts_accept_row(&c, c.offers[0].pool ? 1 : 0);
    CHECK(c.mode == TS_UBER && !c.job.pool, "cards: ride started");
    c.next_wish_t = 1e9; /* no wished stop on the way */
    drive_to(&c, ts_places[c.job.pickup].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 6.0f, 0);
    drive_to(&c, ts_places[c.job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
    if (getenv("DBG")) printf("dbg stage %d wish %d mode %d dist %.0f\n", c.stage, c.wish, c.mode, ts_distance_xz(c.car.pos, ts_places[c.job.dest].pos));
    card_toasts = 0;
    long long wallet = c.wallet;
    int done = 0;
    for (int i = 0; i < 60 * 4 && !done; ++i) {
        frame(&c, i);
        done = c.mode == TS_IDLE;
        if (!done) count_payout_toasts(&c);
    }
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(&c, ev, TS_MAX_EVENTS), payout_sound = 0;
    for (int i = 0; i < n; ++i) {
        card_toasts += ev[i].kind == TS_EV_TOAST && ev[i].icon == TS_ICON_PAYOUT;
        payout_sound += ev[i].kind == TS_EV_SOUND && ev[i].sound == TS_SOUND_PAYOUT;
    }
    CHECK(done, "cards: ride done");
    CHECK(card_toasts == 0 && payout_sound == 1, "cards: no payout pop-up, the payout sound (%d, %d)", card_toasts, payout_sound);
    CHECK(c.card.kind == TS_CARD_RIDE, "cards: the ride's card (%d)", c.card.kind);
    CHECK(c.card.money == (int)(c.wallet - wallet), "cards: the money is what was paid (%d, %lld)", c.card.money, c.wallet - wallet);
    CHECK(c.card.stars >= 1 && c.card.stars <= 5 && c.card.note[0] && c.card.pairs >= 2, "cards: stars %d, comment, pairs %d", c.card.stars, c.card.pairs);
    CHECK(strstr(c.card.note, "„") != NULL, "cards: the comment in German quotes: %s", c.card.note);
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.card.kind == TS_CARD_RIDE && h.card_serial == c.card_serial && h.card_anim > 3.0f, "cards: the HUD carries the card");
    /* it goes after 8 s */
    wait_s(&c, 8.5f, 0);
    CHECK(c.card.kind == TS_CARD_NONE, "cards: gone after 8 s (%d)", c.card.kind);
    /* the next job hides a card at once */
    TsCard k;
    memset(&k, 0, sizeof(k));
    c.card.kind = TS_CARD_RIDE, c.card.secs = 8.0f, c.card_at = c.t;
    phone_with_offers(&c);
    if (!c.phone_open) phone_with_offers(&c);
    ts_accept_row(&c, 0);
    wait_s(&c, 0.2f, 0);
    ts_hud(&c, &h);
    CHECK(c.mode == TS_UBER && h.card.kind == 0 && c.card.kind == 0, "cards: a job hides the card");
    ts_cancel(&c);
    /* without the panel: no card, the pop-up as before */
    TsCore d;
    ts_init(&d, 4243u, 0);
    d.cards_on = 0;
    place_car(ts_places[0].pos);
    card_toasts = 0;
    ride_once(&d, 0);
    for (int i = 0; i < 10; ++i) count_payout_toasts(&d);
    CHECK(d.card.kind == 0, "cards off: no card");

    /* the shift's review */
    TsCore s2;
    ts_init(&s2, 99u, 1);
    s2.cards_on = 1;
    ts_set_car(&s2, "AUD_RS5_11");
    place_car(ts_places[0].pos);
    wait_s(&s2, 0.3f, 0);
    s2.shift.on = 0, ts_shift_toggle(&s2);
    ride_once(&s2, 0);
    wait_s(&s2, 9.0f, 0);
    ride_once(&s2, 1);
    wait_s(&s2, 9.0f, 0);
    CHECK(s2.card.kind == 0, "shift: the ride cards are gone");
    long long earned = s2.shift.earned;
    int jobs = s2.shift.jobs;
    ts_shift_toggle(&s2);
    CHECK(!s2.shift.on && s2.card.kind == TS_CARD_SHIFT, "shift: the review card (%d)", s2.card.kind);
    CHECK(s2.card.money == (int)earned && jobs >= 1, "shift: earned %d = %lld (jobs %d)", s2.card.money, earned, jobs);
    CHECK(s2.card.pairs == 4 && s2.card.secs >= 12.0f && s2.card.note[0], "shift: 4 numbers, a note, 14 s (%d)", s2.card.pairs);
    CHECK(!strcmp(s2.card.label[0], "Fahrten") && s2.card.tier == s2.tier, "shift: rides first, the tier's medal");
    /* queued cards come one after the other */
    TsCore q;
    ts_init(&q, 5u, 1);
    q.cards_on = 1;
    q.job.passenger = 1, q.job.ride_class = TS_CLASS_COMFORT, q.job.party = 1;
    card_ride(&q, 5, 500, 450, 0, 0);
    card_tier(&q, TS_TIER_SILVER);
    CHECK(q.card.kind == TS_CARD_RIDE && q.card_queued == 1, "cards: the tier waits for the ride's card");
    unsigned serial = q.card_serial;
    q.t += 8.1;
    card_step(&q);
    CHECK(q.card.kind == TS_CARD_TIER && q.card_serial == serial + 1 && q.card_queued == 0, "cards: then the tier's card");
    CHECK(!strcmp(q.card.name, "Silber-Fahrer") && q.card.note[0], "cards: %s · %s", q.card.name, q.card.note);
}

static void test_classes(void) {
    TsCore c;
    ts_init(&c, 31u, 1);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    int seen[TS_CLASSES] = {0}, party_ok = 1, xl = 0;
    const char *cars[] = {"ALF_BreraTi_09", "BMW_X5M_11", "AUD_RS5_11"};
    for (int k = 0; k < 3; ++k) {
        ts_set_car(&c, cars[k]);
        for (int r = 0; r < 20; ++r) {
            ts_refresh_offers(&c);
            for (int i = 0; i < c.offer_count; ++i) {
                const TsOffer *o = &c.offers[i];
                seen[o->ride_class]++;
                if (k == 0) CHECK(o->ride_class == TS_CLASS_SAVER, "a small car: Saver (%d)", o->ride_class);
                if (k == 1 && o->ride_class == TS_CLASS_XL) xl++, party_ok &= o->party >= 2 && o->party <= 4;
                if (k == 2) CHECK(o->ride_class != TS_CLASS_BLACK, "Bronze: no Black yet");
            }
        }
    }
    CHECK(xl > 10 && party_ok, "off-roader: XL with 2 to 4 people (%d)", xl);
    c.tier = TS_TIER_SILVER;
    int black = 0, comfort = 0;
    for (int r = 0; r < 30; ++r) {
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) black += c.offers[i].ride_class == TS_CLASS_BLACK, comfort += c.offers[i].ride_class == TS_CLASS_COMFORT;
    }
    CHECK(black > 10 && comfort > 5, "Silver in a saloon: Black and Comfort (%d / %d)", black, comfort);
    c.extras_off = 1;
    black = 0;
    for (int r = 0; r < 20; ++r) {
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) black += c.offers[i].ride_class == TS_CLASS_BLACK;
    }
    CHECK(black == 0, "career extras off: no Black");
    /* pay: Black 1.45, Saver 0.85 of the same ride */
    CHECK(market_class_pay(TS_CLASS_BLACK) > market_class_pay(TS_CLASS_XL) && market_class_pay(TS_CLASS_XL) > 1.0f &&
          market_class_pay(TS_CLASS_SAVER) < 1.0f, "class pay order");
    /* strict: the same hard braking costs a Black passenger more */
    float drop[2];
    for (int k = 0; k < 2; ++k) {
        TsCore b;
        ts_init(&b, 8u, 1);
        place_car(ts_places[0].pos);
        wait_s(&b, 0.3f, 0);
        phone_with_offers(&b);
        ts_accept_row(&b, 0);
        b.job.ride_class = k ? TS_CLASS_BLACK : TS_CLASS_COMFORT;
        b.job.ptype = TS_P_NORMAL;
        b.complaints_off = 1;
        for (int i = 0; i < 60 * 13; ++i) { /* up to 25 m/s gently, before the passenger is in */
            car.vel[2] = car.vel[2] + 2.0f * DT < 25.0f ? car.vel[2] + 2.0f * DT : 25.0f;
            frame(&b, i);
        }
        wait_s(&b, 1.0f, 0);
        b.stage = TS_UBER_TO_DEST, b.sat = 80.0f, b.calm_since = b.t;
        for (int i = 0; i < 60 * 2; ++i) { /* 11 m/s^2 braking for 2 s */
            car.vel[2] = car.vel[2] > 11.0f * DT ? car.vel[2] - 11.0f * DT : 0.0f;
            frame(&b, i);
            ts_take_events(&b, (TsEvent[TS_MAX_EVENTS]){0}, TS_MAX_EVENTS);
        }
        drop[k] = 80.0f - b.sat;
    }
    CHECK(drop[1] > drop[0] * 1.2f && drop[0] > 0.5f, "Black is strict: %.1f vs %.1f", drop[1], drop[0]);
    /* the phone shows the class and the people */
    TsCore p;
    ts_init(&p, 31u, 1);
    place_car(ts_places[0].pos);
    wait_s(&p, 0.3f, 0);
    ts_set_car(&p, "BMW_X5M_11");
    int shown = 0;
    for (int r = 0; r < 60 && !shown; ++r) {
        ts_phone_toggle(&p);
        if (!p.phone_open) ts_phone_toggle(&p);
        ts_refresh_offers(&p);
        TsHud h;
        ts_hud(&p, &h);
        for (int i = 0; i < h.row_count; ++i) shown |= !strncmp(h.row_label[i], "XL · ", 5) && strstr(h.row_label[i], " +") != NULL;
        p.phone_open = 0;
    }
    CHECK(shown, "phone: \"XL · Name +2 · ...\"");
}

static void test_hotspots(void) {
    TsCore c;
    ts_init(&c, 77u, 1);
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    CHECK(c.hot[0].place < 0, "no hotspot at the start");
    c.t = 61.0;
    ts_step(&c);
    CHECK(c.hot[0].place >= 0 && c.hot[0].factor >= 1.19f && c.hot[0].factor <= 1.51f, "a hotspot after a minute (%d, %.1f)", c.hot[0].place,
          c.hot[0].factor);
    CHECK(ts_places[c.hot[0].place].kind != TS_PLACE_SPOT, "hotspots are towns or the festival");
    double left = c.hot[0].until - c.t;
    CHECK(left >= 239.0 && left <= 421.0, "4 to 7 minutes (%.0f)", left);
    float f = market_surge(&c, c.hot[0].place);
    CHECK(fabsf(f - c.hot[0].factor) < 0.01f, "surge at the place");
    /* a place far away: none */
    int far = -1;
    for (unsigned i = 0; i < ts_place_count && far < 0; ++i)
        if (ts_distance_xz(ts_places[i].pos, ts_places[c.hot[0].place].pos) > 5000.0f &&
            (c.hot[1].place < 0 || ts_distance_xz(ts_places[i].pos, ts_places[c.hot[1].place].pos) > 5000.0f)) far = (int)i;
    CHECK(far >= 0 && market_surge(&c, far) == 1.0f, "no surge far away");
    /* offers picked up there pay more, by the factor */
    place_car(ts_places[c.hot[0].place].pos);
    wait_s(&c, 0.3f, 0);
    int surged = 0;
    for (int r = 0; r < 5; ++r) {
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) {
            const TsOffer *o = &c.offers[i];
            float want = market_surge(&c, o->pickup);
            CHECK(fabsf(o->surge - want) < 0.001f, "offer's surge");
            surged += o->surge > 1.01f;
        }
    }
    CHECK(surged > 0, "requests in the hotspot have a surcharge (%d)", surged);
    TsHud h;
    c.phone_open = 1, c.phone_tab = TS_TAB_RIDES;
    ts_hud(&c, &h);
    CHECK(strchr(h.row_value[1], 'x') != NULL, "the phone names the hotspot: %s", h.row_value[1]);
    /* the platinum tier: 0.2 more */
    c.tier = TS_TIER_PLATINUM;
    CHECK(fabsf(market_surge(&c, c.hot[0].place) - c.hot[0].factor - 0.1f) < 0.01f, "Platinum: hotspots pay 0.1x more");
    c.tier = 0;
    /* they end and come again */
    c.t = c.hot[0].until + 1.0;
    int before = c.hot[0].place;
    ts_step(&c);
    CHECK(c.hot[0].place >= 0 && (c.hot[0].until > c.t), "a new hotspot when the old one ends (%d -> %d)", before, c.hot[0].place);
    /* on shift a pop-up says where (once per new hotspot) */
    TsCore s;
    ts_init(&s, 78u, 1);
    place_car(ts_places[0].pos);
    wait_s(&s, 0.3f, 0);
    s.shift.on = 0, ts_shift_toggle(&s);
    ts_take_events(&s, (TsEvent[TS_MAX_EVENTS]){0}, TS_MAX_EVENTS);
    s.t = 61.0;
    ts_step(&s);
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(&s, ev, TS_MAX_EVENTS), busy = 0;
    for (int i = 0; i < n; ++i) busy += ev[i].kind == TS_EV_TOAST && !strncmp(ev[i].title, "Viel los:", 9);
    int near = s.hot[0].place >= 0 && ts_distance_xz(s.car.pos, ts_places[s.hot[0].place].pos) < 6000.0f;
    if (s.hot[1].place >= 0) near |= ts_distance_xz(s.car.pos, ts_places[s.hot[1].place].pos) < 6000.0f;
    CHECK(busy == near, "shift: one pop-up for a near hotspot (%d, near %d)", busy, near);
    /* career extras off: none */
    TsCore o;
    ts_init(&o, 79u, 1);
    o.extras_off = 1;
    o.t = 100.0;
    ts_step(&o);
    CHECK(o.hot[0].place < 0 && market_surge(&o, 0) == 1.0f, "extras off: no hotspots");
}

static void test_tiers(void) {
    TsCore c;
    ts_init(&c, 3u, 1);
    CHECK(ts_tier_for(&c) == TS_TIER_BRONZE, "start: Bronze");
    c.jobs_done = 10, c.rating_sum = 42, c.rating_count = 10, c.reputation = 56.0f;
    CHECK(ts_tier_for(&c) == TS_TIER_SILVER, "10 jobs, 4.2, 56: Silver");
    c.reputation = 54.0f;
    CHECK(ts_tier_for(&c) == TS_TIER_BRONZE, "reputation 54: not yet");
    c.jobs_done = 29, c.bus_lines_done = 1, c.rating_sum = 130, c.rating_count = 30, c.reputation = 70.0f;
    CHECK(ts_tier_for(&c) == TS_TIER_GOLD, "30 jobs (lines count), 4.33, 70: Gold");
    c.jobs_done = 80, c.rating_sum = 4 * 80 + 50, c.rating_count = 80, c.reputation = 85.0f;
    CHECK(ts_tier_for(&c) == TS_TIER_PLATINUM, "80 jobs, 4.63, 85: Platinum");
    /* reaching one: card (with the panel), save; never down again */
    TsCore t;
    ts_init(&t, 4u, 1);
    t.cards_on = 1;
    t.jobs_done = 10, t.rating_sum = 45, t.rating_count = 10, t.reputation = 60.0f;
    market_check_tier(&t);
    TsEvent ev[TS_MAX_EVENTS];
    int n = ts_take_events(&t, ev, TS_MAX_EVENTS), saves = 0;
    for (int i = 0; i < n; ++i) saves += ev[i].kind == TS_EV_SAVE;
    CHECK(t.tier == TS_TIER_SILVER && t.card.kind == TS_CARD_TIER && saves == 1, "Silver reached: card and save");
    t.reputation = 20.0f;
    market_check_tier(&t);
    CHECK(t.tier == TS_TIER_SILVER, "a tier is never lost");
    /* the pay: Gold 5 %, Platinum 10 % */
    float base = career_pay_factor(&t);
    t.tier = TS_TIER_GOLD;
    CHECK(fabsf(career_pay_factor(&t) / base - 1.05f) < 0.001f, "Gold: +5 %%");
    t.tier = TS_TIER_PLATINUM;
    CHECK(fabsf(career_pay_factor(&t) / base - 1.10f) < 0.001f, "Platinum: +10 %%");
    /* the account tab: the tier with what's missing, a meter */
    TsCore a;
    ts_init(&a, 5u, 1);
    a.jobs_done = 6, a.rating_sum = 30, a.rating_count = 6, a.reputation = 60.0f;
    a.phone_open = 1, a.phone_tab = TS_TAB_ACCOUNT;
    TsHud h;
    ts_hud(&a, &h);
    int found = 0;
    for (int i = 0; i < h.row_count; ++i)
        if (!strcmp(h.row_label[i], "Stufe")) {
            found = 1;
            CHECK(!strcmp(h.row_value[i], "Bronze · noch 4"), "tier row: %s", h.row_value[i]);
            CHECK((h.row_meter_on >> i) & 1u && h.row_meter[i] > 0.55f && h.row_meter[i] < 0.65f, "tier meter %.2f", h.row_meter[i]);
        }
    CHECK(found && h.row_count <= 9, "the account shows the tier (%d rows)", h.row_count);
    CHECK(strstr(h.footer, "Trinkgeld") != NULL, "the tips moved to the footer: %s", h.footer);
    /* in English */
    TsCore e;
    ts_init(&e, 6u, 0);
    e.cards_on = 1;
    card_tier(&e, TS_TIER_GOLD);
    CHECK(!strcmp(e.card.name, "Gold driver") && !strcmp(e.card.title, "NEW TIER") && strstr(e.card.note, "pay") != NULL,
          "English tier card: %s / %s", e.card.name, e.card.note);
    e.jobs_done = 6, e.rating_sum = 30, e.rating_count = 6, e.reputation = 60.0f;
    e.phone_open = 1, e.phone_tab = TS_TAB_ACCOUNT;
    ts_hud(&e, &h);
    int en = 0;
    for (int i = 0; i < h.row_count; ++i) en |= !strcmp(h.row_label[i], "Tier") && !strcmp(h.row_value[i], "Bronze · 4 more");
    CHECK(en, "English tier row");
    CHECK(!strcmp(ts_class_name(TS_CLASS_SAVER, 0), "Saver") && !strcmp(ts_class_name(TS_CLASS_SAVER, 1), "Spar"), "class names");
}

static void test_pool(void) {
    TsCore c;
    ts_init(&c, 515u, 1);
    c.cards_on = 1;
    ts_set_car(&c, "AUD_RS5_11");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.3f, 0);
    int row = -1, tries = 0, pools = 0, lists = 0;
    for (; tries < 300 && row < 0; ++tries) {
        ts_refresh_offers(&c);
        lists++;
        for (int i = 0; i < c.offer_count; ++i)
            if (c.offers[i].pool) row = i, pools++;
    }
    CHECK(row >= 0, "a shared ride is offered now and then (%d lists)", lists);
    if (row < 0) return;
    const TsOffer o = c.offers[row];
    CHECK(o.pool_passenger != o.passenger && o.pool_pickup != o.pickup && o.pool_dest != o.dest && o.pool_pickup != o.dest,
          "two people, four places");
    CHECK(o.ride_class == TS_CLASS_SAVER || o.ride_class == TS_CLASS_COMFORT, "shared: Saver or Comfort");
    CHECK(o.pay == uber_pay(o.trip_m, 80.0f, o.ptype, o.pay_factor) + uber_pay(o.pool_trip_m, 80.0f, o.pool_ptype, o.pay_factor), "shared: both fares at 80 %% of alone, added (%d)", o.pay);
    c.offers[0] = c.offers[row], row = 0; /* 0.13: the phone shows one request: this one */
    c.phone_open = 1, c.phone_tab = TS_TAB_RIDES, c.phone_sel = row;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(strstr(h.row_label[2 + 2 * row + 1], "Sammelfahrt · ") != NULL && strstr(h.footer, "steigt zu in") != NULL,
          "phone: %s / %s", h.row_label[2 + 2 * row + 1], h.footer);
    ts_accept_row(&c, row);
    c.next_wish_t = 1e9;
    drive_to(&c, ts_places[o.pickup].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 6.0f, 0);
    CHECK(c.stage == TS_UBER_TO_DEST && c.pool_stage == 0, "first in, to the second (%d %d)", c.stage, c.pool_stage);
    CHECK(ts_distance_xz(c.nav_target, ts_places[o.pool_pickup].pos) < 0.1f, "GPS: the second's pickup");
    ts_hud(&c, &h);
    CHECK(!strcmp(h.trip_label, "2. FAHRGAST ABHOLEN") && h.trip_pool, "ride card: %s", h.trip_label);
    /* driving past without stopping: nobody gets in */
    {
        TsCore k = c;
        TsCar keep = car;
        const float *pp = ts_places[o.pool_pickup].pos;
        place_car(pp);
        car.vel[2] = 15.0f;
        for (int i = 0; i < 60 * 3; ++i) frame(&k, i);
        CHECK(k.pool_stage == 0, "not stopped: the second isn't in");
        car = keep;
    }
    drive_to(&c, ts_places[o.pool_pickup].pos, 18.0f, 2.0f, 2.5f, 0);
    int toasts = 0, feeds = 0;
    for (int i = 0; i < 60 * 9; ++i) {
        frame(&c, i);
        events_print(&c, 0, &toasts, &feeds);
    }
    CHECK(c.pool_stage == 1 && toasts >= 1 && feeds >= 2, "the second in, hello and answer (%d, %d %d)", c.pool_stage, toasts, feeds);
    CHECK(ts_distance_xz(c.nav_target, ts_places[o.dest].pos) < 0.1f, "GPS: the first's destination");
    ts_hud(&c, &h);
    CHECK(strstr(h.trip_label, "STEIGT AUS") != NULL, "ride card: %s", h.trip_label);
    long long wallet = c.wallet;
    int jobs = c.jobs_done;
    drive_to(&c, ts_places[o.dest].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 4.0f, 0);
    CHECK(c.pool_stage == 2 && c.jobs_done == jobs + 1 && c.wallet > wallet, "the first got out and paid (%d)", c.pool_stage);
    CHECK(c.mode == TS_UBER && c.job.passenger == o.pool_passenger && c.job.dest == o.pool_dest, "the ride goes on with the second");
    CHECK(c.pool_paid == (int)(c.wallet - wallet), "what the first paid is kept (%d)", c.pool_paid);
    CHECK(ts_distance_xz(c.nav_target, ts_places[o.pool_dest].pos) < 0.1f, "GPS: the second's destination");
    c.next_wish_t = 1e9;
    drive_to(&c, ts_places[o.pool_dest].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 4.0f, 0);
    CHECK(c.mode == TS_IDLE && c.jobs_done == jobs + 2, "both done");
    CHECK(c.card.kind == TS_CARD_RIDE && strstr(c.card.name, " & ") && c.card.money == (int)(c.wallet - wallet), "the card: %s, %d (%lld)",
          c.card.name, c.card.money, c.wallet - wallet);
    CHECK(c.met[o.passenger] >= 1 && c.met[o.pool_passenger] >= 1, "both met");
    /* career extras off: no shared rides */
    c.extras_off = 1;
    int any = 0;
    for (int r = 0; r < 60; ++r) {
        ts_refresh_offers(&c);
        for (int i = 0; i < c.offer_count; ++i) any |= c.offers[i].pool;
    }
    CHECK(!any, "extras off: no shared rides");
}

/* 0.6: pictures of the cards, the classes, hotspots, tiers and a shared ride */
/* 0.7: the map. A small made-up picture: water round the edge, land, one road along the middle and one down */
static unsigned char *fake_map_bmp(size_t *size, int w, int h) {
    size_t stride = ((size_t)w + 3) & ~(size_t)3, off = 14 + 40 + 1024;
    *size = off + stride * (size_t)h;
    unsigned char *b = calloc(*size, 1);
    b[0] = 'B', b[1] = 'M';
    b[10] = (unsigned char)(off & 255), b[11] = (unsigned char)(off >> 8);
    b[14] = 40;
    b[18] = (unsigned char)(w & 255), b[19] = (unsigned char)(w >> 8);
    b[22] = (unsigned char)(h & 255), b[23] = (unsigned char)(h >> 8);
    b[26] = 1, b[28] = 8;
    /* palette: 0 land, 1 road (flag 1), 2 water (flag 2) */
    unsigned char *p = b + 54;
    p[0] = 0x3A, p[1] = 0x2A, p[2] = 0x1E;
    p[4] = 0x8C, p[5] = 0x76, p[6] = 0x60, p[7] = 1;
    p[8] = 0x22, p[9] = 0x14, p[10] = 0x0C, p[11] = 2;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int top = h - 1 - y; /* bottom-up rows */
            int v = 0;
            if (x < 4 || x >= w - 4 || top < 4 || top >= h - 4) v = 2;
            else if (top == h / 2 || top == h / 2 + 1 || x == w / 3 || x == w / 3 + 1) v = 1;
            b[off + stride * (size_t)y + (size_t)x] = (unsigned char)v;
        }
    return b;
}

static float world_x(int px, int w) { return TS_MAP_X0 + ((float)px + 0.5f) * (TS_MAP_X1 - TS_MAP_X0) / (float)w; }
static float world_z(int py, int h) { return TS_MAP_Z0 - ((float)py + 0.5f) * (TS_MAP_Z0 - TS_MAP_Z1) / (float)h; }

static int pins_of(const TsHud *h, int kind) {
    int n = 0;
    for (int i = 0; i < h->map_pin_count; ++i) n += h->map_pin[i].kind == kind;
    return n;
}

static void test_map(void) {
    /* the picture: bad data is refused, a good one is read */
    size_t size;
    unsigned char *bmp = fake_map_bmp(&size, 256, 128);
    CHECK(!ts_map_load_bmp(bmp, 40), "a cut file is refused");
    CHECK(!ts_map_load_bmp("hello world, no picture at all, really nothing here......", 56), "no BMP is refused");
    bmp[28] = 24;
    CHECK(!ts_map_load_bmp(bmp, size), "only 8-bit pictures");
    bmp[28] = 8;
    CHECK(ts_map_load_bmp(bmp, size) && ts_map_ok() && ts_map_width() == 256 && ts_map_height() == 128, "the picture is read");
    /* colours: the road's colour on the road, water outside the picture */
    uint32_t road = ts_map_color(world_x(128, 256), (world_z(64, 128) + world_z(65, 128)) * 0.5f, 1.0f);
    CHECK(road == 0x60768C, "the road's colour (%06X)", road);
    CHECK(ts_map_color(TS_MAP_X0 - 5000.0f, 0.0f, 1.0f) == 0x0C1422, "outside: the sea");
    uint32_t far = ts_map_color(world_x(128, 256), world_z(30, 128), 500.0f);
    CHECK(far != 0 && far != 0xFFFFFF, "zoomed out: a smaller level (%06X)", far);
    /* a way: from the left of the middle road to the bottom of the road down: along both roads */
    float a[2] = {world_x(20, 256), world_z(64, 128)}, b[2] = {world_x(85, 256), world_z(118, 128)}, way[2 * 200];
    int n = ts_map_route(a, b, way, 200);
    CHECK(n >= 3, "a way with a corner (%d points)", n);
    if (n >= 3) {
        CHECK(way[0] == a[0] && way[1] == a[1] && way[2 * (n - 1)] == b[0] && way[2 * (n - 1) + 1] == b[1], "it starts and ends at the points");
        /* the corner is where the roads cross */
        int corner = 0;
        for (int i = 1; i < n - 1; ++i)
            corner |= fabsf(way[2 * i] - world_x(85, 256)) < 300.0f && fabsf(way[2 * i + 1] - world_z(64, 128)) < 300.0f; /* cells of 173 m */
        CHECK(corner, "it turns where the roads cross");
        if (!corner) for (int i = 0; i < n; ++i) printf("  %.0f %.0f\n", (way[2 * i] - TS_MAP_X0) / (TS_MAP_X1 - TS_MAP_X0) * 256, (TS_MAP_Z0 - way[2 * i + 1]) / (TS_MAP_Z0 - TS_MAP_Z1) * 128);
    }
    float way2[2 * 200];
    CHECK(ts_map_route(a, b, way2, 200) == n && !memcmp(way, way2, sizeof(float) * 2 * (size_t)n), "the same way again (remembered)");
    CHECK(ts_map_route(a, b, way2, 2) == 2, "two points at most: start and end");
    free(bmp);

    /* the map tab */
    TsCore c;
    ts_init(&c, 77u, 1);
    ts_set_car(&c, "AUD_RS5_11");
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    phone_with_offers(&c);
    c.phone_tab = TS_TAB_MAP, c.phone_sel = 0;
    TsHud h;
    ts_hud(&c, &h);
    CHECK(h.map_on && h.map_keys == TS_MAPKEYS_TAB && h.tab == TS_TAB_MAP, "the map tab shows the map");
    CHECK(pins_of(&h, TS_PIN_CAR) == 1 && pins_of(&h, TS_PIN_TOWN) == 7 && pins_of(&h, TS_PIN_FESTIVAL) == 1, "you, 7 towns, the festival");
    CHECK(fabsf(h.map_x - ts_places[0].pos[0]) < 2.0f && h.map_w > 2000.0f && h.map_w < 5000.0f, "near: round the car");
    CHECK(!strcmp(h.tabs[TS_TAB_MAP], "Karte"), "the tab is called Karte");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    CHECK(c.map_zoom == 1 && h.map_w > 8000.0f && h.map_w < 10000.0f && c.mode == TS_IDLE, "Enter: the region");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    CHECK(c.map_zoom == 2 && h.map_w > 14000.0f, "Enter: the whole map");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    float ahx = h.map_x - c.car.pos[0], ahz = h.map_z - c.car.pos[2];
    CHECK(c.map_zoom == 3 && h.map_navi && h.map_w < 1000.0f, "Enter: navi, turned with the car");
    CHECK(fabsf(ahx * cosf(h.map_heading) - ahz * sinf(h.map_heading)) < 2.0f && ahx * sinf(h.map_heading) + ahz * cosf(h.map_heading) > 100.0f,
          "navi: the middle is ahead of the car (you low in the box)");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    CHECK(c.map_zoom == 0 && !h.map_navi, "Enter: near again");

    /* the preview of a ride */
    c.map_preview = 1;
    c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    ts_phone_accept(&c);
    CHECK(c.mode == TS_IDLE && c.map_view && c.phone_open, "Enter on a ride: the preview, no ride yet");
    ts_hud(&c, &h);
    const TsOffer *o = &c.offers[0];
    CHECK(h.map_on && h.map_keys == TS_MAPKEYS_PREVIEW && pins_of(&h, TS_PIN_PICKUP) == 1 && pins_of(&h, TS_PIN_DEST) == 1 + o->pool,
          "the preview: pickup and destination");
    int pick = -1, dest = -1, car_i = -1;
    for (int i = 0; i < h.map_pin_count; ++i) {
        if (h.map_pin[i].kind == TS_PIN_PICKUP) pick = i;
        if (h.map_pin[i].kind == TS_PIN_DEST && dest < 0) dest = i;
        if (h.map_pin[i].kind == TS_PIN_CAR) car_i = i;
    }
    CHECK(car_i >= 0 && pick > car_i && dest > pick, "the route: you, the pickup, the destination");
    if (pick >= 0 && dest >= 0) {
        CHECK(h.map_pin[pick].route == TS_ROUTE_MAIN && h.map_pin[dest].route == TS_ROUTE_LATER, "the way to the pickup first, then the ride");
        CHECK(!strcmp(h.map_pin[dest].label, ts_place_name(&c, o->dest)) && fabsf(h.map_pin[dest].x - ts_places[o->dest].pos[0]) < 0.01f,
              "the destination's pin is the place");
        /* the region holds all three */
        float px[3] = {h.map_pin[car_i].x, h.map_pin[pick].x, h.map_pin[dest].x}, pz[3] = {h.map_pin[car_i].z, h.map_pin[pick].z, h.map_pin[dest].z};
        int all = 1;
        for (int k = 0; k < 3; ++k) all &= fabsf(px[k] - h.map_x) <= h.map_w / 2 && fabsf(pz[k] - h.map_z) <= h.map_h / 2;
        CHECK(all, "the preview shows you, the pickup and the destination");
    }
    CHECK(h.row_selected >= 0 && strstr(h.row_label[h.row_selected], "annehmen") && h.row_count == 4, "the button: Fahrt annehmen (%d rows)", h.row_count);
    ts_phone_tab(&c, -1);
    ts_hud(&c, &h);
    CHECK(c.map_zoom == 1 && h.map_w > 14000.0f && c.phone_tab == TS_TAB_RIDES, "left: the whole map (the tab stays)");
    ts_phone_tab(&c, 1);
    CHECK(c.map_zoom == 0, "right: the route again");
    ts_phone_next(&c);
    ts_hud(&c, &h);
    CHECK(c.map_btn == 1 && !strcmp(h.row_label[h.row_selected], "Zurück"), "down: Zurück");
    ts_phone_accept(&c);
    CHECK(!c.map_view && c.mode == TS_IDLE && c.phone_open && c.phone_sel == 0, "Zurück: the request again, still chosen");
    ts_phone_accept(&c);
    ts_phone_back(&c);
    CHECK(!c.map_view && c.phone_open && c.mode == TS_IDLE, "Backspace closes the preview, not the phone");
    ts_phone_back(&c);
    CHECK(!c.phone_open, "Backspace again closes the phone");
    phone_with_offers(&c);
    c.phone_sel = 0;
    int want = c.offers[0].passenger;
    ts_phone_accept(&c);
    CHECK(c.map_view && c.map_btn == 0, "the preview again, on the button");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER && c.job.passenger == want && !c.map_view && !c.phone_open, "Fahrt annehmen takes the ride");

    /* the job's map */
    phone_with_offers(&c);
    ts_hud(&c, &h);
    CHECK(h.tab_count == 2 && !h.map_on && h.map_keys == TS_MAPKEYS_JOBPAGE, "during the job: two tabs, the job first");
    ts_phone_tab(&c, 1);
    ts_hud(&c, &h);
    CHECK(h.map_on && h.tab == 1 && h.map_keys == TS_MAPKEYS_JOB && pins_of(&h, TS_PIN_PICKUP) == 1, "right: the job's map with the pickup");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_UBER && !c.cancel_armed && c.map_zoom == 1, "Enter on the job's map zooms, doesn't cancel");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    CHECK(c.map_zoom == 2 && h.map_navi && pins_of(&h, TS_PIN_PICKUP) == 1, "Enter again: navi with the way");
    ts_phone_accept(&c);
    CHECK(c.map_zoom == 0, "and again: the route");
    ts_phone_back(&c);
    CHECK(c.phone_open && c.job_tab == 0, "Backspace: back to the job");
    ts_phone_accept(&c);
    CHECK(c.cancel_armed, "on the job tab Enter still cancels (asks first)");
    ts_cancel(&c);

    /* the preview of a bus line: all its stops, numbered in driving order */
    for (unsigned li = 0; li < ts_line_count; ++li) {
        c.phone_open = 0;
        phone_with_offers(&c);
        c.phone_tab = TS_TAB_BUS, c.phone_sel = (int)li;
        ts_phone_accept(&c);
        ts_hud(&c, &h);
        const TsLine *l = &ts_lines[li];
        int ring = l->stops[0] == l->stops[l->stop_count - 1];
        int stops = 0, max_num = 0, in_view = 1, order = 1, last = 0;
        int rev = bus_dir(&c, (int)li), nst = (int)l->stop_count;
        for (int i = 0; i < h.map_pin_count; ++i) {
            const TsMapPin *p = &h.map_pin[i];
            if (p->kind != TS_PIN_STOP && p->kind != TS_PIN_STOP_NEXT) continue;
            if (p->num < 0) continue;
            stops++;
            if (p->num > max_num) max_num = p->num;
            order &= p->num == last + 1;
            last = p->num;
            const float *at = ts_places[l->stops[rev ? nst - 1 - (p->num - 1) : p->num - 1]].pos;
            order &= fabsf(p->x - at[0]) < 0.01f && fabsf(p->z - at[2]) < 0.01f;
            in_view &= fabsf(p->x - h.map_x) <= h.map_w / 2 && fabsf(p->z - h.map_z) <= h.map_h / 2;
        }
        CHECK(c.map_view && stops == (int)l->stop_count - ring && order, "line %u: %d stops in order (of %u)", li + 1, stops, l->stop_count);
        CHECK(in_view, "line %u: all stops in view", li + 1);
        CHECK(h.map_pin_count <= TS_MAP_PINS && strstr(h.row_label[h.row_selected], "Linie starten"), "line %u: the button", li + 1);
        ts_phone_back(&c);
    }
    /* without the preview setting Enter takes the line at once (as before) */
    c.map_preview = 0;
    c.phone_tab = TS_TAB_BUS, c.phone_sel = 0;
    ts_set_car(&c, "PG_BUS_14");
    ts_phone_accept(&c);
    CHECK(c.mode == TS_BUS, "no preview: Enter starts the line");
    phone_with_offers(&c);
    ts_phone_tab(&c, 1);
    ts_hud(&c, &h);
    CHECK(h.map_on && pins_of(&h, TS_PIN_STOP_NEXT) == 1, "the bus job's map: the next stop marked");

    /* drawing it: every device, with and without the picture, nothing outside the screen breaks */
    for (int with = 0; with < 2; ++with) {
        if (with) {
            bmp = fake_map_bmp(&size, 256, 128);
            ts_map_load_bmp(bmp, size);
            free(bmp);
        } else ts_map_free();
        for (int dev = 0; dev < TS_DEVICES; ++dev) {
            int w, hh;
            ts_device_size(dev, 1.0f, &w, &hh);
            uint32_t *px = calloc((size_t)w * hh, 4);
            TsPhoneCanvas cv = {px, w, hh, NULL, NULL, NULL};
            ts_device_draw(&h, dev, TS_THEME_NIGHT, 1.0f, "12:00", &cv);
            ts_device_finish(dev, TS_THEME_NIGHT, 1.0f, &cv);
            int painted = 0;
            for (int i = 0; i < w * hh; ++i) painted += px[i] != 0;
            CHECK(painted > w * hh / 3, "device %d draws (map %d)", dev, with);
            free(px);
        }
    }
    ts_map_free();
}

/* 0.9: the map from the game's files: unpacking, BC7, the Xbox One's tile order, the roads */
static unsigned char *fake_nav(size_t *size, const float (*node)[2], int nodes, const int (*road)[3], int roads, const int *list, int list_n) {
    /* road: type, first list entry, count */
    size_t n = 0x88 + (size_t)nodes * 40 + (size_t)roads * 24 + (size_t)list_n * 8;
    unsigned char *b = calloc(1, n);
    memcpy(b, "WVAN", 4);
    uint32_t hdr[3] = {(uint32_t)nodes, (uint32_t)roads, (uint32_t)list_n};
    memcpy(b + 0x58, hdr, 12);
    for (int i = 0; i < nodes; ++i) {
        float p[3] = {node[i][0], 50.0f, node[i][1]};
        memcpy(b + 0x88 + (size_t)i * 40 + 12, p, 12);
    }
    unsigned char *r = b + 0x88 + (size_t)nodes * 40;
    for (int i = 0; i < roads; ++i) {
        uint32_t rec[6] = {0, 0, (uint32_t)road[i][2], (uint32_t)road[i][0], (uint32_t)(road[i][1] + road[i][2]), 0};
        memcpy(r + (size_t)i * 24, rec, 24);
    }
    unsigned char *l = r + (size_t)roads * 24;
    for (int i = 0; i < list_n; ++i) {
        uint32_t e[2] = {(uint32_t)list[i], 0};
        memcpy(l + (size_t)i * 8, e, 8);
    }
    *size = n;
    return b;
}

static void test_gamemap(void) {
    /* deflate: dynamic and fixed Huffman codes, stored blocks; a cut or wrong-sized stream is refused */
    unsigned char out[sizeof(tv_text) + 16];
    CHECK(ts_inflate(tv_dyn, sizeof(tv_dyn), out, sizeof(tv_text)) && !memcmp(out, tv_text, sizeof(tv_text)), "inflate: dynamic codes");
    CHECK(ts_inflate(tv_fix, sizeof(tv_fix), out, sizeof(tv_text)) && !memcmp(out, tv_text, sizeof(tv_text)), "inflate: fixed codes");
    CHECK(ts_inflate(tv_sto, sizeof(tv_sto), out, 300) && !memcmp(out, tv_text, 300), "inflate: stored");
    CHECK(!ts_inflate(tv_dyn, sizeof(tv_dyn) / 2, out, sizeof(tv_text)), "inflate: a cut stream is refused");
    CHECK(!ts_inflate(tv_dyn, sizeof(tv_dyn), out, sizeof(tv_text) - 5), "inflate: too little room is refused");
    /* a zip: by name, stored or deflated */
    size_t n = 0;
    uint8_t *f = ts_unzip(tv_zip, sizeof(tv_zip), "dir/b.bin", &n);
    CHECK(f && n == sizeof(tv_text) && !memcmp(f, tv_text, n), "unzip: a deflated file");
    free(f);
    f = ts_unzip(tv_zip, sizeof(tv_zip), "a.txt", &n);
    CHECK(f && n == 16 && !memcmp(f, "stored text here", 16), "unzip: a stored file");
    free(f);
    CHECK(!ts_unzip(tv_zip, sizeof(tv_zip), "b.bin", &n), "unzip: only the exact name");
    CHECK(!ts_unzip(tv_zip, 100, "a.txt", &n), "unzip: a cut zip");
    /* BC7: every mode as Pillow decodes it */
    int bad = 0;
    for (unsigned k = 0; k < sizeof(tv_bc7_in) / 16; ++k) {
        uint8_t px[64];
        ts_bc7_block(tv_bc7_in + 16 * k, px);
        bad += memcmp(px, tv_bc7_out + 64 * k, 64) != 0;
    }
    CHECK(!bad, "BC7: %d of %u blocks differ", bad, (unsigned)(sizeof(tv_bc7_in) / 16));
    /* the tile order: each block of the 896 x 512 picture has its own place */
    uint8_t *seen = calloc(896 * 512, 1);
    int dup = 0, past = 0;
    for (uint32_t y = 0; y < 512; ++y)
        for (uint32_t x = 0; x < 896; ++x) {
            uint32_t i = ts_gamemap_tile_index(x, y, 896);
            if (i >= 896 * 512) past++;
            else dup += seen[i]++ != 0;
        }
    CHECK(!dup && !past, "the tile order is a shuffle of the blocks (%d twice, %d past the end)", dup, past);
    CHECK(ts_gamemap_tile_index(1, 0, 896) == 1 && ts_gamemap_tile_index(0, 1, 896) == 2 && ts_gamemap_tile_index(4, 0, 896) == 512,
          "the tile order's first bits");
    free(seen);
    /* the roads (away from the towns): a street with a twin beside it (a motorway), a lone street, a dirt road, a trail */
    float node[40][2];
    int list[40], ln = 0;
    for (int i = 0; i < 10; ++i) node[i][0] = 3000.0f + 100.0f * (float)i, node[i][1] = -4500.0f;      /* 0-9 */
    for (int i = 0; i < 10; ++i) node[10 + i][0] = 3000.0f + 100.0f * (float)i, node[10 + i][1] = -4512.0f; /* the twin */
    for (int i = 0; i < 10; ++i) node[20 + i][0] = 3000.0f + 100.0f * (float)i, node[20 + i][1] = -4900.0f; /* alone */
    for (int i = 0; i < 5; ++i) node[30 + i][0] = 3900.0f, node[30 + i][1] = -4500.0f - 100.0f * (float)i;  /* dirt */
    node[35][0] = 4500.0f, node[35][1] = -5000.0f, node[36][0] = 4550.0f, node[36][1] = -5050.0f;          /* trail */
    for (int i = 0; i < 10; ++i) list[ln++] = i;
    for (int i = 0; i < 10; ++i) list[ln++] = 19 - i; /* the other way */
    for (int i = 0; i < 10; ++i) list[ln++] = 20 + i;
    for (int i = 0; i < 5; ++i) list[ln++] = 30 + i;
    list[ln++] = 35, list[ln++] = 36;
    int road[5][3] = {{4, 0, 10}, {4, 10, 10}, {4, 20, 10}, {5, 30, 5}, {6, 35, 2}};
    size_t nn;
    unsigned char *nav = fake_nav(&nn, (const float (*)[2])node, 37, (const int (*)[3])road, 5, list, ln);
    float seg[4 * 64];
    uint8_t kind[64];
    int segs = ts_gamemap_roads(nav, nn, seg, kind, 64);
    int count[4] = {0, 0, 0, 0};
    for (int i = 0; i < segs; ++i) count[kind[i]]++;
    CHECK(segs == 9 + 9 + 9 + 4 + 1, "the roads' lines (%d)", segs);
    /* 0.13: the nav's types 5 and 6 are streets too (the festival's entrance, Castelletto's coast road) */
    CHECK(count[TS_ROAD_MOTORWAY] == 18 && count[TS_ROAD_STREET] == 9 + 4 + 1 && count[TS_ROAD_DIRT] == 0 && count[TS_ROAD_TRAIL] == 0,
          "motorway %d, street %d, dirt %d, trail %d", count[3], count[2], count[1], count[0]);
    {   /* ... but two of them side by side are no motorway */
        int road2[2][3] = {{5, 0, 10}, {6, 10, 10}};
        size_t n2;
        unsigned char *nav2 = fake_nav(&n2, (const float (*)[2])node, 37, (const int (*)[3])road2, 2, list, 20);
        int s2 = ts_gamemap_roads(nav2, n2, seg, kind, 64), mw = 0;
        for (int i = 0; i < s2; ++i) mw += kind[i] == TS_ROAD_MOTORWAY;
        CHECK(s2 == 18 && !mw, "types 5/6 twins: %d lines, %d motorway", s2, mw);
        free(nav2);
    }
    CHECK(!ts_gamemap_roads(nav, 0x90, seg, kind, 64), "a cut nav file is refused");
    nav[0] = 'X';
    CHECK(!ts_gamemap_roads(nav, nn, seg, kind, 64), "no nav file");
    nav[0] = 'W';
    /* the map from the roads alone (no picture): plain land, the roads drawn sharp when near, the ways on them */
    int got = ts_gamemap_load(NULL, 0, nav, nn);
    CHECK(got == TS_GAMEMAP_ROADS && ts_map_ok() && ts_map_road_count() == segs, "a map from the roads alone (%d)", got);
    uint32_t on = ts_map_color(3450.0f, -4900.0f, 1.0f), off = ts_map_color(3450.0f, -4800.0f, 1.0f);
    CHECK(on != off, "near: the street is drawn (%06X on %06X)", on, off);
    uint32_t mw = ts_map_color(3450.0f, -4500.0f, 1.0f);
    CHECK(mw != on && mw != off, "the motorway has its own colour (%06X)", mw);
    float a[2] = {3010.0f, -4880.0f}, b[2] = {3880.0f, -4920.0f}, way[2 * 200];
    int wn = ts_map_route(a, b, way, 200);
    float far = 0.0f;
    for (int i = 1; i + 1 < wn; ++i) far = fmaxf(far, fabsf(way[2 * i + 1] + 4900.0f));
    CHECK(wn >= 2 && far < 25.0f, "the way keeps to the street (%d points, %.0f m off)", wn, far);
    CHECK(!ts_gamemap_load(NULL, 0, NULL, 0) && ts_map_ok(), "nothing to load: the map stays");
    CHECK(ts_gamemap_load(tv_zip, sizeof(tv_zip), nav, nn) == TS_GAMEMAP_ROADS, "a zip without the picture: roads only");
    free(nav);
    ts_map_free();
}

/* 0.7: pictures of the map (SCREENS=1 MAP_BMP=<the map> PHONE_PREVIEW=<dir>); 0.9: MAP_ZIP=<Anthem_Map.zip> MAP_NAV=<anthem_00.nav> */
static unsigned char *slurp(const char *file, size_t *size) {
    FILE *f = file ? fopen(file, "rb") : NULL;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) free(b), b = NULL;
    fclose(f);
    *size = (size_t)n;
    return b;
}


/* 0.9.1: frames of the phone's animations for GIFs (GIFS=1 with PHONE_PREVIEW; tools/anim_gifs.py makes the GIFs:
   it does the crossfades as the window does, from the keys written here) */
#define GIF_FPS 25.0
static TsMapAnim g_an;
static double g_t;
static int g_n;
static FILE *g_meta;
static void gif_frame(const char *seq, const TsHud *h0) {
    TsHud h = *h0;
    ts_map_anim(&g_an, &h, g_t);
    ts_tab_anim(&g_an, &h, g_t);
    char nm[64];
    snprintf(nm, sizeof(nm), "g_%s_%03d", seq, g_n++);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, nm);
    if (g_meta)
        fprintf(g_meta, "%s %s_0 %u %u %d %d %d %d %d %d %d %d %d %d\n", seq, nm, ts_screen_key(&h), h.map_on ? ts_pins_key(&h) : 0u,
                h.map_on, ts_map_rect_px[0], ts_map_rect_px[1], ts_map_rect_px[2], ts_map_rect_px[3], h.tab_count, ts_tab_rect_px[0],
                ts_tab_rect_px[1], ts_tab_rect_px[2], ts_tab_rect_px[3]);
    g_t += 1.0 / GIF_FPS;
}
static void gif_hold(const char *seq, TsCore *c, float secs) {
    TsHud h;
    for (int i = 0; i < (int)(secs * GIF_FPS + 0.5f); ++i) {
        ts_hud(c, &h);
        gif_frame(seq, &h);
    }
}
static void gif_begin(const char *seq) {
    memset(&g_an, 0, sizeof(g_an));
    g_n = 0, g_t = 100.0;
    if (!g_meta) {
        char path[512];
        snprintf(path, sizeof(path), "%s/gifs.txt", getenv("PHONE_PREVIEW"));
        g_meta = fopen(path, "w");
    }
    (void)seq;
}
/* the car drives along the job's way (navi): the blue line ahead, the map turning */
static void gif_drive(const char *seq, TsCore *c, float secs, float speed) {
    TsHud h;
    ts_hud(c, &h);
    int car = -1, tgt = -1;
    for (int i = 0; i < h.map_pin_count; ++i) {
        if (h.map_pin[i].kind == TS_PIN_CAR) car = i;
        if (h.map_pin[i].route == TS_ROUTE_MAIN && tgt < 0) tgt = i;
    }
    if (car < 0 || tgt < 0) return;
    static float w[2 * 400];
    float a[2] = {h.map_pin[car].x, h.map_pin[car].z}, b[2] = {h.map_pin[tgt].x, h.map_pin[tgt].z};
    int n = ts_map_route(a, b, w, 400);
    float along = 0.0f;
    int k = 0;
    for (int f = 0; f < (int)(secs * GIF_FPS); ++f) {
        float step = speed / (float)GIF_FPS;
        along += step;
        while (k + 1 < n - 1 && along > hypotf(w[2 * k + 2] - w[2 * k], w[2 * k + 3] - w[2 * k + 1])) {
            along -= hypotf(w[2 * k + 2] - w[2 * k], w[2 * k + 3] - w[2 * k + 1]);
            ++k;
        }
        float sx = w[2 * k + 2] - w[2 * k], sz = w[2 * k + 3] - w[2 * k + 1], L = hypotf(sx, sz);
        float t = L > 0.01f ? along / L : 0.0f;
        if (t > 1.0f) t = 1.0f;
        TsHud fr = h;
        fr.map_pin[car].x = w[2 * k] + sx * t, fr.map_pin[car].z = w[2 * k + 1] + sz * t;
        float hd = roundf(atan2f(sx, sz) * 90.0f) / 90.0f;
        fr.map_heading = fr.map_rot = hd;
        fr.map_x = fr.map_pin[car].x + 150.0f * sinf(hd), fr.map_z = fr.map_pin[car].z + 150.0f * cosf(hd);
        gif_frame(seq, &fr);
    }
}

static void screens07(void) {
    size_t zn = 0, nn = 0;
    unsigned char *zip = slurp(getenv("MAP_ZIP"), &zn), *nav = slurp(getenv("MAP_NAV"), &nn);
    if (zip || nav) printf("map from the game's files: %d\n", ts_gamemap_load(zip, zn, nav, nn));
    free(zip), free(nav);
    const char *file = ts_map_ok() ? NULL : getenv("MAP_BMP");
    if (file) {
        FILE *f = fopen(file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char *b = malloc((size_t)n);
            if (b && fread(b, 1, (size_t)n, f) == (size_t)n) ts_map_load_bmp(b, (size_t)n);
            free(b);
            fclose(f);
        }
    }
    if (ts_map_ok()) { /* the real map: a way along the roads between every two towns, not much longer than the road factor says */
        int bad = 0, pairs = 0;
        for (unsigned i = 0; i < 8; ++i)
            for (unsigned j = 0; j < 8; ++j) {
                if (i == j) continue;
                float a[2] = {ts_places[i].pos[0], ts_places[i].pos[2]}, b[2] = {ts_places[j].pos[0], ts_places[j].pos[2]}, w[2 * 400];
                int n = ts_map_route(a, b, w, 400);
                float len = 0.0f;
                for (int k = 0; k + 1 < n; ++k) len += hypotf(w[2 * k + 2] - w[2 * k], w[2 * k + 3] - w[2 * k + 1]);
                float straight = hypotf(b[0] - a[0], b[1] - a[1]);
                pairs++;
                if (n < 3 || len < straight || len > straight * 2.6f) {
                    bad++;
                    printf("  way %s -> %s: %d points, %.0f m for %.0f m\n", ts_places[i].name, ts_places[j].name, n, len, straight);
                }
            }
        /* Alpes de Haute's zone lies out on the lake: its way starts at the nearest shore road (both directions) */
        CHECK(bad <= 2, "ways along the roads between the towns (%d of %d odd)", bad, pairs);
    }
    TsCore c;
    ts_init(&c, 4711u, 1);
    ts_set_car(&c, "AUD_RS5_11");
    c.map_preview = 1;
    c.wallet = 18450, c.reputation = 72.0f;
    place_car(ts_places[4].pos); /* Saint Martin */
    car.fwd[0] = 0.6f, car.fwd[2] = 0.8f;
    wait_s(&c, 0.5f, 0);
    c.t = 61.0;
    ts_step(&c);
    wait_s(&c, 0.5f, 0);
    TsHud h;
    phone_with_offers(&c);
    c.phone_tab = TS_TAB_MAP;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_map_near");
    c.map_zoom = 1;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_map_region");
    c.map_zoom = 2;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_map_whole");
    c.map_zoom = 3;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_map_navi");
    { /* 0.9.1: the glide from near to navi and back, as frames (tools/phone_preview.py; a GIF from them) */
        TsMapAnim an;
        memset(&an, 0, sizeof(an));
        TsHud near_h, navi_h = h;
        c.map_zoom = 0;
        ts_hud(&c, &near_h);
        TsHud f0 = near_h;
        ts_map_anim(&an, &f0, 0.0);
        ts_tab_anim(&an, &f0, 0.0);
        for (int f = 1; f <= 72; ++f) {
            TsHud fr = f <= 36 ? navi_h : near_h;
            ts_map_anim(&an, &fr, f / 30.0);
            if (f % 2 == 0 && (f % 36 <= 24 && f % 36 != 0)) {
                char nm[40];
                snprintf(nm, sizeof(nm), "anim_glide_%02d", f);
                draw_dev(&fr, TS_DEVICE_PHONE, TS_THEME_NIGHT, nm);
            }
        }
    }
    c.map_zoom = 0;
    if (getenv("GIFS")) { /* the map tab's zooms: near, region, whole map, navi, near */
        gif_begin("zoom");
        for (int z = 0; z <= 4; ++z) {
            c.map_zoom = z % 4;
            gif_hold("zoom", &c, z == 0 ? 0.6f : 1.3f);
        }
        c.map_zoom = 0;
    }
    const char *at = getenv("SHOT_AT"); /* "x,z": the near map at that spot (to hold against a picture from the game) */
    float ax, az;
    if (at && sscanf(at, "%f,%f", &ax, &az) == 2) {
        float here[3] = {ax, 100.0f, az}, keep[3] = {ts_places[4].pos[0], ts_places[4].pos[1], ts_places[4].pos[2]};
        place_car(here);
        wait_s(&c, 0.5f, 0);
        ts_hud(&c, &h);
        draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_here");
        place_car(keep);
        car.fwd[0] = 0.6f, car.fwd[2] = 0.8f, car.fwd[1] = 0.0f;
        wait_s(&c, 0.5f, 0);
    }
    /* a ride's preview: one that goes some way */
    c.phone_tab = TS_TAB_RIDES;
    for (int i = 0; i < 60; ++i) {
        ts_refresh_offers(&c);
        if (c.offer_count == 3 && c.offers[0].trip_m > 3500.0f && c.offers[0].pickup_m > 500.0f && !c.offers[0].pool) break;
    }
    c.phone_sel = 0;
    if (getenv("GIFS")) {
        gif_begin("tabs"); /* the tabs: the pill slides, the page fades */
        for (int k = 0; k < 6; ++k) {
            c.phone_tab = k == 4 ? TS_TAB_MAP : k == 5 ? TS_TAB_RIDES : k;
            gif_hold("tabs", &c, k == 0 ? 0.5f : 0.8f);
        }
        gif_begin("auswahl"); /* down the list and back */
        c.phone_tab = TS_TAB_RIDES;
        for (int k = 0; k < 5; ++k) {
            c.phone_sel = k < 3 ? k : 4 - k;
            gif_hold("auswahl", &c, k == 0 ? 0.5f : 0.6f);
        }
        gif_begin("vorschau"); /* Enter on a ride: its map; Left/Right; back */
        c.phone_sel = 0;
        gif_hold("vorschau", &c, 0.6f);
        ts_phone_accept(&c);
        gif_hold("vorschau", &c, 1.4f);
        c.map_zoom = 1;
        gif_hold("vorschau", &c, 1.3f);
        c.map_zoom = 0;
        gif_hold("vorschau", &c, 1.3f);
        ts_phone_back(&c);
        gif_hold("vorschau", &c, 0.8f);
        c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    }
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_rides");
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_preview_ride");
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_PURE, "shot07_preview_ride_pure");
    draw_dev(&h, TS_DEVICE_TABLET, TS_THEME_NIGHT, "shot07_tablet_preview");
    ts_phone_back(&c);
    /* a shared ride */
    for (int i = 0; i < 300; ++i) {
        ts_refresh_offers(&c);
        int k = -1;
        for (int j = 0; j < c.offer_count; ++j) if (c.offers[j].pool) k = j;
        if (k >= 0) { c.phone_sel = k; break; }
    }
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_preview_pool");
    ts_phone_back(&c);
    /* bus lines */
    c.phone_tab = TS_TAB_BUS, c.phone_sel = 0;
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_preview_line1");
    ts_phone_back(&c);
    c.phone_sel = 3;
    ts_phone_accept(&c);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_EDGE, TS_THEME_NIGHT, "shot07_preview_line4");
    ts_phone_back(&c);
    /* during a ride: the job's map */
    c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    ts_phone_accept(&c);
    ts_phone_accept(&c);
    c.next_wish_t = 1e9;
    phone_with_offers(&c);
    ts_phone_tab(&c, 1);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_job_map");
    c.map_zoom = 2;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_job_navi");
    if (getenv("GIFS")) {
        gif_begin("auftrag"); /* the job's map: route, whole map, navi, route */
        for (int z = 0; z <= 3; ++z) {
            c.map_zoom = z % 3;
            gif_hold("auftrag", &c, z == 0 ? 0.6f : 1.3f);
        }
        gif_begin("navi"); /* driving in navi */
        c.map_zoom = 2;
        gif_drive("navi", &c, 9.0f, 26.0f);
    }
    c.map_zoom = 0;
    ts_phone_tab(&c, 1);
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot07_job_page");
    ts_map_free();
}

static void screens06(void) {
    TsCore c;
    ts_init(&c, 4711u, 1);
    c.cards_on = 1;
    ts_set_car(&c, "AUD_RS5_11");
    c.wallet = 18450, c.reputation = 72.0f, c.tier = TS_TIER_GOLD, c.jobs_done = 34, c.rating_sum = 150, c.rating_count = 34;
    place_car(ts_places[0].pos);
    wait_s(&c, 0.5f, 0);
    c.t = 61.0;
    ts_step(&c);
    if (c.hot[0].place >= 0) place_car(ts_places[c.hot[0].place].pos);
    wait_s(&c, 0.5f, 0);
    TsHud h;
    /* the rides tab: Black, a surcharge, a shared ride */
    int found = 0;
    for (int i = 0; i < 200 && !found; ++i) {
        c.phone_open = 0;
        phone_with_offers(&c);
        int black = 0, pool = 0, surge = 0;
        for (int k = 0; k < c.offer_count; ++k)
            black |= c.offers[k].ride_class == TS_CLASS_BLACK, pool |= c.offers[k].pool, surge |= c.offers[k].surge > 1.01f;
        found = black && pool && surge && c.offer_count == 3;
    }
    c.phone_sel = 0;
    for (int k = 0; k < c.offer_count; ++k) if (c.offers[k].pool) c.phone_sel = k;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot06_phone_rides");
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_PURE, "shot06_pure_rides");
    c.phone_tab = TS_TAB_ACCOUNT, c.phone_sel = 0;
    ts_hud(&c, &h);
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_PURE, "shot06_pure_account");
    draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot06_phone_account");
    /* the ride card with a Black ride and a surcharge */
    c.phone_tab = TS_TAB_RIDES, c.phone_sel = 0;
    int pick = 0;
    for (int k = 0; k < c.offer_count; ++k) if (c.offers[k].ride_class == TS_CLASS_BLACK) pick = k;
    ts_accept_row(&c, pick);
    c.job.surge = 1.6f;
    c.next_wish_t = 1e9;
    drive_to(&c, ts_places[c.job.pickup].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 6.0f, 0);
    ts_hud(&c, &h);
    h.trip_progress = 0.35f, h.g_on = 1, h.g_lat = 0.15f, h.g_long = -0.1f;
    snprintf(h.corner, sizeof(h.corner), "%s", "6:40 · 91%");
    dash_style = TS_THEME_NIGHT;
    dash_one(&h, "shot06_dash_black");
    dash_style = TS_THEME_PURE;
    dash_one(&h, "shot06_dash_black_pure");
    /* the card after the ride */
    drive_to(&c, ts_places[c.job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
    wait_s(&c, 4.0f, 0);
    ts_hud(&c, &h);
    const int themes[3] = {TS_THEME_NIGHT, TS_THEME_PURE, TS_THEME_FESTIVAL};
    char name[64];
    for (int t = 0; t < 3; ++t) {
        dash_style = themes[t];
        snprintf(name, sizeof(name), "shot06_card_ride_%d", themes[t]);
        dash_one(&h, name);
    }
    dash_style = TS_THEME_NIGHT;
    h.card_anim = 0.85f; /* in the middle of coming in: three stars lit, the money counting */
    dash_one(&h, "shot06_card_ride_anim");
    /* a shift's review */
    TsCard k;
    memset(&k, 0, sizeof(k));
    c.shift.on = 1, c.shift.start_t = c.t - 8040, c.shift.jobs = 7, c.shift.lines = 1, c.shift.rated = 8, c.shift.stars_sum = 37;
    c.shift.earned = 6840, c.shift.tips = 610, c.shift.best_pay = 1480, c.shift.goal_done = 1, c.shift.rep_start = c.reputation - 6.0f;
    c.shift.km_start = c.km_total - 61.4;
    memset(&c.card, 0, sizeof(c.card));
    c.card_queued = 0;
    ts_shift_toggle(&c);
    ts_hud(&c, &h);
    for (int t = 0; t < 3; ++t) {
        dash_style = themes[t];
        snprintf(name, sizeof(name), "shot06_card_shift_%d", themes[t]);
        dash_one(&h, name);
    }
    /* a new tier */
    memset(&c.card, 0, sizeof(c.card));
    card_tier(&c, TS_TIER_GOLD);
    ts_hud(&c, &h);
    for (int t = 0; t < 3; ++t) {
        dash_style = themes[t];
        snprintf(name, sizeof(name), "shot06_card_tier_%d", themes[t]);
        dash_one(&h, name);
    }
    memset(&c.card, 0, sizeof(c.card));
    card_tier(&c, TS_TIER_PLATINUM);
    ts_hud(&c, &h);
    dash_style = TS_THEME_NIGHT;
    dash_one(&h, "shot06_card_tier_platinum");
    /* a shared ride: the ride card on the way to the second, the phone, the card at the end */
    memset(&c.card, 0, sizeof(c.card));
    int row = -1;
    for (int i = 0; i < 300 && row < 0; ++i) {
        ts_refresh_offers(&c);
        for (int j = 0; j < c.offer_count; ++j) if (c.offers[j].pool) row = j;
    }
    if (row >= 0) {
        ts_accept_row(&c, row);
        c.next_wish_t = 1e9;
        drive_to(&c, ts_places[c.job.pickup].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(&c, 6.0f, 0);
        ts_hud(&c, &h);
        h.trip_progress = 0.55f;
        dash_style = TS_THEME_NIGHT;
        dash_one(&h, "shot06_dash_pool");
        c.phone_open = 1;
        ts_hud(&c, &h);
        draw_dev(&h, TS_DEVICE_PHONE, TS_THEME_NIGHT, "shot06_phone_pool");
        c.phone_open = 0;
        drive_to(&c, ts_places[c.job.pool_pickup].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(&c, 9.0f, 0);
        drive_to(&c, ts_places[c.job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(&c, 4.0f, 0);
        c.next_wish_t = 1e9;
        drive_to(&c, ts_places[c.job.dest].pos, 18.0f, 2.0f, 2.5f, 0);
        wait_s(&c, 4.0f, 0);
        ts_hud(&c, &h);
        dash_one(&h, "shot06_card_pool");
    }
    dash_style = 0;
}

/* 0.9.1: the phone's animations: the map glides between zooms and turns into navi, the screen fades, the tab pill slides */
static void test_phone_anim(void) {
    TsMapAnim a;
    memset(&a, 0, sizeof(a));
    TsHud h;
    memset(&h, 0, sizeof(h));
    h.map_on = 1, h.map_keys = TS_MAPKEYS_TAB;
    h.map_pin_count = 1, h.map_pin[0].kind = TS_PIN_CAR, h.map_pin[0].x = 1000, h.map_pin[0].z = -2000;
    h.map_x = 1000, h.map_z = -2000, h.map_w = h.map_h = 3200, h.map_heading = 1.0f;
    TsHud t = h;
    CHECK(ts_map_anim(&a, &t, 10.0) == 0 && t.map_w == 3200 && t.map_rot == 0.0f, "anim: a new map comes as it is");
    /* Enter: navi (600 m, turned to the heading, ahead of the car) */
    TsHud navi = h;
    navi.map_navi = 1, navi.map_rot = 1.0f, navi.map_navi_t = 1.0f, navi.map_w = navi.map_h = 600;
    navi.map_x = 1000 + 150 * sinf(1.0f), navi.map_z = -2000 + 150 * cosf(1.0f);
    double now = 10.0;
    float w_mid = 0, rot_mid = 0, nv_mid = 0, last_w = 3200;
    int shrinking = 1;
    for (int f = 1; f <= 60; ++f) {
        now += 1.0 / 60.0;
        t = navi;
        ts_map_anim(&a, &t, now);
        if (t.map_w > last_w + 0.01f) shrinking = 0;
        last_w = t.map_w;
        if (f == 18) w_mid = t.map_w, rot_mid = t.map_rot, nv_mid = t.map_navi_t;
    }
    CHECK(w_mid > 650 && w_mid < 3100 && shrinking, "anim: the zoom glides (%.0f m after 0.3 s), never back", w_mid);
    CHECK(rot_mid > 0.2f && rot_mid < 0.99f && nv_mid > 0.2f && nv_mid < 0.99f, "anim: the map turns into navi (%.2f, %.2f)", rot_mid, nv_mid);
    CHECK(fabsf(t.map_w - 600) < 0.5f && fabsf(t.map_rot - 1.0f) < 0.01f && t.map_navi_t > 0.99f, "anim: after 1 s it is navi (%.0f m, %.2f)", t.map_w, t.map_rot);
    CHECK(fabsf(t.map_x - navi.map_x) < 1.0f && fabsf(t.map_z - navi.map_z) < 1.0f, "anim: navi looks ahead of the car");
    /* the car drives on: the view follows at once, no glide */
    for (int f = 0; f < 5; ++f) {
        now += 1.0 / 60.0;
        t = navi;
        t.map_pin[0].x += 1.4f * (f + 1), t.map_x += 1.4f * (f + 1);
        ts_map_anim(&a, &t, now);
    }
    CHECK(!a.gliding && fabsf(t.map_x - (navi.map_x + 7.0f)) < 1.0f, "anim: driving needs no glide");
    /* the rounded heading jumps a bit: the map turns smoothly */
    now += 1.0 / 60.0;
    t = navi, t.map_heading = 1.2f, t.map_rot = 1.2f;
    ts_map_anim(&a, &t, now);
    CHECK(t.map_rot > 1.0f && t.map_rot < 1.12f, "anim: a turn is eased (%.3f)", t.map_rot);
    /* back to the near view: the zoom keeps one spot still (the car stays in the box) */
    for (int f = 1; f <= 60; ++f) {
        now += 1.0 / 60.0;
        t = h;
        ts_map_anim(&a, &t, now);
        float dx = h.map_pin[0].x - t.map_x, dz = h.map_pin[0].z - t.map_z;
        if (f == 1 || f == 30) CHECK(fabsf(dx) < t.map_w * 0.5f && fabsf(dz) < t.map_h * 0.5f, "anim: the car stays in view (frame %d)", f);
    }
    CHECK(fabsf(t.map_w - 3200) < 1 && fabsf(t.map_rot) < 0.01f && t.map_navi_t < 0.01f, "anim: back north up, near");
    t.map_on = 0;
    ts_map_anim(&a, &t, now);
    CHECK(!a.on, "anim: no map, nothing kept");

    /* the tab pill */
    TsHud th;
    memset(&th, 0, sizeof(th));
    th.tab_count = 6, th.tab = 0;
    ts_tab_anim(&a, &th, 20.0);
    th.tab = 2;
    float mid = 0;
    for (int f = 1; f <= 40; ++f) {
        th.tab = 2;
        ts_tab_anim(&a, &th, 20.0 + f / 60.0);
        if (f == 4) mid = th.tab_slide;
    }
    CHECK(mid > 1.2f && mid < 2.95f && th.tab_slide == 0.0f, "anim: the tab pill slides (%.2f), then rests", mid);

    /* keys: a tab or the selection fades, a ticking value doesn't */
    TsHud k;
    memset(&k, 0, sizeof(k));
    k.row_count = 3, k.row_selected = 1;
    snprintf(k.row_label[1], 64, "Lena"), snprintf(k.row_value[1], 48, "34 €"), snprintf(k.row_value[2], 48, "3,4 km");
    uint32_t k0 = ts_screen_key(&k);
    snprintf(k.row_value[2], 48, "3,3 km");
    CHECK(ts_screen_key(&k) == k0, "keys: a ticking value doesn't fade");
    k.row_selected = 2;
    CHECK(ts_screen_key(&k) != k0, "keys: the selection fades");
    TsHud p1 = h, p2 = h;
    p2.map_pin[0].x += 50;
    CHECK(ts_pins_key(&p1) == ts_pins_key(&p2), "keys: the car moving isn't a new pin");

    /* the fader: from the old picture to the new one, a kept part at once */
    TsFader fd;
    memset(&fd, 0, sizeof(fd));
    uint32_t px[4] = {0xFF000000u, 0xFF000000u, 0xFF000000u, 0xFF000000u}, nw[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    CHECK(ts_fader_start(&fd, px, 2, 2, 1.0, 0.2f), "fader: starts");
    fd.keep[0][0] = 1, fd.keep[0][1] = 0, fd.keep[0][2] = 2, fd.keep[0][3] = 1; /* the top right pixel */
    ts_fader_drawn(&fd, nw, 2, 2);
    memcpy(px, nw, sizeof(px));
    CHECK(ts_fader_apply(&fd, px, 2, 2, 1.05), "fader: fading");
    int g = (int)(px[0] & 255);
    CHECK(g > 40 && g < 230 && (px[0] >> 24) == 255 && px[1] == 0xFFFFFFFFu, "fader: half way (%d), the kept part new", g);
    CHECK(!ts_fader_apply(&fd, px, 2, 2, 1.3) && px[0] == 0xFFFFFFFFu && px[3] == 0xFFFFFFFFu, "fader: done, the new picture");
    ts_fader_free(&fd);
    CHECK(ts_px_mix(0x80402010u, 0x80402010u, 128) == 0x80402010u && ts_px_mix(0, 0xFFFFFFFFu, 256) == 0xFFFFFFFFu, "fader: mix");
}

int main(int argc, char **argv) {
    (void)argv;
    int verbose = argc > 1;
    test_stops_are_real();
    test_places_realistic();
    test_go_flow();
    test_app_offline_things();
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
    test_credit_shop();
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
    test_service();
    test_sounds();
    test_wishes_whole_ride();
    test_talk_back();
    test_phone_app();
    test_cards();
    test_classes();
    test_hotspots();
    test_tiers();
    test_pool();
    test_map();
    test_gamemap();
    test_phone_anim();
    if (getenv("SCREENS")) screens(), screens06(), screens07();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
