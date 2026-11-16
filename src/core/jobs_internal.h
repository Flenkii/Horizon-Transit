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
void chat(TsCore *c, int style, const char *text, int force);
void say(TsCore *c, int style, int kind, int force); /* a line of this kind (lines.c), tokens filled in */
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
void career_account_rows(const TsCore *c, TsHud *h); /* the phone's Account tab */

/* wishes.c: what a passenger asks for on the way */
void wish_ride_start(TsCore *c);
void wish_step(TsCore *c);            /* the ride's TO_DEST stage, every step */
void wish_arrived_at_dest(TsCore *c); /* a wished stop still open at the destination: not done */
void wish_chip(const TsCore *c, char *out, size_t size);

/* shop.c: what the company buys */
void shop_apply(TsCore *c);        /* after loading or buying: the bus's seats */
void shop_buy(TsCore *c, int item);
void shop_rows(const TsCore *c, TsHud *h);

/* timetable.c: the bus line's timetable */
void tt_start(TsCore *c);
float tt_arrived(TsCore *c);          /* at stop c->stop_i: returns the delay in seconds (0 at the first stop) */
void tt_chip(const TsCore *c, char *out, size_t size);
float tt_line_minutes(const TsCore *c, int line);

#endif
