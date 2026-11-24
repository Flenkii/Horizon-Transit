/* The job system - see jobs.h. */
#include "jobs.h"
#include "stops.h"
#include "lines.h"
#include "jobs_internal.h"
#include "people.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* 0.4.2: calmer passengers (Marvin: "they still moan even when I drive carefully"): m/s^2 */
#define TS_LIM_GAS 6.0f
#define TS_LIM_BRAKE 8.5f
#define TS_LIM_SIDE 8.5f
#define TS_LIM_SIDE_FAN 10.0f
#define PASSENGER_COUNT (int)(ts_people_count < TS_PEOPLE_MAX ? ts_people_count : TS_PEOPLE_MAX)

const char *ts_passenger_name(int index) {
    return ts_people[(index % PASSENGER_COUNT + PASSENGER_COUNT) % PASSENGER_COUNT].name;
}

/* ------------------------------------------------------------------ small helpers */

static uint32_t next_random(TsCore *c) {
    uint32_t x = c->rng ? c->rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return c->rng = x;
}

/* 0 .. n-1 */
int random_below(TsCore *c, int n) {
    return n > 0 ? (int)(next_random(c) % (uint32_t)n) : 0;
}

float random_unit(TsCore *c) {
    return (float)(next_random(c) >> 8) / 16777216.0f;
}

float ts_distance_xz(const float a[3], const float b[3]) {
    float x = a[0] - b[0], z = a[2] - b[2];
    return sqrtf(x * x + z * z);
}

int ts_stars(float sat) {
    return sat >= 90.0f ? 5 : sat >= 75.0f ? 4 : sat >= 60.0f ? 3 : sat >= 40.0f ? 2 : 1;
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

void event(TsCore *c, int kind, int icon, int sound, const char *title, const char *text) {
    if (c->event_count >= TS_MAX_EVENTS) { /* drop the oldest */
        memmove(c->events, c->events + 1, sizeof(TsEvent) * (TS_MAX_EVENTS - 1));
        c->event_count = TS_MAX_EVENTS - 1;
    }
    TsEvent *e = &c->events[c->event_count++];
    memset(e, 0, sizeof(*e));
    e->kind = kind, e->icon = icon, e->sound = sound;
    if (title) snprintf(e->title, sizeof(e->title), "%s", title);
    if (text) snprintf(e->text, sizeof(e->text), "%s", text);
    e->speaker = -1;
}

/* how a person sounds with "Voices" on: younger a bit higher, older a bit lower, each person a little different */
static void voice_of(const TsCore *c, int person, TsEvent *e) {
    const TsPerson *p = &ts_people[person];
    e->speaker = person;
    e->voice_female = person < (int)ts_people_female_count ? ts_people_female[person] : person % 2 == 0;
    int pitch = p->age < 25 ? 3 : p->age < 40 ? 1 : p->age < 55 ? 0 : -2;
    pitch += (int)((unsigned)person * 7u % 3u) - 1;
    e->voice_pitch = pitch < -10 ? -10 : pitch > 10 ? 10 : pitch;
    int type = c->mode == TS_UBER ? c->job.ptype : TS_P_NORMAL;
    e->voice_rate = type == TS_P_HURRY ? 2 : type == TS_P_RELAXED ? -1 : p->age >= 65 ? -1 : 0;
}

void toast(TsCore *c, int icon, int sound, const char *title, const char *text) {
    event(c, TS_EV_TOAST, icon, sound, title, text);
}

void sound(TsCore *c, int which) {
    event(c, TS_EV_SOUND, 0, which, NULL, NULL);
}

/* a chat line of the passenger(s), over the minimap; at most every 15 s */
void chat(TsCore *c, int style, const char *text, int force) {
    if (!force && c->t < c->chat_cooldown) return;
    c->chat_cooldown = c->t + 15.0;
    /* on the bus, one of the people on board speaks (a different one each time) */
    int person = c->mode == TS_BUS ? random_below(c, PASSENGER_COUNT) : (c->job.passenger % PASSENGER_COUNT + PASSENGER_COUNT) % PASSENGER_COUNT;
    int before = c->event_count;
    char who[64];
    if (c->thinking) snprintf(who, sizeof(who), "%s %s", ts_passenger_name(person), T(c, "(denkt)", "(thinks)"));
    else snprintf(who, sizeof(who), "%s", ts_passenger_name(person));
    event(c, TS_EV_FEED, 0, 0, who, text);
    if (c->event_count == before) return; /* the queue was full */
    c->events[c->event_count - 1].style = style;
    if (!c->thinking) voice_of(c, person, &c->events[c->event_count - 1]); /* a thought is read, not heard */
}

int ts_speed_shown(const TsCore *c, int kmh) {
    if (!c->imperial) return kmh;
    int mph = (int)((float)kmh / 1.609344f / 5.0f + 0.5f) * 5;
    return mph < 5 ? 5 : mph;
}

const char *ts_speed_unit(const TsCore *c) {
    return c->imperial ? "mph" : "km/h";
}

/* ---- 0.4.1: which line of a kind comes next (Marvin: "the croissant line three times in three rides").
   Every line of a kind is said once before any comes back (the fewest uses first), and a line said comes
   back after TS_LINE_REPEAT_S at the earliest; only when every line of the kind was said in the last
   5 minutes, the one said longest ago comes. */
static unsigned line_base[L_KINDS + 1];
static int line_base_done;

static void make_line_slots(void) {
    if (line_base_done) return;
    unsigned at = 0;
    for (int k = 0; k < L_KINDS; ++k) line_base[k] = at, at += ts_line_sets[k].count;
    line_base[L_KINDS] = at;
    line_base_done = 1;
}

int ts_line_slot(int kind, int index) {
    make_line_slots();
    if (kind < 0 || kind >= L_KINDS || index < 0 || index >= (int)ts_line_sets[kind].count) return -1;
    unsigned slot = line_base[kind] + (unsigned)index;
    return slot < TS_LINE_SLOTS ? (int)slot : -1;
}

static int pick_line(TsCore *c, int kind) {
    if (kind < 0 || kind >= L_KINDS) return -1;
    int count = (int)ts_line_sets[kind].count;
    if (count <= 0) return -1;
    float now = (float)c->t + 1.0f;
    int fewest = 256, candidates = 0;
    for (int i = 0; i < count; ++i) { /* the fewest uses among the lines not said in the last 5 minutes */
        int s = ts_line_slot(kind, i);
        if (s < 0) return random_below(c, count);
        if (c->line_used_at[s] > 0.0f && now - c->line_used_at[s] < TS_LINE_REPEAT_S) continue;
        if (c->line_uses[s] < fewest) fewest = c->line_uses[s], candidates = 0;
        if (c->line_uses[s] == fewest) candidates++;
    }
    int pick = -1;
    if (candidates > 0) {
        int nth = random_below(c, candidates);
        for (int i = 0; i < count && pick < 0; ++i) {
            int s = ts_line_slot(kind, i);
            if (c->line_used_at[s] > 0.0f && now - c->line_used_at[s] < TS_LINE_REPEAT_S) continue;
            if (c->line_uses[s] == fewest && nth-- == 0) pick = i;
        }
    } else { /* all said in the last 5 minutes: the oldest */
        float oldest = now + 1.0f;
        for (int i = 0; i < count; ++i) {
            int s = ts_line_slot(kind, i);
            if (c->line_used_at[s] < oldest) oldest = c->line_used_at[s], pick = i;
        }
    }
    int s = ts_line_slot(kind, pick);
    c->line_used_at[s] = now;
    if (c->line_uses[s] < 255) c->line_uses[s]++;
    return pick;
}

/* the feed takes 64 characters a line: longer ones are split at spaces into several lines */
int ts_feed_split(const char *text, int max_chars, char out[][TS_TEXT], int max_parts) {
    int parts = 0;
    const char *p = text ? text : "";
    while (*p == ' ') ++p;
    while (*p && parts < max_parts) {
        /* the bytes of up to max_chars characters, cut at the last space if the rest doesn't fit */
        const char *end = p, *space = NULL;
        int chars = 0;
        while (*end && chars < max_chars) {
            if (*end == ' ') space = end;
            ++end;
            while ((*end & 0xC0) == 0x80) ++end; /* the rest of a UTF-8 letter */
            ++chars;
        }
        if (*end && *end != ' ' && space && space > p) end = space;
        size_t n = (size_t)(end - p);
        if (n >= TS_TEXT) n = TS_TEXT - 1;
        memcpy(out[parts], p, n);
        out[parts][n] = 0;
        ++parts;
        p = end;
        while (*p == ' ') ++p;
    }
    return parts;
}

/* Place names: the data has them as "Nizza · Place Garibaldi" or "Bei Nizza · Cap Ouest" (0.13: the spots have
   made-up French and Italian names, tools/place_names.py). In English only the German words change:
   "Nice · Place Garibaldi", "Near Nice · Cap Ouest". Made once, the first time they're asked for. */
#define TS_PLACES_MAX 512
static char place_en[TS_PLACES_MAX][64];
static int place_en_done;

static void english_place(const char *de, char *out, size_t n) {
    char tmp[64];
    const char *rest = de;
    size_t k = 0;
    if (!strncmp(rest, "Bei ", 4)) {
        k = (size_t)snprintf(tmp, sizeof(tmp), "Near ");
        rest += 4;
    }
    while (*rest && k + 1 < sizeof(tmp)) {
        if (!strncmp(rest, "Nizza", 5)) {
            k += (size_t)snprintf(tmp + k, sizeof(tmp) - k, "Nice");
            rest += 5;
        } else tmp[k++] = *rest++;
        if (k >= sizeof(tmp)) k = sizeof(tmp) - 1;
    }
    tmp[k] = 0;
    snprintf(out, n, "%s", tmp);
}

static void make_place_names(void) { /* from ts_init, before the mod's threads run */
    if (place_en_done) return;
    for (unsigned i = 0; i < ts_place_count && i < TS_PLACES_MAX; ++i) english_place(ts_places[i].name, place_en[i], sizeof(place_en[i]));
    place_en_done = 1;
}

const char *ts_place_name(const TsCore *c, int place) {
    if (place < 0 || place >= (int)ts_place_count) return "";
    if (c->german || place >= TS_PLACES_MAX) return ts_places[place].name;
    make_place_names();
    return place_en[place];
}

const char *ts_line_title(const TsCore *c, int line) {
    static const char *const en[] = {"Festival Ring", "Coast Line", "Mountain Express", "Nice City Bus", "Castelletto Shuttle", "Vineyard Line"};
    if (line < 0 || line >= (int)ts_line_count) return "";
    if (c->german || line >= (int)(sizeof(en) / sizeof(en[0]))) return ts_lines[line].title;
    return en[line];
}

int unit_kmh(const TsCore *c, int kmh) {
    return c->imperial ? (int)((float)ts_speed_shown(c, kmh) * 1.609344f + 0.5f) : kmh;
}

int ride_max_kmh(const TsCore *c) {
    if (c->mode == TS_UBER && c->floorit) return 400; /* 0.5.1: "floor it": no limit for the rest of the ride */
    return unit_kmh(c, c->mode == TS_BUS ? TS_BUS_MAX_KMH : c->job.max_kmh > 0 ? c->job.max_kmh : 120);
}

/* a line of this kind (lines.c), never the same one twice in a row; {car} and {max} filled in */
/* the remarks the "passenger complaints" switch silences: about the driving, being late, the car */
int ts_line_is_complaint(int kind) {
    switch (kind) {
    case L_BRAKE: case L_CORNER: case L_ACCEL: case L_SPEEDING: case L_CRASH: case L_HURRY_NAG: case L_LATE:
    case L_BUS_CRASH: case L_BUS_BRAKE: case L_BUS_CORNER: case L_BUS_SPEEDING: case L_BUS_LATE:
    case L_DISLIKES_CAR: case L_REG_LAST_BAD: case L_WISH_FAILED: case L_THOUGHT: case L_WISH_SLIP:
        return 1;
    default:
        return 0;
    }
}

/* 0.4.2: not everyone says something about the driving. Each person is quiet, now-and-then or outspoken; the hurried
   say it more often, the relaxed hardly ever. The rest keep it to themselves, and sometimes you see what they think. */
int ts_outspoken(const TsCore *c, int person) {
    if (c->mode == TS_UBER && c->job.ptype == TS_P_HURRY) return 2;
    if (c->mode == TS_UBER && c->job.ptype == TS_P_RELAXED) return 0;
    return (int)(((unsigned)person * 7u + 3u) % 3u);
}

void complain(TsCore *c, int kind) {
    int person = (c->job.passenger % PASSENGER_COUNT + PASSENGER_COUNT) % PASSENGER_COUNT;
    int o = c->mode == TS_BUS ? 1 : ts_outspoken(c, person);
    float speak = o == 2 ? 0.8f : o == 1 ? 0.4f : 0.08f;
    if (random_unit(c) < speak) {
        say(c, 2, kind, 0);
    } else if (random_unit(c) < 0.5f) {
        c->thinking = 1;
        say(c, 0, L_THOUGHT, 0);
        c->thinking = 0;
    }
}

void say(TsCore *c, int style, int kind, int force) {
    if (kind < 0 || kind >= L_KINDS || !ts_line_sets[kind].count) return;
    if (!force && c->t < c->chat_cooldown) return;
    if (c->complaints_off && ts_line_is_complaint(kind)) return;
    char text[TS_TEXT];
    line_text(c, kind, text, sizeof(text));
    if (text[0]) chat(c, style, text, force);
}

void line_text(TsCore *c, int kind, char *out, size_t size) {
    out[0] = 0;
    if (kind < 0 || kind >= L_KINDS || !ts_line_sets[kind].count) return;
    const TsLineSet *set = &ts_line_sets[kind];
    int pick = pick_line(c, kind);
    if (pick < 0) return;
    const char *line = c->german ? set->lines[pick].de : set->lines[pick].en;
    char text[TS_TEXT], number[16];
    size_t n = 0;
    snprintf(number, sizeof(number), "%d", ts_speed_shown(c, ride_max_kmh(c)));
    char ride_no[16];
    int p = c->job.passenger;
    snprintf(ride_no, sizeof(ride_no), "%d", (p >= 0 && p < TS_PEOPLE_MAX ? c->met[p] : 0) + 1);
    char lastcar[64] = "", limit[16];
    int lc = p >= 0 && p < TS_PEOPLE_MAX ? c->met_car[p] : 0;
    if (lc > 0 && lc <= (int)ts_car_count) {
        const TsCarInfo *car = &ts_cars[lc - 1];
        if (car->make[0]) snprintf(lastcar, sizeof(lastcar), "%s %s", car->make, car->model);
        else snprintf(lastcar, sizeof(lastcar), "%s", car->model);
    } else snprintf(lastcar, sizeof(lastcar), "%s", T(c, "Auto", "car"));
    snprintf(limit, sizeof(limit), "%d", ts_speed_shown(c, c->wish_limit));
    const char *place = c->wish_place >= 0 && c->wish_place < (int)ts_place_count ? ts_place_name(c, c->wish_place) : "";
    while (*line && n + 1 < sizeof(text)) {
        const char *fill = NULL;
        if (!strncmp(line, "{car}", 5)) fill = c->car_name[0] ? c->car_name : T(c, "Auto", "car"), line += 5;
        else if (!strncmp(line, "{max}", 5)) fill = number, line += 5;
        else if (!strncmp(line, "{n}", 3)) fill = ride_no, line += 3;
        else if (!strncmp(line, "{lastcar}", 9)) fill = lastcar, line += 9;
        else if (!strncmp(line, "{group}", 7)) fill = ts_group_name(c->car_group, c->german), line += 7;
        else if (!strncmp(line, "{limit}", 7)) fill = limit, line += 7;
        else if (!strncmp(line, "{place}", 7)) fill = place, line += 7;
        if (fill) {
            int speed = fill == number || fill == limit;
            while (*fill && n + 1 < sizeof(text)) text[n++] = *fill++;
            if (speed && c->imperial && !strncmp(line, " km/h", 5)) { /* the lines say km/h */
                for (const char *u = " mph"; *u && n + 1 < sizeof(text); ++u) text[n++] = *u;
                line += 5;
            }
            continue;
        }
        text[n++] = *line++;
    }
    text[n] = 0;
    snprintf(out, size, "%s", text);
}

/* where the car is, for what passengers talk about: the festival, a town, the open country (-1: in between) */
static int where_talk(const TsCore *c) {
    if (!c->car_ok) return -1;
    float best = 1e9f;
    int kind = -1;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        if (ts_places[i].kind != TS_PLACE_FESTIVAL && ts_places[i].kind != TS_PLACE_OUTPOST) continue;
        float d = ts_distance_xz(c->car.pos, ts_places[i].pos);
        if (d < best) best = d, kind = ts_places[i].kind;
    }
    if (best < 1200.0f) return kind == TS_PLACE_FESTIVAL ? L_FESTIVAL_TALK : L_TOWN_TALK; /* outposts are the towns */
    if (best > 2500.0f) return L_COUNTRY_TALK;
    return -1;
}

