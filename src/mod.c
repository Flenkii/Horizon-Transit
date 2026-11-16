/* fh2-mods\TransportSim.dll - FH2 Transport Simulator, an FH2 Mod Platform mod.

   Taxi/Uber rides and bus lines with virtual passengers. The job logic is in src/core (no game calls);
   this file connects it to the game through the FH2 Mod Platform:
   - the own car's position and speed: the game's car update (ForzaCar::Update, game+0x2A9400), hooked
     through the platform so every other mod's hook on it keeps working (Gripline, ANNA, Underglow, ...);
   - the GPS: the game's own sat-nav (SatNavComponent::NavigateToPosition game+0x6B58B0 and
     ClearSatNavDestination game+0x6B4400), called on the game's thread from the sat-nav's update
     (game+0x6B4D30, hooked through the platform; FH2 ANNA hooks the same function);
   - the HUD: the platform's HUD layer (chips, a panel as the phone, a countdown, chat lines over the
     minimap) and its pop-ups in the game's own style;
   - keys: the platform's hotkeys (F9 / F10 / F11, rebindable on the mod's page);
   - a page under Pause > MODS > YOUR MODS > FH2 Transport Simulator with the same phone, for the pad.
   Settings and stats: <Azimuth folder>\TransportSim\TransportSim.ini - nothing of other mods is touched.

   Every game address is checked against the game's bytes before it is used; if one differs, that part
   stays off and the log says so. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#include "../third_party/fh2mod/fh2mod.h"
#include "../third_party/fh2mod/fh2mod_hud.h"
#include "core/jobs.h"
#include "core/stops.h"
#include "core/bus_sql.h"
#include "phone_window.h"

#define MOD_NAME "Horizon Transit"
#define MOD_VERSION "0.4.1"
#define MOD_FOLDER L"TransportSim"

/* ------------------------------------------------------------------ game addresses (checked below) */

#define RVA_FORZACAR_VTABLE 0x2C5CB80u
#define RVA_FORZACAR_UPDATE 0x2A9400u
#define CAR_KIND 0x4D08u         /* 0 = the local player's car (fh2game.h) */
#define CAR_DYNAMICS 0x4C08u     /* the car's physics body */
#define CAR_MEDIA_NAME 0x350u    /* the car's media name ("LAM_MiuraP400_67", maybe as a path), as Gripline's logbook
                                    found it in this game (its log: "LOGBOOK: car name found at +0x0350") */
#define RVA_DYNAMICS_VTABLE 0x2EB4BB8u
#define RVA_DYNAMICS_VTABLE_2 0x2EB5E18u
#define DYNAMICS_GET_STATE 0x1B8u /* vtable slot: (body, BodyState *out) */

#define RVA_SATNAV_VTABLE 0x2CC3188u
#define RVA_SATNAV_UPDATE 0x6B4D30u
#define RVA_NAVIGATE_TO_POSITION 0x6B58B0u
#define RVA_CLEAR_DESTINATION 0x6B4400u
#define SATNAV_ACTIVE 0x160u      /* byte: this sat-nav is in use */
#define SATNAV_CONTROL 0x150u

#define RVA_DB_QUERY 0x108F720u /* the game's database query: (db, result *out, const char *sql); NitroSystem and
                                  Underglow hook it too (platform.log: "database query") */
static const unsigned char DB_QUERY_BYTES[] = {0x48, 0x89, 0x54, 0x24, 0x10, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x30};
static const unsigned char CAR_UPDATE_BYTES[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x57, 0x48, 0x81, 0xEC, 0xA0};
static const unsigned char CAR_KIND_BYTES[] = {0x8B, 0x81, 0x08, 0x4D, 0x00, 0x00, 0xC3}; /* game+0xF62280 reads car+0x4D08 */
static const unsigned char SATNAV_UPDATE_BYTES[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE};
static const unsigned char NAVIGATE_BYTES[] = {0x80, 0xB9, 0x60, 0x01, 0x00, 0x00, 0x00, 0x74, 0x0C, 0x48, 0x8B,
                                               0x89, 0x50, 0x01, 0x00, 0x00, 0xE9, 0xCB, 0xCE, 0x39, 0x00};
static const unsigned char CLEAR_BYTES[] = {0x80, 0xB9, 0x60, 0x01, 0x00, 0x00, 0x00, 0x74, 0x0C, 0x48, 0x8B,
                                            0x89, 0x50, 0x01, 0x00, 0x00, 0xE9, 0xAB, 0x99, 0x39, 0x00};

typedef struct {
    __m128 position, velocity, spin, right, up, forward;
} BodyState;
typedef void (*CarUpdateFn)(void *car, float dt, float frame_dt, unsigned frame);
typedef void *(*SatNavUpdateFn)(void *satnav, float dt);
typedef void *(*GetStateFn)(void *body, BodyState *out);
typedef void (*NavigateFn)(void *satnav, const void *points);
typedef void (*ClearFn)(void *satnav);

/* ------------------------------------------------------------------ state */

static const FH2ModApi *api;
static FH2Mod *mod;
static const FH2HudInterface *hud;
static int hud_added;
static unsigned char *game;

static SRWLOCK lock = SRWLOCK_INIT; /* guards core and the nav fields below */
static TsCore core;

static CarUpdateFn original_car_update;
static SatNavUpdateFn original_satnav_update;
static int car_hook_on, nav_hook_on;
static volatile LONG car_samples; /* for the log: the car update is seen */

/* sat-nav: game thread only */
static unsigned nav_applied_serial;
static int nav_we_set; /* the GPS destination is ours (only then we clear it) */

/* settings */
static wchar_t ini_path[MAX_PATH];
static int opt_nav = 1, opt_bus_autoshow = 0, opt_time_limit = 1; /* 0 relaxed, 1 normal, 2 strict */
static int opt_language = 0;                                        /* 0 the game's, 1 Deutsch, 2 English */
static int opt_arrow_keys = 1; /* the phone takes the arrow keys, Enter and Backspace while it's open */
static int opt_bus_needs_bus = 1; /* bus lines only in the game's bus */
static int opt_extras = 1;        /* 0.4: reputation effects, special rides, wishes */
static int opt_phone_style = 0;   /* 0 Night, 1 Light, 2 Festival (the phone window), 3 the HUD list */
static uint32_t key_vk[3] = {VK_F9, VK_F10, VK_F11}, key_mod[3], key_pad[3] = {
    FH2_PAD_LB | FH2_PAD_RB | FH2_PAD_DPAD_UP, FH2_PAD_LB | FH2_PAD_RB | FH2_PAD_DPAD_RIGHT, FH2_PAD_LB | FH2_PAD_RB | FH2_PAD_A};
static const char *const key_names[3] = {"Phone", "Next", "Accept"};

/* asked by keys / the page, done by the worker under the lock */
static volatile LONG want_tab; /* -1 / +1 */
static volatile LONG want_toggle, want_next, want_prev, want_accept, want_row = -1, want_cancel;
static volatile LONG phone_is_open; /* copied from the core by the worker */

static FILE *log_file;

/* ------------------------------------------------------------------ log */

static void log_line(const char *format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (api && mod) api->log(mod, line);
    if (log_file) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        fprintf(log_file, "%02d:%02d:%02d.%03d %s\n", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, line);
        fflush(log_file);
    }
}

/* ------------------------------------------------------------------ settings and stats (own INI) */

static int ini_int(const char *section, const char *key, int fallback) {
    char text[64];
    wchar_t wsection[64], wkey[64], wtext[64];
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 64);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    if (!GetPrivateProfileStringW(wsection, wkey, L"", wtext, 64, ini_path)) return fallback;
    WideCharToMultiByte(CP_UTF8, 0, wtext, -1, text, sizeof(text), NULL, NULL);
    char *end;
    long long v = strtoll(text, &end, 10);
    return end == text ? fallback : (int)v;
}

static long long ini_ll(const char *section, const char *key, long long fallback) {
    wchar_t wsection[64], wkey[64], wtext[64];
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 64);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    if (!GetPrivateProfileStringW(wsection, wkey, L"", wtext, 64, ini_path)) return fallback;
    wchar_t *end;
    long long v = wcstoll(wtext, &end, 10);
    return end == wtext ? fallback : v;
}

static void ini_put(const char *section, const char *key, const char *value) {
    wchar_t wsection[64], wkey[64], wvalue[128];
    MultiByteToWideChar(CP_UTF8, 0, section, -1, wsection, 64);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, 64);
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, 128);
    WritePrivateProfileStringW(wsection, wkey, wvalue, ini_path);
}

static void ini_put_int(const char *section, const char *key, long long value) {
    char text[32];
    snprintf(text, sizeof(text), "%lld", value);
    ini_put(section, key, text);
}

