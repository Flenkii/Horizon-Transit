#ifndef TS_JOBS_INTERNAL_H
#define TS_JOBS_INTERNAL_H
/* Helpers of jobs.c shared with its modules (career.c, wishes.c, timetable.c). Not for the glue. */
#include <stddef.h>
#include "jobs.h"
#include "lines.h"
#include "stops.h"

#define T(c, de, en) ((c)->german ? (de) : (en))

int random_below(TsCore *c, int n);   /* 0 .. n-1 */
float random_unit(TsCore *c);         /* 0 .. 1 */
float clampf(float v, float lo, float hi);
void event(TsCore *c, int kind, int icon, int sound, const char *title, const char *text);
void toast(TsCore *c, int icon, int sound, const char *title, const char *text);
void sound(TsCore *c, int which);
void row_meter(TsHud *h, float full, int quality); /* 0.5.1: a meter under the last row (quality: green/yellow/red) */
/* talk.c (0.5.1) */
int talk_maybe_ask(TsCore *c);
void talk_step(TsCore *c);
void talk_ride_start(TsCore *c);
void talk_chip(const TsCore *c, char *out, size_t size);
void chat(TsCore *c, int style, const char *text, int force);
void say(TsCore *c, int style, int kind, int force);
void complain(TsCore *c, int kind); /* a remark on the driving, or only a thought, or nothing: as the person is */
int ts_outspoken(const TsCore *c, int person); /* 0 quiet, 1 now and then, 2 says it */ /* a line of this kind (lines.c), tokens filled in */
int ride_max_kmh(const TsCore *c);   /* the passenger's limit, as enforced (in mph it is the round mph value) */
int unit_kmh(const TsCore *c, int kmh); /* km/h of the value shown for kmh (rounded to 5 mph when in mph) */
void set_nav(TsCore *c, const float at[3]);
void format_km(const TsCore *c, char *out, size_t size, float metres);
void format_time(char *out, size_t size, double seconds);
float place_distance(int a, int b);
int pick_place(TsCore *c, const float from[3], float min_m, float max_m, int not_this);
int arrived_at(const TsCore *c, const float at[3]);
void row(TsHud *h, const char *label, const char *value, int heading);
const char *type_name(const TsCore *c, int type);

/* career.c: reputation, long-term stats, the working day */
float career_pay_factor(const TsCore *c);
/* after a finished job: returns the reputation's change. bus_line -1 for a ride */
float career_job_done(TsCore *c, int stars, int pay, int base, int special, float trip_m, int bus_line);
void career_cancel(TsCore *c, int passenger_on_board);
void career_auto_shift(TsCore *c);
void career_account_rows(const TsCore *c, TsHud *h); /* the phone's Account tab */

/* wishes.c: what a passenger asks for on the way */
void wish_ride_start(TsCore *c);
void wish_step(TsCore *c);            /* the ride's TO_DEST stage, every step */
void wish_arrived_at_dest(TsCore *c); /* a wished stop still open at the destination: not done */
void wish_chip(const TsCore *c, char *out, size_t size);

/* shop.c: what the company buys */
void shop_apply(TsCore *c);        /* after loading or buying: the bus's seats */
void shop_buy(TsCore *c, int item);
int shop_rows(const TsCore *c, TsHud *h); /* returns the first item shown (0.10.1: it scrolls) */

/* timetable.c: the bus line's timetable */
void tt_start(TsCore *c);
float tt_arrived(TsCore *c);          /* at stop c->stop_i: returns the delay in seconds (0 at the first stop) */
void tt_chip(const TsCore *c, char *out, size_t size);
float tt_line_minutes(const TsCore *c, int line);
float tt_stop_minutes(const TsCore *c, int line, int stop); /* 0.13: from the first stop to this one (the plan) */

/* jobs.c, 0.5 */
int bus_direction(const TsCore *c, int line); /* 1: the line will run backwards */

/* career.c, 0.5: the day's goal */
void career_goal_check(TsCore *c);

/* service.c (0.5): push requests on shift, lost items, the streak, the logbook, the taxi meter and the tablet's column */
void service_idle_step(TsCore *c);
void service_job_starts(TsCore *c);
void service_next_request(TsCore *c, int first); /* 0.13: when the next request comes */
int service_streak(TsCore *c, int stars, int pay); /* after a job: the streak's bonus (paid already), 0 = none */
void service_streak_feed(TsCore *c, int bonus);
void service_log(TsCore *c, int bus, int who, int place, int pay, int stars);
void service_book(TsCore *c, int bus, int who, int from, int to, int pay, int tip, int stars, int late); /* 0.13: book.c */
void service_target_step(TsCore *c);
void service_maybe_lost(TsCore *c, int person, int place);
void service_hud(const TsCore *c, TsHud *h);

/* jobs.c, 0.6: a line of a kind with its tokens filled in, without saying it */
void line_text(TsCore *c, int kind, char *out, size_t size);
int uber_pay(float trip_m, float sat, int type, float factor); /* a ride's pay (0.6: for the tests) */

/* market.c (0.6): ride classes, hotspots, tiers */
float market_class_pay(int ride_class);
float market_class_soft(int ride_class);
float market_class_time(int ride_class);
int market_pick_class(TsCore *c, int type);
float market_tier_progress(const TsCore *c);
void market_tier_hint(const TsCore *c, char *out, size_t size);
const char *market_tier_perks(const TsCore *c, int tier);
float market_tier_pay(const TsCore *c);
void market_check_tier(TsCore *c);
float market_surge(const TsCore *c, int place);
int market_hot_near(const TsCore *c);
int market_hot_pickup(TsCore *c); /* a pickup in a hotspot near the car (70 %), -1 = none */
void market_step(TsCore *c);
void market_surge_text(const TsCore *c, float surge, char *out, size_t size);

/* mapview.c (0.7): the map tab, the preview, the job's map */
void map_hud(const TsCore *c, TsHud *h);

/* cards.c (0.6): the cards in the ride panel's place */
void card_step(TsCore *c);
void card_hud(const TsCore *c, TsHud *h);
void card_ride(TsCore *c, int stars, int pay, int base, int bonus, int late);
void card_bus(TsCore *c, int stars, int pay, int base, int bonus);
void card_shift(TsCore *c);
void card_tier(TsCore *c, int tier);

#endif