/* the passenger tells the next bit of their own story; a regular goes on where they stopped last time */
static int tell(TsCore *c) {
    int p = c->job.passenger;
    if (p < 0 || p >= PASSENGER_COUNT || c->story_said >= 3 || c->t < c->chat_cooldown) return 0;
    int at = (c->met[p] * 2 + c->story_said) % TS_PERSON_LINES;
    const TsLineText *l = &ts_people[p].story[at];
    c->story_said++;
    chat(c, 0, c->german ? l->de : l->en, 0);
    return 1;
}

/* ------------------------------------------------------------------ the driven car */

const TsCarInfo *ts_car_find(const char *media) {
    if (!media || !*media) return NULL;
    for (unsigned i = 0; i < ts_car_count; ++i) {
        const char *a = ts_cars[i].media, *b = media;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) ++a, ++b;
        if (!*a && !*b) return &ts_cars[i];
    }
    return NULL;
}

void ts_set_car(TsCore *c, const char *media) {
    if (!media) media = "";
    if (!strcmp(media, c->car_media) && (c->car_kind != TS_CAR_UNKNOWN || !*media)) return;
    snprintf(c->car_media, sizeof(c->car_media), "%s", media);
    const TsCarInfo *car = ts_car_find(media);
    c->car_index = car ? (int)(car - ts_cars) : -1;
    c->car_group = car ? car->group : -1; /* a car pack's car has no group: it counts as "normal" */
    c->car_open = car ? car->open_top : 0;
    if (car) {
        c->car_kind = car->kind;
        if (car->make[0]) snprintf(c->car_name, sizeof(c->car_name), "%s %s", car->make, car->model);
        else snprintf(c->car_name, sizeof(c->car_name), "%s", car->model);
    } else {
        c->car_kind = *media ? TS_CAR_NORMAL : TS_CAR_UNKNOWN; /* a car pack's car, or not read yet */
        snprintf(c->car_name, sizeof(c->car_name), "%s", media);
    }
}

int ts_bus_stop(const TsCore *c, int line, int i) {
    const TsLine *l = &ts_lines[line];
    int n = (int)l->stop_count;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return l->stops[c->line_rev ? n - 1 - i : i];
}

/* 0.5: which way a line will run: a ring the other way round every second time, a line from its nearer end */
int bus_direction(const TsCore *c, int line) {
    const TsLine *l = &ts_lines[line];
    int n = (int)l->stop_count;
    if (l->stops[0] == l->stops[n - 1]) return line < 8 ? c->line_done[line] % 2 : 0;
    if (!c->car_ok) return 0;
    float to_first = ts_distance_xz(c->car.pos, ts_places[l->stops[0]].pos), to_last = ts_distance_xz(c->car.pos, ts_places[l->stops[n - 1]].pos);
    return to_last + 200.0f < to_first;
}

void set_nav(TsCore *c, const float at[3]) {
    c->nav_on = 1;
    c->nav_target[0] = at[0], c->nav_target[1] = at[1], c->nav_target[2] = at[2];
    c->nav_serial++;
    c->gps_arrived = 0;
}

static void clear_nav(TsCore *c) {
    if (!c->nav_on) return;
    c->nav_on = 0;
    c->nav_serial++;
    c->gps_arrived = 0;
}

void format_km(const TsCore *c, char *out, size_t size, float metres) {
    if (c->imperial) { /* like the game in mph: feet up to a tenth of a mile, then miles */
        float miles = metres / 1609.344f;
        if (miles < 0.1f) snprintf(out, size, "%d ft", (int)(metres * 3.28084f / 10.0f + 0.5f) * 10);
        else snprintf(out, size, "%.1f mi", miles);
        return;
    }
    if (metres < 1000.0f) snprintf(out, size, "%d m", (int)(metres / 10.0f + 0.5f) * 10);
    else {
        snprintf(out, size, "%.1f km", metres / 1000.0f);
        if (c->german)
            for (char *p = out; *p; ++p)
                if (*p == '.') *p = ',';
    }
}

void format_time(char *out, size_t size, double seconds) {
    int s = (int)(seconds + 0.999);
    if (s < 0) s = -s;
    snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

/* ------------------------------------------------------------------ setup */

void ts_init(TsCore *c, uint32_t seed, int german) {
    make_place_names();
    make_line_slots();
    memset(c, 0, sizeof(*c));
    c->rng = seed ? seed : 0x12345678u;
    c->german = german;
    c->bus_capacity = 40;
    c->time_factor = 1.0f;
    c->sat = 80.0f;
    c->reputation = 50.0f;
    c->car_group = c->car_index = -1;
    c->demand = 1;
    c->hot[0].place = c->hot[1].place = -1; /* 0.6 */
    c->game_credits = -1; /* 0.13 */
    c->credit_buy_item = -1;
    c->local_hour = -1;
    c->cr_scale = 5;
    c->hot_rng = c->rng ^ 0x9E3779B9u;
    c->area = -1;
    c->ask = c->replied = -1; /* 0.5.1 */
    c->wish_place = -1;
    c->setting_pressed = -1;
    for (int i = 0; i < TS_SETTING_ROWS; ++i) c->setting_want[i] = -1; /* 0.10.4 */
    c->pick_row = -1;
    c->door = -1;
}

/* ------------------------------------------------------------------ the car and the passengers' comfort */

/* Comfort rules (per second of driving with passengers on board; since 0.4 more forgiving):
   - harsh acceleration  (forward > 4.5 m/s^2, about 0.46 g) ........ -1.4 / s
   - harsh braking       (backward > 6.5 m/s^2) .................. -2.0 / s, emergency (> 10) -4.0 / s
   - fast cornering      (sideways > 6.5 m/s^2) .................. -1.8 / s
   - speeding            (10 km/h over the passenger's limit) ...... -0.7 / s, 40 km/h over -2.5 / s
   - collision           (a jolt: speed changes > 4.5 m/s within one update) ... -8 at once
   - calm driving        (2 s without any of the above, moving) ... +0.6 / s
   A harsh moment counts only once it lasts 0.35 s; a passenger remarks on it at most every 35 s.
   Accelerations are the car's own velocity change, low-pass filtered over ~0.4 s, split into the
   car's forward and sideways directions. */
void ts_car_sample(TsCore *c, float dt, const TsCar *car) {
    if (!(dt > 0.0f) || dt > 0.25f) return;
    for (int i = 0; i < 3; ++i)
        if (!isfinite(car->pos[i]) || !isfinite(car->vel[i])) return;
    c->t += dt;
    c->car = *car;
    c->car_ok = 1;
    c->car_seen_t = c->t;
    c->speed = sqrtf(car->vel[0] * car->vel[0] + car->vel[1] * car->vel[1] + car->vel[2] * car->vel[2]);

    if (c->speed > 4.0f) { /* which way the forward vector really points: most driving goes forwards */
        float d = (car->vel[0] * car->fwd[0] + car->vel[1] * car->fwd[1] + car->vel[2] * car->fwd[2]) / c->speed;
        if (fabsf(d) > 0.7f) {
            float k = clampf(dt / 3.0f, 0.0f, 1.0f);
            c->fwd_sign += ((d > 0 ? 1.0f : -1.0f) - c->fwd_sign) * k;
        }
    }
    float fs = c->fwd_sign < -0.2f ? -1.0f : 1.0f;

    int jump = 0, jolt = 0;
    if (c->have_prev) {
        float moved = sqrtf((car->pos[0] - c->prev_pos[0]) * (car->pos[0] - c->prev_pos[0]) +
                            (car->pos[1] - c->prev_pos[1]) * (car->pos[1] - c->prev_pos[1]) +
                            (car->pos[2] - c->prev_pos[2]) * (car->pos[2] - c->prev_pos[2]));
        float expected = c->speed * dt;
        jump = moved > expected + 5.0f; /* rewind, reset to the road, fast travel: not driving */
        if (!jump) {
            float dv[3] = {car->vel[0] - c->prev_vel[0], car->vel[1] - c->prev_vel[1], car->vel[2] - c->prev_vel[2]};
            float along = fs * (dv[0] * car->fwd[0] + dv[1] * car->fwd[1] + dv[2] * car->fwd[2]) / dt;
            float side = (dv[0] * car->right[0] + dv[1] * car->right[1] + dv[2] * car->right[2]) / dt;
            float k = clampf(dt / 0.4f, 0.0f, 1.0f); /* smoothed over 0.4 s: bumps and kerbs don't count */
            c->acc_long += (along - c->acc_long) * k;
            c->acc_lat += (side - c->acc_lat) * k;
            float change = sqrtf(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]);
            jolt = change > 4.5f && dt <= 0.1f;
            float flat = sqrtf((car->pos[0] - c->prev_pos[0]) * (car->pos[0] - c->prev_pos[0]) +
                               (car->pos[2] - c->prev_pos[2]) * (car->pos[2] - c->prev_pos[2]));
            if (c->mode != TS_IDLE) {
                c->leg_m += flat;
                c->trip_driven_m += flat;
            }
            c->km_total += flat / 1000.0;
        } else {
            c->acc_long = c->acc_lat = 0.0f;
        }
    }
    c->prev_pos[0] = car->pos[0], c->prev_pos[1] = car->pos[1], c->prev_pos[2] = car->pos[2];
    c->prev_vel[0] = car->vel[0], c->prev_vel[1] = car->vel[1], c->prev_vel[2] = car->vel[2];
    c->have_prev = 1;

    int riding = (c->mode == TS_UBER && c->stage == TS_UBER_TO_DEST) || (c->mode == TS_BUS && c->onboard > 0);
    if (!riding || jump) return;

    if (jolt && c->t >= c->crash_cooldown) {
        c->crash_cooldown = c->t + 1.5;
        c->sat -= ts_has(c, TS_SHOP_DASHCAM) ? 4.0f : 8.0f; /* 0.13: the dashcam */
        c->n_crash++;
        c->calm_since = c->t;
        say(c, 3, c->mode == TS_BUS ? L_BUS_CRASH : L_CRASH, 1);
        sound(c, TS_SOUND_ERROR);
    }
    /* the passenger's kind changes the rules: a relaxed one minds jolts more, a car fan enjoys acceleration */
    int bus = c->mode == TS_BUS, type = bus ? TS_P_NORMAL : c->job.ptype;
    static const float type_soft[TS_P_KINDS] = {1.0f, 1.5f, 1.0f, 1.0f, 1.5f, 1.3f, 1.7f, 0.9f};
    float soft = type_soft[type < 0 || type >= TS_P_KINDS ? 0 : type] * (ts_has(c, TS_SHOP_COMFORT) ? 0.75f : 1.0f) *
                 (bus ? 1.0f : market_class_soft(c->job.ride_class)); /* 0.6: Black is strict, Saver easy-going */
    float limit = (float)ride_max_kmh(c) / 3.6f;
    float drop = 0.0f;
    int harsh = 0, fun = 0;
    /* 0.4: more forgiving (players: "they moan too often"): higher limits, less loss, 10 km/h leeway */
    if (c->acc_long > TS_LIM_GAS) {
        if (type == TS_P_FAN || (!bus && c->floorit)) fun = 1; /* 0.5.1: they asked for it */
        else drop += 1.4f * soft, harsh = 1;
    }
    if (c->acc_long < -12.0f) drop += 4.0f * soft, harsh = 2;
    else if (c->acc_long < -TS_LIM_BRAKE) drop += 2.0f * soft, harsh = 2;
    if (fabsf(c->acc_lat) > (type == TS_P_FAN ? TS_LIM_SIDE_FAN : TS_LIM_SIDE)) drop += 1.8f * soft, harsh = 3;
    if (c->speed > limit + 11.1f) drop += 2.5f, harsh = harsh ? harsh : 4;      /* 40 km/h over */
    else if (c->speed > limit + 4.2f) drop += 0.7f * soft, harsh = harsh ? harsh : 4; /* 10 km/h over */
    /* a harsh moment counts once it lasts 0.35 s (a short twitch of the wheel doesn't) */
    if (!harsh) c->harsh_since = c->t;
    else if (c->t - c->harsh_since < 0.6) harsh = 0, drop = 0.0f;
    if (harsh && !c->was_harsh) { /* count each harsh moment once */
        if (harsh == 1) c->n_accel++;
        else if (harsh == 2) c->n_brake++;
        else if (harsh == 3) c->n_corner++;
        else c->n_speeding++;
        if (c->t >= c->complain_at && c->t >= c->chat_cooldown) { /* a remark at most every 35 s */
            c->complain_at = c->t + 60.0;
            int kind = harsh == 2 ? (bus ? L_BUS_BRAKE : L_BRAKE) : harsh == 3 ? (bus ? L_BUS_CORNER : L_CORNER)
                     : harsh == 4 ? (bus ? L_BUS_SPEEDING : L_SPEEDING) : L_ACCEL;
            complain(c, kind);
        }
    }
    c->was_harsh = harsh != 0;
    if (fun) { /* a car fan loves the push */
        c->sat += 1.5f * dt;
        if (!c->fan_happy_said || c->t - c->calm_since > 20.0) {
            c->fan_happy_said = 1;
            say(c, 1, L_FAN_HAPPY, 0);
        }
    }
    if (harsh) {
        c->sat -= drop * dt;
        c->calm_since = c->t;
    } else if (c->speed > 3.0f && c->t - c->calm_since > 2.0) {
        c->sat += 0.6f * dt;
        if (type == TS_P_FAN && c->speed > limit * 0.6f) c->sat += 0.3f * dt; /* a fan likes it brisk */
        if (c->t - c->calm_since > 90.0 && c->sat > 70.0f) {
            say(c, 1, bus ? L_BUS_SMOOTH : L_SMOOTH, 0);
            c->calm_since = c->t - 3.0; /* not again for a while */
        }
    }
    c->sat = clampf(c->sat, 0.0f, 100.0f);
}

/* ------------------------------------------------------------------ offers */

