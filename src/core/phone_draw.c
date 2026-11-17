/* The phone as a picture, see phone_draw.h. Every shape is drawn here with soft (anti-aliased) edges; sizes
   are in units of a 1080-line picture and multiplied by the scale. */
#include "phone_draw.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* the phone, in units: body size, the shadow margin round it */
#define PH_W 340.0f
#define PH_H 660.0f
#define PH_M 22.0f
#define PH_R 54.0f     /* corner radius of the body */
#define PH_BEZEL 11.0f /* black edge round the screen */

typedef struct {
    uint32_t frame, rim, edge;        /* body, its shiny outer rim, the bezel round the screen */
    uint32_t top, bottom;             /* the screen's background, top to bottom */
    uint32_t card, card_line;         /* list entries, their thin outline */
    int card_alpha;                   /* 0..255: how much of the card colour covers the background */
    uint32_t text, dim, sel_text;     /* text, grey text, text on the selected entry */
    uint32_t accent, accent2;         /* the app colour (selected entry, active tab, logo), its second tone */
    uint32_t seg;                     /* the tab switch's background */
    int seg_alpha;
    uint32_t warn;
    uint32_t value;                   /* numbers on the entries */
} Theme;

static const Theme themes[TS_THEMES] = {
    /* Nacht / Night: black glass, Horizon pink into orange */
    {0x1B1C21, 0x5A5D68, 0x050506, 0x141826, 0x07080D, 0x1D2130, 0x2A3042, 255, 0xF4F5FA, 0x9097AB, 0xFFFFFF,
     0xFF2D78, 0xFF7A2F, 0x1D2130, 255, 0xFFC531, 0xFF5A8F},
    /* Hell / Light: silver, white cards, the menus' orange */
    {0xC9CCD3, 0xF3F4F7, 0x0B0B0D, 0xF2F2F7, 0xE9E9F0, 0xFFFFFF, 0xDDDDE4, 255, 0x15161A, 0x6E7180, 0xFFFFFF,
     0xFF5A1F, 0xFF9A1F, 0xDEDEE6, 255, 0xC77700, 0xE8501A},
    /* Festival: a sunset behind frosted cards, yellow */
    {0x2A1440, 0x8D5BC0, 0x08040C, 0x5B2A9E, 0xF0457A, 0xFFFFFF, 0xFFFFFF, 46, 0xFFFFFF, 0xF3DDF1, 0x2A1238,
     0xFFD21F, 0xFFAA00, 0xFFFFFF, 40, 0xFFE27A, 0xFFFFFF},
    /* Ozean / Ocean: deep blue, cyan into turquoise */
    {0x13263A, 0x5C86AD, 0x04080D, 0x0B3A5C, 0x061524, 0x12324D, 0x1F4A6B, 255, 0xEEF7FF, 0x8FB3CC, 0x04121F,
     0x22D3EE, 0x2DD4BF, 0x12324D, 255, 0xFFC531, 0x5EEAD4},
    /* Carbon: matt black, racing red */
    {0x202020, 0x6A6A6A, 0x050505, 0x1A1A1A, 0x0A0A0A, 0x232323, 0x333333, 255, 0xF2F2F2, 0x9A9A9A, 0xFFFFFF,
     0xE10600, 0xFF4D2E, 0x262626, 255, 0xFFC531, 0xFF5A4A},
    /* Riviera: warm sand and the Mediterranean's teal */
    {0xD9CBB0, 0xF6EEDF, 0x0B0B0D, 0xFBF5EA, 0xF1E6D2, 0xFFFDF8, 0xE6D9C2, 255, 0x2B2620, 0x857868, 0xFFFFFF,
     0x0E9F9A, 0x1FB5C9, 0xEDE2CE, 255, 0xC77700, 0x0B8A86},
    /* Gold: black and gold */
    {0x1A1712, 0x8C7A4E, 0x050403, 0x16130E, 0x080705, 0x221E16, 0x3A3324, 255, 0xF7F1E3, 0xA89C82, 0x1A1408,
     0xE8B530, 0xF6D57A, 0x221E16, 255, 0xFFD666, 0xF0C860},
};

const char *ts_theme_name(int theme, int german) {
    switch (theme) {
    case TS_THEME_LIGHT: return german ? "Hell" : "Light";
    case TS_THEME_FESTIVAL: return "Festival";
    case TS_THEME_OCEAN: return german ? "Ozean" : "Ocean";
    case TS_THEME_CARBON: return "Carbon";
    case TS_THEME_RIVIERA: return "Riviera";
    case TS_THEME_GOLD: return "Gold";
    default: return german ? "Nacht" : "Night";
    }
}