static void load_settings(void) {
    opt_nav = ini_int("Settings", "AutoNavigation", 1) != 0;
    opt_arrow_keys = ini_int("Settings", "PhoneArrowKeys", 1) != 0;
    opt_bus_needs_bus = ini_int("Settings", "BusLinesNeedBus", 1) != 0;
    opt_bus_autoshow = ini_int("Settings", "BusInAutoshow", 0) != 0;
    opt_extras = ini_int("Settings", "CareerExtras", 1) != 0;
    opt_time_limit = ini_int("Settings", "TimeLimit", 1);
    if (opt_time_limit < 0 || opt_time_limit > 2) opt_time_limit = 1;
    opt_phone_style = ini_int("Settings", "PhoneStyle", 0);
    if (opt_phone_style < 0 || opt_phone_style > 3) opt_phone_style = 0;
    opt_language = ini_int("Settings", "Language", 0);
    if (opt_language < 0 || opt_language > 2) opt_language = 0;
    for (int i = 0; i < 3; ++i) {
        char k[32];
        snprintf(k, sizeof(k), "%sKey", key_names[i]);
        key_vk[i] = (uint32_t)ini_int("Keys", k, (int)key_vk[i]);
        snprintf(k, sizeof(k), "%sModifiers", key_names[i]);
        key_mod[i] = (uint32_t)ini_int("Keys", k, (int)key_mod[i]);
        snprintf(k, sizeof(k), "%sPad", key_names[i]);
        key_pad[i] = (uint32_t)ini_int("Keys", k, (int)key_pad[i]);
    }
}

static void save_settings(void) {
    ini_put_int("Settings", "AutoNavigation", opt_nav);
    ini_put_int("Settings", "PhoneArrowKeys", opt_arrow_keys);
    ini_put_int("Settings", "BusLinesNeedBus", opt_bus_needs_bus);
    ini_put_int("Settings", "BusInAutoshow", opt_bus_autoshow);
    ini_put_int("Settings", "CareerExtras", opt_extras);
    ini_put_int("Settings", "TimeLimit", opt_time_limit);
    ini_put_int("Settings", "Language", opt_language);
    ini_put_int("Settings", "PhoneStyle", opt_phone_style);
    for (int i = 0; i < 3; ++i) {
        char k[32];
        snprintf(k, sizeof(k), "%sKey", key_names[i]);
        ini_put_int("Keys", k, key_vk[i]);
        snprintf(k, sizeof(k), "%sModifiers", key_names[i]);
        ini_put_int("Keys", k, key_mod[i]);
        snprintf(k, sizeof(k), "%sPad", key_names[i]);
        ini_put_int("Keys", k, key_pad[i]);
    }
}

/* under the lock */
static void load_stats(void) {
    core.wallet = ini_ll("Stats", "Wallet", 0);
    core.jobs_done = ini_int("Stats", "Rides", 0);
    core.bus_lines_done = ini_int("Stats", "BusLines", 0);
    core.passengers_total = ini_int("Stats", "Passengers", 0);
    core.km_total = (double)ini_ll("Stats", "Metres", 0) / 1000.0;
    core.rating_sum = ini_ll("Stats", "RatingSum", 0);
    core.rating_count = ini_int("Stats", "RatingCount", 0);
    /* rides with each passenger, two hex digits each */
    wchar_t met[2 * TS_PEOPLE_MAX + 4];
    GetPrivateProfileStringW(L"Stats", L"Met", L"", met, (DWORD)(sizeof(met) / sizeof(met[0])), ini_path);
    for (int i = 0; i < TS_PEOPLE_MAX && met[2 * i] && met[2 * i + 1]; ++i) {
        wchar_t two[3] = {met[2 * i], met[2 * i + 1], 0};
        core.met[i] = (unsigned char)wcstoul(two, NULL, 16);
    }
    /* 0.4: the career */
    core.reputation = (float)ini_int("Career", "Reputation10", 500) / 10.0f;
    if (core.reputation < 0.0f || core.reputation > 100.0f) core.reputation = 50.0f;
    core.tips_total = ini_ll("Career", "Tips", 0);
    core.upgrades = (unsigned)ini_int("Company", "Upgrades", 0) & ((1u << TS_SHOP_ITEMS) - 1);
    core.crashes_total = ini_int("Career", "Crashes", 0);
    core.on_time_total = ini_int("Career", "OnTime", 0);
    core.late_total = ini_int("Career", "Late", 0);
    core.five_stars = ini_int("Career", "FiveStars", 0);
    core.specials_done = ini_int("Career", "Specials", 0);
    core.wishes_ok = ini_int("Career", "WishesOk", 0);
    core.wishes_failed = ini_int("Career", "WishesFailed", 0);
    core.best_pay = ini_int("Career", "BestPay", 0);
    core.longest_m = (float)ini_int("Career", "LongestMetres", 0);
    for (int i = 0; i < 8; ++i) {
        char k[16];
        snprintf(k, sizeof(k), "Line%d", i + 1);
        core.line_done[i] = ini_int("Career", k, 0);
    }
    /* per passenger: the last car (4 hex digits) and the last stars (1 digit) */
    wchar_t mem[5 * TS_PEOPLE_MAX + 4];
    GetPrivateProfileStringW(L"Career", L"Memory", L"", mem, (DWORD)(sizeof(mem) / sizeof(mem[0])), ini_path);
    for (int i = 0; i < TS_PEOPLE_MAX; ++i) {
        int ok = 1;
        for (int k = 0; k < 5; ++k) ok &= mem[5 * i + k] != 0;
        if (!ok) break;
        wchar_t car4[5] = {mem[5 * i], mem[5 * i + 1], mem[5 * i + 2], mem[5 * i + 3], 0}, star[2] = {mem[5 * i + 4], 0};
        unsigned long car = wcstoul(car4, NULL, 16), stars = wcstoul(star, NULL, 16);
        core.met_car[i] = (unsigned short)(car <= ts_car_count ? car : 0);
        core.met_stars[i] = (unsigned char)(stars <= 5 ? stars : 0);
    }
}

static void save_stats(const TsCore *c) {
    ini_put_int("Stats", "Wallet", c->wallet);
    ini_put_int("Stats", "Rides", c->jobs_done);
    ini_put_int("Stats", "BusLines", c->bus_lines_done);
    ini_put_int("Stats", "Passengers", c->passengers_total);
    ini_put_int("Stats", "Metres", (long long)(c->km_total * 1000.0));
    ini_put_int("Stats", "RatingSum", c->rating_sum);
    ini_put_int("Stats", "RatingCount", c->rating_count);
    char met[2 * TS_PEOPLE_MAX + 1];
    for (int i = 0; i < TS_PEOPLE_MAX; ++i) snprintf(met + 2 * i, 3, "%02x", c->met[i]);
    ini_put("Stats", "Met", met);
    ini_put_int("Career", "Reputation10", (long long)(c->reputation * 10.0f + 0.5f));
    ini_put_int("Career", "Tips", c->tips_total);
    ini_put_int("Company", "Upgrades", c->upgrades);
    ini_put_int("Career", "Crashes", c->crashes_total);
    ini_put_int("Career", "OnTime", c->on_time_total);
    ini_put_int("Career", "Late", c->late_total);
    ini_put_int("Career", "FiveStars", c->five_stars);
    ini_put_int("Career", "Specials", c->specials_done);
    ini_put_int("Career", "WishesOk", c->wishes_ok);
    ini_put_int("Career", "WishesFailed", c->wishes_failed);
    ini_put_int("Career", "BestPay", c->best_pay);
    ini_put_int("Career", "LongestMetres", (long long)c->longest_m);
    for (int i = 0; i < 8; ++i) {
        char k[16];
        snprintf(k, sizeof(k), "Line%d", i + 1);
        ini_put_int("Career", k, c->line_done[i]);
    }
    char mem[5 * TS_PEOPLE_MAX + 1];
    for (int i = 0; i < TS_PEOPLE_MAX; ++i) snprintf(mem + 5 * i, 6, "%04x%x", c->met_car[i], c->met_stars[i] & 15);
    ini_put("Career", "Memory", mem);
}

static float time_factor(void) {
    return opt_time_limit == 0 ? 1.4f : opt_time_limit == 2 ? 0.8f : 1.0f;
}

/* ------------------------------------------------------------------ the game: the own car */

/* The bytes at game+rva as the game's exe file on disk has them (the PE headers in memory map rva to the
   file offset). Needed because another mod (FH2 ANNA, Gripline, ...) may already have hooked the same
   function, and then memory starts with its jump instead of the game's code. */