float place_distance(int a, int b) {
    return ts_distance_xz(ts_places[a].pos, ts_places[b].pos);
}

static int place_recent(const TsCore *c, int place) {
    for (int k = 0; k < 24; ++k)
        if (c->recent_places[k] == place + 1) return 1;
    return 0;
}

static void remember_place(TsCore *c, int place) {
    if (place < 0) return;
    c->recent_places[c->recent_pos % 24] = (short)(place + 1);
    c->recent_pos = (c->recent_pos + 1) % 24;
}

/* the area of a place: its name up to " · " ("Nizza · Place Garibaldi" -> "Nizza") */
static int same_area(int a, int b) {
    const char *na = ts_places[a].name, *nb = ts_places[b].name;
    const char *da = strstr(na, " · "), *db = strstr(nb, " · ");
    size_t la = da ? (size_t)(da - na) : strlen(na), lb = db ? (size_t)(db - nb) : strlen(nb);
    return la == lb && !strncmp(na, nb, la);
}

/* 0.8: a real place (bus stop, parking spot, festival, outpost), not a race start on the land */
static int place_real(int place) { return ts_places[place].kind != TS_PLACE_SPOT; }

/* 0.4.2: more variety: places of the last rides are left out, and a destination in another area than the pickup
   (`not_this`) is preferred, when enough others are in range. 0.8: real places first (`real_only`); -2 when fewer
   than two fresh real places are in range */
static int pick_place_of(TsCore *c, const float from[3], float min_m, float max_m, int not_this, int real_only) {
    int candidates[512], n = 0, fresh[512], nf = 0, away[512], na = 0;
    for (unsigned i = 0; i < ts_place_count && n < 512; ++i) {
        if ((int)i == not_this || (real_only && !place_real((int)i))) continue;
        float d = ts_distance_xz(from, ts_places[i].pos);
        if (d >= min_m && d <= max_m) {
            candidates[n++] = (int)i;
            if (!place_recent(c, (int)i)) {
                fresh[nf++] = (int)i;
                if (not_this >= 0 && !same_area((int)i, not_this)) away[na++] = (int)i;
            }
        }
    }
    if (real_only && nf < 2) return -2;
    if (na >= 2 && random_unit(c) < 0.75f) return away[random_below(c, na)];
    if (nf >= 2) return fresh[random_below(c, nf)];
    if (!n) { /* nothing in range: the nearest one that is far enough */
        int best = -1;
        float best_d = 1e9f;
        for (unsigned i = 0; i < ts_place_count; ++i) {
            float d = ts_distance_xz(from, ts_places[i].pos);
            if ((int)i != not_this && d >= min_m * 0.5f && d < best_d) best = (int)i, best_d = d;
        }
        return best;
    }
    return candidates[random_below(c, n)];
}

/* 0.8: people get in and out where people are: real places nine times in ten, a race start on the land only where
   no real place is in range (or now and then, for the countryside) */
int pick_place(TsCore *c, const float from[3], float min_m, float max_m, int not_this) {
    if (random_unit(c) < 0.9f) {
        int real = pick_place_of(c, from, min_m, max_m, not_this, 1);
        if (real >= 0) return real;
    }
    return pick_place_of(c, from, min_m, max_m, not_this, 0);
}

/* by passenger kind: pay, time limit, speed limit */
static const float type_pay[TS_P_KINDS] = {1.0f, 0.9f, 1.3f, 1.15f, 2.2f, 1.5f, 1.8f, 1.25f};
static const float type_time[TS_P_KINDS] = {1.0f, 1.3f, 0.75f, 0.9f, 0.85f, 0.9f, 1.3f, 1.1f};

static int pay_scale = 5; /* 0.13: c->cr_scale, taken over in ts_step: the pay is in the game's credits */
static int uber_pay_typed(float trip_m, float sat, int type, float factor) {
    if (!(factor > 0.0f)) factor = 1.0f;
    float base = (150.0f + 0.22f * trip_m) * type_pay[type < 0 || type >= TS_P_KINDS ? 0 : type] * factor; /* 220 CR per km plus a start fee */
    float pay = base * (0.6f + 0.6f * sat / 100.0f);
    if (sat >= 90.0f) pay *= 1.15f; /* tip */
    return (int)(pay * (float)pay_scale / 10.0f + 0.5f) * 10; /* 0.13: CR in steps of 10 */
}



int uber_pay(float trip_m, float sat, int type, float factor) { return uber_pay_typed(trip_m, sat, type, factor); }

/* the area around the car: how many places are near (town, festival) and the nearest town's name */
static void read_area(TsCore *c) {
    int near = 0, best = -1;
    float best_d = 1e9f;
    for (unsigned i = 0; i < ts_place_count; ++i) {
        float d = ts_distance_xz(c->car.pos, ts_places[i].pos);
        if (d < 1500.0f) near++;
        if (ts_places[i].kind <= TS_PLACE_OUTPOST && d < best_d) best = (int)i, best_d = d;
    }
    c->demand = near >= 12 ? 2 : near <= 3 ? 0 : 1;
    c->area = best_d < 2500.0f ? best : -1;
}

/* a special ride for the driven car's group, if the company's reputation is good enough (else -1) */
static int special_type(TsCore *c, int slot) {
    if (c->extras_off) return -1;
    int g = c->car_group, premium = ts_has(c, TS_SHOP_PREMIUM);
    float rep = c->reputation + (premium ? 10.0f : 0.0f), more = premium ? 1.5f : 1.0f, roll = random_unit(c);
    float vip_more = c->tier >= TS_TIER_GOLD ? 1.5f : 1.0f; /* 0.6: the Gold tier brings more VIPs */
    if (rep >= 60.0f && (g == TS_GROUP_SALOON || g == TS_GROUP_SUPER || g == TS_GROUP_CLASSIC || g == TS_GROUP_SPORTS) && roll < 0.22f * more * vip_more)
        return TS_P_VIP;
    roll = random_unit(c);
    if (rep >= 35.0f && ((g == TS_GROUP_SALOON && roll < 0.45f * more) || (g == TS_GROUP_SUPER && roll < 0.12f * more))) return TS_P_BUSINESS;
    roll = random_unit(c);
    if (rep >= 45.0f && (g == TS_GROUP_CLASSIC || g == TS_GROUP_SALOON) && roll < 0.18f * more) return TS_P_WEDDING;
    roll = random_unit(c);
    if (g == TS_GROUP_OFFROAD && slot == TS_PHONE_OFFERS - 1 && roll < 0.45f * more) return TS_P_TOURIST;
    return -1;
}

static int type_max_kmh(const TsCore *c, int type) {
    switch (type) {
    case TS_P_RELAXED: return 90;
    case TS_P_HURRY: return 140;
    case TS_P_FAN: return c->car_kind == TS_CAR_DREAM ? 180 : 160;
    case TS_P_VIP: return 130;
    case TS_P_BUSINESS: return 120;
    case TS_P_WEDDING: return 80;
    case TS_P_TOURIST: return 100;
    default: return 120;
    }
}

/* 0.8: keeps only the real places of a candidate list, if there are any */
static int real_first(int *cand, int n) {
    int r = 0;
    for (int i = 0; i < n; ++i)
        if (place_real(cand[i])) cand[r++] = cand[i];
    return r ? r : n;
}

/* 0.6: a shared ride: a second passenger who gets in on the way (between a fifth and three fifths of the way, at
   most 900 m off the line) and gets out after the first, 0.5 to 2.5 km on. Both pay 20 % less than alone. */
static int make_pool(TsCore *c, TsOffer *o, int taken_count) {
    const float *a = ts_places[o->pickup].pos, *b = ts_places[o->dest].pos;
    float abx = b[0] - a[0], abz = b[2] - a[2], len2 = abx * abx + abz * abz, len = sqrtf(len2);
    if (len < 1500.0f) return 0;
    int cand[256], n = 0;
    for (unsigned i = 0; i < ts_place_count && n < 256; ++i) {
        if ((int)i == o->pickup || (int)i == o->dest) continue;
        const float *p = ts_places[i].pos;
        float apx = p[0] - a[0], apz = p[2] - a[2];
        float t = (apx * abx + apz * abz) / len2, side = fabsf(apx * abz - apz * abx) / len;
        if (t >= 0.2f && t <= 0.6f && side < 900.0f && ts_distance_xz(a, p) >= 400.0f) cand[n++] = (int)i;
    }
    n = real_first(cand, n);
    if (!n) return 0;
    int pp = cand[random_below(c, n)];
    n = 0;
    float ab = ts_distance_xz(a, b);
    for (unsigned i = 0; i < ts_place_count && n < 256; ++i) {
        if ((int)i == o->pickup || (int)i == o->dest || (int)i == pp) continue;
        float d = ts_distance_xz(b, ts_places[i].pos);
        if (d >= 500.0f && d <= 2500.0f && ts_distance_xz(a, ts_places[i].pos) > ab * 0.8f) cand[n++] = (int)i;
    }
    n = real_first(cand, n);
    if (!n) return 0;
    int pd = cand[random_below(c, n)];
    int who = -1;
    for (int tries = 0; tries < 30 && who < 0; ++tries) {
        int k = random_below(c, PASSENGER_COUNT), taken = k == o->passenger || k == c->job.passenger;
        for (int j = 0; j < taken_count; ++j) taken |= c->offers[j].passenger == k || (c->offers[j].pool && c->offers[j].pool_passenger == k);
        if (!taken) who = k;
    }
    if (who < 0) return 0;
    o->pool = 1;
    o->pool_pickup = pp, o->pool_dest = pd, o->pool_passenger = who;
    o->pool_ptype = random_unit(c) < 0.5f ? TS_P_NORMAL : TS_P_RELAXED;
    o->pool_trip_m = place_distance(pp, pd) * TS_ROAD_FACTOR;
    o->party = 1;
    if (o->ptype != TS_P_NORMAL && o->ptype != TS_P_RELAXED && o->ptype != TS_P_HURRY) o->ptype = TS_P_NORMAL;
    o->max_kmh = type_max_kmh(c, o->ptype) < type_max_kmh(c, o->pool_ptype) ? type_max_kmh(c, o->ptype) : type_max_kmh(c, o->pool_ptype);
    /* the first's time covers the detour and the stop */
    float route_m = (place_distance(o->pickup, pp) + place_distance(pp, o->dest)) * TS_ROAD_FACTOR;
    o->limit_s = (route_m / TS_UBER_AVG_SPEED + 75.0f) * c->time_factor * (ts_has(c, TS_SHOP_DISPATCH) ? 1.15f : 1.0f) *
                 market_class_time(o->ride_class);
    o->limit_s = (float)((int)(o->limit_s / 15.0f + 0.5f) * 15);
    o->pay_factor *= 0.8f;
    o->pay = uber_pay_typed(o->trip_m, 80.0f, o->ptype, o->pay_factor) + uber_pay_typed(o->pool_trip_m, 80.0f, o->pool_ptype, o->pay_factor);
    return 1;
}

static void make_offers(TsCore *c) {
    c->offer_count = 0;
    if (!c->car_ok || !c->shift.on) return; /* 0.13: rides only after GO, like a ride app (Marvin) */
    read_area(c);
    /* a bad name, or the countryside: fewer requests */
    int ads = ts_has(c, TS_SHOP_ADS);
    int count = !ads && ((c->reputation < 20.0f && !c->extras_off) || c->demand == 0) ? 2 : TS_PHONE_OFFERS;
    float area_pay = (c->demand == 2 ? 1.1f : c->demand == 0 ? 1.15f : 1.0f) * (ads ? 1.05f : 1.0f);
    int special_done = 0;
    for (int i = 0; i < count; ++i) {
        TsOffer *o = &c->offers[c->offer_count];
        memset(o, 0, sizeof(*o));
        o->pickup = -1;
        if (i == 0) o->pickup = market_hot_pickup(c); /* 0.6: near a hotspot the first request comes from there */
        if (o->pickup < 0) o->pickup = pick_place(c, c->car.pos, 200.0f, c->demand == 0 ? 3000.0f : 2200.0f, -1);
        if (o->pickup < 0) break;
        int slot = count == 2 ? i + 1 : i; /* with two requests: the medium and the long one */
        float min_trip = slot == 0 ? 1200.0f : slot == 1 ? 2500.0f : 4000.0f; /* short, medium, long */
        float max_trip = slot == 0 ? 2600.0f : slot == 1 ? 4500.0f : 8000.0f;
        if (slot == 2 && random_unit(c) < 0.3f) min_trip = 7000.0f, max_trip = 14000.0f; /* now and then a trip across the map */
        if (slot == 0 && random_unit(c) < 0.25f) min_trip = 600.0f, max_trip = 1400.0f;  /* or a quick hop */
        o->dest = pick_place(c, ts_places[o->pickup].pos, min_trip, max_trip, o->pickup);
        if (o->dest < 0) break;
        o->pickup_m = ts_distance_xz(c->car.pos, ts_places[o->pickup].pos);
        o->trip_m = place_distance(o->pickup, o->dest) * TS_ROAD_FACTOR;
        /* who it is: a car fan only for a sporty car or a dream car; in a busy town more people in a hurry */
        int special = special_done ? -1 : special_type(c, slot);
        if (special >= 0) {
            o->ptype = special;
            special_done = 1;
        } else {
            float roll = random_unit(c);
            float fan = c->car_kind == TS_CAR_DREAM ? 0.45f : c->car_kind == TS_CAR_SPORTY ? 0.25f : 0.0f;
            float hurry = c->demand == 2 ? 0.28f : 0.2f;
            o->ptype = roll < fan ? TS_P_FAN : roll < fan + 0.25f ? TS_P_RELAXED : roll < fan + 0.25f + hurry ? TS_P_HURRY : TS_P_NORMAL;
        }
        o->max_kmh = type_max_kmh(c, o->ptype);
        /* 0.6: the ride's class by the car, the group's size, a hotspot's surcharge */
        o->ride_class = market_pick_class(c, o->ptype);
        o->party = o->ride_class == TS_CLASS_XL ? 2 + random_below(c, 3) : o->ptype == TS_P_WEDDING ? 2 : 1;
        o->surge = market_surge(c, o->pickup);
        o->limit_s = (o->trip_m / TS_UBER_AVG_SPEED + 45.0f) * c->time_factor * type_time[o->ptype] *
                     (ts_has(c, TS_SHOP_DISPATCH) ? 1.15f : 1.0f) * market_class_time(o->ride_class);
        o->limit_s = (float)((int)(o->limit_s / 15.0f + 0.5f) * 15);
        float extra = market_class_pay(o->ride_class) * o->surge; /* class and surcharge together at most x2 */
        o->pay_factor = area_pay * career_pay_factor(c) * (extra > 2.0f ? 2.0f : extra);
        o->pay = uber_pay_typed(o->trip_m, 80.0f, o->ptype, o->pay_factor);
        /* someone who isn't already asking, and not the one just driven */
        for (int tries = 0; tries < 20; ++tries) {
            o->passenger = random_below(c, PASSENGER_COUNT);
            int taken = o->passenger == c->job.passenger;
            for (int k = 0; k < i; ++k) taken |= c->offers[k].passenger == o->passenger;
            if (!taken) break;
        }
        /* 0.5: now and then a regular asks for you by name (a normal ride only): 15 % more. 0.8: a bit more often */
        int regulars = 0;
        for (int k = 0; k < PASSENGER_COUNT; ++k) regulars += c->met[k] >= 2;
        int asked = 0;
        for (int j = 0; j < i; ++j) asked |= c->offers[j].regular; /* one regular per list at most */
        if (!c->extras_off && regulars && !asked && o->ptype < TS_P_VIP && random_unit(c) < (ts_has(c, TS_SHOP_REGULARS) ? 0.4f : 0.25f)) {
            int pick = -1, seen = 0;
            for (int k = 0; k < PASSENGER_COUNT; ++k) {
                if (c->met[k] < 2 || k == c->job.passenger) continue;
                int taken = 0;
                for (int j = 0; j < i; ++j) taken |= c->offers[j].passenger == k;
                if (!taken && random_below(c, ++seen) == 0) pick = k;
            }
            if (pick >= 0) {
                o->passenger = pick;
                o->regular = 1;
                o->pay_factor *= 1.15f;
                o->pay = uber_pay_typed(o->trip_m, 80.0f, o->ptype, o->pay_factor);
            }
        }
        /* 0.6: now and then the medium request is a shared ride (Saver and Comfort, ordinary passengers) */
        if (!c->extras_off && slot == 1 && !o->regular && (o->ride_class == TS_CLASS_SAVER || o->ride_class == TS_CLASS_COMFORT) &&
            o->ptype <= TS_P_HURRY && random_unit(c) < 0.3f)
            make_pool(c, o, i);
        c->offer_count++;
    }
}