typedef struct {
    TsPhoneCanvas *cv;
    float s;
    const Theme *t;
} Ctx;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* how much of the pixel centred at (px, py) a rounded box covers, 0..1 */
static float rbox_cover(float px, float py, float x, float y, float w, float h, float r) {
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float qx = fabsf(px - cx) - (w * 0.5f - r), qy = fabsf(py - cy) - (h * 0.5f - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float inside = (qx > qy ? qx : qy);
    float d = sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
    return clampf(0.5f - d, 0.0f, 1.0f);
}

static uint32_t mix(uint32_t a, uint32_t b, float t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    int r = (int)(ar + (br - ar) * t + 0.5f), g = (int)(ag + (bg - ag) * t + 0.5f), bl = (int)(ab + (bb - ab) * t + 0.5f);
    return (uint32_t)(r << 16 | g << 8 | bl);
}

/* a rounded box in units; colour from c0 (left/top) to c1 (right/bottom), alpha 0..255 */
static void box(Ctx *c, float ux, float uy, float uw, float uh, float ur, uint32_t c0, uint32_t c1, int vertical, int alpha) {
    float s = c->s, x = ux * s, y = uy * s, w = uw * s, h = uh * s, r = ur * s;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    if (y1 > c->cv->h) y1 = c->cv->h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            float cov = rbox_cover(px + 0.5f, py + 0.5f, x, y, w, h, r);
            if (cov <= 0.0f) continue;
            float t = vertical ? clampf((py + 0.5f - y) / (h > 1 ? h : 1), 0, 1) : clampf((px + 0.5f - x) / (w > 1 ? w : 1), 0, 1);
            uint32_t col = c0 == c1 ? c0 : mix(c0, c1, t);
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, cov * alpha / 255.0f);
        }
    }
}

static void fill(Ctx *c, float x, float y, float w, float h, float r, uint32_t col) { box(c, x, y, w, h, r, col, col, 1, 255); }

static void text(Ctx *c, const char *s, float x, float y, float w, float h, float size, int weight, uint32_t color, int align) {
    if (!s || !s[0] || !c->cv->text) return;
    TsPhoneText t;
    memset(&t, 0, sizeof(t));
    t.utf8 = s;
    t.x = (int)(x * c->s + 0.5f), t.y = (int)(y * c->s + 0.5f);
    t.w = (int)(w * c->s + 0.5f), t.h = (int)(h * c->s + 0.5f);
    t.size = (int)(size * c->s + 0.5f);
    t.weight = weight;
    t.color = color;
    t.align = align;
    c->cv->text(c->cv->user, &t);
}

/* the width a text needs, in units */
static float measure(Ctx *c, const char *s, float size, int weight) {
    if (!s || !s[0]) return 0;
    if (c->cv->measure) {
        TsPhoneText t;
        memset(&t, 0, sizeof(t));
        t.utf8 = s;
        t.size = (int)(size * c->s + 0.5f);
        t.weight = weight;
        return c->cv->measure(c->cv->user, &t) / c->s;
    }
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) n += (*p & 0xC0) != 0x80;
    return n * size * (weight >= 600 ? 0.6f : 0.55f);
}

static void text_wrap(Ctx *c, const char *s, float x, float y, float w, float h, float size, uint32_t color) {
    if (!s || !s[0] || !c->cv->text) return;
    TsPhoneText t;
    memset(&t, 0, sizeof(t));
    t.utf8 = s;
    t.x = (int)(x * c->s + 0.5f), t.y = (int)(y * c->s + 0.5f);
    t.w = (int)(w * c->s + 0.5f), t.h = (int)(h * c->s + 0.5f);
    t.size = (int)(size * c->s + 0.5f);
    t.weight = 400;
    t.color = color;
    t.align = TS_TEXT_CENTER;
    t.wrap = 1;
    c->cv->text(c->cv->user, &t);
}

void ts_phone_size(float scale, int *w, int *h) {
    *w = (int)ceilf((PH_W + 2 * PH_M) * scale);
    *h = (int)ceilf((PH_H + 2 * PH_M) * scale);
}

/* the status bar: clock left, signal, Wi-Fi and battery right of the camera island */
static void status_bar(Ctx *c, float sx0, float sx1, float sy, const char *clock, uint32_t col) {
    text(c, clock, sx0 + 18, sy + 6, 70, 24, 15, 600, col, TS_TEXT_CENTER);
    float x = sx1 - 30 - 26;
    /* battery: outline, the level, the tip */
    box(c, x, sy + 12, 25, 12, 4, col, col, 1, 110);
    fill(c, x + 2, sy + 14, 17, 8, 2.5f, col);
    box(c, x + 26, sy + 16, 2.2f, 4.5f, 1, col, col, 1, 140);
    /* signal: four bars */
    x -= 30;
    for (int i = 0; i < 4; ++i) fill(c, x + i * 5.0f, sy + 22 - (i + 1) * 2.6f, 3.4f, (i + 1) * 2.6f + 0.4f, 1, col);
}