static int file_code_is(unsigned rva, const unsigned char *bytes, size_t size) {
    if (!game || !api->game_dir || !api->game_dir[0]) return 0;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)game;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(game + dos->e_lfanew);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    DWORD offset = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        if (rva >= section->VirtualAddress && rva + size <= section->VirtualAddress + section->SizeOfRawData) {
            offset = rva - section->VirtualAddress + section->PointerToRawData;
            break;
        }
    if (!offset) return 0;
    wchar_t path[MAX_PATH];
    if (_snwprintf(path, MAX_PATH, L"%ls\\forza_durango_release_final.exe", api->game_dir) <= 0) return 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    unsigned char got[64];
    DWORD read = 0;
    LARGE_INTEGER at;
    at.QuadPart = offset;
    int ok = size <= sizeof(got) && SetFilePointerEx(file, at, NULL, FILE_BEGIN) && ReadFile(file, got, (DWORD)size, &read, NULL) &&
             read == size && memcmp(got, bytes, size) == 0;
    CloseHandle(file);
    return ok;
}

/* The game's code is the one this mod was made for: in memory, or (when another mod's hook jump already sits
   at the start) in the exe file. */
static int code_is(unsigned rva, const unsigned char *bytes, size_t size) {
    if (!game) return 0;
    if (memcmp(game + rva, bytes, size) == 0) return 1;
    int hooked = game[rva] == 0xE9 || (game[rva] == 0xFF && game[rva + 1] == 0x25);
    int ok = hooked && file_code_is(rva, bytes, size);
    log_line(ok ? "game+0x%X: another mod's hook is already there; the exe file has the expected code, so it's shared"
                : "game+0x%X: the code differs from what this mod expects (first byte %02X)", rva, game[rva]);
    return ok;
}

