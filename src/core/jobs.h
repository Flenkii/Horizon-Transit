#ifndef TS_JOBS_H
#define TS_JOBS_H
/* The job system: Uber-style rides and bus lines, with virtual passengers.

   Pure C, no Windows and no game calls, so it can be tested on any PC (tests/test_core.c).
   The game glue (src/mod.c) feeds it the own car's state from the game's car update and asks it
   every 100 ms what to show and where the GPS should point. One TsCore, guarded by the glue's lock. */

#include <stdint.h>
#include "cars.h"
#include "people.h"

#define TS_MAX_EVENTS 16
#define TS_PHONE_OFFERS 3
#define TS_TEXT 96

enum { TS_IDLE = 0, TS_UBER, TS_BUS };

/* kinds of passengers (rides) */
enum {
    TS_P_NORMAL = 0, TS_P_RELAXED, TS_P_HURRY, TS_P_FAN,
    /* 0.4 special rides: by the driven car's group and the company's reputation */
    TS_P_VIP,      /* reputation 60+, a fine car: strict, pays the most */
    TS_P_BUSINESS, /* in a saloon (or a supercar now and then): smooth, pays well */
    TS_P_WEDDING,  /* in a classic or a saloon, reputation 45+: slow and very gentle */
    TS_P_TOURIST,  /* in an off-roader: a long trip out, relaxed */
    TS_P_KINDS
};

/* wishes a passenger may have on the way (wishes.c) */
enum { TS_WISH_NONE = 0, TS_WISH_SLOWER, TS_WISH_SMOOTH, TS_WISH_FASTER, TS_WISH_STOP };

/* the phone's tabs while no job runs */
enum { TS_TAB_RIDES = 0, TS_TAB_BUS, TS_TAB_ACCOUNT, TS_TAB_SHOP, TS_TAB_SETTINGS, TS_TABS };
#define TS_SETTING_ROWS 7

/* what the company can buy (shop.c), bits of TsCore.upgrades */
enum { TS_SHOP_DISPATCH = 0, TS_SHOP_ADS, TS_SHOP_COMFORT, TS_SHOP_BIGBUS, TS_SHOP_PREMIUM, TS_SHOP_ITEMS };

enum {
    TS_UBER_TO_PICKUP = 0, /* drive to the passenger */
    TS_UBER_BOARDING,      /* stopped at the pickup, the passenger gets in */
    TS_UBER_TO_DEST,       /* drive the passenger to the destination */
    TS_UBER_ALIGHTING,     /* stopped at the destination, the passenger gets out */
    TS_BUS_TO_STOP,        /* drive to the next stop */
    TS_BUS_AT_STOP         /* stopped at a stop: passengers get out and in */
};

/* what the glue should do (toasts and chat lines) */
enum { TS_EV_TOAST = 1, TS_EV_FEED, TS_EV_SAVE, TS_EV_SOUND };
enum { TS_SOUND_NOTIFY = 1, TS_SOUND_TICK, TS_SOUND_TICK_END, TS_SOUND_ERROR };
enum { TS_ICON_NONE = 0, TS_ICON_PAYOUT, TS_ICON_ROADS, TS_ICON_TROPHY };
typedef struct {
    int kind;
    int icon, sound, style; /* style: 0 white, 1 green, 2 orange, 3 yellow (chat lines) */
    char title[TS_TEXT], text[TS_TEXT];
} TsEvent;

/* one sample of the own car, world space, metres and metres per second */
typedef struct {
    float pos[3], vel[3], fwd[3], right[3];
} TsCar;

typedef struct {
    int pickup, dest;   /* indices into ts_places */
    float pickup_m;     /* straight metres car -> pickup when offered */
    float trip_m;       /* estimated road metres pickup -> destination */
    float limit_s;      /* time limit pickup -> destination */
    int pay;            /* expected pay at 80 % satisfaction */
    int passenger;      /* name index */
    int ptype;          /* TS_P_ */
    int max_kmh;        /* the speed the passenger is still happy with */
    float pay_factor;   /* the area's demand times the reputation (career.c) */
} TsOffer;