/* the app's logo: a round badge with a road bending into an H */
static void logo(Ctx *c, float x, float y, float d) {
    const Theme *t = c->t;
    box(c, x, y, d, d, d * 0.3f, t->accent, t->accent2, 0, 255);
    uint32_t w = 0xFFFFFF;
    fill(c, x + d * 0.26f, y + d * 0.22f, d * 0.13f, d * 0.56f, d * 0.06f, w);
    fill(c, x + d * 0.61f, y + d * 0.22f, d * 0.13f, d * 0.56f, d * 0.06f, w);
    fill(c, x + d * 0.26f, y + d * 0.44f, d * 0.48f, d * 0.12f, d * 0.06f, w);
}

typedef struct {
    int label_row, sub_row; /* row index of the entry's line, of its grey second line (-1 none) */
    int caption;            /* a grey line of its own */
    int disabled;           /* an entry in grey (a bus line without the bus) */
} Entry;

static const char *pick(int german, const char *de, const char *en) { return german ? de : en; }


void ts_phone_draw(const TsHud *h, int theme, float scale, const char *clock, TsPhoneCanvas *cv) {
    Ctx c = {cv, scale, &themes[theme >= 0 && theme < TS_THEMES ? theme : 0]};
    const Theme *t = c.t;
    float X = PH_M, Y = PH_M;
    /* body: shiny rim, the frame, the black bezel, the screen */
    box(&c, X, Y, PH_W, PH_H, PH_R, t->rim, t->frame, 1, 255);
    fill(&c, X + 2.5f, Y + 2.5f, PH_W - 5, PH_H - 5, PH_R - 2.5f, t->frame);
    fill(&c, X + 5, Y + 5, PH_W - 10, PH_H - 10, PH_R - 5, t->edge);
    float sx0 = X + PH_BEZEL, sy0 = Y + PH_BEZEL, sw = PH_W - 2 * PH_BEZEL, sh = PH_H - 2 * PH_BEZEL;
    float sx1 = sx0 + sw, sy1 = sy0 + sh;
    box(&c, sx0, sy0, sw, sh, PH_R - PH_BEZEL, t->top, t->bottom, 1, 255);
    /* side buttons, lighter marks on the frame's edges */
    fill(&c, X + 0.8f, Y + 130, 3, 30, 1.5f, t->rim);
    fill(&c, X + 0.8f, Y + 175, 3, 52, 1.5f, t->rim);
    fill(&c, X + 0.8f, Y + 238, 3, 52, 1.5f, t->rim);
    fill(&c, X + PH_W - 3.8f, Y + 190, 3, 80, 1.5f, t->rim);

    /* camera island and status bar */
    fill(&c, X + PH_W / 2 - 52, sy0 + 10, 104, 30, 15, 0x000000);
    fill(&c, X + PH_W / 2 + 28, sy0 + 19, 12, 12, 6, 0x10141E);
    fill(&c, X + PH_W / 2 + 31, sy0 + 22, 4, 4, 2, 0x26324A);
    status_bar(&c, sx0, sx1, sy0 + 7, clock, t->text);

    float x0 = sx0 + 16, x1 = sx1 - 16, cw = x1 - x0;
    float y = sy0 + 54;
    int de = h->german;

    /* the app's head: logo, name, slogan */
    logo(&c, x0, y, 40);
    text(&c, "Horizon Transit", x0 + 52, y - 1, cw - 52, 24, 20, 700, t->text, TS_TEXT_LEFT);
    text(&c, pick(de, "Dein Festival. Deine Route.", "Your festival. Your route."), x0 + 52, y + 21, cw - 52, 18, 12.5f, 400,
         t->dim, TS_TEXT_LEFT);
    y += 54;

    /* tabs: a switch with the open one filled */
    int tabs = h->tab_count > 0 ? h->tab_count : 1;
    box(&c, x0, y, cw, 32, 16, t->seg, t->seg, 1, t->seg_alpha);
    /* each tab as wide as its name, the rest shared out; smaller letters for many tabs */
    float tsize = tabs > 4 ? 12.0f : 13.5f, tws[8], sum = 0.0f;
    for (int i = 0; i < tabs && i < 8; ++i) sum += tws[i] = measure(&c, h->tabs[i], tsize, 700) + 10.0f;
    float extra = (cw - 4 - sum) / tabs, tx = x0 + 2;
    for (int i = 0; i < tabs && i < 8; ++i) {
        float tw = extra >= 0.0f ? tws[i] + extra : (cw - 4) / tabs;
        int on = i == h->tab;
        if (on) box(&c, tx, y + 2, tw, 28, 14, t->accent, t->accent2, 0, 255);
        text(&c, h->tabs[i], tx, y + 2, tw, 28, tsize, on ? 700 : 600, on ? t->sel_text : t->dim, TS_TEXT_CENTER);
        tx += tw;
    }
    y += 44;

    /* the entries: the rows after the app's heading; a grey row right under an entry is its second line */
    Entry e[8];
    int n = 0;
    int first = h->row_count > 0 && h->row_heading[0] == 1 ? 1 : 0;
    for (int i = first; i < h->row_count && n < 8; ++i) {
        Entry *en = &e[n];
        memset(en, 0, sizeof(*en));
        en->label_row = i, en->sub_row = -1;
        if (h->row_heading[i] == 2) {
            if (i == first) en->caption = 1;
            else en->disabled = 1;
        } else if (h->row_heading[i] == 1) {
            en->caption = 1;
        } else if (i + 1 < h->row_count && h->row_heading[i + 1] == 2 && i + 1 != h->row_selected) {
            en->sub_row = i + 1;
            ++i;
        }
        ++n;
    }
    for (int k = 0; k < n; ++k) {
        const Entry *en = &e[k];
        int r = en->label_row;
        if (en->caption) {
            float cv = h->row_value[r][0] ? measure(&c, h->row_value[r], 12.5f, 600) + 4 : 0;
            if (cv > (cw - 8) * 0.5f) cv = (cw - 8) * 0.5f;
            text(&c, h->row_label[r], x0 + 4, y, cw - 8 - cv - (cv > 0 ? 8 : 0), 22, 12.5f, 600, t->dim, TS_TEXT_LEFT);
            if (cv > 0) text(&c, h->row_value[r], x1 - 4 - cv, y, cv, 22, 12.5f, 600, t->dim, TS_TEXT_RIGHT);
            y += 26;
            continue;
        }
        int sel = r == h->row_selected || (en->sub_row >= 0 && en->sub_row == h->row_selected);
        float ch = en->sub_row >= 0 ? 56 : 42;
        if (sel) {
            box(&c, x0, y, cw, ch, 13, t->accent, t->accent2, 0, 255);
        } else {
            box(&c, x0, y, cw, ch, 13, t->card_line, t->card_line, 1, t->card_alpha);
            box(&c, x0 + 1, y + 1, cw - 2, ch - 2, 12, t->card, t->card, 1, t->card_alpha);
        }
        uint32_t main = sel ? t->sel_text : en->disabled ? t->dim : t->text;
        uint32_t second = sel ? t->sel_text : t->dim;
        uint32_t value = sel ? t->sel_text : en->disabled ? t->dim : t->value;
        float inner = cw - 26;
        if (en->sub_row >= 0) {
            /* two lines: the entry over the whole width; under it its grey line, the value on the right */
            int s2 = en->sub_row;
            char sub[112];
            if (h->row_value[s2][0]) snprintf(sub, sizeof(sub), "%s · %s", h->row_label[s2], h->row_value[s2]);
            else snprintf(sub, sizeof(sub), "%s", h->row_label[s2]);
            /* a value "2,8 km · 960 CR": the money up on the right of line 1, the distance on line 2 */
            char v1[48], v2[48];
            snprintf(v1, sizeof(v1), "%s", h->row_value[r]);
            v2[0] = 0;
            char *dot = strstr(v1, " · ");
            if (dot) {
                snprintf(v2, sizeof(v2), "%.*s", (int)(dot - v1), v1);
                memmove(v1, dot + strlen(" · "), strlen(dot + strlen(" · ")) + 1);
            }
            float w1 = measure(&c, v1, 14, 700) + 4;
            if (w1 <= 4) w1 = 0;
            if (w1 > inner * 0.5f) w1 = inner * 0.5f;
            text(&c, h->row_label[r], x0 + 13, y + 6, inner - w1 - (w1 > 0 ? 8 : 0), 24, 14.5f, 600, main, TS_TEXT_LEFT);
            if (w1 > 0) text(&c, v1, x1 - 13 - w1, y + 6, w1, 24, 14, 700, value, TS_TEXT_RIGHT);
            float w2 = measure(&c, v2, 12.5f, 600) + 4;
            if (w2 <= 4) w2 = 0;
            if (w2 > inner * 0.4f) w2 = inner * 0.4f;
            if (w2 > 0) text(&c, v2, x1 - 13 - w2, y + 29, w2, 20, 12.5f, 600, sel ? t->sel_text : t->text, TS_TEXT_RIGHT);
            text(&c, sub, x0 + 13, y + 29, inner - w2 - (w2 > 0 ? 8 : 0), 20, 12, 400, second, TS_TEXT_LEFT);
        } else {
            /* one line: the label gets what the value leaves */
            float vw = measure(&c, h->row_value[r], 13.5f, 700) + 4;
            float lw = measure(&c, h->row_label[r], 14.5f, 600) + 4;
            if (vw <= 4) vw = 0;
            /* both don't fit: the value keeps its width (up to 45 %), the label is shortened */
            if (lw + vw + 10 > inner && vw > inner * 0.45f) vw = inner * 0.45f;
            text(&c, h->row_label[r], x0 + 13, y + 9, inner - vw - (vw > 0 ? 10 : 0), 24, 14.5f, 600, main, TS_TEXT_LEFT);
            if (vw > 0) text(&c, h->row_value[r], x1 - 13 - vw, y + 9, vw, 24, 13.5f, 700, value, TS_TEXT_RIGHT);
        }
        y += ch + 6;
    }

    /* footer, the keys, the home bar */
    float bottom = sy1;
    float fy = y + 2;
    if (fy < bottom - 112) fy = bottom - 112;
    if (h->footer[0]) {
        box(&c, x0, fy, cw, 50, 12, t->seg, t->seg, 1, t->seg_alpha / 2 + 10);
        text_wrap(&c, h->footer, x0 + 10, fy + 6, cw - 20, 40, 12, t->dim);
    }
    const char *keys = h->tab_count == 1 ? pick(de, "Enter 2× = Auftrag abbrechen", "Enter twice = cancel the job")
                                         : pick(de, "↑↓ wählen   ←→ Tab   Enter nehmen", "↑↓ choose   ←→ tab   Enter take");
    text(&c, keys, x0, bottom - 50, cw, 20, 12, 600, t->dim, TS_TEXT_CENTER);
    box(&c, X + PH_W / 2 - 60, bottom - 18, 120, 5, 2.5f, t->text, t->text, 1, 200);
}