static int readable(const void *p, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    if (!p || !VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT) return 0;
    if (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    return (const unsigned char *)p + size <= (const unsigned char *)info.BaseAddress + info.RegionSize;
}

/* the car's media name at p: plain text, or a pointer to it; a path and an extension are cut off */
static int media_text(const unsigned char *p, char *out, size_t size) {
    if (!readable(p, 48)) return 0;
    size_t n = 0;
    for (size_t k = 0; k < 48; ++k) {
        unsigned char ch = p[k];
        if (!ch) break;
        if (ch < 33 || ch > 126) return 0;
        if (ch == '/' || ch == '\\' || ch == ':') {
            n = 0;
            continue;
        }
        if (ch == '.') break;
        if (n + 1 >= size) return 0;
        out[n++] = (char)ch;
    }
    out[n] = 0;
    return n >= 5 && (out[2] == '_' || out[3] == '_');
}

static int car_media_name(unsigned char *car, char *out, size_t size) {
    if (media_text(car + CAR_MEDIA_NAME, out, size)) return 1;
    if (!readable(car + CAR_MEDIA_NAME, 8)) return 0;
    const unsigned char *p = *(const unsigned char **)(car + CAR_MEDIA_NAME);
    return (uintptr_t)p >= 0x10000 && media_text(p, out, size);
}

static void *named_car;
static unsigned name_checks;
static char car_media[48];

static void car_update(void *car, float dt, float frame_dt, unsigned frame) {
    original_car_update(car, dt, frame_dt, frame);
    unsigned char *c = (unsigned char *)car;
    if (!c || *(void **)c != game + RVA_FORZACAR_VTABLE || *(int *)(c + CAR_KIND) != 0) return;
    void *body = *(void **)(c + CAR_DYNAMICS);
    if (!body) return;
    void *vtable = *(void **)body;
    if (vtable != game + RVA_DYNAMICS_VTABLE && vtable != game + RVA_DYNAMICS_VTABLE_2) return;
    GetStateFn get = *(GetStateFn *)((unsigned char *)vtable + DYNAMICS_GET_STATE);
    _Alignas(16) BodyState state;
    get(body, &state);
    TsCar sample;
    _Alignas(16) float p[4], v[4], f[4], r[4];
    _mm_store_ps(p, state.position);
    _mm_store_ps(v, state.velocity);
    _mm_store_ps(f, state.forward);
    _mm_store_ps(r, state.right);
    for (int i = 0; i < 3; ++i) sample.pos[i] = p[i], sample.vel[i] = v[i], sample.fwd[i] = f[i], sample.right[i] = r[i];
    /* which car: when it changes, and now and then (a car swap can reuse the object) */
    int read_name = car != named_car || (++name_checks % 120) == 0;
    char media[48] = {0};
    if (read_name) {
        named_car = car;
        if (!car_media_name(c, media, sizeof(media))) media[0] = 0;
    }
    AcquireSRWLockExclusive(&lock);
    ts_car_sample(&core, dt, &sample);
    if (read_name) ts_set_car(&core, media);
    ReleaseSRWLockExclusive(&lock);
    if (read_name && strcmp(media, car_media)) {
        snprintf(car_media, sizeof(car_media), "%s", media);
        const TsCarInfo *info = ts_car_find(media);
        static const char *const kinds[] = {"unknown", "normal", "sporty", "dream car", "van", "bus"};
        if (!media[0]) log_line("driven car: its name wasn't found at car+0x%X", CAR_MEDIA_NAME);
        else log_line("driven car: %s (%s %s, %d CR, %s)", media, info ? info->make : "?", info ? info->model : "not in the game's table",
                      info ? info->cost : 0, kinds[info ? info->kind : 1]);
    }
    InterlockedIncrement(&car_samples);
}

/* ------------------------------------------------------------------ the game: the GPS */

/* the game's GPS says its destination is reached (as Gripline found it: route control +0x8A) */
static int gps_reached(unsigned char *satnav) {
    unsigned char *control, *route_control, *route;
    if (!readable(satnav + SATNAV_CONTROL, 8)) return 0;
    control = *(unsigned char **)(satnav + SATNAV_CONTROL);
    if (!readable(control, 8)) return 0;
    route_control = *(unsigned char **)control;
    if (!readable(route_control, 0x120)) return 0;
    route = *(unsigned char **)(route_control + 0x118);
    if (!readable(route, 16) || !route[8]) return 0;
    return route_control[0x8A] != 0;
}

static void *satnav_update(void *satnav, float dt) {
    void *result = original_satnav_update(satnav, dt);
    unsigned char *s = (unsigned char *)satnav;
    if (!s || *(void **)s != game + RVA_SATNAV_VTABLE || !s[SATNAV_ACTIVE]) return result;

    AcquireSRWLockShared(&lock);
    unsigned serial = core.nav_serial;
    int on = core.nav_on;
    float at[3] = {core.nav_target[0], core.nav_target[1], core.nav_target[2]};
    ReleaseSRWLockShared(&lock);

    if (serial != nav_applied_serial) {
        nav_applied_serial = serial;
        if (on && opt_nav) {
            _Alignas(16) __m128 point = _mm_setr_ps(at[0], at[1], at[2], 0.0f);
            struct {
                __m128 *first, *last, *end;
            } points = {&point, &point + 1, &point + 1}; /* std::vector<__m128> */
            ((NavigateFn)(game + RVA_NAVIGATE_TO_POSITION))(satnav, &points);
            nav_we_set = 1;
        } else if (nav_we_set) {
            ((ClearFn)(game + RVA_CLEAR_DESTINATION))(satnav);
            nav_we_set = 0;
        }
    }
    int reached = nav_we_set && gps_reached(s);
    AcquireSRWLockExclusive(&lock);
    core.gps_arrived = reached;
    ReleaseSRWLockExclusive(&lock);
    return result;
}

static void bus_autoshow_hook(void);

static void install_hooks(void) {
    if (api->api_version < 3) {
        log_line("the platform is too old for hooks (API %u): nothing works", api->api_version);
        return;
    }
    if (code_is(RVA_FORZACAR_UPDATE, CAR_UPDATE_BYTES, sizeof(CAR_UPDATE_BYTES)) &&
        code_is(0xF62280u, CAR_KIND_BYTES, sizeof(CAR_KIND_BYTES))) {
        FH2Hook *hook = api->hook_function(mod, "ForzaCar::Update (Transport: own car)", game + RVA_FORZACAR_UPDATE,
                                           (void *)car_update, (void **)&original_car_update);
        car_hook_on = hook && original_car_update && api->enable_hooks(mod, hook);
    }
    log_line(car_hook_on ? "car update hooked: the own car's position and speed are read"
                         : "car update NOT hooked (game code differs or the platform refused, see above): jobs can't work");
    if (code_is(RVA_SATNAV_UPDATE, SATNAV_UPDATE_BYTES, sizeof(SATNAV_UPDATE_BYTES)) &&
        code_is(RVA_NAVIGATE_TO_POSITION, NAVIGATE_BYTES, sizeof(NAVIGATE_BYTES)) &&
        code_is(RVA_CLEAR_DESTINATION, CLEAR_BYTES, sizeof(CLEAR_BYTES))) {
        FH2Hook *hook = api->hook_function(mod, "SatNavComponent update (Transport: GPS target)", game + RVA_SATNAV_UPDATE,
                                           (void *)satnav_update, (void **)&original_satnav_update);
        nav_hook_on = hook && original_satnav_update && api->enable_hooks(mod, hook);
    }
    log_line(nav_hook_on ? "sat-nav hooked: the GPS is set to each pickup, destination and stop"
                         : "sat-nav NOT hooked: no GPS route (the HUD still shows the distance)");
}

/* ------------------------------------------------------------------ HUD */

static TsHud shown_hud;
static uint32_t shown_parts;
static int hud_was_set;

static uint32_t hud_ok(void) {
    return hud && hud_added ? hud->hud_status(mod) : 0;
}

static void push_hud(const TsHud *h) {
    uint32_t status = hud_ok(); /* also keeps the layer's lease */
    if (!(status & FH2_HUD_AVAILABLE)) return;
    uint32_t parts = (h->chips ? FH2_HUD_PART_CHIPS : 0) | (h->panel ? FH2_HUD_PART_PANEL : 0) |
                     (h->countdown ? FH2_HUD_PART_COUNTDOWN : 0);
    int changed = !hud_was_set || memcmp(h, &shown_hud, sizeof(*h)) != 0;
    if (!changed && parts == shown_parts) return;

    FH2HudState s;
    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.chips[0].text = h->chip0;
    s.chips[1].text = h->chip1;
    s.chips[2].text = h->chip2;
    if (h->chip3[0]) s.chips[3].text = h->chip3; /* a wish or the timetable */
    s.corner.text = h->corner;
    if (h->stars) s.corner.dots = 5, s.corner.dots_filled = (uint8_t)h->stars;
    for (int i = 0; i < h->row_count && i < FH2_HUD_MAX_ROWS; ++i) {
        s.rows[i].label = h->row_label[i];
        s.rows[i].value = h->row_value[i];
        s.rows[i].style = h->row_heading[i] == 1 ? FH2_ROW_HEADING : h->row_heading[i] == 2 ? FH2_ROW_DIM : FH2_ROW_NORMAL;
    }
    s.row_count = (uint32_t)h->row_count;
    s.row_selected = h->row_selected < 0 ? FH2_HUD_MAX_ROWS : (uint32_t)h->row_selected;
    /* the bar under the phone: the app's name, its tabs, the keys */
    for (int i = 0; i < h->tab_count && i < FH2_HUD_MAX_TABS; ++i) s.tabs[i] = h->tabs[i];
    s.tab = (uint32_t)h->tab;
    static const char *const tab_keys[2] = {"Left", "Right"};
    if (h->tab_count > 1) s.tab_keys[0] = tab_keys[0], s.tab_keys[1] = tab_keys[1];
    s.line1 = h->line1;
    s.line2 = h->line2;
    if (h->panel) {
        int de = h->german;
        int job = h->tab_count == 1;
        FH2HudPrompt *pr = s.prompts;
        pr[0].glyph = FH2_GLYPH_DPAD_UP, pr[0].chord = 1, pr[0].key = "Backspace", pr[0].label = de ? "Schließen" : "Close";
        pr[1].glyph = FH2_GLYPH_A, pr[1].chord = 1, pr[1].key = "Enter",
        pr[1].label = job ? (de ? "Abbrechen" : "Cancel") : (de ? "Annehmen" : "Take");
        s.prompt_count = 2;
        if (!job) {
            pr[2].glyph = FH2_GLYPH_DPAD_RIGHT, pr[2].chord = 1, pr[2].key = "Up/Down", pr[2].label = de ? "Wählen" : "Choose";
            s.prompt_count = 3;
        }
    }
    s.panel_footer = h->footer;
    s.countdown = (uint32_t)h->countdown;
    s.countdown_label = h->countdown_label;

    if (parts != shown_parts && (shown_parts & ~parts)) { /* parts going out: hide first, then the new state */
        hud->show_hud_layer(mod, parts);
        hud->set_hud_state(mod, &s);
    } else {
        hud->set_hud_state(mod, &s);
        if (parts != shown_parts || (status & (FH2_HUD_TAKEN | FH2_HUD_LAPSED) && parts && changed))
            hud->show_hud_layer(mod, parts);
    }
    shown_parts = parts;
    shown_hud = *h;
    hud_was_set = 1;
}

static void play(int which) {
    uint32_t sound = which == TS_SOUND_NOTIFY ? FH2_SOUND_NOTIFY : which == TS_SOUND_TICK ? FH2_SOUND_TICK
                   : which == TS_SOUND_TICK_END ? FH2_SOUND_TICK_END : which == TS_SOUND_ERROR ? FH2_SOUND_ERROR : 0;
    if (sound && api->api_version >= 10) api->play_ui_sound(mod, sound);
}

static void deliver(const TsEvent *e) {
    if (e->kind == TS_EV_TOAST) {
        log_line("pop-up: %s | %s", e->title, e->text);
        if (api->api_version < 10) return;
        FH2Toast t;
        memset(&t, 0, sizeof(t));
        t.size = sizeof(t);
        t.title = e->title;
        t.subtitle = e->text;
        t.icon = e->icon == TS_ICON_PAYOUT ? FH2_ICON_PAYOUT : e->icon == TS_ICON_ROADS ? FH2_ICON_ROADS
               : e->icon == TS_ICON_TROPHY ? FH2_ICON_TROPHY : FH2_ICON_NONE;
        t.sound = e->sound == TS_SOUND_NOTIFY ? FH2_SOUND_NOTIFY : 0;
        t.expire_s = 30;
        api->show_toast(mod, &t);
    } else if (e->kind == TS_EV_FEED) {
        log_line("chat: %s: %s", e->title, e->text);
        if (hud && hud_added && hud->version >= 2) {
            FH2HudFeedLine line;
            memset(&line, 0, sizeof(line));
            line.size = sizeof(line);
            line.name = e->title;
            line.text = e->text;
            line.style = (uint32_t)e->style;
            line.ms = 7000;
            hud->post_hud_feed(mod, &line);
        }
    } else if (e->kind == TS_EV_SOUND) {
        play(e->sound);
    }
}

/* Without the HUD layer: the job's status as a pop-up every 25 s */
static ULONGLONG fallback_next;
static void fallback_status(const TsHud *h, int job) {
    if (!job || api->api_version < 10 || (hud_ok() & FH2_HUD_AVAILABLE)) return;
    ULONGLONG now = GetTickCount64();
    if (now < fallback_next) return;
    fallback_next = now + 25000;
    char title[64], text[96];
    snprintf(title, sizeof(title), "%s · %s", h->chip0, h->chip1);
    snprintf(text, sizeof(text), "%s", h->corner);
    FH2Toast t;
    memset(&t, 0, sizeof(t));
    t.size = sizeof(t);
    t.key = "status";
    t.title = title;
    t.subtitle = text;
    t.expire_s = 20;
    api->show_toast(mod, &t);
}

/* Without the HUD layer: the phone as the game's pop-ups. Each F9 / F10 shows the selected entry; F11 takes it.
   (In a session where the game didn't read the platform's HUD page, the layer is off: platform.log says
   "the HUD page was shown without the layer's values".) */
static char phone_shown[256];
static void phone_popup(const TsHud *h, int de, int job) {
    if (api->api_version < 10 || (hud_ok() & FH2_HUD_AVAILABLE)) {
        phone_shown[0] = 0;
        return;
    }
    if (!h->panel) {
        if (phone_shown[0]) api->clear_toast(mod, "phone");
        phone_shown[0] = 0;
        return;
    }
    int sel = h->row_selected;
    if (sel < 0 || sel >= h->row_count) return;
    char key[256];
    snprintf(key, sizeof(key), "%d|%s|%s", sel, h->row_label[sel], h->row_value[sel]);
    if (!strcmp(key, phone_shown)) return;
    snprintf(phone_shown, sizeof(phone_shown), "%s", key);

    char title[64], text[128];
    int number = 0, total = 0;
    for (int i = 0; i < h->row_count; ++i)
        if (h->row_heading[i] == 0) total++, number += i <= sel;
    snprintf(title, sizeof(title), "%s", h->row_label[sel]);
    if (job)
        snprintf(text, sizeof(text), "%s", h->row_value[sel][0] ? (de ? "Enter = wirklich abbrechen" : "Enter = cancel for real")
                                                                : (de ? "Enter 2x = abbrechen, F9 = zu" : "Enter twice = cancel, F9 = close"));
    else
        snprintf(text, sizeof(text), "%s · %s (%d/%d)", h->row_value[sel], de ? "Enter nehmen" : "Enter take", number, total);
    FH2Toast t;
    memset(&t, 0, sizeof(t));
    t.size = sizeof(t);
    t.key = "phone";
    t.title = title;
    t.subtitle = text;
    t.expire_s = 15;
    api->show_toast(mod, &t);
    log_line("phone as pop-up: %s | %s", title, text);
}

/* ------------------------------------------------------------------ the phone's keys (arrows, Enter, Backspace)

   Wished for on 16.11.2026: navigate the phone with the arrow keys and Enter, and while it is open those keys
   must not do anything in the game. Every keyboard reader here asks Windows with GetAsyncKeyState: the
   platform's hotkeys, Gripline's keyboard driving and FH2 Mouse & Keyboard (both turn keys into the
   controller, the arrows are its turn signals / Gripline's camera, Enter is A / Interact). So
   user32!GetAsyncKeyState is hooked through the platform (shared with any other mod): while the phone is
   open and the game is in front, these keys read as up for everyone, and only this mod reads them, through
   the original. A key still held when the phone closes stays up for the others until it is let go, so the
   Enter that took a job doesn't also press A in the game. Nothing else is changed, no file of another mod. */

typedef SHORT(WINAPI *AsyncKeyFn)(int);
static AsyncKeyFn original_async_key;
static volatile LONG phone_keys_on;
static volatile LONG swallowed[256];
static const int phone_vks[] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_BACK};

