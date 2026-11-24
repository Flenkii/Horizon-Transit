#ifndef TS_JOBS_H
#define TS_JOBS_H
/* The job system: Uber-style rides and bus lines, with virtual passengers.

   Pure C, no Windows and no game calls, so it can be tested on any PC (tests/test_core.c).
   The game glue (src/mod.c) feeds it the own car's state from the game's car update and asks it
   every 100 ms what to show and where the GPS should point. One TsCore, guarded by the glue's lock. */

#include <stddef.h>
#include <stdint.h>
#include "cars.h"
#include "people.h"

#define TS_MAX_EVENTS 16
#define TS_LINE_SLOTS 4096   /* all passenger lines together (lines.c), at most */
#define TS_LINE_REPEAT_S 300.0f /* a line said comes back after 5 minutes at the earliest */
#define TS_PHONE_OFFERS 3
#define TS_TEXT 192 /* long passenger lines are split for the feed (ts_feed_split) */

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
enum { TS_WISH_NONE = 0, TS_WISH_SLOWER, TS_WISH_SMOOTH, TS_WISH_FASTER, TS_WISH_STOP, TS_WISH_FLOORIT /* 0.5.1: "floor it" in a supercar */ };

/* the phone's tabs while no job runs */
enum { TS_TAB_RIDES = 0, TS_TAB_BUS, TS_TAB_MAP, TS_TAB_ACCOUNT, TS_TAB_SHOP, TS_TAB_SETTINGS, TS_TABS }; /* 0.7: the map */
#define TS_SETTING_ROWS 16 /* 0.10.1: every setting of the mod menu (the phone scrolls) */

/* what the company can buy (shop.c), bits of TsCore.upgrades */
/* 0.10.1: tip box, customer file, shift planner, ticket machine */
enum { TS_SHOP_DISPATCH = 0, TS_SHOP_ADS, TS_SHOP_COMFORT, TS_SHOP_BIGBUS, TS_SHOP_PREMIUM, TS_SHOP_TIPS, TS_SHOP_REGULARS,
       TS_SHOP_PLANNER, TS_SHOP_TICKETS,
       /* 0.13 */
       TS_SHOP_SNACKS, TS_SHOP_DASHCAM, TS_SHOP_ONTIME, TS_SHOP_LONGHAUL, TS_SHOP_RADAR, TS_SHOP_NIGHT, TS_SHOP_AIRCON,
       TS_SHOP_BILLBOARD, TS_SHOP_INSURANCE, TS_SHOP_ITEMS };

enum {
    TS_UBER_TO_PICKUP = 0, /* drive to the passenger */
    TS_UBER_BOARDING,      /* stopped at the pickup, the passenger gets in */
    TS_UBER_TO_DEST,       /* drive the passenger to the destination */
    TS_UBER_ALIGHTING,     /* stopped at the destination, the passenger gets out */
    TS_BUS_TO_STOP,        /* drive to the next stop */
    TS_BUS_AT_STOP         /* stopped at a stop: passengers get out and in */
};

/* what the glue should do (toasts and chat lines) */
enum { TS_EV_TOAST = 1, TS_EV_FEED, TS_EV_SAVE, TS_EV_SOUND, TS_EV_DOOR };

/* 0.4.3: the passenger's door (doors.c). TS_EV_DOOR: sound = TS_DOOR_ step, style = TS_DOOR_LF.. (the door) */
enum { TS_DOOR_LF = 0, TS_DOOR_RF, TS_DOOR_LR, TS_DOOR_RR, TS_DOORS }; /* left/right, front/rear, as the driver sits */
enum {
    TS_DOOR_PREPARE = 1, /* the glue gets the car's animation ready (its showroom detail) */
    TS_DOOR_OPEN,        /* the door opens: animation and the open sound */
    TS_DOOR_CLOSE,       /* the door closes: animation */
    TS_DOOR_SHUT,        /* the door hits the frame: the close sound */
    TS_DOOR_DONE         /* the glue gives the car's animation back to the game */
};
enum { TS_SOUND_NOTIFY = 1, TS_SOUND_TICK, TS_SOUND_TICK_END, TS_SOUND_ERROR,
       /* 0.5.1: the mod's own sounds (the WAVs in sounds/, built into the DLL); with "Sounds" off the game's sound instead:
          bell -> tick, gong -> none, request and payout -> notify */
       TS_SOUND_BELL, TS_SOUND_GONG, TS_SOUND_REQUEST, TS_SOUND_PAYOUT };