void ts_phone_finish(int theme, float scale, TsPhoneCanvas *cv) {
    (void)theme;
    float s = scale;
    float x = PH_M * s, y = PH_M * s, w = PH_W * s, h = PH_H * s, r = PH_R * s;
    float spread = PH_M * s * 0.9f;
    for (int py = 0; py < cv->h; ++py) {
        for (int px = 0; px < cv->w; ++px) {
            uint32_t *d = &cv->px[py * cv->w + px];
            float fx = px + 0.5f, fy = py + 0.5f;
            float cov = rbox_cover(fx, fy, x, y, w, h, r);
            /* the shadow: a box a little lower, its edge blurred over `spread` */
            float sc = 0.0f;
            if (cov < 1.0f) {
                float cx = x + w * 0.5f, cy = y + h * 0.5f + 6 * s;
                float qx = fabsf(fx - cx) - (w * 0.5f - r), qy = fabsf(fy - cy) - (h * 0.5f - r);
                float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                float dist = sqrtf(ox * ox + oy * oy) - r;
                if (dist < spread) {
                    float k = 1.0f - clampf(dist / spread, 0, 1);
                    sc = 0.45f * k * k;
                }
            }
            float a = cov + (1.0f - cov) * sc;
            uint32_t rgb = *d & 0xFFFFFF;
            /* rgb is already the colour times cov (drawn over black), so it is premultiplied */
            int ai = (int)(a * 255.0f + 0.5f);
            if (ai <= 0) {
                *d = 0;
                continue;
            }
            if (ai < 255) { /* never brighter than its alpha (premultiplied) */
                uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255, m = (uint32_t)ai;
                rgb = (r > m ? m : r) << 16 | (g > m ? m : g) << 8 | (b > m ? m : b);
            }
            *d = (uint32_t)ai << 24 | rgb;
        }
    }
}