static int phone_vk(int vk) {
    for (unsigned i = 0; i < sizeof(phone_vks) / sizeof(phone_vks[0]); ++i)
        if (phone_vks[i] == vk) return 1;
    return 0;
}

static SHORT WINAPI async_key_detour(int vk) {
    SHORT state = original_async_key(vk);
    if (vk > 0 && vk < 256 && phone_vk(vk)) {
        if (phone_keys_on) return 0;
        if (swallowed[vk]) {
            if (state & 0x8000) return 0;
            InterlockedExchange(&swallowed[vk], 0);
            return 0; /* also hides "pressed since the last call" */
        }
    }
    return state;
}

static int game_in_front(void) {
    HWND window = GetForegroundWindow();
    DWORD pid = 0;
    if (window) GetWindowThreadProcessId(window, &pid);
    return pid == GetCurrentProcessId();
}

static void install_key_hook(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    void *target = user32 ? (void *)GetProcAddress(user32, "GetAsyncKeyState") : NULL;
    FH2Hook *hook = target ? api->hook_function(mod, "user32!GetAsyncKeyState (Transport: the phone's arrow keys)", target,
                                                (void *)async_key_detour, (void **)&original_async_key)
                           : NULL;
    if (hook && original_async_key && api->enable_hooks(mod, hook))
        log_line("arrow keys: while the phone is open, Up/Down/Left/Right/Enter/Backspace go to the phone only");
    else {
        original_async_key = NULL;
        log_line("arrow keys: GetAsyncKeyState NOT hooked: the phone still reads them, but the game sees them too");
    }
}

static DWORD WINAPI keys_thread(LPVOID unused) {
    (void)unused;
    int down[256] = {0};
    ULONGLONG repeat_at[256] = {0};
    for (;;) {
        Sleep(15);
        AsyncKeyFn read = original_async_key ? original_async_key : GetAsyncKeyState;
        LONG want = opt_arrow_keys && phone_is_open && game_in_front();
        if (phone_keys_on && !want && original_async_key) /* closing: held keys stay away from the game */
            for (unsigned i = 0; i < sizeof(phone_vks) / sizeof(phone_vks[0]); ++i)
                if (read(phone_vks[i]) & 0x8000) InterlockedExchange(&swallowed[phone_vks[i]], 1);
        if (original_async_key) InterlockedExchange(&phone_keys_on, want);
        ULONGLONG now = GetTickCount64();
        for (unsigned i = 0; i < sizeof(phone_vks) / sizeof(phone_vks[0]); ++i) {
            int vk = phone_vks[i];
            int is_down = want && (read(vk) & 0x8000) != 0;
            int press = 0;
            if (is_down && !down[vk]) press = 1, repeat_at[vk] = now + 400;
            else if (is_down && (vk == VK_UP || vk == VK_DOWN) && now >= repeat_at[vk]) press = 1, repeat_at[vk] = now + 150;
            down[vk] = is_down;
            if (!press) continue;
            if (vk == VK_UP) InterlockedExchange(&want_prev, 1);
            else if (vk == VK_DOWN) InterlockedExchange(&want_next, 1);
            else if (vk == VK_RETURN) InterlockedExchange(&want_accept, 1);
            else if (vk == VK_BACK) InterlockedExchange(&want_toggle, 1);
            else if (vk == VK_LEFT) InterlockedExchange(&want_tab, -1);
            else if (vk == VK_RIGHT) InterlockedExchange(&want_tab, 1);
        }
    }
}

/* ------------------------------------------------------------------ the worker */

static int language_german(void) {
    if (opt_language) return opt_language == 1;
    char code[8] = {0};
    if (api->api_version >= 10 && api->game_language(code, sizeof(code))) return strcmp(code, "DE") == 0;
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
}

/* ------------------------------------------------------------------ the phone's Settings tab: the same settings as the mod menu */

static void on_setting(void *user, const char *key, int value);

typedef struct {
    const char *key, *de, *en;
    int *value, count;
} PhoneSetting;

static PhoneSetting phone_settings[TS_SETTING_ROWS] = {
    {"PhoneStyle", "Handy-Design", "Phone design", &opt_phone_style, 4},
    {"Language", "Sprache", "Language", &opt_language, 3},
    {"TimeLimit", "Zeitlimits", "Time limits", &opt_time_limit, 3},
    {"AutoNavigation", "Navigation automatisch", "Auto GPS", &opt_nav, 2},
    {"CareerExtras", "Ruf, Sonderfahrten, Wünsche", "Reputation, specials, wishes", &opt_extras, 2},
    {"BusLinesNeedBus", "Buslinien nur im Bus", "Bus lines need the bus", &opt_bus_needs_bus, 2},
    {"PhoneArrowKeys", "Handy mit Pfeiltasten", "Phone with arrow keys", &opt_arrow_keys, 2},
};

static const char *phone_setting_value(int i, int de) {
    static const char *const styles[4] = {"Nacht", "Hell", "Festival", "HUD-Liste"}, *const styles_en[4] = {"Night", "Light", "Festival", "HUD list"};
    static const char *const langs[3] = {"Spiel", "Deutsch", "English"}, *const langs_en[3] = {"Game", "Deutsch", "English"};
    static const char *const limits[3] = {"Locker", "Normal", "Streng"}, *const limits_en[3] = {"Relaxed", "Normal", "Strict"};
    int v = *phone_settings[i].value;
    if (v < 0 || v >= phone_settings[i].count) v = 0;
    switch (i) {
    case 0: return de ? styles[v] : styles_en[v];
    case 1: return de ? langs[v] : langs_en[v];
    case 2: return de ? limits[v] : limits_en[v];
    default: return v ? (de ? "An" : "On") : (de ? "Aus" : "Off");
    }
}

/* under the lock */
static void fill_phone_settings(TsCore *c) {
    c->setting_count = TS_SETTING_ROWS;
    for (int i = 0; i < TS_SETTING_ROWS; ++i) {
        snprintf(c->setting_label[i], sizeof(c->setting_label[i]), "%s", c->german ? phone_settings[i].de : phone_settings[i].en);
        snprintf(c->setting_value[i], sizeof(c->setting_value[i]), "%s", phone_setting_value(i, c->german));
    }
}

/* not under the lock (on_setting may take it): the next value, shown in the mod menu too */
static void phone_setting_press(int i) {
    if (i < 0 || i >= TS_SETTING_ROWS) return;
    const PhoneSetting *s = &phone_settings[i];
    int next = (*s->value + 1) % s->count;
    on_setting(NULL, s->key, next);
    if (api->api_version >= 4) api->set_setting(mod, s->key, next);
    log_line("phone: setting %s = %d", s->key, next);
}

/* Which speed unit the game shows. The game keeps a locale object (pointer at game+RVA_LOCALE): at +0x2C its
   Units option (-1 = as the language, 0 = English units, 2 = metric), at +0x28 the index of the language, and from the
   object's start one 0xE0-byte entry per language whose +0x98 is that language's unit system. 0 and 1 mean miles. -1: not readable. */