enum { TS_ICON_NONE = 0, TS_ICON_PAYOUT, TS_ICON_ROADS, TS_ICON_TROPHY };
typedef struct {
    int kind;
    int icon, sound, style; /* style: 0 white, 1 green, 2 orange, 3 yellow (chat lines) */
    char title[TS_TEXT], text[TS_TEXT];
    int speaker;            /* TS_EV_FEED: the person (ts_people index) who says it, -1 = not a passenger */
    int voice_female, voice_pitch, voice_rate; /* for "Voices": pitch -10..10, rate -10..10 (SAPI's scales) */
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
    int regular;        /* 0.5: a regular who asked for you by name (pays 15 % more) */
    int ride_class;     /* 0.6: TS_CLASS_ */
    int party;          /* 0.6: people riding (XL: 2 to 4), 1 otherwise */
    float surge;        /* 0.6: a hotspot's surcharge, 1.0 = none (in pay_factor already) */
    int pool;           /* 0.6: a shared ride: a second passenger (pool_*) gets in on the way */
    int pool_passenger, pool_pickup, pool_dest, pool_ptype;
    float pool_trip_m;  /* the second passenger's road metres, their pickup to their destination */
} TsOffer;

/* 0.13: the ride book (the glue saves it): every job with its date, newest first, for the app's earnings and history */
#define TS_BOOK 300
typedef struct {
    long long when;          /* unix seconds from the glue's clock (TsCore.wall_time), 0 = not known */
    short bus, who, from, to; /* bus: 0 a ride, 1 a line; who: the passenger / the line; from, to: places */
    short car;               /* index in ts_cars + 1, 0 = not known */
    unsigned char stars, late;
    int pay, tip;            /* CR: all of it (bonus too); the part above the plain fare */
    int metres, seconds;
} TsBookEntry;

/* 0.5: the logbook (the tablet shows it): the last jobs, newest first, not saved */
#define TS_LOG 5
typedef struct {
    int bus;            /* 0 a ride, 1 a bus line */
    int who;            /* the passenger (ride) or the line (bus) */
    int place;          /* the destination / the line's last stop */
    int pay, stars;
} TsLogEntry;

/* 0.5: the tablet's detail column (ts_hud fills it on every tab; the phone doesn't show it) */
#define TS_DETAIL_ROWS 12
enum { TS_DET_PAIR = 0, TS_DET_STOP, TS_DET_STOP_NOW, TS_DET_STOP_DONE, TS_DET_TEXT };

/* 0.6: ride classes by the driven car (market.c): Saver in a small car or a hot hatch, Comfort, XL in an off-roader
   (two to four people), Black in a luxury saloon or a supercar (from the Silver tier: strict, pays the most) */
enum { TS_CLASS_SAVER = 0, TS_CLASS_COMFORT, TS_CLASS_XL, TS_CLASS_BLACK, TS_CLASSES };
/* 0.6: the driver's tier (market.c): by jobs, average stars and reputation; never goes down */
enum { TS_TIER_BRONZE = 0, TS_TIER_SILVER, TS_TIER_GOLD, TS_TIER_PLATINUM, TS_TIERS };

/* 0.6: a card in the ride panel's place (cards.c): after a ride or a line, at the end of a shift, a new tier */
enum { TS_CARD_NONE = 0, TS_CARD_RIDE, TS_CARD_SHIFT, TS_CARD_TIER };
#define TS_CARD_PAIRS 4
typedef struct {
    int kind;                 /* TS_CARD_ */
    float secs;               /* how long it stays */
    char title[32];           /* small, in the design's colour: "FAHRT BEENDET" */
    char name[48];            /* "Lena · Black" */
    char note[96];            /* the passenger's comment, the best ride ... */
    int money;                /* counts up as the card comes in; -1 = none */
    int stars;                /* 0..5 light up one by one; -1 = none */
    int tier;                 /* a medal (TS_TIER_), -1 = none */
    int pairs;                /* along the bottom: label and value */
    char label[TS_CARD_PAIRS][20], value[TS_CARD_PAIRS][24];
} TsCard;