/* What the HUD shows (filled by ts_hud) */
typedef struct {
    int chips;                       /* chips at the top: status, distance, speed limit; the corner with time and stars */
    char chip0[32], chip1[32], chip2[32], chip3[32], corner[32]; /* chip3: a passenger's wish */
    int stars;                       /* 0..5, the corner chip's dots */
    int panel;                       /* the phone: the panel's rows and the bar under them (app name, tabs) */
    int row_count, row_selected;     /* row_selected -1 = none */
    char row_label[8][64], row_value[8][48];
    int row_heading[8];              /* 0 normal, 1 heading, 2 dim */
    int tab_count, tab;
    char tabs[TS_TABS][24];
    char line1[48], line2[72];
    char footer[100];
    int german;                      /* the texts' language */
    int countdown;                   /* boarding seconds, 0 = none */
    char countdown_label[64];
    int sign_limit;                  /* the speed-limit sign while a passenger rides (in the shown unit), 0 = none */
    int sign_over;                   /* too fast now */
    char sign_unit[8];               /* "km/h" / "mph" */
} TsHud;

typedef struct {
    /* settings */
    int german;
    int bus_capacity;     /* virtual seats */
    float time_factor;    /* 1.0 = normal time limits */
    int bus_needs_bus;    /* bus lines only in the game's bus (PG_BUS_14) */
    int extras_off;       /* 0.4 switch: no reputation effect on pay and requests, no special rides, no wishes */
    unsigned upgrades;    /* bought in the shop (saved): 1 << TS_SHOP_ */
    int imperial;         /* the game shows mph and miles (its Units option, or its language's): set by the glue */
    /* the phone's Settings tab: the glue fills the rows (its own settings) and acts on a press */
    char setting_label[TS_SETTING_ROWS][40], setting_value[TS_SETTING_ROWS][24];
    int setting_count;
    int setting_pressed;  /* the row Enter was pressed on, -1 none (the glue takes it) */

    uint32_t rng;
    double t;             /* game seconds (sum of the car update's dt: stops while the game pauses) */

    /* the car */
    int car_ok;
    TsCar car;
    float speed, acc_long, acc_lat;
    float prev_vel[3], prev_pos[3];
    int have_prev;
    double car_seen_t;
    int car_kind;         /* TS_CAR_ of the driven car (TS_CAR_UNKNOWN until its name is read) */
    char car_media[40], car_name[64];

    /* the phone */
    int phone_open, phone_sel, phone_tab;
    TsOffer offers[TS_PHONE_OFFERS];
    int offer_count;
    int cancel_armed;     /* phone open during a job: accept twice = cancel */

    /* the running job */
    int mode, stage;
    TsOffer job;
    double stage_t, leg_t, deadline, trip_t;
    float sat;            /* passenger satisfaction 0..100 */
    float leg_m, trip_driven_m;
    int n_accel, n_brake, n_corner, n_crash, n_speeding;
    double calm_since, crash_cooldown, chat_cooldown, harsh_until;
    int was_harsh;
    double harsh_since, complain_at; /* 0.4: a harsh moment counts after 0.35 s; complaints at most every 35 s */
    int late_noted;
    double slow_since, talk_at, nag_at;
    int fan_happy_said;
    unsigned char met[TS_PEOPLE_MAX]; /* rides done with each person (saved); a regular is greeted as one */
    int story_said;                   /* lines of the passenger's own story told this ride */
    unsigned char last_line[80]; /* per kind of line: the last one said, so it isn't said twice in a row */

    /* bus */
    int line, stop_i, onboard, delivered, board_total;
    int to_alight, to_board, boarded_now, alighted_now;
    float stop_secs;
    int bus_pay;
    float bus_late_s;

    /* GPS target: the glue sets the game's GPS when nav_serial changes */
    int nav_on;
    float nav_target[3];
    unsigned nav_serial;
    int gps_arrived;      /* the game's GPS says the destination is reached (glue) */

    /* stats (saved by the glue) */
    long long wallet;
    int jobs_done, bus_lines_done, passengers_total;
    double km_total;
    long long rating_sum;
    int rating_count;
    /* 0.4: career (saved): see career.c */
    float reputation;           /* 0..100, starts at 50 */
    long long tips_total;
    int crashes_total, on_time_total, late_total, five_stars, specials_done, wishes_ok, wishes_failed;
    int best_pay;
    float longest_m;
    int line_done[8];
    unsigned short met_car[TS_PEOPLE_MAX]; /* the car each one rode in last (index in ts_cars + 1, 0 = none) */
    unsigned char met_stars[TS_PEOPLE_MAX]; /* the stars they gave last time */

    /* 0.4: the working day (not saved: a shift ends with the game) */
    struct {
        int on;
        double start_t, km_start;
        int jobs, lines, stars_sum, rated, best_pay;
        long long earned, tips;
        float rep_start;
    } shift;

    /* 0.4: the car's group and the area's demand */
    int car_group, car_open, car_index;   /* TS_GROUP_, an open-top car, index in ts_cars (-1 unknown) */
    int demand;                           /* 0 quiet countryside, 1 normal, 2 busy (town, festival) */
    int area;                             /* the nearest town or the festival (index in ts_places) */
    int memory_said;                      /* the regular's memory line was said this ride */

    /* 0.4: a passenger's wish (wishes.c) */
    int wish, wish_count, wish_limit, wish_place, wish_stop_stage;
    double wish_at, wish_start, wish_until, wish_stopped_t, next_wish_t;

    /* 0.4: the bus timetable (timetable.c): seconds after the first stop, per stop */
    float plan[24];
    double plan_t0;
    int plan_started, stops_on_time, stops_late;
    float last_delay;

    TsEvent events[TS_MAX_EVENTS];
    int event_count;
} TsCore;