#define RVA_LOCALE 0x36629A0u
static int game_imperial(void) {
    if (!game) return -1;
    const unsigned char *const *slot = (const unsigned char *const *)(game + RVA_LOCALE);
    if (!readable(slot, sizeof(*slot))) return -1;
    const unsigned char *loc = *slot;
    if (!readable(loc + 0x28, 8)) return -1;
    int option = *(const int *)(loc + 0x2C);
    int units = option;
    if (option == -1) {
        int lang = *(const int *)(loc + 0x28);
        if (lang < 0 || lang > 64) return -1;
        const unsigned char *entry = loc + (size_t)lang * 0xE0u; /* the language entries lie in the object itself */
        if (!readable(entry + 0x98, 4)) return -1;
        units = *(const int *)(entry + 0x98);
    }
    if (units < 0 || units > 8) return -1;
    return units == 0 || units == 1;
}

static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    game = (unsigned char *)api->wait_for_code(0xFFFFFFFFu);
    if (!game) {
        log_line("the game never came (wait_for_code): nothing to do");
        return 0;
    }
    log_line("game at %p", (void *)game);
    install_hooks();
    bus_autoshow_hook();
    int logged_car = 0, logged_page_wait = 0, units_tick = 0, imperial = -1;
    for (;;) {
        Sleep(100);
        if (units_tick-- <= 0) { /* the game's unit setting, every 3 seconds */
            units_tick = 30;
            int now = game_imperial();
            if (now >= 0 && now != imperial) {
                imperial = now;
                log_line("speed unit from the game: %s", now ? "mph" : "km/h");
            }
        }
        TsEvent events[TS_MAX_EVENTS];
        int n = 0, save = 0, job = 0, de = 0;
        TsHud h;
        TsCore stats;
        AcquireSRWLockExclusive(&lock);
        core.german = language_german();
        core.time_factor = time_factor();
        core.bus_needs_bus = opt_bus_needs_bus;
        core.extras_off = !opt_extras;
        if (imperial >= 0) core.imperial = imperial;
        if (InterlockedExchange(&want_toggle, 0)) ts_phone_toggle(&core);
        if (InterlockedExchange(&want_next, 0)) ts_phone_next(&core);
        if (InterlockedExchange(&want_prev, 0)) ts_phone_prev(&core);
        LONG tab = InterlockedExchange(&want_tab, 0);
        if (tab) ts_phone_tab(&core, (int)tab);
        if (InterlockedExchange(&want_accept, 0)) ts_phone_accept(&core);
        int pressed = core.setting_pressed;
        core.setting_pressed = -1;
        LONG row = InterlockedExchange(&want_row, -1);
        if (row >= 0) ts_accept_row(&core, (int)row);
        if (InterlockedExchange(&want_cancel, 0)) ts_cancel(&core);
        ts_step(&core);
        fill_phone_settings(&core);
        n = ts_take_events(&core, events, TS_MAX_EVENTS);
        for (int i = 0; i < n; ++i) save |= events[i].kind == TS_EV_SAVE;
        ts_hud(&core, &h);
        job = core.mode != TS_IDLE;
        de = core.german;
        InterlockedExchange(&phone_is_open, core.phone_open);
        if (save) stats = core;
        ReleaseSRWLockExclusive(&lock);
        if (pressed >= 0) phone_setting_press(pressed);

        if (!logged_car && car_samples > 0) {
            logged_car = 1;
            log_line("the own car is seen (first sample)");
        }
        for (int i = 0; i < n; ++i) deliver(&events[i]);
        if (save) save_stats(&stats);
        /* the phone: its own window over the game (the HUD list when it can't, or when chosen) */
        TsHud hud_h = h;
        int window = opt_phone_style < 3 && phone_window_ok();
        if (window) {
            uint32_t ui = api->api_version >= 10 ? api->ui_status(mod) : 0;
            int page = !(ui & FH2_UI_TOASTS) || (ui & FH2_UI_ON_SCREEN); /* not over the pause menu or the map */
            phone_window_show(&h, opt_phone_style, h.panel && page);
            hud_h.panel = 0;
            if (h.panel && !page && !logged_page_wait++) log_line("phone window waits: the HUD page is not on screen");
            /* with a passenger: a speed-limit sign top left instead of the "Max 120" chip */
            int sign = h.sign_limit > 0 && page;
            phone_window_sign(h.sign_limit, h.sign_unit, h.sign_over, sign);
            if (sign) {
                if (h.sign_over) snprintf(hud_h.chip2, sizeof(hud_h.chip2), "%s", de ? "Zu schnell!" : "Too fast!");
                else hud_h.chip2[0] = 0;
            }
        } else {
            phone_window_show(&h, 0, 0);
            phone_window_sign(0, "", 0, 0);
        }
        push_hud(&hud_h);
        phone_popup(&hud_h, de, job);
        fallback_status(&h, job && !h.panel);
    }
}

/* ------------------------------------------------------------------ keys, settings rows, the page */

static void on_hotkey(void *user, const char *key) {
    (void)user;
    if (!strcmp(key, "Phone")) InterlockedExchange(&want_toggle, 1);
    else if (!strcmp(key, "Next")) InterlockedExchange(&want_next, 1);
    else if (!strcmp(key, "Accept")) InterlockedExchange(&want_accept, 1);
}

static void on_rebind(void *user, const char *key, uint32_t vk, uint32_t modifiers, uint32_t pad) {
    (void)user;
    for (int i = 0; i < 3; ++i)
        if (!strcmp(key, key_names[i])) key_vk[i] = vk, key_mod[i] = modifiers, key_pad[i] = pad;
    save_settings();
}

static void on_setting(void *user, const char *key, int value) {
    (void)user;
    if (!strcmp(key, "PhoneArrowKeys")) opt_arrow_keys = value != 0;
    else if (!strcmp(key, "BusLinesNeedBus")) opt_bus_needs_bus = value != 0;
    else if (!strcmp(key, "CareerExtras")) opt_extras = value != 0;
    else if (!strcmp(key, "AutoNavigation")) {
        opt_nav = value != 0;
        AcquireSRWLockExclusive(&lock);
        ts_renew_nav(&core);
        ReleaseSRWLockExclusive(&lock);
    } else if (!strcmp(key, "BusInAutoshow")) opt_bus_autoshow = value != 0;
    else if (!strcmp(key, "TimeLimit")) opt_time_limit = value;
    else if (!strcmp(key, "Language")) opt_language = value;
    else if (!strcmp(key, "PhoneStyle")) opt_phone_style = value;
    save_settings();
}

static char page_text[16][2][96];
static FH2PageRow page_rows[16];