void ts_refresh_offers(TsCore *c) {
    if (c->mode == TS_IDLE) make_offers(c);
}

/* 0.13: GO: the shift starts; the first request comes in a few seconds, then one after the other (service.c) */
void ts_go_online(TsCore *c) {
    if (c->mode != TS_IDLE || c->shift.on) return;
    ts_shift_toggle(c);
    c->offer_count = 0, c->req_until = 0.0, c->hail_at = 0.0;
    c->phone_sel = 0;
}

void ts_decline(TsCore *c) {
    if (c->mode != TS_IDLE || !c->offer_count) return;
    c->offer_count = 0, c->req_until = 0.0;
    c->phone_sel = 0;
    service_next_request(c, 0);
}

void ts_renew_nav(TsCore *c) {
    if (c->nav_on) c->nav_serial++;
}

/* ------------------------------------------------------------------ phone */

/* what can be picked on a tab: offers, bus lines, nothing on the account */
static int tab_items(const TsCore *c, int tab) {
    return tab == TS_TAB_RIDES ? (c->offer_count ? 2 : 1) : /* 0.13: GO, or the request: accept, decline */ tab == TS_TAB_BUS ? (int)ts_line_count : tab == TS_TAB_SHOP ? TS_SHOP_ITEMS
         : tab == TS_TAB_SETTINGS ? c->setting_count : 1; /* the account: the shift; the map: its zoom */
}

void ts_phone_toggle(TsCore *c) {
    c->phone_open = !c->phone_open;
    c->cancel_armed = 0;
    c->map_view = 0, c->map_zoom = 0, c->job_tab = 0;
    c->pick_row = -1;
    if (c->phone_open) {
        /* 0.5: a push request's offers stay for a minute (what the pop-up said is what the phone shows) */
        /* 0.8: only offers a push request brought (before: any offers while a shift ran, so the phone kept showing
           the same ones, even after a cancelled ride) */
        if (c->mode == TS_IDLE && !c->shift.on) c->offer_count = 0; /* 0.13: offline: GO first */
        else if (c->mode != TS_IDLE) ts_renew_nav(c); /* opening the phone during a job sets the GPS again */
        c->phone_sel = 0;
        c->phone_tab = TS_TAB_RIDES;
        if (c->mode == TS_IDLE && !c->car_ok)
            toast(c, TS_ICON_NONE, TS_SOUND_ERROR, "Horizon Transit",
                  T(c, "Kein Auto erkannt: erst losfahren.", "No car found yet: drive first."));
    }
}

/* down: the next entry, past the last one on to the next tab (so a pad's one button reaches everything) */
void ts_phone_next(TsCore *c) {
    if (!c->phone_open || c->mode != TS_IDLE) return;
    if (c->map_view) { /* 0.7: the preview's two buttons */
        c->map_btn = !c->map_btn;
        return;
    }
    if (c->pick_row >= 0) { /* 0.13: the list of values */
        int n = c->setting_choices[c->pick_row];
        c->pick_sel = c->pick_sel + 1 < n ? c->pick_sel + 1 : 0;
        return;
    }
    /* 0.13: down past the last entry starts at the top again (Marvin: not the next tab) */
    int n = tab_items(c, c->phone_tab);
    c->phone_sel = c->phone_sel + 1 < n ? c->phone_sel + 1 : 0;
}

void ts_phone_prev(TsCore *c) {
    if (!c->phone_open || c->mode != TS_IDLE) return;
    if (c->map_view) {
        c->map_btn = !c->map_btn;
        return;
    }
    if (c->pick_row >= 0) {
        int n = c->setting_choices[c->pick_row];
        c->pick_sel = c->pick_sel > 0 ? c->pick_sel - 1 : (n > 0 ? n - 1 : 0);
        return;
    }
    int n = tab_items(c, c->phone_tab); /* 0.13: up past the top goes to the last entry of the same tab */
    c->phone_sel = c->phone_sel > 0 ? c->phone_sel - 1 : (n > 0 ? n - 1 : 0);
}

void ts_phone_tab(TsCore *c, int direction) {
    if (!c->phone_open) return;
    if (c->mode != TS_IDLE) { /* 0.7: the job and its map */
        c->job_tab = !c->job_tab;
        c->map_zoom = 0;
        c->cancel_armed = 0;
        return;
    }
    if (c->map_view) { /* the preview: the route / everything */
        c->map_zoom = direction < 0;
        return;
    }
    c->pick_row = -1;
    c->phone_tab = (c->phone_tab + (direction < 0 ? TS_TABS - 1 : 1)) % TS_TABS;
    c->phone_sel = 0;
    c->map_zoom = 0;
}

void ts_phone_back(TsCore *c) {
    if (c->phone_open && c->pick_row >= 0) { /* 0.13: closes the list of values */
        c->pick_row = -1;
        return;
    }
    if (c->phone_open && c->mode == TS_IDLE && c->map_view) {
        c->map_view = 0;
        return;
    }
    if (c->phone_open && c->mode != TS_IDLE && c->job_tab) {
        c->job_tab = 0;
        return;
    }
    ts_phone_toggle(c);
}

static void plan_stop(TsCore *c);

static void start_uber(TsCore *c, const TsOffer *o) {
    service_job_starts(c);
    remember_place(c, o->pickup);
    remember_place(c, o->dest);
    c->mode = TS_UBER;
    c->job = *o;
    c->stage = TS_UBER_TO_PICKUP;
    c->stage_t = c->leg_t = c->trip_t = c->t;
    c->door = -1; /* 0.4.3: picked when the passenger gets in */
    c->door_step = 0;
    /* the passenger waits this long before getting impatient */
    c->deadline = c->t + (o->pickup_m * TS_ROAD_FACTOR / TS_UBER_AVG_SPEED + 60.0f) * c->time_factor;
    c->sat = 80.0f;
    c->leg_m = c->trip_driven_m = 0.0f;
    c->n_accel = c->n_brake = c->n_corner = c->n_crash = c->n_speeding = 0;
    c->calm_since = c->t;
    c->late_noted = 0;
    c->fan_happy_said = 0;
    c->slow_since = c->t;
    c->memory_said = 0;
    wish_ride_start(c);
    talk_ride_start(c);
    c->pool_stage = 0, c->pool_stop_t = 0.0, c->pool_reply_at = 0.0, c->pool_paid = c->pool_fare = c->pool_tip = 0;
    c->pool_first = o->passenger;
    set_nav(c, ts_places[o->pickup].pos);
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%s: %s", ts_passenger_name(o->passenger), ts_place_name(c, o->pickup));
    toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Fahrt angenommen", "Ride accepted"), text);
}

static void start_bus(TsCore *c, int line) {
    service_job_starts(c);
    c->line_rev = bus_direction(c, line);
    c->mode = TS_BUS;
    c->line = line;
    c->stop_i = 0;
    c->onboard = c->delivered = c->board_total = 0;
    c->bus_pay = 0;
    c->bus_late_s = 0.0f;
    c->stage = TS_BUS_TO_STOP;
    c->stage_t = c->leg_t = c->trip_t = c->t;
    c->sat = ts_shop_start_sat(c);
    c->leg_m = c->trip_driven_m = 0.0f;
    c->n_accel = c->n_brake = c->n_corner = c->n_crash = c->n_speeding = 0;
    c->calm_since = c->t;
    const TsPlace *first = &ts_places[ts_bus_stop(c, line, 0)];
    float away = c->car_ok ? ts_distance_xz(c->car.pos, first->pos) : 0.0f;
    c->deadline = 0.0; /* no time limit on the way to the first stop */
    tt_start(c);
    plan_stop(c);
    c->stop_request = 1;
    set_nav(c, first->pos);
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%s %s · %s → %s", T(c, "Linie", "Line"), ts_lines[line].number, ts_place_name(c, ts_bus_stop(c, line, 0)),
             ts_place_name(c, ts_bus_stop(c, line, (int)ts_lines[line].stop_count - 1)));
    toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Buslinie gestartet", "Bus line started"), text);
    if (away > TS_ARRIVE_RADIUS) {
        char d[24];
        format_km(c, d, sizeof(d), away);
        snprintf(text, sizeof(text), "%s (%s)", T(c, "Fahre zur ersten Haltestelle", "Drive to the first stop"), d);
        event(c, TS_EV_FEED, 0, 0, T(c, "Leitstelle", "Dispatch"), text);
    }
}

void ts_accept_row(TsCore *c, int row) {
    if (c->mode != TS_IDLE) return;
    c->map_view = 0;
    if (row >= 0 && row < c->offer_count) start_uber(c, &c->offers[row]);
    else if (row >= TS_PHONE_OFFERS && row - TS_PHONE_OFFERS < (int)ts_line_count) {
        if (c->bus_needs_bus && c->car_kind != TS_CAR_BUS) {
            toast(c, TS_ICON_NONE, TS_SOUND_ERROR, T(c, "Dafür brauchst du den Bus", "You need the bus for this"),
                  c->car_kind == TS_CAR_UNKNOWN ? T(c, "Dein Auto wurde noch nicht erkannt.", "Your car isn't recognised yet.")
                                                : T(c, "Buslinien gehen nur im Bus (PG Bus).", "Bus lines only work in the bus (PG Bus)."));
            return;
        }
        start_bus(c, row - TS_PHONE_OFFERS);
    } else return;
    c->phone_open = 0;
}

static void finish_stats(TsCore *c, int stars) {
    c->rating_sum += stars;
    c->rating_count++;
    event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
}

static void end_job(TsCore *c) {
    ts_door_abort(c); /* 0.4.3: a door still open closes */
    c->mode = TS_IDLE;
    c->stage = 0;
    c->phone_open = 0;
    c->cancel_armed = 0;
    c->job_tab = 0, c->map_zoom = 0, c->map_view = 0;
    clear_nav(c);
}

void ts_cancel(TsCore *c) {
    if (c->mode == TS_IDLE) return;
    c->streak = 0;
    if (c->mode == TS_UBER) career_cancel(c, c->stage >= TS_UBER_TO_DEST);
    else if (c->onboard > 0) career_cancel(c, 1);
    if (c->mode == TS_UBER && c->stage >= TS_UBER_TO_DEST) {
        finish_stats(c, 1); /* the passenger was in the car: one star */
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, T(c, "Fahrt abgebrochen", "Ride cancelled"),
              T(c, "Keine Bezahlung · Bewertung 1 Stern", "No pay · rated 1 star"));
    } else if (c->mode == TS_BUS && c->onboard > 0) {
        c->wallet += c->bus_pay;
        finish_stats(c, 1);
        char text[TS_TEXT];
        snprintf(text, sizeof(text), "%s %s · %s", T(c, "Bisher verdient:", "Earned so far:"), ts_cr(c->german, c->bus_pay),
                 T(c, "1 Stern", "1 star"));
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, T(c, "Linie abgebrochen", "Line cancelled"), text);
    } else {
        if (c->mode == TS_BUS && c->bus_pay > 0) {
            c->wallet += c->bus_pay;
            event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
        }
        toast(c, TS_ICON_NONE, 0, T(c, "Auftrag abgebrochen", "Job cancelled"),
              c->mode == TS_UBER ? T(c, "Ohne Bezahlung · Ruf -1", "No pay · reputation -1") : T(c, "Ohne Abzug", "No penalty"));
    }
    end_job(c);
}

void ts_phone_accept(TsCore *c) {
    if (c->mode != TS_IDLE) { /* during a job the button cancels: press twice */
        if (!c->phone_open) return;
        if (c->job_tab) { /* 0.7: on the job's map it zooms (0.9.1: the route, everything, navi) */
            c->map_zoom = (c->map_zoom + 1) % 3;
            return;
        }
        if (!c->cancel_armed) {
            c->cancel_armed = 1;
            toast(c, TS_ICON_NONE, 0, T(c, "Auftrag abbrechen?", "Cancel the job?"),
                  T(c, "Zum Bestätigen nochmal drücken", "Press again to confirm"));
            return;
        }
        ts_cancel(c);
        return;
    }
    if (!c->phone_open) return;
    if (c->map_view) { /* 0.7: the preview's buttons: take it, back */
        int row = c->phone_tab == TS_TAB_RIDES ? c->phone_sel : TS_PHONE_OFFERS + c->phone_sel;
        if (c->map_btn) c->map_view = 0;
        else if (c->phone_tab != TS_TAB_RIDES || (c->offer_count && c->phone_sel == 0)) ts_accept_row(c, row);
        else c->map_view = 0;
        return;
    }
    if (c->map_preview && ((c->phone_tab == TS_TAB_RIDES && c->offer_count && c->phone_sel == 0) || c->phone_tab == TS_TAB_BUS)) {
        c->map_view = 1, c->map_btn = 0, c->map_zoom = 0;
        return;
    }
    if (c->phone_tab == TS_TAB_MAP) {
        c->map_zoom = (c->map_zoom + 1) % 4;
        return;
    }
    if (c->phone_tab == TS_TAB_RIDES && !c->shift.on) ts_go_online(c);
    else if (c->phone_tab == TS_TAB_RIDES && c->offer_count) {
        if (c->phone_sel == 0) ts_accept_row(c, 0);
        else ts_decline(c);
    }
    else if (c->phone_tab == TS_TAB_BUS) ts_accept_row(c, TS_PHONE_OFFERS + c->phone_sel);
    else if (c->phone_tab == TS_TAB_ACCOUNT) {
        if (c->shift.on) ts_shift_toggle(c);
        else ts_go_online(c);
    }
    else if (c->phone_tab == TS_TAB_SHOP) shop_buy(c, c->phone_sel);
    else if (c->phone_tab == TS_TAB_SETTINGS && c->pick_row >= 0) { /* 0.13: the value picked in the list */
        if (c->pick_sel != c->setting_index[c->pick_row]) c->setting_want[c->pick_row] = c->pick_sel;
        c->pick_row = -1;
    }
    else if (c->phone_tab == TS_TAB_SETTINGS && c->phone_sel < c->setting_count) {
        int r = c->phone_sel;
        if (r < TS_SETTING_ROWS && c->setting_choices[r] > 2) c->pick_row = r, c->pick_sel = c->setting_index[r]; /* 0.13: a list, not tapping through */
        else c->setting_pressed = r;
    }
}