void ts_init(TsCore *c, uint32_t seed, int german);

/* game thread, from the own car's update: dt in game seconds */
void ts_car_sample(TsCore *c, float dt, const TsCar *car);

/* the player's keys / phone */
void ts_phone_toggle(TsCore *c);
void ts_phone_next(TsCore *c);
void ts_phone_prev(TsCore *c);
void ts_phone_tab(TsCore *c, int direction); /* -1 / +1: the tab to the left / right */
/* the driven car, by its media name ("LAM_MiuraP400_67"; NULL or "" = unknown) */
void ts_set_car(TsCore *c, const char *media);
void ts_phone_accept(TsCore *c);   /* phone open: accept the selected row; during a job: cancel (twice) */
/* the pause-menu page: row 0..2 = ride offers, 3.. = bus lines */
void ts_accept_row(TsCore *c, int row);
void ts_cancel(TsCore *c);
/* new ride requests around the car (the pause-menu page asks for them as it opens) */
void ts_refresh_offers(TsCore *c);
/* the GPS target again (the player removed it, or the phone was opened) */
void ts_renew_nav(TsCore *c);

/* every ~100 ms (worker thread) */
void ts_step(TsCore *c);

/* HUD texts for now */
void ts_hud(const TsCore *c, TsHud *out);

/* takes the queued events (returns how many, up to max) */
int ts_take_events(TsCore *c, TsEvent *out, int max);

/* 0.4: the working day (phone, Account tab): starts a shift, or ends it with the day's statement */
void ts_shift_toggle(TsCore *c);
/* the reputation's name: "Top", "Sehr gut" ... */
const char *ts_rep_name(const TsCore *c);
/* the group's name ("Sportwagen" / "sports car") */
const char *ts_group_name(int group, int german);
/* the company owns this from the shop */
int ts_has(const TsCore *c, int item);

/* speeds as the game shows them: km/h, or mph rounded to 5 (75 mph for 120 km/h) */
int ts_speed_shown(const TsCore *c, int kmh);
const char *ts_speed_unit(const TsCore *c);

/* helpers shared with the glue */
float ts_distance_xz(const float a[3], const float b[3]);
int ts_stars(float sat);
const char *ts_passenger_name(int index);

/* tunables (also used by the tests) */
#define TS_ARRIVE_RADIUS 30.0f     /* metres to the target that count as "there" */
#define TS_ARRIVE_RADIUS_GPS 90.0f /* when the game's GPS itself says the destination is reached */
#define TS_STOPPED_SPEED 2.0f      /* m/s (7 km/h): standing for boarding */
#define TS_LEAVE_SPEED 4.0f        /* m/s: driving off while boarding */
#define TS_ROAD_FACTOR 1.35f       /* road metres per straight metre (estimate) */
#define TS_UBER_AVG_SPEED 15.0f    /* m/s (54 km/h) for time limits */
#define TS_BUS_AVG_SPEED 12.5f     /* m/s (45 km/h) */
#define TS_BUS_MAX_KMH 80          /* bus passengers' speed limit */

#endif
