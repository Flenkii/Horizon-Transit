/* The company's shop: upgrades bought with the game's own credits (0.13: there is no mod money any more). Each thing
   once, for good (saved in [Company] Upgrades). Prices below are the base; the shop asks base x cr_scale CR (5: the
   dispatch radio costs 25,000 CR). The glue pays a purchase on the game's thread (credit_buy_item, then
   ts_shop_credit_done) after checking the game account.

   - Funkzentrale    5 000  dispatch radio: time limits 15 % longer
   - Werbung         6 000  ads: always 3 requests (also in quiet areas and with a poor name), pay +5 %
   - Komfortpaket    8 000  comfort pack: passengers start happier (85 instead of 80), harsh moments cost 25 % less
   - Großer Bus     10 000  bigger bus: 60 seats instead of 40
   - Premium-Kunden 15 000  premium clients: special rides more often, and 10 reputation points earlier
   0.10.1:
   - Trinkgeldbox    4 000  tip box: tips 30 % bigger
   - Kundenkartei    7 000  customer file: regulars ask for you more often (40 % instead of 25 % per list)
   - Schichtplaner  12 000  shift planner: the day's goal pays twice the bonus
   - Ticketautomat  12 000  ticket machine: bus fares 20 % higher
   0.13:
   - Wasser & Snacks       3 000  passengers start 3 points happier
   - Pünktlichkeitsprämie  6 000  10 % more pay when on time (rides and lines)
   - Nachtzuschlag         8 000  15 % more pay from 22 to 6 o'clock (the PC's clock)
   - Dashcam               9 000  a crash costs the passenger's mood half as much
   - Bus-Klimaanlage       9 000  bus lines pay 10 % more
   - Versicherung         11 000  a cancelled job costs half the reputation
   - Hotspot-Radar        13 000  hotspots pay 0.1x more
   - Fernfahrer-Lizenz    16 000  rides of 7 km and more pay 15 % more
   - Werbetafeln          20 000  rides in busy towns pay 10 % more */
#include "jobs_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int price;
    const char *de, *en, *what_de, *what_en;
} ShopItem;

static const ShopItem items[TS_SHOP_ITEMS] = {
    {5000, "Funkzentrale", "Dispatch radio", "15 % mehr Zeit für jede Fahrt", "15 % more time for every ride"},
    {6000, "Werbung", "Advertising", "Immer 3 Anfragen, 5 % mehr Lohn", "Always 3 requests, 5 % more pay"},
    {8000, "Komfortpaket", "Comfort pack", "Fahrgäste sind zufriedener und verzeihen mehr", "Happier, more forgiving passengers"},
    {10000, "Großer Bus", "Bigger bus", "60 statt 40 Plätze im Bus", "60 seats in the bus instead of 40"},
    {15000, "Premium-Kunden", "Premium clients", "Öfter VIPs und Sonderfahrten", "More VIPs and special rides"},
    {4000, "Trinkgeldbox", "Tip box", "30 % mehr Trinkgeld", "30 % bigger tips"},
    {7000, "Kundenkartei", "Customer file", "Stammgäste fragen öfter nach dir", "Regulars ask for you more often"},
    {12000, "Schichtplaner", "Shift planner", "Doppelter Bonus für das Tagesziel", "Twice the bonus for the day's goal"},
    {12000, "Ticketautomat", "Ticket machine", "20 % mehr Fahrgeld im Bus", "20 % higher bus fares"},
    /* 0.13 */
    {3000, "Wasser & Snacks", "Water & snacks", "Fahrgäste steigen besser gelaunt ein", "Passengers get in in a better mood"},
    {9000, "Dashcam", "Dashcam", "Unfälle verärgern Fahrgäste nur halb so sehr", "Crashes upset passengers half as much"},
    {6000, "Pünktlichkeitsprämie", "On-time bonus", "10 % mehr Lohn, wenn du pünktlich bist", "10 % more pay when you're on time"},
    {16000, "Fernfahrer-Lizenz", "Long-haul licence", "15 % mehr Lohn für Fahrten ab 7 km", "15 % more pay for rides of 7 km and more"},
    {13000, "Hotspot-Radar", "Hotspot radar", "Hotspots zahlen 0,1x mehr", "Hotspots pay 0.1x more"},
    {8000, "Nachtzuschlag", "Night surcharge", "15 % mehr Lohn von 22 bis 6 Uhr", "15 % more pay from 10 pm to 6 am"},
    {9000, "Bus-Klimaanlage", "Bus air conditioning", "Buslinien bringen 10 % mehr", "Bus lines pay 10 % more"},
    {20000, "Werbetafeln", "Billboards", "10 % mehr Lohn in belebten Städten", "10 % more pay in busy towns"},
    {11000, "Versicherung", "Insurance", "Ein Abbruch kostet nur halb so viel Ruf", "A cancelled job costs half the reputation"},
};