/* ------------------------------------------------------------------ the speed-limit sign */

#define SG_D 86.0f /* the disc, in units */
#define SG_M 7.0f  /* shadow margin */

void ts_sign_size(float scale, int *w, int *h) {
    *w = *h = (int)ceilf((SG_D + 2 * SG_M) * scale);
}

void ts_sign_draw(int number, const char *unit, int flash, float scale, TsPhoneCanvas *cv) {
    Ctx c = {cv, scale, &themes[0]};
    float x = SG_M, y = SG_M, r = SG_D / 2;
    fill(&c, x, y, SG_D, SG_D, r, 0xD71920);                                              /* the red ring */
    fill(&c, x + 10, y + 10, SG_D - 20, SG_D - 20, r - 10, flash ? 0xFFD21F : 0xFFFFFF);  /* the disc */
    char n[16];
    snprintf(n, sizeof(n), "%d", number);
    float size = number >= 100 ? 27.0f : 33.0f;
    int has_unit = unit && unit[0];
    text(&c, n, x, y + (has_unit ? 17 : 22), SG_D, 40, size, 700, 0x111111, TS_TEXT_CENTER);
    if (has_unit) text(&c, unit, x, y + 53, SG_D, 14, 10.5f, 700, 0x333333, TS_TEXT_CENTER);
}