/* 0.7: the phone's map (mapview.c builds it, phone_draw.c draws it on the picture of map.c) */
#define TS_MAP_PINS 32
enum { TS_PIN_TOWN = 0, TS_PIN_FESTIVAL, TS_PIN_HOT, TS_PIN_STOP, TS_PIN_STOP_DONE, TS_PIN_STOP_NEXT, TS_PIN_PICKUP, TS_PIN_RIDER2,
       TS_PIN_DEST, TS_PIN_CAR };
/* how a pin joins the route: the way runs from the route's previous pin to this one */
enum { TS_ROUTE_NONE = 0, TS_ROUTE_MAIN, TS_ROUTE_DASHED, TS_ROUTE_LATER, TS_ROUTE_START };
enum { TS_MAPKEYS_NONE = 0, TS_MAPKEYS_PREVIEW, TS_MAPKEYS_TAB, TS_MAPKEYS_JOB, TS_MAPKEYS_JOBPAGE /* the job's rows (no map) */ };
typedef struct {
    float x, z;               /* world position */
    signed char kind, route;  /* TS_PIN_, TS_ROUTE_ */
    short num;                /* a stop's number (1..), a hotspot's radius in metres */
    char label[32];           /* a name by the pin ("" = none) */
} TsMapPin;

/* What the HUD shows (filled by ts_hud) */
typedef struct {
    int chips;                       /* chips at the top: status, distance, speed limit; the corner with time and stars */
    char chip0[32], chip1[32], chip2[32], chip3[32], corner[32]; /* chip3: a passenger's wish */
    int stars;                       /* 0..5, the corner chip's dots */
    int panel;                       /* the phone: the panel's rows and the bar under them (app name, tabs) */
    int row_count, row_selected;     /* row_selected -1 = none */
    char row_label[9][64], row_value[9][48]; /* 9: the heading and 8 settings */
    int row_heading[9];              /* 0 normal, 1 heading, 2 dim */
    unsigned row_meter_on;           /* 0.5.1: rows (bits) with a meter under their text: progress, satisfaction, reputation */
    unsigned row_meter_quality;      /* ... coloured by how good (else the app's colour: progress) */
    float row_meter[9];              /* ... how full, 0..1 */
    int tab_count, tab;
    char tabs[TS_TABS][24];
    char line1[48], line2[72];
    char footer[100];
    int german;                      /* the texts' language */
    int countdown;                   /* boarding seconds, 0 = none */
    char countdown_label[64];
    int sign_limit;                  /* the speed-limit sign while a passenger rides (in the shown unit), 0 = none */
    int sign_over;                   /* too fast now: 1 a little (yellow), 2 past the 15 km/h leeway (red) */
    int sign_free;                   /* 0.5.1: no limit ("floor it" wish): the end-of-restrictions sign instead of a number */
    char sign_unit[8];               /* "km/h" / "mph" */
    int g_on;                        /* the G-meter while a passenger rides */
    float g_lat, g_long;             /* sideways (+ right) and forward (+ gas, - braking), 1.0 = the passenger's limit */
    int g_over;                      /* outside the circle now */
    /* 0.5 */
    int meter;                       /* the taxi meter / the bus's fares so far, in CR, -1 = none */
    char meter_label[16];            /* "TAXAMETER" / "FARES" */
    char detail_title[64];           /* the tablet's right column: a title, then pairs, a route or text */
    int detail_count;
    char detail_label[TS_DETAIL_ROWS][64], detail_value[TS_DETAIL_ROWS][40];
    int detail_kind[TS_DETAIL_ROWS]; /* TS_DET_ */
    /* 0.5.1: the ride card by the speedometer: where to, how far along */
    int trip_on;
    float trip_progress;             /* 0..1 along the leg (pickup, destination) or the line's stops */
    char trip_label[40];             /* "ZUM ZIEL" / "LINIE 3 · NÄCHSTER HALT" */
    char trip_to[64];                /* the place's name */
    /* 0.6 */
    TsCard card;                     /* card.kind 0 = none: the ride panel shows the card instead of the ride */
    unsigned card_serial;            /* a new card: the window animates it in */
    float card_anim;                 /* seconds since the card came (the window sets it; ts_hud: 9 = all shown) */
    int trip_class;                  /* TS_CLASS_ of the ride, -1 = none (bus) */
    int trip_pool;                   /* a shared ride */
    float trip_surge;                /* the ride's surcharge (1.0 = none) */
    /* 0.7: the map: on the phone between the tabs and the rows, on the tablet in the right column */
    int map_on;
    int map_keys;                    /* TS_MAPKEYS_: what the keys line says */
    float map_x, map_z;              /* the middle of what is shown (world) */
    float map_w, map_h;              /* at least this much is shown (metres) */
    float map_heading;               /* the car's direction: radians from north (+z), clockwise */
    int map_navi;                    /* 0.9.1: like the game's navigation: turned so you drive up, you low in the box */
    float map_rot;                   /* 0.9.1: the map is turned by this (radians; the heading in navi, else 0; the window
                                        eases it) */
    float tab_slide;                 /* 0.9.1: the open tab's pill on its way, as tab + 1 (0 = right at tab; the window eases it) */
    float map_navi_t;                /* 0.9.1: 0..1, the blue dot becoming the navi arrow and the compass (the window eases it) */
    int map_pin_count;
    TsMapPin map_pin[TS_MAP_PINS];
    /* 0.12: the phone app's new look (like the Android app): which page, the head's car and money, the shift's GO */
    int page;                        /* TS_PAGE_ */
    char head_sub[48];               /* under the app's name: the car */
    char money[24], money_label[24]; /* the game account in the head ("184.500 CR", "Spielkonto") */
    int go_row;                      /* the shift's row (a GO card), -1 none */
    int go_on;                       /* the shift runs */
    char go_title[32], go_text[64];  /* "Du bist offline", "Starte eine Schicht ..." */
    int speed_now;                   /* the car's speed in the shown unit, -1 = unknown */
    char speed_unit[8];
} TsHud;
enum { TS_PAGE_RIDES = 0, TS_PAGE_BUS, TS_PAGE_MAP, TS_PAGE_ACCOUNT, TS_PAGE_SHOP, TS_PAGE_SETTINGS, TS_PAGE_JOB, TS_PAGE_JOBMAP };