long long ts_shop_cost(const TsCore *c, int item) {
    if (item < 0 || item >= TS_SHOP_ITEMS) return 0;
    return (long long)items[item].price * (c->cr_scale > 0 ? c->cr_scale : 5);
}
long long ts_shop_balance(const TsCore *c) { return c->game_credits; }
/* "12.500" (German) or "12,500" */
static void amount_text(const TsCore *c, long long v, char *out, size_t size) {
    char d[32];
    snprintf(d, sizeof(d), "%lld", v < 0 ? -v : v);
    size_t n = strlen(d), o = 0;
    if (v < 0 && o + 1 < size) out[o++] = '-';
    for (size_t i = 0; i < n && o + 2 < size; ++i) {
        if (i && (n - i) % 3 == 0) out[o++] = c->german ? '.' : ',';
        out[o++] = d[i];
    }
    out[o] = 0;
}

float ts_shop_pay_factor(const TsCore *c, int bus, float trip_m, int on_time) {
    float f = 1.0f;
    if (on_time && ts_has(c, TS_SHOP_ONTIME)) f += 0.10f;
    if (c->local_hour >= 0 && (c->local_hour >= 22 || c->local_hour < 6) && ts_has(c, TS_SHOP_NIGHT)) f += 0.15f;
    if (bus) {
        if (ts_has(c, TS_SHOP_AIRCON)) f += 0.10f;
    } else {
        if (trip_m >= 7000.0f && ts_has(c, TS_SHOP_LONGHAUL)) f += 0.15f;
        if (c->demand == 2 && ts_has(c, TS_SHOP_BILLBOARD)) f += 0.10f;
    }
    return f;
}

float ts_shop_start_sat(const TsCore *c) {
    return 80.0f + (ts_has(c, TS_SHOP_COMFORT) ? 5.0f : 0.0f) + (ts_has(c, TS_SHOP_SNACKS) ? 3.0f : 0.0f);
}

const char *ts_shop_name(const TsCore *c, int item) { return item >= 0 && item < TS_SHOP_ITEMS ? T(c, items[item].de, items[item].en) : ""; }
const char *ts_shop_what(const TsCore *c, int item) { return item >= 0 && item < TS_SHOP_ITEMS ? T(c, items[item].what_de, items[item].what_en) : ""; }
void ts_shop_buy(TsCore *c, int item) { shop_buy(c, item); }

int ts_scroll_first(int n, int sel, int visible) {
    if (visible <= 0 || n <= visible) return 0;
    int first = sel - visible / 2;
    if (first < 0) first = 0;
    if (first > n - visible) first = n - visible;
    return first;
}

int ts_has(const TsCore *c, int item) {
    return item >= 0 && item < TS_SHOP_ITEMS && (c->upgrades & (1u << item)) != 0;
}

void shop_apply(TsCore *c) {
    c->bus_capacity = ts_has(c, TS_SHOP_BIGBUS) ? 60 : 40;
}