void ts_sign_finish(float scale, TsPhoneCanvas *cv) {
    float s = scale, cx = (SG_M + SG_D / 2) * s, cy = cx, r = SG_D / 2 * s, spread = SG_M * s;
    for (int py = 0; py < cv->h; ++py)
        for (int px = 0; px < cv->w; ++px) {
            uint32_t *d = &cv->px[py * cv->w + px];
            float dx = px + 0.5f - cx, dy = py + 0.5f - cy - 1.5f * s, dist = sqrtf(dx * dx + dy * dy);
            float cov = clampf(r - sqrtf((px + 0.5f - cx) * (px + 0.5f - cx) + (py + 0.5f - cy) * (py + 0.5f - cy)) + 0.5f, 0, 1);
            float sh = dist - r < spread ? 0.4f * (1.0f - clampf((dist - r) / spread, 0, 1)) : 0.0f;
            float a = cov + (1.0f - cov) * sh;
            int ai = (int)(a * 255.0f + 0.5f);
            if (ai <= 0) {
                *d = 0;
                continue;
            }
            uint32_t rgb = *d & 0xFFFFFF;
            if (ai < 255) {
                uint32_t rr = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255, m = (uint32_t)ai;
                rgb = (rr > m ? m : rr) << 16 | (g > m ? m : g) << 8 | (b > m ? m : b);
            }
            *d = (uint32_t)ai << 24 | rgb;
        }
}

/* ------------------------------------------------------------------ the ride panel by the speedometer */

#define DS_W 316.0f
#define DS_H 128.0f
#define DS_M 8.0f
#define DS_R 20.0f

void ts_dash_size(float scale, int *w, int *h) {
    *w = (int)ceilf((DS_W + 2 * DS_M) * scale);
    *h = (int)ceilf((DS_H + 2 * DS_M) * scale);
}

/* a ring: a disc of `outer` with one of `inner` cut out, in the colour of what's under it */
static void ring(Ctx *c, float cx, float cy, float r, float width, uint32_t col, uint32_t inside) {
    fill(c, cx - r, cy - r, 2 * r, 2 * r, r, col);
    fill(c, cx - r + width, cy - r + width, 2 * (r - width), 2 * (r - width), r - width, inside);
}

