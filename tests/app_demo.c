/* 0.10: the phone app's server without the game, for trying the app on a PC (Linux):
     cc -std=gnu11 -O1 -Isrc/core -Isrc tests/app_demo.c src/companion_net.c src/core/companion.c ... -lm -lpthread
     ./app_demo [port] [code]
   A simulated car drives along the map's roads to each target, so a ride or a bus line runs through by itself.
   The map is a stand-in (land round the places, roads between them): the real one is built from the game's files. */
#define _GNU_SOURCE
#include "jobs.h"
#include "jobs_internal.h"
#include "stops.h"
#include "map.h"
#include "companion.h"
#include "companion_net.h"
#include "phone_draw.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void logf_(const char *f, ...) {
    va_list a;
    va_start(a, f);
    vprintf(f, a);
    va_end(a);
    printf("\n");
    fflush(stdout);
}

static void stand_in_map(void) {
    const int W = 2100, H = 1200;
    const float rect[4] = {-7500.0f, 13500.0f, 1000.0f, -11000.0f};
    uint32_t *rgb = malloc(sizeof(uint32_t) * W * H);
    uint8_t *flags = calloc(W * H, 1);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float wx = rect[0] + (x + 0.5f) * 10.0f, wz = rect[2] - (y + 0.5f) * 10.0f, best = 1e9f;
            for (unsigned i = 0; i < ts_place_count; ++i) {
                float dx = ts_places[i].pos[0] - wx, dz = ts_places[i].pos[2] - wz, d = dx * dx + dz * dz;
                if (d < best) best = d;
            }
            best = sqrtf(best);
            int land = best < 2600.0f + 500.0f * sinf(wx / 900.0f) * cosf(wz / 700.0f);
            int forest = land && sinf(wx / 300.0f) * sinf(wz / 260.0f) > 0.55f;
            rgb[y * W + x] = !land ? 0x17263C : forest ? 0x1F3A2E : 0x242F3E;
            if (!land) flags[y * W + x] = 2;
        }
    /* roads: each place to its 3 nearest within 4 km */
    int cap = ts_place_count * 3, n = 0;
    float *seg = malloc(sizeof(float) * 4 * cap);
    uint8_t *kind = malloc(cap);
    for (unsigned i = 0; i < ts_place_count; ++i) {
        int best[3] = {-1, -1, -1};
        float bd[3] = {1e18f, 1e18f, 1e18f};
        for (unsigned j = 0; j < ts_place_count; ++j) {
            if (j == i) continue;
            float d = ts_distance_xz(ts_places[i].pos, ts_places[j].pos);
            if (d > 4000.0f) continue;
            for (int k = 0; k < 3; ++k)
                if (d < bd[k]) {
                    for (int m = 2; m > k; --m) bd[m] = bd[m - 1], best[m] = best[m - 1];
                    bd[k] = d, best[k] = (int)j;
                    break;
                }
        }
        for (int k = 0; k < 3; ++k) {
            if (best[k] < 0 || n >= cap) continue;
            seg[4 * n] = ts_places[i].pos[0], seg[4 * n + 1] = ts_places[i].pos[2];
            seg[4 * n + 2] = ts_places[best[k]].pos[0], seg[4 * n + 3] = ts_places[best[k]].pos[2];
            kind[n++] = bd[k] > 2500.0f ? TS_ROAD_MOTORWAY : TS_ROAD_STREET;
        }
    }
    int ok = ts_map_load_rgb(W, H, rgb, NULL, flags, seg, kind, n, rect);
    logf_("stand-in map: %s, %d roads", ok ? "ok" : "FAILED", n);
    free(rgb), free(flags), free(seg), free(kind);
}

static TsCore core;

/* the glue's Options rows, made up here: row 1 the design (the app follows it) */
static int demo_values[TS_SETTING_ROWS] = {1, 0, 0, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1};
static const char *demo_labels[TS_SETTING_ROWS][2] = {
    {"Connect phone app", "Handy-App verbinden"}, {"Design", "Design"}, {"Device", "Gerät"}, {"Time limits", "Zeitlimits"},
    {"Passenger complaints", "Fahrgäste meckern"}, {"Auto GPS", "Navigation automatisch"}, {"Reputation, specials, wishes", "Ruf, Sonderfahrten, Wünsche"},
    {"Voices (experimental)", "Stimmen (Versuch)"}, {"Language", "Sprache"}, {"Mod sounds", "Mod-Geräusche"}, {"Speed warning tones", "Tempo-Warnton"},
    {"Answer passengers", "Fahrgästen antworten"}, {"Passenger doors", "Fahrgast-Türen"}, {"Ride panel", "Fahrt-Panel am Tacho"},
    {"Map preview", "Kartenvorschau"}, {"Bus lines need the bus", "Buslinien nur im Bus"}};