/* ------------------------------------------------------------------ the jobs */

int arrived_at(const TsCore *c, const float at[3]) {
    if (!c->car_ok) return 0;
    float d = ts_distance_xz(c->car.pos, at);
    return d <= TS_ARRIVE_RADIUS || (c->gps_arrived && d <= TS_ARRIVE_RADIUS_GPS);
}

/* the passenger in c->job gets out: pay, rate, remember. 0.6: also the first of a shared ride (pool) halfway */
static int uber_settle(TsCore *c, int *stars_out, int *base_out, int *bonus_out, int *late_out) {
    double took = c->t - c->leg_t;
    double late = took - c->job.limit_s;
    if (late <= 0.0) c->sat += 8.0f;
    else c->sat -= clampf((float)late / 6.0f, 0.0f, 25.0f);
    c->sat = clampf(c->sat, 0.0f, 100.0f);
    int stars = ts_stars(c->sat);
    int pay = uber_pay_typed(c->job.trip_m, c->sat, c->job.ptype, c->job.pay_factor);
    int base = uber_pay_typed(c->job.trip_m, 80.0f, c->job.ptype, c->job.pay_factor);
    if (ts_has(c, TS_SHOP_TIPS) && pay > base) pay = base + (int)((float)(pay - base) * 1.3f); /* 0.10.1: the tip box */
    {   /* 0.13: the surcharges bought in the shop */
        float f = ts_shop_pay_factor(c, 0, c->job.trip_m, late <= 0.0);
        pay = (int)((float)pay * f / 5.0f + 0.5f) * 5;
        base = (int)((float)base * f / 5.0f + 0.5f) * 5;
    }
    c->wallet += pay;
    c->jobs_done++;
    c->passengers_total++;
    if (late <= 0.0) c->on_time_total++;
    else c->late_total++;
    int p = c->job.passenger;
    if (p >= 0 && p < TS_PEOPLE_MAX) {
        if (c->met[p] < 250) c->met[p]++;
        c->met_car[p] = (unsigned short)(c->car_index >= 0 ? c->car_index + 1 : 0);
        c->met_stars[p] = (unsigned char)stars;
    }
    float rep = career_job_done(c, stars, pay, base, c->job.ptype >= TS_P_VIP, c->trip_driven_m, -1);
    int streak_bonus = service_streak(c, stars, pay);
    service_log(c, 0, c->job.passenger, c->job.dest, pay + streak_bonus, stars);
    service_book(c, 0, c->job.passenger, c->job.pickup, c->job.dest, pay + streak_bonus, pay - base, stars, late > 0.0);
    finish_stats(c, stars);

    char title[TS_TEXT], text[TS_TEXT], km[24], time[16];
    format_km(c, km, sizeof(km), c->trip_driven_m);
    format_time(time, sizeof(time), took);
    snprintf(title, sizeof(title), "+%s · %d/5 %s", ts_cr(c->german, pay), stars, T(c, "Sterne", "stars"));
    snprintf(text, sizeof(text), "%s · %s · %s %d%%%s", km, time, T(c, "Zufrieden", "Satisfied"), (int)(c->sat + 0.5f),
             late > 0.0 ? T(c, " · zu spät", " · late") : T(c, " · pünktlich", " · on time"));
    if (c->cards_on) sound(c, TS_SOUND_PAYOUT); /* 0.6: the card after the ride says it all */
    else toast(c, TS_ICON_PAYOUT, TS_SOUND_PAYOUT, title, text);
    if (pay != base && !c->cards_on) {
        snprintf(text, sizeof(text), "%s %+d CR (%s)", T(c, "Bonus/Abzug:", "Bonus/deduction:"), pay - base,
                 pay > base ? T(c, "zufriedener Fahrgast", "happy passenger") : T(c, "unzufrieden", "unhappy"));
        event(c, TS_EV_FEED, 0, 0, "Horizon Transit", text);
    }
    if (rep >= 0.05f || rep <= -0.05f) {
        snprintf(text, sizeof(text), "%s %+.1f · %d (%s)", T(c, "Ruf", "Reputation"), (double)rep, (int)(c->reputation + 0.5f), ts_rep_name(c));
        event(c, TS_EV_FEED, 0, 0, "Horizon Transit", text);
    }
    say(c, stars >= 4 ? 1 : stars >= 3 ? 0 : 2, stars >= 4 ? L_BYE_GOOD : stars >= 3 ? L_BYE_OK : L_BYE_BAD, 1);
    service_streak_feed(c, streak_bonus);
    *stars_out = stars, *base_out = base, *bonus_out = streak_bonus, *late_out = late > 0.0;
    return pay;
}

static void uber_complete(TsCore *c) {
    int stars, base, bonus, late;
    int pay = uber_settle(c, &stars, &base, &bonus, &late);
    card_ride(c, stars, pay, base, bonus, late);
    int passenger = c->job.passenger, dest = c->job.dest;
    end_job(c);
    service_maybe_lost(c, passenger, dest);
    career_goal_check(c);
    market_check_tier(c);
}

/* seconds standing at the pickup (boarding 1) or the destination: 3 / 2, longer while the door opens and closes (0.4.3) */
double ts_uber_stop_s(const TsCore *c, int boarding) {
    double base = boarding ? 3.0 : 2.0, doors = ts_door_total_s(c);
    return doors > base ? doors : base;
}

/* ---- 0.6: a shared ride */

/* the second passenger, saying something (the first is c->job.passenger until they get out) */
static void pool_say_second(TsCore *c, int style, int kind) {
    int keep = c->job.passenger;
    c->job.passenger = c->job.pool_passenger;
    say(c, style, kind, 1);
    c->job.passenger = keep;
}

static void pool_second_in(TsCore *c) {
    c->pool_stage = 1;
    c->pool_board_t = c->t;
    c->pool_driven_at = c->trip_driven_m;
    set_nav(c, ts_places[c->job.dest].pos);
    c->pool_leg_m = c->car_ok ? ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) : 1.0f;
    char text[TS_TEXT];
    snprintf(text, sizeof(text), "%s → %s", ts_passenger_name(c->job.pool_passenger), ts_place_name(c, c->job.pool_dest));
    toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Zweiter Fahrgast eingestiegen", "Second passenger in"), text);
    pool_say_second(c, 0, L_POOL_HELLO);
    c->pool_reply_at = c->t + 5.0;
}

static void pool_first_out(TsCore *c) {
    float keep_sat = c->sat; /* the second's mood goes on from here (the first's on-time bonus is theirs) */
    int stars, base, bonus, late;
    int first = c->job.passenger;
    c->pool_reply_at = 0.0;
    int pay = uber_settle(c, &stars, &base, &bonus, &late);
    c->pool_first = first;
    c->pool_paid = pay + bonus;
    c->pool_fare = pay < base ? pay : base;
    c->pool_tip = pay > base ? pay - base : 0;
    if (c->cards_on) { /* the card comes at the end, for both; now a line */
        char text[TS_TEXT];
        snprintf(text, sizeof(text), "%s %s · +%s · %d/5", ts_passenger_name(first), T(c, "ist ausgestiegen", "got out"), ts_cr(c->german, pay + bonus), stars);
        event(c, TS_EV_FEED, 0, 0, "Horizon Transit", text);
    }
    /* the ride goes on with the second */
    c->sat = keep_sat;
    c->pool_stage = 2;
    c->job.passenger = c->job.pool_passenger;
    c->job.ptype = c->job.pool_ptype;
    c->job.pickup = c->job.pool_pickup;
    c->job.dest = c->job.pool_dest;
    c->job.trip_m = c->job.pool_trip_m;
    c->job.max_kmh = type_max_kmh(c, c->job.ptype);
    float to_go = c->car_ok ? ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) * TS_ROAD_FACTOR : c->job.pool_trip_m;
    c->leg_t = c->pool_board_t;
    c->job.limit_s = (float)(c->t - c->pool_board_t) + (to_go / TS_UBER_AVG_SPEED + 45.0f) * c->time_factor;
    c->trip_driven_m -= c->pool_driven_at;
    if (c->trip_driven_m < 0.0f) c->trip_driven_m = 0.0f;
    c->late_noted = 0;
    c->pool_leg_m = c->car_ok ? ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) : 1.0f;
    wish_ride_start(c);
    set_nav(c, ts_places[c->job.dest].pos);
}

/* stopping at the second's pickup (stage 0) or the first's destination (stage 1): they get in / out */
static void pool_stop(TsCore *c) {
    const float *at = ts_places[c->pool_stage == 0 ? c->job.pool_pickup : c->job.dest].pos;
    if (!arrived_at(c, at)) {
        if (c->pool_stop_t > 0.0) ts_door_abort(c);
        c->pool_stop_t = 0.0;
        return;
    }
    if (c->pool_stop_t > 0.0 && c->speed > TS_LEAVE_SPEED) { /* drove off too early */
        ts_door_abort(c);
        c->pool_stop_t = 0.0;
        if (c->pool_stage == 1) say(c, 3, L_WANT_OUT, 1);
        return;
    }
    if (c->pool_stop_t <= 0.0) {
        if (c->speed >= TS_STOPPED_SPEED) return;
        c->pool_stop_t = c->t;
        c->door = -1; /* the other passenger's door */
        ts_door_start(c);
    }
    ts_door_step(c);
    if (c->t - c->pool_stop_t < ts_uber_stop_s(c, c->pool_stage == 0)) return;
    ts_door_abort(c);
    c->pool_stop_t = 0.0;
    if (c->pool_stage == 0) pool_second_in(c);
    else pool_first_out(c);
}

static void uber_step(TsCore *c) {
    const float *pickup = ts_places[c->job.pickup].pos, *dest = ts_places[c->job.dest].pos;
    switch (c->stage) {
    case TS_UBER_TO_PICKUP:
        if (c->t > c->deadline && !c->late_noted) {
            c->late_noted = 1;
            say(c, 3, L_WAITING, 1);
        }
        if (arrived_at(c, pickup) && c->speed < TS_STOPPED_SPEED) {
            c->stage = TS_UBER_BOARDING;
            c->stage_t = c->t;
            ts_door_start(c);
        }
        break;
    case TS_UBER_BOARDING:
        if (c->speed > TS_LEAVE_SPEED) { /* drove off before the passenger was in */
            ts_door_abort(c);
            c->stage = TS_UBER_TO_PICKUP;
            say(c, 3, L_NOT_IN_YET, 1);
            break;
        }
        ts_door_step(c);
        if (c->t - c->stage_t >= ts_uber_stop_s(c, 1)) {
            ts_door_abort(c); /* whatever step is still due happens now */
            /* waited too long: the passenger starts less happy (1 point per 5 s, at most 20) */
            float waited = (float)(c->t - c->deadline);
            c->sat = ts_shop_start_sat(c) - (waited > 0.0f ? clampf(waited / 5.0f, 0.0f, 20.0f) : 0.0f);
            c->stage = TS_UBER_TO_DEST;
            c->stage_t = c->leg_t = c->t;
            c->leg_m = c->trip_driven_m = 0.0f;
            c->calm_since = c->t;
            /* 0.6: a shared ride goes to the second passenger first */
            const float *next = c->job.pool ? ts_places[c->job.pool_pickup].pos : dest;
            set_nav(c, next);
            c->pool_leg_m = c->car_ok ? ts_distance_xz(c->car.pos, next) : 1.0f;
            char text[TS_TEXT], km[24], time[16];
            format_km(c, km, sizeof(km), c->job.trip_m);
            format_time(time, sizeof(time), c->job.limit_s);
            snprintf(text, sizeof(text), "%s · ~%s · %s %s", ts_place_name(c, c->job.dest), km, T(c, "Limit", "limit"), time);
            if (c->job.pool)
                snprintf(text, sizeof(text), "%s %s: %s", T(c, "Erst", "First"), ts_passenger_name(c->job.pool_passenger),
                         ts_place_name(c, c->job.pool_pickup));
            toast(c, TS_ICON_ROADS, TS_SOUND_NOTIFY, T(c, "Fahrgast eingestiegen", "Passenger picked up"), text);
            static const int greet[TS_P_KINDS] = {L_GREET_NORMAL, L_GREET_RELAXED, L_GREET_HURRY, L_GREET_FAN,
                                                  L_GREET_VIP, L_GREET_BUSINESS, L_GREET_WEDDING, L_GREET_TOURIST};
            int p = c->job.passenger, type = c->job.ptype;
            c->story_said = 0;
            if (p >= 0 && p < TS_PEOPLE_MAX && c->met[p] > 0 && type != TS_P_HURRY && type < TS_P_VIP && random_unit(c) < 0.8f)
                say(c, 0, L_GREET_REGULAR, 1); /* knows you from before */
            else
                say(c, type == TS_P_FAN ? 1 : 0, greet[type], 1);
            /* the car: the person's taste and the kind of ride */
            int g = c->car_group;
            if (g >= 0 && p >= 0 && p < (int)ts_people_count) {
                if (ts_people_taste[p][0] == g) c->sat += 6.0f;
                else if (ts_people_taste[p][1] == g) c->sat -= 6.0f;
            }
            if (g == TS_GROUP_SALOON && (type == TS_P_RELAXED || type == TS_P_VIP || type == TS_P_BUSINESS || type == TS_P_WEDDING))
                c->sat += 4.0f; /* comfy */
            else if ((g == TS_GROUP_SUPER || g == TS_GROUP_HATCH) && (type == TS_P_RELAXED || type == TS_P_WEDDING || type == TS_P_BUSINESS))
                c->sat -= 3.0f; /* low, hard and loud */
            else if (g == TS_GROUP_SUPER && type == TS_P_FAN)
                c->sat += 4.0f;
            c->sat = clampf(c->sat, 0.0f, 100.0f);
            /* the car, if it's worth a word */
            c->talk_at = c->t + 6.0;
            c->nag_at = c->t + 25.0;
            c->slow_since = c->t;
        }
        break;
    case TS_UBER_TO_DEST:
        if (c->speed > 13.9f) c->slow_since = c->t; /* 50 km/h */
        if (c->pool_reply_at > 0.0 && c->t >= c->pool_reply_at) { /* 0.6: the first answers the second's hello */
            c->pool_reply_at = 0.0;
            say(c, 0, L_POOL_REPLY, 1);
        }
        if (c->talk_at > 0.0 && c->t >= c->talk_at && c->t >= c->chat_cooldown && c->wish == TS_WISH_NONE) {
            int type = c->job.ptype, p = c->job.passenger, g = c->car_group;
            int first = c->talk_at - c->leg_t < 10.0;
            static const int special_talk[4] = {L_VIP_TALK, L_BUSINESS_TALK, L_WEDDING_TALK, L_TOURIST_TALK};
            if (first) { /* just after getting in: what they remember, or the car */
                int regular = p >= 0 && p < TS_PEOPLE_MAX && c->met[p] > 0;
                int kind = -1;
                if (regular && !c->memory_said) {
                    if (c->met_stars[p] > 0 && c->met_stars[p] <= 2) kind = L_REG_LAST_BAD;
                    else if (c->met_car[p] > 0 && c->car_index >= 0)
                        kind = c->met_car[p] == c->car_index + 1 ? L_REG_SAME_CAR : L_REG_OTHER_CAR;
                    else if (c->met_stars[p] >= 5 && random_unit(c) < 0.4f) kind = L_REG_LAST_GOOD;
                    if (kind >= 0) c->memory_said = 1;
                }
                if (kind < 0 && g >= 0 && p >= 0 && p < (int)ts_people_count) {
                    if (ts_people_taste[p][0] == g) kind = L_LIKES_CAR;
                    else if (ts_people_taste[p][1] == g) kind = L_DISLIKES_CAR;
                }
                if (kind < 0 && c->car_open && random_unit(c) < 0.5f) kind = L_OPEN_TOP;
                if (kind < 0 && type != TS_P_FAN && (c->car_kind == TS_CAR_DREAM || c->car_kind == TS_CAR_SPORTY ||
                                                     c->car_kind == TS_CAR_VAN) && random_unit(c) < 0.5f)
                    kind = c->car_kind == TS_CAR_VAN ? L_CAR_VAN : L_CAR_COMPLIMENT;
                if (kind >= 0) say(c, kind == L_REG_LAST_BAD || kind == L_DISLIKES_CAR ? 3 : kind == L_LIKES_CAR ? 1 : 0, kind, 0);
            } else if (talk_maybe_ask(c)) { /* 0.5.1: a question you may answer */
            } else if (type >= TS_P_VIP && random_unit(c) < 0.5f) {
                say(c, 0, special_talk[type - TS_P_VIP], 0);
            } else if (!(type != TS_P_HURRY && random_unit(c) < 0.55f && tell(c))) {
                /* 0.4.1: what they talk about depends on who they are and where you drive */
                int where = where_talk(c);
                float r = random_unit(c);
                int kind = L_SMALL_TALK;
                if (where >= 0 && r < 0.3f) kind = where;
                else if (type == TS_P_RELAXED && r < 0.7f) kind = L_RELAXED_TALK;
                else if (type == TS_P_HURRY && r < 0.75f) kind = L_HURRY_TALK;
                else if (type == TS_P_FAN && r < 0.7f) kind = L_FAN_TALK;
                say(c, 0, kind, 0);
            }
            c->talk_at = c->t + 40.0 + 35.0 * random_unit(c);
        }
        wish_step(c);
        if (c->wish == TS_WISH_STOP) break; /* on the way to the wished stop: the destination comes after */
        if (c->t >= c->nag_at && c->t - c->slow_since > 15.0 && ts_distance_xz(c->car.pos, dest) > 300.0f) {
            if (c->job.ptype == TS_P_FAN) say(c, 0, L_FAN_FASTER, 0);
            else if (c->job.ptype == TS_P_HURRY) say(c, 3, L_HURRY_NAG, 0);
            c->nag_at = c->t + 40.0;
        }
        if (!c->late_noted && c->t - c->leg_t > c->job.limit_s) {
            c->late_noted = 1;
            say(c, 3, L_LATE, 1);
            sound(c, TS_SOUND_ERROR);
        }
        if (c->job.pool && c->pool_stage < 2) { /* 0.6: the second's pickup, then the first's destination */
            pool_stop(c);
            break;
        }
        if (arrived_at(c, dest) && c->speed < TS_STOPPED_SPEED) {
            wish_arrived_at_dest(c);
            c->stage = TS_UBER_ALIGHTING;
            c->stage_t = c->t;
            ts_door_start(c);
        }
        break;
    case TS_UBER_ALIGHTING:
        if (c->speed > TS_LEAVE_SPEED) {
            ts_door_abort(c);
            c->stage = TS_UBER_TO_DEST;
            say(c, 3, L_WANT_OUT, 1);
            break;
        }
        ts_door_step(c);
        if (c->t - c->stage_t >= ts_uber_stop_s(c, 0)) {
            ts_door_abort(c);
            uber_complete(c);
        }
        break;
    }
}