void ts_dash_draw(const TsHud *h, float scale, TsPhoneCanvas *cv) {
    Ctx c = {cv, scale, &themes[0]};
    const uint32_t bg = 0x12151C;
    float x = DS_M, y = DS_M;
    fill(&c, x, y, DS_W, DS_H, DS_R, bg);

    /* the sign */
    float d = 92, sx = x + 14, sy = y + (DS_H - 18 - d) / 2;
    if (h->sign_limit <= 0) { /* no passenger yet: the job's step and the distance, e.g. "Pick up" / "0.8 mi" */
        text(&c, h->chip0, sx - 4, sy + 14, d + 8, 20, 13, 700, 0x8A91A3, TS_TEXT_CENTER);
        text(&c, h->chip1, sx - 4, sy + 36, d + 8, 32, 24, 700, 0xFFFFFF, TS_TEXT_CENTER);
    } else {
        fill(&c, sx, sy, d, d, d / 2, 0xD71920);
        fill(&c, sx + 10, sy + 10, d - 20, d - 20, d / 2 - 10, h->sign_over == 2 ? 0xFF3B3B : h->sign_over ? 0xFFD21F : 0xFFFFFF);
        char n[16];
        snprintf(n, sizeof(n), "%d", h->sign_limit);
        float size = h->sign_limit >= 100 ? 28.0f : 34.0f;
        int has_unit = h->sign_unit[0] != 0;
        text(&c, n, sx, sy + (has_unit ? 19 : 25), d, 40, size, 700, h->sign_over == 2 ? 0xFFFFFF : 0x111111, TS_TEXT_CENTER);
        if (has_unit) text(&c, h->sign_unit, sx, sy + 57, d, 14, 11, 700, h->sign_over == 2 ? 0xFFFFFF : 0x333333, TS_TEXT_CENTER);
    }

    /* the G-meter */
    float gr = 46, gcx = sx + d + 16 + gr, gcy = sy + d / 2;
    uint32_t edge = h->g_over ? 0xFF3B3B : 0x2FD27A;
    fill(&c, gcx - gr, gcy - gr, 2 * gr, 2 * gr, gr, 0x0A0C11);
    float inner = gr / 1.45f; /* the passenger's limit; the outer ring is 1.45 times that */
    ring(&c, gcx, gcy, inner + 1.5f, 3.0f, edge, 0x0A0C11);
    ring(&c, gcx, gcy, inner * 0.5f + 0.75f, 1.5f, 0x2A2F3A, 0x0A0C11);
    fill(&c, gcx - gr + 6, gcy - 0.75f, 2 * gr - 12, 1.5f, 0.75f, 0x2A2F3A);
    fill(&c, gcx - 0.75f, gcy - gr + 6, 1.5f, 2 * gr - 12, 0.75f, 0x2A2F3A);
    ring(&c, gcx, gcy, gr, 1.5f, 0x3A4050, 0x0A0C11);
    /* the cross was drawn over the inner circles: put the limit ring back on top */
    ring(&c, gcx, gcy, inner + 1.5f, 3.0f, edge, 0x0A0C11);
    fill(&c, gcx - inner * 0.5f - 0.75f, gcy - 0.75f, inner + 1.5f, 1.5f, 0.75f, 0x2A2F3A);
    fill(&c, gcx - 0.75f, gcy - inner * 0.5f - 0.75f, 1.5f, inner + 1.5f, 0.75f, 0x2A2F3A);
    text(&c, "G", gcx + gr - 16, gcy - gr + 2, 14, 14, 10, 700, 0x6C7385, TS_TEXT_CENTER);
    if (h->g_on) {
        float gx = h->g_lat, gy = h->g_long; /* braking moves the dot up, gas down, like a race car's display */
        float len = sqrtf(gx * gx + gy * gy), maxlen = 1.38f;
        if (len > maxlen) gx *= maxlen / len, gy *= maxlen / len;
        float px = gcx + gx * inner, py = gcy + gy * inner;
        uint32_t dot = h->g_over ? 0xFF3B3B : 0xFFFFFF;
        fill(&c, px - 7, py - 7, 14, 14, 7, 0x000000);
        fill(&c, px - 5.5f, py - 5.5f, 11, 11, 5.5f, dot);
    }

    /* the time left and the mood: "4:30 · 92%" */
    float tx = gcx + gr + 12, tw = x + DS_W - 12 - tx;
    char a[32] = "", b[32] = "";
    const char *dot = strstr(h->corner, " · ");
    if (dot) {
        snprintf(a, sizeof(a), "%.*s", (int)(dot - h->corner), h->corner);
        snprintf(b, sizeof(b), "%s", dot + strlen(" · "));
    } else {
        snprintf(b, sizeof(b), "%s", h->corner);
    }
    int de = h->german;
    if (a[0]) {
        text(&c, de ? "ZEIT" : "TIME", tx, sy + 2, tw, 14, 10, 700, 0x8A91A3, TS_TEXT_LEFT);
        text(&c, a, tx, sy + 14, tw, 30, 24, 700, a[0] == '+' ? 0xFF5A4A : 0xFFFFFF, TS_TEXT_LEFT);
    }
    if (b[0]) {
        int mood = atoi(b);
        text(&c, de ? "FAHRGAST" : "PASSENGER", tx, sy + 50, tw, 14, 10, 700, 0x8A91A3, TS_TEXT_LEFT);
        text(&c, b, tx, sy + 62, tw, 30, 24, 700, mood >= 75 ? 0x2FD27A : mood >= 50 ? 0xFFC531 : 0xFF5A4A, TS_TEXT_LEFT);
    }

    /* the wish or the timetable along the bottom */
    if (h->chip3[0]) text(&c, h->chip3, x + 14, y + DS_H - 24, DS_W - 28, 18, 12.5f, 700, 0xFFD21F, TS_TEXT_LEFT);
}

void ts_dash_finish(float scale, TsPhoneCanvas *cv) {
    float s = scale, x = DS_M * s, y = DS_M * s, w = DS_W * s, hh = DS_H * s, r = DS_R * s, spread = DS_M * s;
    const float body = 0.9f; /* the panel lets a little of the game through */
    for (int py = 0; py < cv->h; ++py)
        for (int px = 0; px < cv->w; ++px) {
            uint32_t *dp = &cv->px[py * cv->w + px];
            float fx = px + 0.5f, fy = py + 0.5f;
            float cov = rbox_cover(fx, fy, x, y, w, hh, r);
            float sc = 0.0f;
            if (cov < 1.0f) {
                float cx = x + w * 0.5f, cy = y + hh * 0.5f + 3 * s;
                float qx = fabsf(fx - cx) - (w * 0.5f - r), qy = fabsf(fy - cy) - (hh * 0.5f - r);
                float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                float dist = sqrtf(ox * ox + oy * oy) - r;
                if (dist < spread) {
                    float k = 1.0f - clampf(dist / spread, 0, 1);
                    sc = 0.35f * k * k;
                }
            }
            float a = cov * body + (1.0f - cov) * sc;
            int ai = (int)(a * 255.0f + 0.5f);
            if (ai <= 0) {
                *dp = 0;
                continue;
            }
            uint32_t rgb = *dp & 0xFFFFFF;
            uint32_t rr = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
            rr = (uint32_t)(rr * body + 0.5f), g = (uint32_t)(g * body + 0.5f), b = (uint32_t)(b * body + 0.5f);
            uint32_t m = (uint32_t)ai;
            rgb = (rr > m ? m : rr) << 16 | (g > m ? m : g) << 8 | (b > m ? m : b);
            *dp = (uint32_t)ai << 24 | rgb;
        }
}