typedef struct {
    /* settings */
    int german;
    int bus_capacity;     /* virtual seats */
    float time_factor;    /* 1.0 = normal time limits */
    int bus_needs_bus;    /* bus lines only in the game's bus (PG_BUS_14) */
    short recent_places[24];   /* 0.4.2: pickups and destinations of the last rides (place + 1, 0 = empty), avoided for new requests */
    int recent_pos;
    int thinking;         /* say(): the line is a thought (chat shows "Name (thinks)", no voice) */
    int complaints_off;   /* 0.4.1 switch: passengers keep remarks about the driving to themselves (the rating still counts) */
    int extras_off;       /* 0.4 switch: no reputation effect on pay and requests, no special rides, no wishes */
    unsigned upgrades;    /* bought in the shop (saved): 1 << TS_SHOP_ */
    int imperial;         /* the game shows mph and miles (its Units option, or its language's): set by the glue */
    /* the phone's Settings tab: the glue fills the rows (its own settings) and acts on a press */
    char setting_label[TS_SETTING_ROWS][40], setting_value[TS_SETTING_ROWS][24];
    int setting_count;
    int pick_row, pick_sel; /* 0.13: the list of one setting's values is open (-1 none), its selection */
    int setting_pressed;  /* the row Enter was pressed on, -1 none (the glue takes it) */
    char setting_note[TS_SETTING_ROWS][100]; /* 0.10.1: the footer while that row is selected ("" = the usual one) */
    /* 0.10.4: each row's choices (the glue fills them), so the phone app can show and change them without the game;
       setting_want: the value the app asked for, -1 none (the glue takes it) */
    int setting_index[TS_SETTING_ROWS], setting_choices[TS_SETTING_ROWS], setting_want[TS_SETTING_ROWS];
    char setting_choice[TS_SETTING_ROWS][12][24]; /* 0.13: 12 (9 designs did not fit in 8) */
    int app_theme;        /* 0.10.1: the phone's design (TS_THEME_), the phone app takes it over (glue) */

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
    /* 0.7: the map */
    int map_preview;      /* setting: Enter on a ride or a line shows it on the map first (the glue sets it; 0 = takes it at once) */
    int map_view;         /* the preview of the selected ride or line is open */
    int map_btn;          /* its buttons: 0 take it, 1 back */
    float fwd_sign;       /* 0.7.1: -1 when the game's forward vector points backwards (learned while driving); 0 = not known yet */
    int map_zoom;         /* the map tab: 0 near, 1 the region, 2 everything, 3 navi; the preview: 0 the route, 1 everything;
                             the job: 0 the route, 1 everything, 2 navi */
    int job_tab;          /* during a job: 0 the job, 1 the map */
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
    unsigned char last_line[80]; /* unused since 0.4.1 (line_used_at) */
    /* 0.4.1: every line once before any comes back, and none again within 5 minutes (index: ts_line_slot) */
    float line_used_at[TS_LINE_SLOTS];   /* c->t + 1 when it was said last, 0 = not yet */
    unsigned char line_uses[TS_LINE_SLOTS];

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

    /* 0.13: the shop with the game's credits (set by the glue): credits_mode on = the game's functions were found;
       purchases are paid from the game account; game_credits = its balance (-1 unknown); a purchase waits in credit_buy_item
       (-1 none) for the glue, which pays on the game's thread and answers with ts_shop_credit_done */
    int credits_mode;
    long long game_credits;
    int credit_buy_item;
    long long credit_buy_cost;
    int local_hour; /* the PC's hour (-1 unknown): the night surcharge */
    int cr_scale;   /* 0.13: all money is the game's credits: every amount of the old € scale times this (INI [Credits] Scale, 5) */

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
        int goal_done;    /* 0.5: the day's goal reached (its bonus paid) */
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
    /* 0.5.1: wishes last until the destination (no timers) */
    int floorit;          /* the passenger lifted the speed limit for the rest of the ride */
    int wish_slips, wish_was_harsh;
    double wish_over_since; /* "slower": since when clearly too fast, 0 = not */
    /* 0.5.1: talking back (talk.c) */
    int replies_on;       /* the glue: the reply key works (keyboard only) */
    char reply_key[24];   /* the glue: its name for the chip, e.g. "B" or "LB+RB+↓" (0.8) */
    int ask, replied, asks_done; /* the question waiting for an answer / answered (index), -1 = none */
    unsigned asked_mask;  /* questions already asked this game */
    double ask_until, react_at;

    /* 0.4: the bus timetable (timetable.c): seconds after the first stop, per stop */
    float plan[24];
    double plan_t0;
    int plan_started, stops_on_time, stops_late;
    float last_delay;

    /* 0.4.3: the passenger's door when getting in and out (doors.c; Uber rides only) */
    int doors_on;         /* the setting (glue) */
    int door_mask;        /* the doors the driven car can open (1 << TS_DOOR_), 0 = not known (glue) */
    int driver_right;     /* the wheel is on the right (glue) */
    float door_shut_s;    /* seconds from "close" to the close sound (glue; 0 = the default) */
    int door;             /* the passenger's door this ride, -1 = none chosen */
    int door_step;        /* the last TS_DOOR_ step sent, 0 = none */
    double door_t0;       /* when the sequence started */

    /* 0.5 */
    int line_rev;         /* the bus line runs backwards (it started at the nearer end, or a ring the other way round) */
    int stop_request;     /* someone wants to get on or off at the next stop (else the bus may drive through) */
    double bell_at;       /* 0.5.1: when a passenger presses the stop button (after the announcement), 0 = not */
    int bell_stop;        /* ... for this stop (c->stop_i) */
    double hail_at;       /* on shift: when the next push request comes */
    double req_until;     /* 0.13: like a driver app: the one request on the phone runs out then (0 = none) */
    int streak;           /* jobs in a row with 4 or 5 stars */
    struct {
        int state;        /* 0 none, 1 noticed soon, 2 bring it back */
        int person, place, item;
        double at, until;
    } lost;               /* a passenger left something in the car */
    TsLogEntry log[TS_LOG];
    int log_count;
    /* 0.13: the ride book and the glue's clock; a target picked on the app's map (the GPS takes it while free) */
    TsBookEntry book[TS_BOOK];
    int book_count;
    unsigned book_serial;    /* +1 per entry (the glue saves, the app reloads) */
    long long wall_time;     /* the glue: unix seconds now (0 = not known) */
    int target_on;
    float target[3];

    /* 0.6 */
    int cards_on;         /* the glue shows cards in the ride panel (else the payout is a pop-up as before) */
    TsCard card;          /* showing now (kind 0 = none) */
    TsCard card_queue[3];
    int card_queued;
    double card_at;
    unsigned card_serial;
    int tier;             /* the driver's tier (saved), TS_TIER_ */
    struct {
        int place;        /* the hotspot's middle (a place), -1 = none */
        float factor;     /* 1.2 .. 1.8 */
        double until;
    } hot[2];
    double hot_next;      /* when the hotspots change next */
    uint32_t hot_rng;     /* their own dice (so they don't change anything else's) */
    int hot_said;         /* the shift's pop-up about a new hotspot */
    /* 0.6: a shared ride (pool.c): the first passenger, then the second on the way; the first gets out at their
       destination (paid there), then the ride goes on with the second (c->job becomes theirs) */
    int pool_stage;       /* 0 the second not in yet, 1 both in, 2 the first got out */
    int pool_first;       /* the first passenger (for the card) */
    int pool_paid, pool_fare, pool_tip; /* what the first paid: all, the fare, the tip */
    double pool_stop_t;   /* stopped at the second's pickup or the first's destination since, 0 = not */
    double pool_board_t;  /* when the second got in */
    float pool_leg_m;     /* straight metres of the leg when it started (the ride card's route) */
    float pool_driven_at; /* trip_driven_m when the second got in */
    double pool_reply_at; /* the first answers the second's hello */

    TsEvent events[TS_MAX_EVENTS];
    int event_count;
} TsCore;