static void page_open(void *user, const char *page_key) {
    (void)user;
    AcquireSRWLockExclusive(&lock);
    ts_refresh_offers(&core);
    TsCore c = core;
    ReleaseSRWLockExclusive(&lock);
    int de = c.german, n = 0;
#define ROW(key_, label_, value_, desc_, style_)                                                                       \
    do {                                                                                                               \
        if (n < 16) {                                                                                                  \
            memset(&page_rows[n], 0, sizeof(page_rows[n]));                                                            \
            page_rows[n].size = sizeof(FH2PageRow);                                                                    \
            page_rows[n].key = key_;                                                                                   \
            snprintf(page_text[n][0], 96, "%s", label_);                                                               \
            snprintf(page_text[n][1], 96, "%s", value_);                                                               \
            page_rows[n].label = page_text[n][0];                                                                      \
            page_rows[n].value = page_text[n][1];                                                                      \
            page_rows[n].description = desc_;                                                                          \
            page_rows[n].style = style_;                                                                               \
            n++;                                                                                                       \
        }                                                                                                              \
    } while (0)
    char v[96], d[96];
    static char descs[8][200];
    if (c.mode != TS_IDLE) {
        ROW(NULL, de ? "Laufender Auftrag" : "Current job", "", NULL, FH2_ROW_HEADING);
        snprintf(v, sizeof(v), "%d%%", (int)(c.sat + 0.5f));
        ROW(NULL, c.mode == TS_BUS ? (de ? "Buslinie" : "Bus line") : (de ? "Fahrt" : "Ride"), v, NULL, FH2_ROW_NORMAL);
        ROW("cancel", de ? "Auftrag abbrechen" : "Cancel job", "", de ? "Bricht den laufenden Auftrag ab. Mit Fahrgast an Bord gibt es eine schlechte Bewertung." : "Cancels the job. With a passenger on board you get a bad rating.", FH2_ROW_NORMAL);
    } else {
        static const char *const offer_keys[TS_PHONE_OFFERS] = {"offer0", "offer1", "offer2"};
        ROW(NULL, de ? "Fahraufträge" : "Ride requests", c.car_name, NULL, FH2_ROW_HEADING);
        for (int i = 0; i < c.offer_count && i < TS_PHONE_OFFERS; ++i) {
            const TsOffer *o = &c.offers[i];
            char dist[24];
            if (c.imperial) snprintf(dist, sizeof(dist), "%.1f mi", o->trip_m / 1609.344f);
            else snprintf(dist, sizeof(dist), "%.1f km", o->trip_m / 1000.0f);
            snprintf(v, sizeof(v), "%s · %d €", dist, o->pay);
            static const char *const types_de[TS_P_KINDS] = {"Normal", "Gemütlich", "Eilig", "Autofan", "VIP", "Geschäftskunde", "Hochzeitspaar", "Ausflug"};
            static const char *const types_en[TS_P_KINDS] = {"Normal", "Relaxed", "In a hurry", "Car fan", "VIP", "Business", "Wedding", "Day trip"};
            snprintf(descs[i], sizeof(descs[i]), "%s (%s) %s %s %s. %s %d:%02d, %s %d %s.", ts_passenger_name(o->passenger),
                     (de ? types_de : types_en)[o->ptype >= 0 && o->ptype < TS_P_KINDS ? o->ptype : 0], de ? "wartet bei" : "waits at", ts_places[o->pickup].name,
                     de ? "und möchte dorthin" : "and wants to go there", de ? "Zeitlimit" : "Time limit", (int)o->limit_s / 60,
                     (int)o->limit_s % 60, de ? "höchstens" : "at most", ts_speed_shown(&c, o->max_kmh), ts_speed_unit(&c));
            ROW(offer_keys[i], ts_places[o->dest].name, v, descs[i], FH2_ROW_NORMAL);
        }
        if (!c.offer_count) ROW(NULL, de ? "Keine Anfragen: erst losfahren" : "No requests: drive first", "", NULL, FH2_ROW_DIM);
        static const char *const line_keys[3] = {"line0", "line1", "line2"};
        ROW(NULL, de ? "Buslinien" : "Bus lines", "", NULL, FH2_ROW_HEADING);
        for (unsigned i = 0; i < ts_line_count && i < 3; ++i) {
            snprintf(d, sizeof(d), "%s %s · %s", de ? "Linie" : "Line", ts_lines[i].number, ts_lines[i].title);
            int locked = c.bus_needs_bus && c.car_kind != TS_CAR_BUS;
            snprintf(v, sizeof(v), "%s", locked ? (de ? "Bus nötig" : "needs the bus") : "");
            if (!locked) snprintf(v, sizeof(v), "%u %s", ts_lines[i].stop_count, de ? "Halte" : "stops");
            snprintf(descs[4 + i], sizeof(descs[4 + i]), "%s: %s. %s", de ? "Start" : "Start", ts_places[ts_lines[i].stops[0]].name,
                     locked ? (de ? "Geht nur im Bus des Spiels (PG Bus)." : "Only in the game's bus (PG Bus).")
                            : (c.imperial ? (de ? "Höchstens 50 mph." : "At most 50 mph.") : (de ? "Höchstens 80 km/h." : "At most 80 km/h.")));
            ROW(line_keys[i], d, v, descs[4 + i], locked ? FH2_ROW_DIM : FH2_ROW_NORMAL);
        }
    }
    ROW(NULL, de ? "Konto" : "Account", "", NULL, FH2_ROW_HEADING);
    snprintf(v, sizeof(v), "%lld €", c.wallet);
    ROW(NULL, de ? "Firmenkasse" : "Company funds", v, de ? "Das Geld deiner Firma Horizon Transit. Es ist getrennt von den Credits deines Spielstands." : "Your company's money, kept apart from your save's credits.", FH2_ROW_NORMAL);
    snprintf(v, sizeof(v), "%d / %d", c.jobs_done, c.bus_lines_done);
    ROW(NULL, de ? "Fahrten / Buslinien" : "Rides / bus lines", v, NULL, FH2_ROW_NORMAL);
    snprintf(v, sizeof(v), "%.1f / 5", c.rating_count ? (double)c.rating_sum / c.rating_count : 0.0);
    ROW(NULL, de ? "Bewertung" : "Rating", v, NULL, FH2_ROW_NORMAL);
    snprintf(v, sizeof(v), "%d · %s", (int)(c.reputation + 0.5f), ts_rep_name(&c));
    ROW(NULL, de ? "Ruf" : "Reputation", v, de ? "Gute Bewertungen heben den Ruf. Er macht die Bezahlung etwas besser und bringt Sonderfahrten (VIP ab 60)." : "Good ratings raise it. It pays a little more and brings special rides (VIPs from 60).", FH2_ROW_NORMAL);
#undef ROW
    api->set_page_rows(mod, page_key, page_rows, (uint32_t)n);
}

static void page_select(void *user, const char *page_key, const char *row_key) {
    (void)user;
    (void)page_key;
    if (!strncmp(row_key, "offer", 5)) InterlockedExchange(&want_row, row_key[5] - '0');
    else if (!strncmp(row_key, "line", 4)) InterlockedExchange(&want_row, TS_PHONE_OFFERS + (row_key[4] - '0'));
    else if (!strcmp(row_key, "cancel")) InterlockedExchange(&want_cancel, 1);
}

/* ------------------------------------------------------------------ the menu texts in the game's language
   English in the code, German from l10n_de.h; the platform shows each in the game's language (API 10) and
   follows it when the game picks its language. Older platforms get the English. */
#include "l10n_de.h"
static const char *TR(const char *key, const char *english) {
    if (api->api_version < 10 || api->size < offsetof(FH2ModApi, tr) + sizeof(api->tr) || !api->tr) return english;
    const char *t = api->tr(mod, key, english);
    return t ? t : english;
}

static void add_rows(void) {
    if (api->api_version < 4) return;
    FH2Setting s;
    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.key = "AutoNavigation";
    s.label = TR("row.AutoNavigation.label", "Auto GPS");
    s.description = TR("row.AutoNavigation.description", "Sets the game's GPS to each pickup, destination and stop.");
    s.value = opt_nav;
    s.on_change = on_setting;
    api->add_setting(mod, &s);

    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.key = "BusLinesNeedBus";
    s.label = TR("row.BusLinesNeedBus.label", "Bus lines need the bus");
    s.description = TR("row.BusLinesNeedBus.description", "On: bus lines only in the game's bus (PG Bus). Off: in any car.");
    s.value = opt_bus_needs_bus;
    s.on_change = on_setting;
    api->add_setting(mod, &s);

    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.key = "CareerExtras";
    s.label = TR("row.CareerExtras.label", "Reputation, specials, wishes");
    s.description = TR("row.CareerExtras.description",
                       "On: reputation changes pay and requests, VIPs and special rides appear, passengers make wishes. Off: like 0.3.");
    s.value = opt_extras;
    s.on_change = on_setting;
    api->add_setting(mod, &s);

    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.key = "PhoneArrowKeys";
    s.label = TR("row.PhoneArrowKeys.label", "Phone with arrow keys");
    s.description = TR("row.PhoneArrowKeys.description",
                       "While the phone is open: Up/Down = choose, Enter = take, Backspace = close; these keys then do nothing in the game.");
    s.value = opt_arrow_keys;
    s.on_change = on_setting;
    api->add_setting(mod, &s);

    if (api->api_version >= 9) {
        const char *limits[3] = {TR("row.TimeLimit.choice.0", "Relaxed"), TR("row.TimeLimit.choice.1", "Normal"),
                                 TR("row.TimeLimit.choice.2", "Strict")};
        memset(&s, 0, sizeof(s));
        s.size = sizeof(s);
        s.key = "TimeLimit";
        s.label = TR("row.TimeLimit.label", "Time limits");
        s.description = TR("row.TimeLimit.description", "How much time passengers give you.");
        s.kind = FH2_SETTING_CHOICE;
        s.choices = limits;
        s.choice_count = 3;
        s.value = opt_time_limit;
        s.on_change = on_setting;
        api->add_setting(mod, &s);

        const char *languages[3] = {TR("row.Language.choice.0", "Game"), "Deutsch", "English"};
        memset(&s, 0, sizeof(s));
        s.size = sizeof(s);
        s.key = "Language";
        s.label = TR("row.Language.label", "Language");
        s.description = TR("row.Language.description",
                           "Language of the phone, pop-ups and passengers. Game: the game's language. This menu always follows the game.");
        s.kind = FH2_SETTING_CHOICE;
        s.choices = languages;
        s.choice_count = 3;
        s.value = opt_language;
        s.on_change = on_setting;
        api->add_setting(mod, &s);

        const char *styles[4] = {TR("row.PhoneStyle.choice.0", "Night"), TR("row.PhoneStyle.choice.1", "Light"),
                                 TR("row.PhoneStyle.choice.2", "Festival"), TR("row.PhoneStyle.choice.3", "HUD list")};
        memset(&s, 0, sizeof(s));
        s.size = sizeof(s);
        s.key = "PhoneStyle";
        s.label = TR("row.PhoneStyle.label", "Phone design");
        s.description = TR("row.PhoneStyle.description", "How the phone looks. HUD list: the old list in the HUD instead of the phone.");
        s.kind = FH2_SETTING_CHOICE;
        s.choices = styles;
        s.choice_count = 4;
        s.value = opt_phone_style;
        s.on_change = on_setting;
        api->add_setting(mod, &s);
    }

    memset(&s, 0, sizeof(s));
    s.size = sizeof(s);
    s.key = "BusInAutoshow";
    s.label = TR("row.BusInAutoshow.label", "Bus in the Autoshow");
    s.description = TR("row.BusInAutoshow.description",
                       "EXPERIMENTAL: makes the game's traffic bus (PG_BUS_14) buyable in the Autoshow, in the Ford Transit's group. "
                       "Also works from the FH2MP launcher; the database file isn't changed. Don't buy or drive it online. "
                       "A save backup is made.");
    s.value = opt_bus_autoshow;
    s.needs_restart = 1;
    s.on_change = on_setting;
    api->add_setting(mod, &s);
}