/* ------------------------------------------------------------------ the passengers' lines (own window) */

#define FD_W 560.0f
#define FD_ROW 27.0f
#define FD_GAP 6.0f
#define FD_ROWS 9 /* 3 lines of up to 3 rows */
#define FD_H (FD_ROWS * FD_ROW + TS_FEED_LINES * FD_GAP)

static struct { float x, y, w, h; } feed_boxes[TS_FEED_LINES];
static int feed_box_count;

void ts_feed_size(float scale, int *w, int *h) {
    *w = (int)ceilf((FD_W + 4) * scale);
    *h = (int)ceilf((FD_H + 4) * scale);
}

void ts_feed_draw(const TsFeedLine *lines, int n, float scale, TsPhoneCanvas *cv) {
    Ctx c = {cv, scale, &themes[0]};
    static const uint32_t name_col[4] = {0x8FD3FF, 0x2FD27A, 0xFF9A3C, 0xFFD21F};
    feed_box_count = 0;
    float y = FD_H; /* from the bottom up: the newest line last in `lines`, lowest on screen */
    for (int i = n - 1; i >= 0 && feed_box_count < TS_FEED_LINES; --i) {
        char parts[3][TS_TEXT];
        int k = ts_feed_split(lines[i].text, 58, parts, 3);
        if (k <= 0) continue;
        float nw = measure(&c, lines[i].name, 15, 700) + 12;
        if (nw > 200) nw = 200;
        float tw = 0;
        for (int p = 0; p < k; ++p) {
            float w = measure(&c, parts[p], 15, 600);
            if (w > tw) tw = w;
        }
        float bw = 14 + nw + tw + 14, bh = k * FD_ROW + 8;
        if (bw > FD_W) bw = FD_W;
        y -= bh;
        if (y < 0) break;
        fill(&c, 2, y, bw, bh, 10, 0x0E1117);
        feed_boxes[feed_box_count].x = 2 * scale, feed_boxes[feed_box_count].y = y * scale;
        feed_boxes[feed_box_count].w = bw * scale, feed_boxes[feed_box_count].h = bh * scale;
        feed_box_count++;
        int st = lines[i].style >= 0 && lines[i].style < 4 ? lines[i].style : 0;
        text(&c, lines[i].name, 14, y + 4, nw - 6, FD_ROW, 15, 700, name_col[st], TS_TEXT_LEFT);
        for (int p = 0; p < k; ++p) text(&c, parts[p], 14 + nw, y + 4 + p * FD_ROW, bw - nw - 26, FD_ROW, 15, 600, 0xF4F5FA, TS_TEXT_LEFT);
        y -= FD_GAP;
    }
}

void ts_feed_finish(float scale, TsPhoneCanvas *cv) {
    (void)scale;
    const float body = 0.82f;
    for (int py = 0; py < cv->h; ++py)
        for (int px = 0; px < cv->w; ++px) {
            uint32_t *dp = &cv->px[py * cv->w + px];
            float cov = 0.0f;
            for (int b = 0; b < feed_box_count; ++b) {
                float r = 10 * scale;
                float v = rbox_cover(px + 0.5f, py + 0.5f, feed_boxes[b].x, feed_boxes[b].y, feed_boxes[b].w, feed_boxes[b].h, r);
                if (v > cov) cov = v;
            }
            uint32_t rgb = *dp & 0xFFFFFF;
            /* the text keeps full strength, the box behind it lets the game through a little */
            uint32_t rr = (rgb >> 16) & 255, g = (rgb >> 8) & 255, bb = rgb & 255;
            uint32_t lum = rr > g ? (rr > bb ? rr : bb) : (g > bb ? g : bb);
            float a = cov * (body + (1.0f - body) * (lum / 255.0f));
            int ai = (int)(a * 255.0f + 0.5f);
            if (ai <= 0) {
                *dp = 0;
                continue;
            }
            uint32_t m = (uint32_t)ai;
            rgb = (rr > m ? m : rr) << 16 | (g > m ? m : g) << 8 | (bb > m ? m : bb);
            *dp = m << 24 | rgb;
        }
}
