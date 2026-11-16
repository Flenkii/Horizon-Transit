/* The company's shop: what the Firmenkasse (the mod's own money) buys. Each thing once, for good (saved).

   - Funkzentrale    5 000 €  dispatch radio: time limits 15 % longer
   - Werbung         6 000 €  ads: always 3 requests (also in quiet areas and with a poor name), pay +5 %
   - Komfortpaket    8 000 €  comfort pack: passengers start happier (85 instead of 80), harsh moments cost 25 % less
   - Großer Bus     10 000 €  bigger bus: 60 seats instead of 40
   - Premium-Kunden 15 000 €  premium clients: special rides more often, and 10 reputation points earlier
   Only the mod's money is spent, never the game's credits. */
#include "jobs_internal.h"
#include <stdio.h>

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
};

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
    char text[TS_TEXT];
    if (ts_has(c, item)) {
        toast(c, TS_ICON_NONE, 0, name, T(c, "Hat deine Firma schon.", "Your company has it already."));
        return;
    }
    if (c->wallet < it->price) {
        snprintf(text, sizeof(text), "%s %lld €", T(c, "Es fehlen noch", "Still missing"), (long long)it->price - c->wallet);
        toast(c, TS_ICON_NONE, TS_SOUND_ERROR, name, text);
        return;
    }
    c->wallet -= it->price;
    c->upgrades |= 1u << item;
    shop_apply(c);
    snprintf(text, sizeof(text), "%s · -%d €", T(c, it->what_de, it->what_en), it->price);
    toast(c, TS_ICON_TROPHY, TS_SOUND_NOTIFY, name, text);
    event(c, TS_EV_SAVE, 0, 0, NULL, NULL);
}

void shop_rows(const TsCore *c, TsHud *h) {
    char b[48];
    for (int i = 0; i < TS_SHOP_ITEMS; ++i) {
        if (ts_has(c, i)) snprintf(b, sizeof(b), "%s", T(c, "gekauft", "bought"));
        else if (c->german) snprintf(b, sizeof(b), "%d.%03d €", items[i].price / 1000, items[i].price % 1000);
        else snprintf(b, sizeof(b), "%d,%03d €", items[i].price / 1000, items[i].price % 1000);
        row(h, T(c, items[i].de, items[i].en), b, 0);
    }
    int sel = c->phone_sel >= 0 && c->phone_sel < TS_SHOP_ITEMS ? c->phone_sel : 0;
    snprintf(h->footer, sizeof(h->footer), "%s · %s %lld €", T(c, items[sel].what_de, items[sel].what_en), T(c, "Kasse", "Funds"), c->wallet);
}