void ts_init(TsCore *c, uint32_t seed, int german);

/* 0.5.1: the speed warning's tones (chimes.c). The glue passes the sign's level (TsHud.sign_over while the sign is on
   screen, else 0) every step and plays what comes back:
   YELLOW once when the sign turns yellow (again only after TS_WARN_REARM_S below the limit),
   RED every TS_WARN_RING_S while it is red, STOP when red ends (cut the ringing). */
enum { TS_WARN_NONE = 0, TS_WARN_YELLOW, TS_WARN_RED, TS_WARN_STOP };
#define TS_WARN_REARM_S 1.5
#define TS_WARN_RING_S 1.0
typedef struct {
    int level;            /* the last level seen */
    int armed;            /* the yellow tone may play */
    double below_t;       /* since when the level is 0 */
    double next_ring;     /* red: when the next ring is due */
} TsSpeedWarn;
void ts_speed_warn_init(TsSpeedWarn *w);
int ts_speed_warn(TsSpeedWarn *w, int level, double t);

/* game thread, from the own car's update: dt in game seconds */
void ts_car_sample(TsCore *c, float dt, const TsCar *car);

/* the player's keys / phone */
void ts_phone_toggle(TsCore *c);
void ts_phone_next(TsCore *c);
void ts_phone_prev(TsCore *c);
void ts_phone_tab(TsCore *c, int direction); /* -1 / +1: the tab to the left / right */
void ts_phone_back(TsCore *c);  /* 0.7: Backspace: closes the map's preview (the job's map), else the phone */