/* passengers at a stop: who gets out, who gets in (never more out than are on board, never more in than fit) */
static void plan_stop(TsCore *c) {
    const TsLine *line = &ts_lines[c->line];
    const TsPlace *stop = &ts_places[ts_bus_stop(c, c->line, c->stop_i)];
    int last = c->stop_i == (int)line->stop_count - 1;
    int out = 0;
    if (last) out = c->onboard;
    else if (c->onboard > 0) {
        float p = stop->kind == TS_PLACE_SPOT ? 0.25f : stop->kind >= TS_PLACE_STOP ? 0.35f : 0.5f; /* each gets out with this chance */
        for (int i = 0; i < c->onboard; ++i)
            if (random_unit(c) < p) out++;
    }
    int in = 0;
    if (!last) {
        int most = stop->kind == TS_PLACE_FESTIVAL ? 18 : stop->kind == TS_PLACE_OUTPOST ? 12 : stop->kind == TS_PLACE_STOP ? 9 : 6;
        int room = c->bus_capacity - (c->onboard - out);
        if (most > room) most = room;
        in = most > 0 ? random_below(c, most + 1) : 0;
        if (c->stop_i == 0 && in < 3 && most >= 3) in = 3 + random_below(c, most - 2); /* the first stop: always some */
    }
    c->to_alight = out;
    c->to_board = in;
    c->alighted_now = c->boarded_now = 0;
    c->stop_secs = 3.0f + 0.5f * (float)(out + in);
}

static void bus_leg_to_next(TsCore *c) {
    const TsLine *line = &ts_lines[c->line];
    int from = ts_bus_stop(c, c->line, c->stop_i);
    c->stop_i++;
    int to = ts_bus_stop(c, c->line, c->stop_i);
    int last = c->stop_i == (int)line->stop_count - 1;
    float est = place_distance(from, to) * TS_ROAD_FACTOR;
    c->stage = TS_BUS_TO_STOP;
    c->stage_t = c->leg_t = c->t;
    c->leg_m = 0.0f;
    c->late_noted = 0;
    c->deadline = c->t + (est / TS_BUS_AVG_SPEED + 45.0f) * c->time_factor;
    set_nav(c, ts_places[to].pos);
    /* 0.5: who gets off and on is known now: nobody = no stop request, the bus may drive through */
    plan_stop(c);
    c->stop_request = last || c->to_alight > 0 || c->to_board > 0;
    /* the announcement, as on a real bus (read out with "Voices" on) */
    char text[TS_TEXT];
    if (last) snprintf(text, sizeof(text), "%s %s. %s", T(c, "Nächster Halt:", "Next stop:"), ts_place_name(c, to),
                       T(c, "Endhaltestelle, bitte alle aussteigen.", "Last stop, all change please."));
    else if (c->stop_request) snprintf(text, sizeof(text), "%s %s.", T(c, "Nächster Halt:", "Next stop:"), ts_place_name(c, to));
    else snprintf(text, sizeof(text), "%s %s. %s", T(c, "Nächster Halt:", "Next stop:"), ts_place_name(c, to),
                  T(c, "Kein Haltewunsch.", "No stop requested."));
    sound(c, TS_SOUND_GONG); /* 0.5.1: the gong before the announcement */
    event(c, TS_EV_FEED, 0, 0, T(c, "Ansage", "Announcement"), text);
    if (c->event_count) c->events[c->event_count - 1].speaker = TS_SPEAKER_ANNOUNCE, c->events[c->event_count - 1].style = 3;
    /* the stop button: someone presses it a little after the announcement (0.5.1: not on top of the gong) */
    if (!last && c->to_alight > 0) c->bell_at = c->t + 2.5 + 3.0 * random_unit(c), c->bell_stop = c->stop_i;
    else c->bell_at = 0.0;
    if (c->onboard > 0 && random_unit(c) < 0.35f) say(c, 0, L_BUS_TALK, 0);
}

/* 0.5: at a stop (or driving through one): the timetable and the fares; returns the delay */
static float bus_reach_stop(TsCore *c) {
    const TsLine *line = &ts_lines[c->line];
    (void)line;
    if (c->deadline > 0.0) {
        double late = c->t - c->deadline;
        if (late > 0.0) c->bus_late_s += (float)late;
        else c->sat = clampf(c->sat + 2.0f, 0.0f, 100.0f);
    }
    float delay = tt_arrived(c);
    /* fare: every passenger on board pays for the leg (4 CR per estimated km) */
    if (c->stop_i > 0) {
        float leg_km = place_distance(ts_bus_stop(c, c->line, c->stop_i - 1), ts_bus_stop(c, c->line, c->stop_i)) * TS_ROAD_FACTOR / 1000.0f;
        c->bus_pay += (int)((float)c->onboard * leg_km * (ts_has(c, TS_SHOP_TICKETS) ? 4.8f : 4.0f) * (float)c->cr_scale); /* 0.10.1: the ticket machine; 0.13: CR */
    }
    return delay;
}

static void bus_complete(TsCore *c) {
    float on_time = c->bus_late_s <= 0.0f ? 6.0f : -clampf(c->bus_late_s / 10.0f, 0.0f, 25.0f);
    c->sat = clampf(c->sat + on_time, 0.0f, 100.0f);
    int stars = ts_stars(c->sat);
    int base = (int)((float)c->bus_pay * 1.08f * career_pay_factor(c));
    int pay = (int)((float)c->bus_pay * (0.6f + 0.6f * c->sat / 100.0f) * career_pay_factor(c));
    if (ts_has(c, TS_SHOP_TIPS) && pay > base) pay = base + (int)((float)(pay - base) * 1.3f); /* 0.10.1: the tip box */
    {   /* 0.13: the surcharges bought in the shop */
        float f = ts_shop_pay_factor(c, 1, c->trip_driven_m, c->bus_late_s <= 0.0f);
        pay = (int)((float)pay * f);
        base = (int)((float)base * f);
    }
    pay = (pay / 5) * 5;
    c->wallet += pay;
    c->bus_lines_done++;
    c->passengers_total += c->delivered;
    career_job_done(c, stars, pay, base, 0, c->trip_driven_m, c->line);
    int streak_bonus = service_streak(c, stars, pay);
    service_log(c, 1, c->line, ts_bus_stop(c, c->line, (int)ts_lines[c->line].stop_count - 1), pay + streak_bonus, stars);
    service_book(c, 1, c->line, ts_bus_stop(c, c->line, 0), ts_bus_stop(c, c->line, (int)ts_lines[c->line].stop_count - 1), pay + streak_bonus,
                 pay - base, stars, c->bus_late_s > 0.0f);
    finish_stats(c, stars);
    char title[TS_TEXT], text[TS_TEXT], km[24], time[16];
    format_km(c, km, sizeof(km), c->trip_driven_m);
    format_time(time, sizeof(time), c->t - c->trip_t);
    snprintf(title, sizeof(title), "+%s · %d/5 %s", ts_cr(c->german, pay), stars, T(c, "Sterne", "stars"));
    snprintf(text, sizeof(text), "%s %s · %d %s · %s · %s", T(c, "Linie", "Line"), ts_lines[c->line].number, c->delivered,
             T(c, "Fahrgäste", "passengers"), km, time);
    if (c->cards_on) sound(c, TS_SOUND_PAYOUT);
    else toast(c, TS_ICON_PAYOUT, TS_SOUND_PAYOUT, title, text);
    int timed = c->stops_on_time + c->stops_late;
    if (timed && !c->cards_on) {
        snprintf(text, sizeof(text), "%s %d %s %d %s", T(c, "Pünktlich an", "On time at"), c->stops_on_time, T(c, "von", "of"), timed,
                 T(c, "Halten", "stops"));
        event(c, TS_EV_FEED, 0, 0, T(c, "Fahrplan", "Timetable"), text);
    }
    service_streak_feed(c, streak_bonus);
    card_bus(c, stars, pay, base, streak_bonus);
    end_job(c);
    career_goal_check(c);
    market_check_tier(c);
}

static void bus_step(TsCore *c) {
    const TsLine *line = &ts_lines[c->line];
    const TsPlace *stop = &ts_places[ts_bus_stop(c, c->line, c->stop_i)];
    if (c->stage == TS_BUS_TO_STOP) {
        if (c->deadline > 0.0 && !c->late_noted && c->t > c->deadline && c->onboard > 0) {
            c->late_noted = 1;
            say(c, 3, L_BUS_LATE, 1);
        }
        /* 0.5: nobody wants off or on: drive through (or stop, as you like) */
        if (!c->stop_request && arrived_at(c, stop->pos)) {
            bus_reach_stop(c);
            char text[TS_TEXT];
            snprintf(text, sizeof(text), "%s %s", T(c, "Durchgefahren:", "Drove through:"), ts_place_name(c, ts_bus_stop(c, c->line, c->stop_i)));
            event(c, TS_EV_FEED, 0, 0, T(c, "Leitstelle", "Dispatch"), text);
            bus_leg_to_next(c);
            return;
        }
        if (arrived_at(c, stop->pos) && c->speed < TS_STOPPED_SPEED) {
            float delay = bus_reach_stop(c);
            c->alighted_now = c->boarded_now = 0;
            c->stage = TS_BUS_AT_STOP;
            c->stage_t = c->t;
            char text[TS_TEXT];
            if (c->to_alight && c->to_board)
                snprintf(text, sizeof(text), "%d %s · %d %s", c->to_alight, T(c, "steigen aus", "getting off"), c->to_board,
                         T(c, "steigen ein", "getting on"));
            else if (c->to_alight) snprintf(text, sizeof(text), "%d %s", c->to_alight, T(c, "Fahrgäste steigen aus", "passengers getting off"));
            else if (c->to_board) snprintf(text, sizeof(text), "%d %s", c->to_board, T(c, "Fahrgäste steigen ein", "passengers getting on"));
            else snprintf(text, sizeof(text), "%s", T(c, "Niemand steigt ein oder aus", "Nobody gets on or off"));
            if (c->stop_i > 0 && delay > 60.0f) {
                char late[16];
                size_t used = strlen(text);
                format_time(late, sizeof(late), delay);
                snprintf(text + used, sizeof(text) - used, " · +%s", late);
            }
            toast(c, TS_ICON_NONE, TS_SOUND_NOTIFY, ts_place_name(c, ts_bus_stop(c, c->line, c->stop_i)), text);
        }
        return;
    }
    /* at the stop: passengers get off first, then on, one every 0.5 s */
    float elapsed = (float)(c->t - c->stage_t);
    int moved = (int)((elapsed - 1.5f) / 0.5f);
    if (moved < 0) moved = 0;
    int out_now = moved < c->to_alight ? moved : c->to_alight;
    int in_now = moved - c->to_alight;
    if (in_now < 0) in_now = 0;
    if (in_now > c->to_board) in_now = c->to_board;
    while (c->alighted_now < out_now) c->alighted_now++, c->onboard--, c->delivered++, c->bus_pay += 45;
    while (c->boarded_now < in_now) c->boarded_now++, c->onboard++, c->board_total++;

    if (c->speed > TS_LEAVE_SPEED) { /* drove off early: the rest stay behind */
        int left_behind = (c->to_alight - c->alighted_now) + (c->to_board - c->boarded_now);
        if (left_behind > 0) {
            /* those who wanted out ride on to the next stop; those who wanted in are left standing */
            c->sat = clampf(c->sat - 5.0f - (float)left_behind, 0.0f, 100.0f);
            say(c, 3, L_BUS_NOT_DONE, 1);
            sound(c, TS_SOUND_ERROR);
        }
        c->to_alight = c->alighted_now, c->to_board = c->boarded_now;
        elapsed = c->stop_secs; /* done */
    }
    if (elapsed >= c->stop_secs && c->alighted_now == c->to_alight && c->boarded_now == c->to_board) {
        if (c->stop_i >= (int)line->stop_count - 1) bus_complete(c);
        else bus_leg_to_next(c);
    }
}