static void demo_settings(const char *code) {
    int de = core.german;
    core.setting_count = TS_SETTING_ROWS;
    core.app_theme = demo_values[1];
    for (int i = 0; i < TS_SETTING_ROWS; ++i) {
        snprintf(core.setting_label[i], sizeof(core.setting_label[i]), "%s", demo_labels[i][de]);
        core.setting_note[i][0] = 0;
        if (i == 0) snprintf(core.setting_value[i], sizeof(core.setting_value[i]), "Code %s", code);
        else if (i == 1) snprintf(core.setting_value[i], sizeof(core.setting_value[i]), "%s", ts_theme_name(demo_values[1], de));
        else if (i == 2) snprintf(core.setting_value[i], sizeof(core.setting_value[i]), "%s", ts_device_name(demo_values[2], de));
        else snprintf(core.setting_value[i], sizeof(core.setting_value[i]), "%s", demo_values[i] ? (de ? "An" : "On") : (de ? "Aus" : "Off"));
        int n = i == 0 ? 0 : i == 1 ? TS_THEMES : i == 2 ? TS_DEVICES : 2; /* 0.10.4: the choices */
        core.setting_choices[i] = n, core.setting_index[i] = demo_values[i];
        for (int k = 0; k < n; ++k)
            snprintf(core.setting_choice[i][k], sizeof(core.setting_choice[i][k]), "%s",
                     i == 1 ? ts_theme_name(k, de) : i == 2 ? ts_device_name(k, de) : k ? (de ? "An" : "On") : (de ? "Aus" : "Off"));
    }
}
static TsCar car;
static float way[800];
static int way_n, way_i;
static float way_to[2];

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8710;
    const char *code = argc > 2 ? argv[2] : "1234";
    int german = getenv("DE") != NULL;
    ts_init(&core, 12345, german);
    core.bus_capacity = 40;
    core.time_factor = 1.0f;
    core.replies_on = 1;
    snprintf(core.reply_key, sizeof(core.reply_key), "B");
    stand_in_map();
    /* the car at the festival */
    const float *start = ts_places[0].pos;
    car.pos[0] = start[0], car.pos[1] = start[1], car.pos[2] = start[2];
    car.fwd[2] = 1.0f, car.right[0] = 1.0f;
    for (int i = 0; i < 30; ++i) ts_car_sample(&core, 1.0f / 60.0f, &car);
    ts_set_car(&core, "PG_BUS_14");
    ts_refresh_offers(&core);
    core.setting_pressed = -1;
    if (getenv("THEME")) demo_values[1] = atoi(getenv("THEME"));
    {   /* 0.13: a ride book of the last two weeks (newest first) */
        long long now = (long long)time(NULL);
        unsigned r = 7;
        core.wall_time = now;
        for (int k = 0; k < 46; ++k) {
            r = r * 1103515245u + 12345u;
            TsBookEntry e;
            memset(&e, 0, sizeof(e));
            e.when = now - 3600LL * 5 - (long long)k * 7 * 3600 - (r >> 20) % 3000;
            e.bus = (r >> 8) % 7 == 0;
            e.who = (short)(e.bus ? (r >> 10) % ts_line_count : (r >> 10) % 40);
            e.from = (short)((r >> 4) % ts_place_count), e.to = (short)((r >> 12) % ts_place_count);
            e.car = (short)(1 + (r >> 6) % 50);
            e.stars = (unsigned char)(3 + (r >> 14) % 3), e.late = (r >> 16) % 9 == 0;
            e.pay = 400 + (int)((r >> 9) % 2600) / 5 * 5, e.tip = (int)((r >> 11) % 300) / 5 * 5;
            e.metres = 1200 + (int)((r >> 5) % 9000), e.seconds = 120 + (int)((r >> 7) % 600);
            core.book[core.book_count++] = e;
        }
        core.book_serial = 1;
    }
    core.credits_mode = 1; /* 0.13: the game account, as the mod reads it from the game */
    core.game_credits = getenv("MONEY") ? atoll(getenv("MONEY")) : 184500;
    demo_settings(code);
    app_net_set_name("TEST-PC");
    if (!app_net_start(port, code, "0.13.0", logf_)) return 1;
    char ip[64];
    app_net_address(ip, sizeof(ip));
    logf_("listening on %s:%d, code %s", ip, app_net_port(), code);
    static char json[1 << 20];
    float speed = 0.0f;
    const float DT = 1.0f / 60.0f, CRUISE = 30.0f; /* fast, to see something happen */
    for (long frame = 0;; ++frame) {
        struct timespec ts = {0, 16666667};
        nanosleep(&ts, NULL);
        /* drive along the roads to the GPS target */
        int moving = 0;
        if (core.mode != TS_IDLE && core.nav_on && core.stage != TS_UBER_BOARDING && core.stage != TS_UBER_ALIGHTING && core.stage != TS_BUS_AT_STOP) {
            float to[2] = {core.nav_target[0], core.nav_target[2]};
            if (!way_n || to[0] != way_to[0] || to[1] != way_to[1]) {
                float a[2] = {car.pos[0], car.pos[2]};
                way_n = ts_map_route(a, to, way, 400);
                way_i = 1, way_to[0] = to[0], way_to[1] = to[1];
            }
            if (way_i < way_n) {
                float dx = way[2 * way_i] - car.pos[0], dz = way[2 * way_i + 1] - car.pos[2], d = sqrtf(dx * dx + dz * dz);
                float left = ts_distance_xz(car.pos, core.nav_target);
                float want = left < 60.0f ? fmaxf(0.0f, (left - 8.0f) * 0.6f) : CRUISE;
                speed += fmaxf(-8.0f * DT, fminf(3.0f * DT, want - speed));
                if (d < 3.0f) way_i++;
                else {
                    car.fwd[0] = dx / d, car.fwd[2] = dz / d;
                    car.right[0] = car.fwd[2], car.right[2] = -car.fwd[0];
                }
                moving = 1;
            }
        }
        if (!moving) speed = fmaxf(0.0f, speed - 8.0f * DT);
        if (core.mode == TS_IDLE) way_n = 0;
        car.vel[0] = car.fwd[0] * speed, car.vel[2] = car.fwd[2] * speed;
        car.pos[0] += car.vel[0] * DT, car.pos[2] += car.vel[2] * DT;
        ts_car_sample(&core, DT, &car);
        if (frame % 6) continue;
        int action, arg;
        while (app_net_take(&action, &arg)) logf_("app: action %d %d -> %d", action, arg, ts_app_do(&core, action, arg));
        core.wall_time = (long long)time(NULL);
        ts_step(&core);
        if (core.credit_buy_item >= 0) { /* 0.13: the game pays the purchase */
            long long left = core.game_credits - core.credit_buy_cost;
            ts_shop_credit_done(&core, core.credit_buy_item, left >= 0, left >= 0 ? left : core.game_credits);
        }
        if (core.setting_pressed >= 0) { /* what the glue does: the next value */
            int i = core.setting_pressed;
            demo_values[i] = (demo_values[i] + 1) % (i == 1 ? TS_THEMES : i == 2 ? TS_DEVICES : 2);
            core.setting_pressed = -1;
        }
        for (int i = 1; i < TS_SETTING_ROWS; ++i) /* 0.10.4: the app set a value */
            if (core.setting_want[i] >= 0) demo_values[i] = core.setting_want[i], core.setting_want[i] = -1;
        demo_settings(code);
        TsEvent ev[TS_MAX_EVENTS];
        int n = ts_take_events(&core, ev, TS_MAX_EVENTS);
        for (int i = 0; i < n; ++i) {
            if (ev[i].kind == TS_EV_FEED) ts_app_feed(ev[i].title, ev[i].text, ev[i].style);
            if (ev[i].kind == TS_EV_TOAST || ev[i].kind == TS_EV_FEED) logf_("  [%s] %s | %s", ev[i].kind == TS_EV_TOAST ? "toast" : "chat", ev[i].title, ev[i].text);
        }
        TsHud h;
        ts_hud(&core, &h);
        size_t len = ts_app_state_json(&core, &h, json, sizeof(json));
        if (len) app_net_publish(json, len, core.german);
        if (frame % 600 == 0) {
            len = ts_app_lines_json(&core, json, sizeof(json));
            if (len) app_net_lines(json, len);
            len = ts_app_history_json(&core, json, sizeof(json));
            if (len) app_net_history(json, len);
        }
    }
}