/* the driven car, by its media name ("LAM_MiuraP400_67"; NULL or "" = unknown) */
void ts_set_car(TsCore *c, const char *media);
void ts_phone_accept(TsCore *c);   /* phone open: accept the selected row; during a job: cancel (twice) */
/* the pause-menu page: row 0..2 = ride offers, 3.. = bus lines */
void ts_accept_row(TsCore *c, int row);
void ts_cancel(TsCore *c);
void ts_reply(TsCore *c);          /* 0.5.1: the reply key: answer the passenger's question (talk.c) */
/* new ride requests around the car (the pause-menu page asks for them as it opens) */
void ts_refresh_offers(TsCore *c);
void ts_go_online(TsCore *c); /* 0.13: GO: the shift starts, requests come one by one */
void ts_decline(TsCore *c);   /* 0.13: say no to the request on the phone */
#define TS_REQUEST_S 20.0     /* 0.13: how long a request waits for a yes */
/* the GPS target again (the player removed it, or the phone was opened) */
void ts_renew_nav(TsCore *c);

/* every ~100 ms (worker thread) */
void ts_step(TsCore *c);

/* HUD texts for now */
void ts_hud(const TsCore *c, TsHud *out);

/* 0.6 (market.c): names */
const char *ts_class_name(int ride_class, int german);
const char *ts_tier_name(int tier, int german);
/* 0.10.1: the shop (shop.c): an item's name, what it does, its price */
const char *ts_shop_name(const TsCore *c, int item);
const char *ts_shop_what(const TsCore *c, int item);
void ts_shop_buy(TsCore *c, int item);
long long ts_shop_cost(const TsCore *c, int item);    /* 0.13: what it costs in CR (base x cr_scale) */
long long ts_shop_balance(const TsCore *c);           /* the game's credits (-1 unknown) */
void ts_shop_credit_done(TsCore *c, int item, int ok, long long balance);
float ts_shop_pay_factor(const TsCore *c, int bus, float trip_m, int on_time); /* 0.13: the surcharges bought */
float ts_shop_start_sat(const TsCore *c);
/* 0.10.1: the first of n entries to show when at most `visible` fit and `sel` must be seen (the phone scrolls) */
int ts_scroll_first(int n, int sel, int visible);
/* 0.13: the ride book as a line of text and back (the glue's file); a target from the app's map */
int ts_book_format(const TsBookEntry *e, char *out, size_t size);
int ts_book_parse(const char *line, TsBookEntry *e);
void ts_book_add(TsCore *c, const TsBookEntry *e); /* in front (newest first) */
void ts_set_target(TsCore *c, float x, float z);   /* the GPS goes there while no job runs */
void ts_clear_target(TsCore *c);
void ts_tier_need(int tier, int *jobs, float *stars, float *rep);
int ts_tier_for(const TsCore *c); /* the tier the numbers reach now (TsCore.tier keeps the highest) */

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
void ts_money_text(const TsCore *c, long long v, char *out, size_t size); /* 0.12 */
const char *ts_cr(int german, long long v); /* 0.13: "12.345 CR" in one of 8 rotating buffers (for one printf) */
const char *ts_speed_unit(const TsCore *c);
/* a place's or a bus line's name in the player's language ("Nice · Spot 4" / "Nizza · Treffpunkt 4") */
const char *ts_place_name(const TsCore *c, int place);
const char *ts_line_title(const TsCore *c, int line);
/* splits a chat line into parts of at most max_chars characters (UTF-8), at spaces; returns how many */
int ts_feed_split(const char *text, int max_chars, char out[][TS_TEXT], int max_parts);
/* the slot of a line in TsCore.line_used_at, -1 if the lines don't fit */
int ts_line_is_complaint(int kind); /* 1: silenced by complaints_off */
int ts_line_slot(int kind, int index);