void ts_step(TsCore *c) {
    if (c->cr_scale > 0) pay_scale = c->cr_scale;
    if (c->mode != TS_BUS) shop_apply(c); /* the bus's seats (not while a line runs) */
    market_step(c);                       /* 0.6: hotspots come and go */
    card_step(c);
    if (c->mode == TS_UBER) uber_step(c);
    else if (c->mode == TS_BUS) bus_step(c);
    else service_idle_step(c), service_target_step(c);
    talk_step(c);
    if (c->bell_at > 0.0 && c->t >= c->bell_at) {
        if (c->mode == TS_BUS && c->stage == TS_BUS_TO_STOP && c->stop_i == c->bell_stop) sound(c, TS_SOUND_BELL);
        c->bell_at = 0.0;
    }
}

int ts_take_events(TsCore *c, TsEvent *out, int max) {
    int n = c->event_count < max ? c->event_count : max;
    memcpy(out, c->events, sizeof(TsEvent) * (size_t)n);
    memmove(c->events, c->events + n, sizeof(TsEvent) * (size_t)(c->event_count - n));
    c->event_count -= n;
    return n;
}

/* ------------------------------------------------------------------ HUD */

void row(TsHud *h, const char *label, const char *value, int heading) {
    if (h->row_count >= 9) return;
    snprintf(h->row_label[h->row_count], sizeof(h->row_label[0]), "%s", label);
    snprintf(h->row_value[h->row_count], sizeof(h->row_value[0]), "%s", value ? value : "");
    h->row_heading[h->row_count] = heading;
    h->row_count++;
}

void row_meter(TsHud *h, float full, int quality) {
    if (h->row_count <= 0) return;
    if (quality) h->row_meter_quality |= 1u << (h->row_count - 1);
    h->row_meter_on |= 1u << (h->row_count - 1);
    h->row_meter[h->row_count - 1] = full < 0.0f ? 0.0f : full > 1.0f ? 1.0f : full;
}

const char *type_name(const TsCore *c, int type) {
    switch (type) {
    case TS_P_RELAXED: return T(c, "Gemütlich", "Relaxed");
    case TS_P_HURRY: return T(c, "Eilig", "In a hurry");
    case TS_P_FAN: return T(c, "Autofan", "Car fan");
    case TS_P_VIP: return "VIP";
    case TS_P_BUSINESS: return T(c, "Geschäftskunde", "Business");
    case TS_P_WEDDING: return T(c, "Hochzeitspaar", "Wedding");
    case TS_P_TOURIST: return T(c, "Ausflug", "Day trip");
    default: return T(c, "Normal", "Normal");
    }
}

/* 0.12: money with thousands marks as the app shows it: "10.089 CR" (German) or "10,089 CR" (0.13: credits) */
static void money_text(int german, long long v, char *out, size_t size) {
    char d[32];
    snprintf(d, sizeof(d), "%lld", v < 0 ? -v : v);
    char r[48];
    int n = (int)strlen(d), k = 0;
    if (v < 0) r[k++] = '-';
    for (int i = 0; i < n; ++i) {
        r[k++] = d[i];
        if ((n - 1 - i) % 3 == 0 && i < n - 1) r[k++] = german ? '.' : ',';
    }
    r[k] = 0;
    snprintf(out, size, "%s CR", r);
}

void ts_money_text(const TsCore *c, long long v, char *out, size_t size) { money_text(c->german, v, out, size); }

const char *ts_cr(int german, long long v) {
    static _Thread_local char buf[8][32];
    static _Thread_local int next;
    char *out = buf[next++ & 7];
    money_text(german, v, out, 32);
    return out;
}

static void car_line(const TsCore *c, char *out, size_t size) {
    if (c->car_kind == TS_CAR_UNKNOWN) snprintf(out, size, "%s", c->car_ok ? T(c, "Auto wird erkannt ...", "Reading your car ...")
                                                                         : T(c, "Kein Auto erkannt", "No car yet"));
    else snprintf(out, size, "%s", c->car_name);
}