void shop_buy(TsCore *c, int item) {
    if (item < 0 || item >= TS_SHOP_ITEMS) return;
    const ShopItem *it = &items[item];
    const char *name = T(c, it->de, it->en);
    char text[TS_TEXT], num[32];
    if (ts_has(c, item)) {
        toast(c, TS_ICON_NONE, 0, name, T(c, "Hat deine Firma schon.", "Your company has it already."));
        return;
    }
    if (!c->credits_mode) { /* the game's spend isn't the code the mod knows (the log says it) */
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, name, T(c, "Mit dieser Spielversion geht der Shop nicht.", "The shop doesn't work with this game version."));
        return;
    }
    if (c->credit_buy_item >= 0) return; /* one purchase at a time: the game is paying */
    long long cost = ts_shop_cost(c, item), have = ts_shop_balance(c);
    if (have < 0) {
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, name, T(c, "Dein Spielkonto wird noch gelesen. Gleich nochmal.", "Reading your game account. Try again in a moment."));
        return;
    }
    if (have < cost) {
        amount_text(c, cost - have, num, sizeof(num));
        snprintf(text, sizeof(text), "%s %s CR", T(c, "Es fehlen noch", "Still missing"), num);
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, name, text);
        return;
    }
    c->credit_buy_item = item; /* the glue pays it on the game's thread, then ts_shop_credit_done */
    c->credit_buy_cost = cost;
}

/* 0.13: the game paid the purchase (ok 1) or refused it (0); balance = the game account now (-1 unknown) */
void ts_shop_credit_done(TsCore *c, int item, int ok, long long balance) {
    if (item < 0 || item >= TS_SHOP_ITEMS) return;
    const ShopItem *it = &items[item];
    const char *name = T(c, it->de, it->en);
    char text[TS_TEXT], num[32];
    long long cost = c->credit_buy_cost;
    c->credit_buy_item = -1;
    if (balance >= 0) c->game_credits = balance;
    if (!ok) {
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, name, T(c, "Nicht genug Credits auf dem Spielkonto.", "Not enough credits on your game account."));
        return;
    }
    c->upgrades |= 1u << item;
    shop_apply(c);
    amount_text(c, cost, num, sizeof(num));
    snprintf(text, sizeof(text), "%s · -%s CR", T(c, it->what_de, it->what_en), num);
    toast(c, TS_ICON_TROPHY, TS_SOUND_NOTIFY, name, text);
    event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
}

/* 0.10.1: as many as fit (the list scrolls with the selection); returns the first one shown */
int shop_rows(const TsCore *c, TsHud *h) {
    char b[48], num[32];
    int sel = c->phone_sel >= 0 && c->phone_sel < TS_SHOP_ITEMS ? c->phone_sel : 0;
    int first = ts_scroll_first(TS_SHOP_ITEMS, sel, 8 - h->row_count); /* the phone shows 8 rows */
    for (int i = first; i < TS_SHOP_ITEMS && h->row_count < 8; ++i) {
        if (ts_has(c, i)) snprintf(b, sizeof(b), "%s", T(c, "gekauft", "bought"));
        else {
            amount_text(c, ts_shop_cost(c, i), num, sizeof(num));
            snprintf(b, sizeof(b), "%s CR", num);
        }
        row(h, T(c, items[i].de, items[i].en), b, 0);
    }
    long long have = ts_shop_balance(c), cost = ts_shop_cost(c, sel);
    if (c->credit_buy_item >= 0)
        snprintf(h->footer, sizeof(h->footer), "%s", T(c, "Wird bezahlt …", "Paying …"));
    else if (have < 0)
        snprintf(h->footer, sizeof(h->footer), "%s · %s", T(c, items[sel].what_de, items[sel].what_en), T(c, "Spielkonto wird gelesen", "reading the game account"));
    else if (!ts_has(c, sel) && have < cost) {
        amount_text(c, cost - have, num, sizeof(num));
        snprintf(h->footer, sizeof(h->footer), "%s · %s %s CR", T(c, items[sel].what_de, items[sel].what_en), T(c, "es fehlen", "missing"), num);
    } else {
        amount_text(c, have, num, sizeof(num));
        snprintf(h->footer, sizeof(h->footer), "%s · %s %s CR", T(c, items[sel].what_de, items[sel].what_en),
                 T(c, "Spielkonto", "Game account"), num);
    }
    return first;
}