static void add_keys(void) {
    if (api->api_version < 9) {
        log_line("the platform has no hotkeys (API %u): use the page under Pause > MODS > YOUR MODS", api->api_version);
        return;
    }
    const char *labels[3] = {TR("hotkey.Phone.label", "Phone open/close"), TR("hotkey.Next.label", "Next entry"),
                             TR("hotkey.Accept.label", "Accept / cancel")};
    for (int i = 0; i < 3; ++i) {
        FH2Hotkey k;
        memset(&k, 0, sizeof(k));
        k.size = sizeof(k);
        k.key = key_names[i];
        k.label = labels[i];
        k.vk = key_vk[i];
        k.modifiers = key_mod[i];
        k.pad_chord = key_pad[i];
        k.on_press = on_hotkey;
        k.on_rebind = on_rebind;
        if (!api->add_hotkey(mod, &k)) log_line("hotkey %s not added (see above)", key_names[i]);
    }
}

static void add_page(void) {
    if (api->api_version < 8) return;
    FH2Page p;
    memset(&p, 0, sizeof(p));
    p.size = sizeof(p);
    p.key = "App";
    p.title = "Horizon Transit";
    p.label = TR("page.App.label", "Horizon Transit (jobs)");
    p.description = TR("page.App.description", "Take rides and bus lines; account and rating.");
    p.on_open = page_open;
    p.on_select = page_select;
    if (!api->add_page(mod, &p)) log_line("the page could not be added (see above)");
}

static void add_hud(void) {
    hud = api->api_version >= 2 ? (const FH2HudInterface *)api->get_interface(FH2_HUD_INTERFACE, 1) : NULL;
    if (hud && hud->size < sizeof(FH2HudInterface) - sizeof(void *)) hud = NULL; /* v1 has no feed */
    if (!hud) {
        log_line("no HUD layer on this platform: status comes as pop-ups and on the page");
        return;
    }
    FH2HudLayer layer;
    memset(&layer, 0, sizeof(layer));
    layer.size = sizeof(layer);
    layer.parts = FH2_HUD_PART_BAR | FH2_HUD_PART_CHIPS | FH2_HUD_PART_PANEL | FH2_HUD_PART_COUNTDOWN |
                  (hud->version >= 2 ? FH2_HUD_PART_FEED : 0);
    layer.lease_ms = 3000;
    hud_added = hud->add_hud_layer(mod, &layer);
    log_line(hud_added ? "HUD layer added (version %u)" : "HUD layer refused (version %u, see above)", hud->version);
}

/* EXPERIMENTAL, off by default: the game's traffic bus PG_BUS_14 (Data_Car 414) in the Autoshow.
   0.2 wrote it into the database (add_sql). That never runs when the game is started from the FH2MP
   launcher (the platform leaves FH2MP's database alone), and it missed that the Autoshow leaves out every
   car in TrafficCars. Now the game's own questions to its database are rewritten as they are asked
   (src/core/bus_sql.c): the database file stays as it is. Cameras and the driver may be missing (its
   Locators.xml is empty). Untested in game. */
typedef void *(*DbQueryFn)(void *db, void *out, const char *sql);
static DbQueryFn original_db_query;
static volatile LONG bus_sql_logged;

static void *db_query(void *db, void *out, const char *sql) {
    if (opt_bus_autoshow && sql) {
        size_t len = strlen(sql), cap = len + 4096;
        char *changed = (char *)malloc(cap);
        if (changed) {
            int r = ts_bus_sql(sql, changed, cap);
            if (r == 1) {
                if (InterlockedIncrement(&bus_sql_logged) <= 8) log_line("bus in the Autoshow: query changed: %.70s", sql);
                void *result = original_db_query(db, out, changed);
                free(changed);
                return result;
            }
            free(changed);
        }
    }
    return original_db_query(db, out, sql);
}

static void bus_autoshow_hook(void) {
    if (!opt_bus_autoshow) return;
    if (code_is(RVA_DB_QUERY, DB_QUERY_BYTES, sizeof(DB_QUERY_BYTES))) {
        FH2Hook *hook = api->hook_function(mod, "database query (Transport: the bus in the Autoshow)", game + RVA_DB_QUERY,
                                           (void *)db_query, (void **)&original_db_query);
        if (hook && original_db_query && api->enable_hooks(mod, hook)) {
            log_line("EXPERIMENTAL: the bus (PG_BUS_14) is shown in the Autoshow (group 21, like the Transit SSV); "
                     "the database file isn't changed");
            return;
        }
    }
    log_line("EXPERIMENTAL bus in the Autoshow: NOT hooked (see above)");
}

static void bus_autoshow(void) {
    if (!opt_bus_autoshow) return;
    if (api->api_version >= 9) api->request_save_snapshot(mod, "FH2 Transport: bus in the Autoshow (experimental)", 3);
}

/* ------------------------------------------------------------------ entry */

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi *platform) {
    if (!platform || platform->api_version < 3) return 1;
    api = platform;
    FH2ModInfo info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    info.name = MOD_NAME;
    info.version = MOD_VERSION;
    info.platform_version = "2.9.0";
    info.check_updates = 0;
    info.description = "Horizon Transit - Your festival. Your route. Rides and bus lines with virtual passengers: "
                       "phone on F9, GPS route, time limit, speed, passenger comfort and the company's funds.";
    info.offline_only = 1;
    mod = api->register_mod(&info);
    if (!mod) return 0; /* online: the platform doesn't run this mod */
    if (api->api_version >= 10 && api->size >= offsetof(FH2ModApi, add_translation) + sizeof(api->add_translation) &&
        api->add_translation)
        log_line("German menu texts: %d", api->add_translation(mod, "de", ts_l10n_de, strlen(ts_l10n_de)));

    /* own folder next to azimuth.exe: settings, stats and logs */
    wchar_t folder[MAX_PATH];
    if (api->azimuth_dir && _snwprintf(folder, MAX_PATH, L"%ls\\%ls", api->azimuth_dir, MOD_FOLDER) > 0) {
        CreateDirectoryW(folder, NULL);
        _snwprintf(ini_path, MAX_PATH, L"%ls\\TransportSim.ini", folder);
    }
    if (api->api_version >= 9) {
        const wchar_t *path = api->session_log_path(mod, MOD_FOLDER, 5);
        if (path) log_file = _wfopen(path, L"w");
    }
    log_line("%s %s, platform %s (API %u)", MOD_NAME, MOD_VERSION, api->platform_version, api->api_version);

    load_settings();
    save_settings(); /* the INI shows every setting */
    ts_init(&core, (uint32_t)GetTickCount() ^ (uint32_t)GetCurrentProcessId() * 2654435761u, language_german());
    load_stats();
    core.time_factor = time_factor();

    if (api->api_version >= 10) api->ui_uses(mod, FH2_UI_USES_TOASTS);
    add_rows();
    add_keys();
    add_page();
    add_hud();
    bus_autoshow();

    install_key_hook(); /* a Windows DLL: no need to wait for the game */
    HANDLE keys = CreateThread(NULL, 0, keys_thread, NULL, 0, NULL);
    if (keys) CloseHandle(keys);
    phone_window_start(log_line);
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
    else log_line("could not start the worker thread");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