void ts_hud(const TsCore *c, TsHud *h) {
    memset(h, 0, sizeof(*h));
    h->row_selected = -1;
    h->german = c->german;
    service_hud(c, h); /* 0.5: the meter and the tablet's column */
    card_hud(c, h);    /* 0.6: a card after a ride, a shift, a new tier */
    h->trip_class = -1;
    h->trip_surge = 1.0f;
    char a[64], b[48], km[24], time[16];

    if (c->mode != TS_IDLE) {
        const float *target = c->nav_on ? c->nav_target : NULL;
        h->chips = 1;
        if (target && c->car_ok) {
            format_km(c, km, sizeof(km), ts_distance_xz(c->car.pos, target));
            snprintf(h->chip1, sizeof(h->chip1), "%s", km);
        }
        /* the speed the passengers are happy with; it says so when it's over */
        int max = ride_max_kmh(c);
        int over = c->car_ok && c->speed * 3.6f > (float)max + 2.0f;
        if (over) snprintf(h->chip2, sizeof(h->chip2), "%s %d", T(c, "Zu schnell! Max", "Too fast! Max"), ts_speed_shown(c, max));
        else snprintf(h->chip2, sizeof(h->chip2), "Max %d %s", ts_speed_shown(c, max), ts_speed_unit(c));
        int carrying = (c->mode == TS_UBER && c->stage >= TS_UBER_TO_DEST) || (c->mode == TS_BUS && c->onboard > 0);
        if (carrying) {
            h->sign_limit = ts_speed_shown(c, max);
            h->sign_free = c->mode == TS_UBER && c->floorit;
            /* 1 a little over (yellow), 2 clearly over (red): 10 km/h, in mph 6 mph over the sign (0.4.3: 15 km/h was rarely reached) */
            float red_kmh = c->imperial ? 6.0f * 1.609344f : 10.0f;
            h->sign_over = !over ? 0 : c->speed * 3.6f > (float)max + red_kmh ? 2 : 1;
            snprintf(h->sign_unit, sizeof(h->sign_unit), "%s", ts_speed_unit(c));
            /* the G-meter: the same limits ts_car_sample counts as harsh (gas 4.5, braking 6.5, sideways 6.5 / a fan 8 m/s^2),
               rounded to 0.05 so the picture changes only when the dot moves */
            float lat_limit = c->mode == TS_UBER && c->job.ptype == TS_P_FAN ? TS_LIM_SIDE_FAN : TS_LIM_SIDE;
            float gl = c->acc_lat / lat_limit, gf = c->acc_long > 0.0f ? c->acc_long / TS_LIM_GAS : c->acc_long / TS_LIM_BRAKE;
            h->g_on = c->car_ok;
            h->g_lat = roundf(clampf(gl, -1.6f, 1.6f) * 20.0f) / 20.0f;
            h->g_long = roundf(clampf(gf, -1.6f, 1.6f) * 20.0f) / 20.0f;
            h->g_over = h->g_lat * h->g_lat + h->g_long * h->g_long > 1.0f;
        }
        double left = 0.0;
        int timed = 0;
        if (c->mode == TS_UBER) {
            switch (c->stage) {
            case TS_UBER_TO_PICKUP: snprintf(h->chip0, sizeof(h->chip0), "%s", T(c, "Abholen", "Pick up")); break;
            case TS_UBER_BOARDING: snprintf(h->chip0, sizeof(h->chip0), "%s", T(c, "Steigt ein", "Getting in")); break;
            case TS_UBER_TO_DEST: snprintf(h->chip0, sizeof(h->chip0), "%s", T(c, "Zum Ziel", "To destination")); break;
            default: snprintf(h->chip0, sizeof(h->chip0), "%s", T(c, "Steigt aus", "Getting out")); break;
            }
            if (c->stage >= TS_UBER_TO_DEST) left = c->job.limit_s - (c->t - c->leg_t), timed = 1;
            else left = c->deadline - c->t, timed = 1;
        } else {
            snprintf(h->chip0, sizeof(h->chip0), "%s %s · %d %s", T(c, "Bus", "Bus"), ts_lines[c->line].number, c->onboard,
                     T(c, "FG", "PAX"));
            if (c->deadline > 0.0) left = c->deadline - c->t, timed = 1;
        }
        /* 0.5.1: the ride card: where to and how far along */
        h->trip_on = 1;
        if (c->mode == TS_UBER) {
            int to_dest = c->stage >= TS_UBER_TO_DEST;
            h->trip_class = c->job.ride_class;
            h->trip_surge = c->job.surge > 1.0f ? c->job.surge : 1.0f;
            snprintf(h->trip_label, sizeof(h->trip_label), "%s", c->stage == TS_UBER_TO_PICKUP ? T(c, "ABHOLEN", "PICK UP")
                     : c->stage == TS_UBER_BOARDING ? T(c, "STEIGT EIN", "GETTING IN") : c->stage == TS_UBER_TO_DEST ? T(c, "ZUM ZIEL", "TO DESTINATION")
                     : T(c, "STEIGT AUS", "GETTING OUT"));
            snprintf(h->trip_to, sizeof(h->trip_to), "%s", ts_place_name(c, to_dest ? c->job.dest : c->job.pickup));
            h->trip_pool = c->job.pool;
            float p = 0.0f;
            int pool_leg = c->job.pool && c->pool_stage < 2 && c->stage == TS_UBER_TO_DEST;
            if (pool_leg) { /* 0.6: the second's pickup, then where the first gets out */
                int at = c->pool_stage == 0 ? c->job.pool_pickup : c->job.dest;
                if (c->pool_stage == 0)
                    snprintf(h->trip_label, sizeof(h->trip_label), "%s", T(c, "2. FAHRGAST ABHOLEN", "PICK UP RIDER 2"));
                else snprintf(h->trip_label, sizeof(h->trip_label), "%s %s", ts_passenger_name(c->job.passenger), T(c, "STEIGT AUS", "GETS OUT"));
                if (c->pool_stage == 1) for (char *q = h->trip_label; *q; ++q) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
                snprintf(h->trip_to, sizeof(h->trip_to), "%s", ts_place_name(c, at));
                if (c->car_ok && c->pool_leg_m > 1.0f) p = 1.0f - ts_distance_xz(c->car.pos, ts_places[at].pos) / c->pool_leg_m;
            } else
            if (c->stage == TS_UBER_TO_PICKUP && c->job.pickup_m > 1.0f && c->car_ok)
                p = 1.0f - ts_distance_xz(c->car.pos, ts_places[c->job.pickup].pos) / c->job.pickup_m;
            else if (c->stage == TS_UBER_TO_DEST && c->job.pool && c->pool_leg_m > 1.0f && c->car_ok) /* the second's last leg */
                p = 1.0f - ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) / c->pool_leg_m;
            else if (c->stage == TS_UBER_TO_DEST && c->job.trip_m > 1.0f && c->car_ok)
                p = 1.0f - ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) * TS_ROAD_FACTOR / c->job.trip_m;
            else if (c->stage == TS_UBER_BOARDING) p = 0.0f;
            else if (c->stage > TS_UBER_TO_DEST) p = 1.0f;
            h->trip_progress = roundf(clampf(p, 0.0f, 1.0f) * 50.0f) / 50.0f; /* 2 % steps: the card is redrawn only when it moves */
        } else {
            int n = (int)ts_lines[c->line].stop_count, i = c->stop_i < 0 ? 0 : c->stop_i >= n ? n - 1 : c->stop_i;
            snprintf(h->trip_label, sizeof(h->trip_label), "%s %s · %s", T(c, "LINIE", "LINE"), ts_lines[c->line].number,
                     c->stage == TS_BUS_AT_STOP ? T(c, "HALT", "STOP") : T(c, "NÄCHSTER HALT", "NEXT STOP"));
            snprintf(h->trip_to, sizeof(h->trip_to), "%s", ts_place_name(c, ts_bus_stop(c, c->line, i)));
            h->trip_progress = n > 1 ? (float)i / (float)(n - 1) : 0.0f;
        }
        if (timed) {
            format_time(time, sizeof(time), left);
            snprintf(h->corner, sizeof(h->corner), "%s%s · %d%%", left < 0 ? "+" : "", time, (int)(c->sat + 0.5f));
        } else {
            snprintf(h->corner, sizeof(h->corner), "%d%%", (int)(c->sat + 0.5f));
        }
        if (c->mode == TS_UBER) {
            talk_chip(c, h->chip3, sizeof(h->chip3)); /* 0.5.1: a question waiting goes before the wish */
            if (!h->chip3[0]) wish_chip(c, h->chip3, sizeof(h->chip3));
        }
        else tt_chip(c, h->chip3, sizeof(h->chip3));
        int riding = (c->mode == TS_UBER && c->stage >= TS_UBER_TO_DEST) || (c->mode == TS_BUS && c->onboard > 0);
        h->stars = riding ? ts_stars(c->sat) : 0;
        if (c->mode == TS_BUS && c->stage == TS_BUS_AT_STOP) {
            float left_s = c->stop_secs - (float)(c->t - c->stage_t);
            h->countdown = left_s > 0.0f ? (int)(left_s + 0.999f) : 0;
            int pending_in = c->to_board - c->boarded_now, pending_out = c->to_alight - c->alighted_now;
            if (pending_out > 0) snprintf(h->countdown_label, sizeof(h->countdown_label), "%d %s", pending_out, T(c, "steigen aus", "getting off"));
            else if (pending_in > 0) snprintf(h->countdown_label, sizeof(h->countdown_label), "%d %s", pending_in, T(c, "steigen ein", "getting on"));
            else snprintf(h->countdown_label, sizeof(h->countdown_label), "%s", T(c, "Türen schließen", "Doors closing"));
        } else if (c->mode == TS_UBER && (c->stage == TS_UBER_BOARDING || c->stage == TS_UBER_ALIGHTING)) {
            float total = (float)ts_uber_stop_s(c, c->stage == TS_UBER_BOARDING);
            float left_s = total - (float)(c->t - c->stage_t);
            h->countdown = left_s > 0.0f ? (int)(left_s + 0.999f) : 0;
            snprintf(h->countdown_label, sizeof(h->countdown_label), "%s %s", ts_passenger_name(c->job.passenger),
                     c->stage == TS_UBER_BOARDING ? T(c, "steigt ein", "gets in") : T(c, "steigt aus", "gets out"));
        }
    }

    if (!c->phone_open) return;
    /* the phone: an app called Horizon Transit on the left, its name and tab on the top row (< Rides > for
       the Left/Right keys); line1/line2/tabs stay filled but no bar is shown */
    h->panel = 1;
    snprintf(h->line1, sizeof(h->line1), "Horizon Transit");
    snprintf(h->line2, sizeof(h->line2), "%s", T(c, "Dein Festival. Deine Route.", "Your festival. Your route."));
    { /* 0.12: the head as in the Android app: the car under the name, the company funds on the right */
        car_line(c, h->head_sub, sizeof(h->head_sub));
        if (c->game_credits >= 0) ts_money_text(c, c->game_credits, h->money, sizeof(h->money)); /* 0.13: the game account */
        else snprintf(h->money, sizeof(h->money), "… CR");
        snprintf(h->money_label, sizeof(h->money_label), "%s", T(c, "Spielkonto", "Game account"));
        h->go_row = -1;
        h->speed_now = c->car_ok ? ts_speed_shown(c, (int)lroundf(c->speed * 3.6f)) : -1;
        snprintf(h->speed_unit, sizeof(h->speed_unit), "%s", ts_speed_unit(c));
        h->page = c->mode == TS_IDLE ? c->phone_tab : TS_PAGE_JOB + c->job_tab;
    }
    if (c->mode == TS_IDLE) {
        h->tab_count = TS_TABS;
        h->tab = c->phone_tab;
        snprintf(h->tabs[TS_TAB_RIDES], sizeof(h->tabs[0]), "%s", T(c, "Fahrten", "Rides"));
        snprintf(h->tabs[TS_TAB_BUS], sizeof(h->tabs[0]), "%s", T(c, "Bus", "Bus"));
        snprintf(h->tabs[TS_TAB_MAP], sizeof(h->tabs[0]), "%s", T(c, "Karte", "Map"));
        snprintf(h->tabs[TS_TAB_ACCOUNT], sizeof(h->tabs[0]), "%s", T(c, "Konto", "Account"));
        snprintf(h->tabs[TS_TAB_SHOP], sizeof(h->tabs[0]), "%s", T(c, "Shop", "Shop"));
        snprintf(h->tabs[TS_TAB_SETTINGS], sizeof(h->tabs[0]), "%s", T(c, "Optionen", "Options"));
        int sel = c->phone_sel;
        row(h, "Horizon Transit", h->tabs[c->phone_tab], 1);
        if (c->phone_tab == TS_TAB_RIDES) {
            car_line(c, a, sizeof(a));
            int hot = market_hot_near(c);
            if (!c->car_ok || !c->offer_count) b[0] = 0;
            else if (hot >= 0) { /* 0.6: where many want a ride: the nearest hotspot */
                char sx[16];
                market_surge_text(c, market_surge(c, c->hot[hot].place), sx, sizeof(sx));
                const char *town = ts_place_name(c, c->hot[hot].place);
                char name[32];
                snprintf(name, sizeof(name), "%s", town);
                char *cut = strstr(name, " · ");
                if (cut) *cut = 0;
                snprintf(b, sizeof(b), "%s %s", name, sx);
            }
            else if (c->demand == 2) snprintf(b, sizeof(b), "%s", T(c, "Viel los", "Busy area"));
            else if (c->demand == 0) snprintf(b, sizeof(b), "%s", T(c, "Ruhige Gegend", "Quiet area"));
            else b[0] = 0;
            row(h, a, b, 2);
            if (!c->shift.on) { /* 0.13: offline: only the GO card */
                row(h, T(c, "Du bist offline", "You're offline"), "", 0);
                h->row_selected = 2;
                h->go_row = 2, h->go_on = 0;
                snprintf(h->go_title, sizeof(h->go_title), "%s", T(c, "Du bist offline", "You're offline"));
                snprintf(h->go_text, sizeof(h->go_text), "%s", T(c, "GO drücken, dann kommen Fahrten.", "Press GO to get rides."));
                snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Enter = GO. Dann kommen Anfragen nacheinander: annehmen oder ablehnen.",
                                                               "Enter = GO. Then requests come one by one: accept or decline."));
            }
            for (int i = 0; i < c->offer_count && i < 1; ++i) { /* 0.13: one request at a time */
                const TsOffer *o = &c->offers[i];
                format_km(c, km, sizeof(km), o->trip_m);
                snprintf(b, sizeof(b), "%s · %s", km, ts_cr(c->german, o->pay));
                row(h, ts_place_name(c, o->dest), b, 0);
                char who[40];
                if (o->party > 1) snprintf(who, sizeof(who), "%s +%d", ts_passenger_name(o->passenger), o->party - 1);
                else snprintf(who, sizeof(who), "%s", ts_passenger_name(o->passenger));
                if (o->pool) /* 0.6: a shared ride: both names */
                    snprintf(a, sizeof(a), "%s · %s & %s", T(c, "Sammelfahrt", "Shared"), ts_passenger_name(o->passenger),
                             ts_passenger_name(o->pool_passenger));
                else if (o->ride_class != TS_CLASS_COMFORT) /* 0.6: the class first, then who */
                    snprintf(a, sizeof(a), "%s · %s · %s", ts_class_name(o->ride_class, c->german), who,
                             o->regular ? T(c, "fragt nach dir", "asked for you") : type_name(c, o->ptype));
                else snprintf(a, sizeof(a), "%s · %s", who, o->regular ? T(c, "fragt nach dir", "asked for you") : type_name(c, o->ptype));
                if (o->surge > 1.01f) {
                    char sx[16];
                    market_surge_text(c, o->surge, sx, sizeof(sx));
                    snprintf(b, sizeof(b), "%s · max %d", sx, ts_speed_shown(c, o->max_kmh));
                } else snprintf(b, sizeof(b), "max %d %s", ts_speed_shown(c, o->max_kmh), ts_speed_unit(c));
                row(h, a, b, 2);
            }
            if (!c->shift.on) {
            } else if (!c->offer_count) {
                row(h, c->car_ok ? T(c, "Suche Fahrten …", "Finding rides …") : T(c, "Kein Auto erkannt", "No car found"),
                    c->car_ok ? "" : T(c, "erst losfahren", "drive first"), 2);
                snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Du bist online. Anfragen kommen von selbst, an Hotspots öfter.",
                                                               "You're online. Requests come by themselves, more often at hotspots."));
            } else {
                row(h, T(c, "Ablehnen", "Decline"), "", 0);
                h->row_selected = sel ? 4 : 2;
                const TsOffer *o = &c->offers[0];
                char pk[24];
                format_km(c, pk, sizeof(pk), o->pickup_m);
                format_time(time, sizeof(time), o->limit_s);
                snprintf(h->footer, sizeof(h->footer), "%s %s · %s %s · %s", T(c, "Abholung in", "Pickup"), pk,
                         T(c, "Zeitlimit", "time limit"), time, ts_place_name(c, o->pickup));
                if (o->pool)
                    snprintf(h->footer, sizeof(h->footer), "%s %s · %s %s %s · %s", T(c, "Abholung in", "Pickup"), pk,
                             ts_passenger_name(o->pool_passenger), T(c, "steigt zu in", "joins at"), ts_place_name(c, o->pool_pickup),
                             T(c, "2 x Fahrgeld", "2 fares"));
                if (c->req_until > 0.0) { /* 0.13: the countdown in front */
                    char pre[32];
                    int n = snprintf(pre, sizeof(pre), "%s %d s · ", T(c, "Noch", "Expires in"), (int)(c->req_until - c->t + 0.99));
                    size_t len = strlen(h->footer), room = sizeof(h->footer) - 1 - (size_t)n;
                    memmove(h->footer + n, h->footer, len < room ? len : room);
                    memcpy(h->footer, pre, (size_t)n);
                    h->footer[(size_t)n + (len < room ? len : room)] = 0;
                }
            }
        } else if (c->phone_tab == TS_TAB_BUS) {
            int locked = c->bus_needs_bus && c->car_kind != TS_CAR_BUS;
            row(h, locked ? T(c, "Nur mit dem Bus", "Bus only") : T(c, "Bus bereit", "Bus ready"), NULL, 2);
            for (unsigned i = 0; i < ts_line_count && h->row_count < 8; ++i) {
                snprintf(a, sizeof(a), "%s %s · %s", T(c, "Linie", "Line"), ts_lines[i].number, ts_line_title(c, i));
                if (locked) snprintf(b, sizeof(b), "%s", T(c, "Bus nötig", "needs the bus"));
                else snprintf(b, sizeof(b), "%u %s", ts_lines[i].stop_count, T(c, "Halte", "stops"));
                row(h, a, b, locked ? 2 : 0);
            }
            h->row_selected = 2 + sel;
            const TsLine *l = &ts_lines[sel < (int)ts_line_count ? sel : 0];
            if (locked)
                snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Steig in den Bus (PG Bus) um, dann geht's los.", "Get into the bus (PG Bus) to drive a line."));
            else
            {
                int li = sel < (int)ts_line_count ? sel : 0, n = (int)l->stop_count, rev = bus_direction(c, li);
                snprintf(h->footer, sizeof(h->footer), "%s → %s · %s %d min · max %d %s",
                         ts_place_name(c, l->stops[rev ? n - 1 : 0]), ts_place_name(c, l->stops[rev ? 0 : n - 1]), T(c, "ca.", "about"),
                         (int)(tt_line_minutes(c, li) + 0.5f), ts_speed_shown(c, TS_BUS_MAX_KMH), ts_speed_unit(c));
            }
        } else if (c->phone_tab == TS_TAB_SETTINGS) {
            /* 0.10.1: all the mod menu's settings; the list scrolls with the selection */
            if (c->pick_row >= 0 && c->pick_row < c->setting_count) { /* 0.13: the list of one setting's values */
                int pr = c->pick_row, n = c->setting_choices[pr], base_row = h->row_count;
                row(h, c->setting_label[pr], "", 2);
                int first = ts_scroll_first(n, c->pick_sel, 8 - base_row - 1);
                for (int i = first; i < n && h->row_count < 8; ++i)
                    row(h, c->setting_choice[pr][i], i == c->setting_index[pr] ? T(c, "aktiv", "current") : "", 0);
                h->row_selected = base_row + 1 + c->pick_sel - first;
                snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Enter wählt aus. Zurück schließt die Liste.", "Enter picks it. Back closes the list."));
            } else {
            int n = c->setting_count < TS_SETTING_ROWS ? c->setting_count : TS_SETTING_ROWS, base_row = h->row_count;
            int first = ts_scroll_first(n, sel, 8 - base_row);
            for (int i = first; i < n && h->row_count < 8; ++i) row(h, c->setting_label[i], c->setting_value[i], 0);
            if (!c->setting_count) row(h, T(c, "Keine Einstellungen", "No settings"), "", 2);
            else h->row_selected = base_row + sel - first;
            if (sel >= 0 && sel < n && c->setting_note[sel][0]) snprintf(h->footer, sizeof(h->footer), "%s", c->setting_note[sel]);
            else snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Enter ändert die Einstellung. Gespeichert wie im Mod-Menü.",
                                                               "Enter changes the setting. Saved like in the mod menu."));
            }
        } else if (c->phone_tab == TS_TAB_MAP) {
            /* 0.7: map_hud fills it */
        } else if (c->phone_tab == TS_TAB_SHOP) {
            if (c->game_credits >= 0) ts_money_text(c, c->game_credits, b, sizeof(b));
            else snprintf(b, sizeof(b), "… CR");
            row(h, T(c, "Spielkonto", "Game account"), b, 2);
            int base_row = h->row_count, first = shop_rows(c, h);
            h->row_selected = base_row + sel - first;
        } else {
            career_account_rows(c, h);
            h->row_selected = 1; /* the shift */
            h->go_row = 1, h->go_on = c->shift.on; /* 0.12 */
            snprintf(h->go_title, sizeof(h->go_title), "%s", c->shift.on ? T(c, "Du bist online", "You're online") : T(c, "Du bist offline", "You're offline"));
            snprintf(h->go_text, sizeof(h->go_text), "%s", c->shift.on ? h->row_value[1]
                                                                       : T(c, "Starte eine Schicht für Anfragen.", "Start a shift to get requests."));
        }
    } else {
        h->tab_count = 2; /* 0.7: the job, its map */
        h->tab = c->job_tab;
        snprintf(h->tabs[0], sizeof(h->tabs[0]), "%s", T(c, "Auftrag", "Job"));
        snprintf(h->tabs[1], sizeof(h->tabs[0]), "%s", T(c, "Karte", "Map"));
        row(h, "Horizon Transit", h->tabs[c->job_tab], 1);
        if (c->mode == TS_UBER) {
            const TsPerson *who = &ts_people[c->job.passenger % PASSENGER_COUNT];
            snprintf(a, sizeof(a), "%s, %d · %s · %s", who->name, who->age, c->german ? who->job_de : who->job_en, type_name(c, c->job.ptype));
            row(h, a, NULL, 2);
            if (c->job.pool && c->pool_stage < 2) { /* 0.6: the second passenger instead of the pickup */
                snprintf(b, sizeof(b), "%s · %s", ts_passenger_name(c->job.pool_passenger),
                         c->pool_stage == 0 ? ts_place_name(c, c->job.pool_pickup) : T(c, "im Auto", "in the car"));
                row(h, T(c, "Mitfahrer", "Co-rider"), b, 0);
            } else
            row(h, T(c, "Abholort", "Pickup"), ts_place_name(c, c->job.pickup), 0);
            /* 0.5.1: how far along: to the pickup, then to the destination */
            if (c->stage == TS_UBER_TO_PICKUP && c->job.pickup_m > 1.0f && c->car_ok)
                row_meter(h, 1.0f - ts_distance_xz(c->car.pos, ts_places[c->job.pickup].pos) / c->job.pickup_m, 0);
            else if (c->stage > TS_UBER_TO_PICKUP) row_meter(h, 1.0f, 0);
            row(h, T(c, "Ziel", "Destination"), ts_place_name(c, c->job.dest), 0);
            if (c->stage == TS_UBER_TO_DEST && c->job.trip_m > 1.0f && c->car_ok)
                row_meter(h, 1.0f - ts_distance_xz(c->car.pos, ts_places[c->job.dest].pos) * TS_ROAD_FACTOR / c->job.trip_m, 0);
            else if (c->stage > TS_UBER_TO_DEST) row_meter(h, 1.0f, 0);
            snprintf(b, sizeof(b), "%s", ts_cr(c->german, c->job.pay));
            row(h, T(c, "Bezahlung (erwartet)", "Pay (expected)"), b, 0);
        } else {
            const TsLine *l = &ts_lines[c->line];
            snprintf(a, sizeof(a), "%s %s · %s", T(c, "Linie", "Line"), l->number, ts_line_title(c, c->line));
            row(h, a, NULL, 2);
            snprintf(b, sizeof(b), "%d / %u · %s", c->stop_i + 1, l->stop_count, ts_places[ts_bus_stop(c, c->line, c->stop_i)].name);
            row(h, T(c, "Haltestelle", "Stop"), b, 0);
            row_meter(h, (float)c->stop_i / (float)(l->stop_count > 1 ? l->stop_count - 1 : 1), 0); /* 0.5.1: the line so far */
            snprintf(b, sizeof(b), "%d / %d · %s", c->onboard, c->bus_capacity, ts_cr(c->german, c->bus_pay));
            row(h, T(c, "Fahrgäste", "Passengers"), b, 0);
            if (c->plan_started && c->stop_i > 0) snprintf(b, sizeof(b), "%d / %d %s", c->stops_on_time, c->stops_on_time + c->stops_late,
                                                           T(c, "pünktlich", "on time"));
            else snprintf(b, sizeof(b), "%s %d min", T(c, "ca.", "about"), (int)(tt_line_minutes(c, c->line) + 0.5f));
            row(h, T(c, "Fahrplan", "Timetable"), b, 0);
        }
        snprintf(b, sizeof(b), "%d %s", ts_speed_shown(c, ride_max_kmh(c)), ts_speed_unit(c));
        row(h, T(c, "Tempo höchstens", "Speed at most"), b, 0);
        snprintf(b, sizeof(b), "%d%% (%d/5)", (int)(c->sat + 0.5f), ts_stars(c->sat));
        row(h, T(c, "Zufriedenheit", "Satisfaction"), b, 0);
        row_meter(h, c->sat / 100.0f, 1);
        row(h, c->cancel_armed ? T(c, "Wirklich abbrechen?", "Really cancel?") : T(c, "Auftrag abbrechen", "Cancel job"),
            c->cancel_armed ? T(c, "nochmal drücken", "press again") : "", 0);
        h->row_selected = h->row_count - 1;
        h->map_keys = TS_MAPKEYS_JOBPAGE;
    }
    map_hud(c, h); /* 0.7: the map tab, the preview, the job's map */
}