/* 0.4.3: doors (doors.c) */
int ts_door_pick(TsCore *c);                 /* the passenger's door for this car: rear left, rear right or the front passenger door */
int ts_door_front_passenger(int driver_right);
void ts_door_start(TsCore *c);               /* stopped at the pickup or the destination: the sequence starts */
void ts_door_step(TsCore *c);                /* from the job step: sends the steps that are due */
void ts_door_abort(TsCore *c);               /* drove off: an open door closes at once, the car's animation goes back */
double ts_door_total_s(const TsCore *c);     /* seconds from the start to DONE (0 with doors off) */
const char *ts_door_name(int door);          /* "doorRR" (the car's animation names) */
double ts_uber_stop_s(const TsCore *c, int boarding); /* seconds the passenger takes to get in (1) or out (0) */

/* 0.5: the bus line's stop i in the direction it runs (c->line_rev) */
int ts_bus_stop(const TsCore *c, int line, int i);
/* 0.5: a voiced announcement (TsEvent.speaker) */
#define TS_SPEAKER_ANNOUNCE (-2)
#define TS_SPEAKER_DRIVER (-3)   /* 0.5.1: you, answering a passenger (talk.c): the driver's own fixed voice */
/* 0.5: the day's goal (career.c): jobs, average stars, the bonus */
#define TS_GOAL_JOBS 5
#define TS_GOAL_STARS 4.0f
#define TS_GOAL_BONUS 250

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
