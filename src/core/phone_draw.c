/* The phone as a picture, see phone_draw.h. Every shape is drawn here with soft (anti-aliased) edges; sizes
   are in units of a 1080-line picture and multiplied by the scale. */
#include "phone_draw.h"
#include "map.h"
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
    /* 0.5.1 Pur / Pure: like a ride-hailing app: white, black type, grey cards without outlines, black pills */
    {0x111111, 0x3A3A3A, 0x000000, 0xFFFFFF, 0xFFFFFF, 0xF3F3F3, 0xF3F3F3, 255, 0x000000, 0x6B6B6B, 0xFFFFFF,
     0x000000, 0x1A1A1A, 0xEEEEEE, 255, 0xC77700, 0x000000},
    /* 0.13 Pur Dunkel / Pure Dark: the ride app's dark mode (Marvin's screenshots): near black, grey cards, white
       type, white pills with black type, green only for good news */
    {0x111111, 0x3A3A3A, 0x000000, 0x1C1C1C, 0x121212, 0x2A2A2A, 0x3A3A3A, 255, 0xFFFFFF, 0xA8A8A8, 0x000000,
     0xFFFFFF, 0xE2E2E2, 0x2A2A2A, 255, 0xF5B700, 0xFFFFFF},
};

/* 0.10.1: a design's colours for the phone app: top, bottom, card, card line, text, dim, selected text, accent, accent 2,
   warn, value (0xRRGGBB); card alpha 0..255 */
void ts_theme_colors(int theme, uint32_t out[11], int *card_alpha) {
    const Theme *t = &themes[theme >= 0 && theme < TS_THEMES ? theme : 0];
    uint32_t v[11] = {t->top, t->bottom, t->card, t->card_line, t->text, t->dim, t->sel_text, t->accent, t->accent2, t->warn, t->value};
    for (int i = 0; i < 11; ++i) out[i] = v[i];
    if (card_alpha) *card_alpha = t->card_alpha;
}

const char *ts_theme_name(int theme, int german) {
    switch (theme) {
    case TS_THEME_LIGHT: return german ? "Hell" : "Light";
    case TS_THEME_FESTIVAL: return "Festival";
    case TS_THEME_OCEAN: return german ? "Ozean" : "Ocean";
    case TS_THEME_CARBON: return "Carbon";
    case TS_THEME_RIVIERA: return "Riviera";
    case TS_THEME_GOLD: return "Gold";
    case TS_THEME_PURE: return german ? "Pur" : "Pure";
    case TS_THEME_PURE_DARK: return german ? "Pur Dunkel" : "Pure Dark";
    default: return german ? "Nacht" : "Night";
    }
}

typedef struct {
    TsPhoneCanvas *cv;
    float s;
    const Theme *t;
    int dry;                  /* 0.7: only measuring: nothing is drawn */
    int clip;                 /* 0.7: shapes stay inside this box (pixels) */
    int clip_x0, clip_y0, clip_x1, clip_y1;
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
    if (c->dry) return;
    float s = c->s, x = ux * s, y = uy * s, w = uw * s, h = uh * s, r = ur * s;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    if (y1 > c->cv->h) y1 = c->cv->h;
    if (c->clip) {
        if (x0 < c->clip_x0) x0 = c->clip_x0;
        if (y0 < c->clip_y0) y0 = c->clip_y0;
        if (x1 > c->clip_x1) x1 = c->clip_x1;
        if (y1 > c->clip_y1) y1 = c->clip_y1;
    }
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

/* ------------------------------------------------------------------ 0.11: glass (like Liquid Glass / Material You) */
static int clip_px(Ctx *c, float x, float y, float w, float h, int *x0, int *y0, int *x1, int *y1) {
    *x0 = (int)floorf(x), *y0 = (int)floorf(y), *x1 = (int)ceilf(x + w), *y1 = (int)ceilf(y + h);
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > c->cv->w) *x1 = c->cv->w;
    if (*y1 > c->cv->h) *y1 = c->cv->h;
    if (c->clip) {
        if (*x0 < c->clip_x0) *x0 = c->clip_x0;
        if (*y0 < c->clip_y0) *y0 = c->clip_y0;
        if (*x1 > c->clip_x1) *x1 = c->clip_x1;
        if (*y1 > c->clip_y1) *y1 = c->clip_y1;
    }
    return *x0 < *x1 && *y0 < *y1;
}

/* a rounded box whose colour fades out downwards: alpha a0 at the top to a1 at `fade` of its height (a gloss) */
static void box_fade(Ctx *c, float ux, float uy, float uw, float uh, float ur, uint32_t col, int a0, int a1, float fade) {
    if (c->dry) return;
    float s = c->s, x = ux * s, y = uy * s, w = uw * s, h = uh * s, r = ur * s;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    int x0, y0, x1, y1;
    if (!clip_px(c, x, y, w, h, &x0, &y0, &x1, &y1)) return;
    for (int py = y0; py < y1; ++py) {
        float t = clampf((py + 0.5f - y) / (h * (fade > 0.01f ? fade : 1.0f)), 0, 1);
        float a = (a0 + (a1 - a0) * t) / 255.0f;
        if (a <= 0.0f) continue;
        for (int px = x0; px < x1; ++px) {
            float cov = rbox_cover(px + 0.5f, py + 0.5f, x, y, w, h, r);
            if (cov <= 0.0f) continue;
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, cov * a);
        }
    }
}

/* a thin line round a rounded box (inside its edge) */
static void outline(Ctx *c, float ux, float uy, float uw, float uh, float ur, float uwidth, uint32_t col, int alpha) {
    if (c->dry) return;
    float s = c->s, x = ux * s, y = uy * s, w = uw * s, h = uh * s, r = ur * s, lw = uwidth * s;
    if (lw < 1.0f) lw = 1.0f;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    float ri = r - lw > 0 ? r - lw : 0;
    int x0, y0, x1, y1;
    if (!clip_px(c, x, y, w, h, &x0, &y0, &x1, &y1)) return;
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            float fx = px + 0.5f, fy = py + 0.5f;
            float cov = rbox_cover(fx, fy, x, y, w, h, r) - rbox_cover(fx, fy, x + lw, y + lw, w - 2 * lw, h - 2 * lw, ri);
            if (cov <= 0.0f) continue;
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, cov * alpha / 255.0f);
        }
}

static int theme_light(const Theme *t) { return (0.299f * (t->bottom >> 16 & 255) + 0.587f * (t->bottom >> 8 & 255) + 0.114f * (t->bottom & 255)) > 150.0f; }

/* a frosted card: the card colour (see-through, the wallpaper shows), a light rim, a gloss at the top */
static void glass(Ctx *c, float x, float y, float w, float h, float r) {
    const Theme *t = c->t;
    int light = theme_light(t);
    int a = t->card_alpha >= 255 ? (light ? 200 : 175) : t->card_alpha;
    box(c, x, y, w, h, r, t->card, t->card, 1, a);
    box_fade(c, x, y, w, h, r, 0xFFFFFF, light ? 70 : 22, 0, 0.55f);
    outline(c, x, y, w, h, r, 1.0f, light ? 0xFFFFFF : mix(t->card_line, 0xFFFFFF, 0.25f), light ? 230 : 150);
}

/* the gradient pill of the chosen thing, with a gloss on its upper half */
static void glossy(Ctx *c, float x, float y, float w, float h, float r) {
    const Theme *t = c->t;
    box(c, x, y, w, h, r, t->accent, t->accent2, 0, 255);
    box_fade(c, x, y, w, h, r, 0xFFFFFF, 70, 0, 0.6f);
    outline(c, x, y, w, h, r, 1.0f, 0xFFFFFF, 70);
}

/* the wallpaper: soft glows of the design's colours over the screen (inside the rounded screen box) */
static void glow(Ctx *c, float sx, float sy, float sw, float sh, float sr, float ucx, float ucy, float ur, uint32_t col, int alpha) {
    if (c->dry) return;
    float s = c->s, cx = ucx * s, cy = ucy * s, r = ur * s;
    float X = sx * s, Y = sy * s, W = sw * s, H = sh * s, R = sr * s;
    int x0, y0, x1, y1;
    if (!clip_px(c, cx - r, cy - r, 2 * r, 2 * r, &x0, &y0, &x1, &y1)) return;
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            float dx = (px + 0.5f - cx) / r, dy = (py + 0.5f - cy) / r, d2 = dx * dx + dy * dy;
            if (d2 >= 1.0f) continue;
            float k = 1.0f - d2;
            float cov = rbox_cover(px + 0.5f, py + 0.5f, X, Y, W, H, R);
            if (cov <= 0.0f) continue;
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, cov * k * k * alpha / 255.0f);
        }
}

static void text(Ctx *c, const char *s, float x, float y, float w, float h, float size, int weight, uint32_t color, int align) {
    if (!s || !s[0] || !c->cv->text || c->dry) return;
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
    if (!s || !s[0] || !c->cv->text || c->dry) return;
    if (measure(c, s, size, 400) <= w - 2) { /* 0.13: one line: in the middle of the box, not at its top */
        text(c, s, x, y, w, h, size, 400, color, TS_TEXT_CENTER);
        return;
    }
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
    box_fade(c, x, y, d, d, d * 0.3f, 0xFFFFFF, 80, 0, 0.6f); /* 0.11 */
    uint32_t w = 0xFFFFFF;
    fill(c, x + d * 0.26f, y + d * 0.22f, d * 0.13f, d * 0.56f, d * 0.06f, w);
    fill(c, x + d * 0.61f, y + d * 0.22f, d * 0.13f, d * 0.56f, d * 0.06f, w);
    fill(c, x + d * 0.26f, y + d * 0.44f, d * 0.48f, d * 0.12f, d * 0.06f, w);
}

static void ring(Ctx *c, float cx, float cy, float r, float width, uint32_t col, uint32_t inside);

typedef struct {
    int label_row, sub_row; /* row index of the entry's line, of its grey second line (-1 none) */
    int caption;            /* a grey line of its own */
    int disabled;           /* an entry in grey (a bus line without the bus) */
} Entry;

static const char *pick(int german, const char *de, const char *en) { return german ? de : en; }


/* ------------------------------------------------------------------ the devices (0.5: the phone, the Edge phone, the tablet) */

/* each device's body in units: size, corner radius, the bezel round the screen */
typedef struct {
    float w, h, r, bezel;
} Body;
static const Body bodies[TS_DEVICES] = {
    {PH_W, PH_H, PH_R, PH_BEZEL}, /* the phone (rounded, camera island, home bar) */
    {336.0f, 676.0f, 36.0f, 7.0f}, /* the Edge: slim bezels, squarer corners, a punch-hole camera, Android keys */
    {820.0f, 560.0f, 38.0f, 16.0f}, /* the tablet: landscape, two columns */
};

static const Body *body_of(int device) { return &bodies[device >= 0 && device < TS_DEVICES ? device : 0]; }

void ts_device_size(int device, float scale, int *w, int *h) {
    const Body *b = body_of(device);
    *w = (int)ceilf((b->w + 2 * PH_M) * scale);
    *h = (int)ceilf((b->h + 2 * PH_M) * scale);
}

void ts_phone_size(float scale, int *w, int *h) { ts_device_size(TS_DEVICE_PHONE, scale, w, h); }

int ts_tab_rect_px[4];

static const char *keys_text(const TsHud *h) {
    switch (h->map_keys) { /* 0.7 */
    case TS_MAPKEYS_PREVIEW: return pick(h->german, "↑↓ wählen   ←→ Zoom   ⌫ zurück", "↑↓ choose   ←→ zoom   ⌫ back");
    case TS_MAPKEYS_TAB: return pick(h->german, "Enter Zoom   ←→ Tab", "Enter zoom   ←→ tab");
    case TS_MAPKEYS_JOB: return pick(h->german, "Enter Zoom   ←→ Auftrag", "Enter zoom   ←→ job");
    case TS_MAPKEYS_JOBPAGE: return pick(h->german, "Enter 2× = abbrechen   ←→ Karte", "Enter twice = cancel   ←→ map");
    default: break;
    }
    return h->tab_count == 1 ? pick(h->german, "Enter 2× = Auftrag abbrechen", "Enter twice = cancel the job")
                             : pick(h->german, "↑↓ wählen   ←→ Tab   Enter nehmen", "↑↓ choose   ←→ tab   Enter take");
}

/* the body: shiny rim (flat on the Edge), the frame, the black bezel, the screen; returns the screen's box */
static void device_body(Ctx *c, int device, float *sx0, float *sy0, float *sx1, float *sy1) {
    const Theme *t = c->t;
    const Body *b = body_of(device);
    float X = PH_M, Y = PH_M;
    if (device == TS_DEVICE_EDGE) { /* a flat metal band, only a hint of light on its edge */
        box(c, X, Y, b->w, b->h, b->r, mix(t->rim, t->frame, 0.55f), t->frame, 1, 255);
        fill(c, X + 1.5f, Y + 1.5f, b->w - 3, b->h - 3, b->r - 1.5f, t->frame);
        fill(c, X + 3, Y + 3, b->w - 6, b->h - 6, b->r - 3, t->edge);
    } else {
        box(c, X, Y, b->w, b->h, b->r, t->rim, t->frame, 1, 255);
        fill(c, X + 2.5f, Y + 2.5f, b->w - 5, b->h - 5, b->r - 2.5f, t->frame);
        fill(c, X + 5, Y + 5, b->w - 10, b->h - 10, b->r - 5, t->edge);
    }
    *sx0 = X + b->bezel, *sy0 = Y + b->bezel, *sx1 = X + b->w - b->bezel, *sy1 = Y + b->h - b->bezel;
    float sr = b->r - b->bezel;
    if (sr < 8) sr = 8;
    box(c, *sx0, *sy0, *sx1 - *sx0, *sy1 - *sy0, sr, t->top, t->bottom, 1, 255);
    { /* 0.11: the wallpaper's glows */
        float W = *sx1 - *sx0, H = *sy1 - *sy0;
        int light = theme_light(t);
        glow(c, *sx0, *sy0, W, H, sr, *sx0 + W * 0.15f, *sy0 + H * 0.12f, W * 0.75f, t->accent, light ? 55 : 95);
        glow(c, *sx0, *sy0, W, H, sr, *sx0 + W * 0.95f, *sy0 + H * 0.45f, W * 0.7f, t->accent2, light ? 45 : 70);
        glow(c, *sx0, *sy0, W, H, sr, *sx0 + W * 0.25f, *sy0 + H * 0.92f, W * 0.8f, mix(t->accent, t->top, 0.5f), light ? 40 : 75);
    }
    /* the keys on the frame's edges */
    if (device == TS_DEVICE_PHONE) {
        fill(c, X + 0.8f, Y + 130, 3, 30, 1.5f, t->rim);
        fill(c, X + 0.8f, Y + 175, 3, 52, 1.5f, t->rim);
        fill(c, X + 0.8f, Y + 238, 3, 52, 1.5f, t->rim);
        fill(c, X + b->w - 3.8f, Y + 190, 3, 80, 1.5f, t->rim);
    } else if (device == TS_DEVICE_EDGE) { /* volume and power, both on the right */
        fill(c, X + b->w - 2.6f, Y + 150, 2.4f, 92, 1.2f, t->rim);
        fill(c, X + b->w - 2.6f, Y + 262, 2.4f, 50, 1.2f, t->rim);
    } else { /* the tablet: power and volume on the top edge */
        fill(c, X + 70, Y + 0.6f, 46, 2.6f, 1.3f, t->rim);
        fill(c, X + b->w - 150, Y + 0.6f, 70, 2.6f, 1.3f, t->rim);
    }
}

/* the tablet's right column: the selected request's passenger, the line's stops (a route like on a network map), the logbook */
/* ------------------------------------------------------------------ 0.7: the map */

static int in_poly(const float *vx, const float *vy, int n, float px, float py);

typedef struct {
    float bx, by, bw, bh; /* the box, units */
    float cx, cz;         /* the world in its middle */
    float mpu;            /* metres per unit */
    float rc, rs;         /* 0.9.1: turned (navi): cos and sin of the heading that points up; 1, 0 = north up */
} MapView;

static void map_xy(const MapView *v, float x, float z, float *ux, float *uy) {
    float dx = x - v->cx, dz = z - v->cz;
    *ux = v->bx + v->bw * 0.5f + (dx * v->rc - dz * v->rs) / v->mpu;
    *uy = v->by + v->bh * 0.5f - (dx * v->rs + dz * v->rc) / v->mpu;
}

static void blend_px(Ctx *c, int px, int py, uint32_t col, float a) {
    if (px < 0 || py < 0 || px >= c->cv->w || py >= c->cv->h) return;
    if (c->clip && (px < c->clip_x0 || py < c->clip_y0 || px >= c->clip_x1 || py >= c->clip_y1)) return;
    uint32_t *d = &c->cv->px[py * c->cv->w + px];
    *d = mix(*d & 0xFFFFFF, col, a);
}

/* a thick line with round ends (units) */
static void seg(Ctx *c, float ax, float ay, float bx, float by, float width, uint32_t col, int alpha) {
    if (c->dry) return;
    float s = c->s, r = width * s * 0.5f;
    ax *= s, ay *= s, bx *= s, by *= s;
    int x0 = (int)floorf((ax < bx ? ax : bx) - r - 1), x1 = (int)ceilf((ax > bx ? ax : bx) + r + 1);
    int y0 = (int)floorf((ay < by ? ay : by) - r - 1), y1 = (int)ceilf((ay > by ? ay : by) + r + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    if (y1 > c->cv->h) y1 = c->cv->h;
    if (c->clip) {
        if (x0 < c->clip_x0) x0 = c->clip_x0;
        if (y0 < c->clip_y0) y0 = c->clip_y0;
        if (x1 > c->clip_x1) x1 = c->clip_x1;
        if (y1 > c->clip_y1) y1 = c->clip_y1;
    }
    float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            float qx = px + 0.5f - ax, qy = py + 0.5f - ay;
            float t = l2 > 1e-6f ? clampf((qx * vx + qy * vy) / l2, 0.0f, 1.0f) : 0.0f;
            float dx = qx - vx * t, dy = qy - vy * t;
            float cov = clampf(r + 0.5f - sqrtf(dx * dx + dy * dy), 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(c, px, py, col, cov * (float)alpha / 255.0f);
        }
}

/* a filled polygon (units), 4x4 samples a pixel */
static void poly(Ctx *c, const float *ux, const float *uy, int n, uint32_t col, int alpha) {
    if (c->dry || n < 3) return;
    float vx[8], vy[8], x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < n && i < 8; ++i) {
        vx[i] = ux[i] * c->s, vy[i] = uy[i] * c->s;
        if (vx[i] < x0) x0 = vx[i];
        if (vx[i] > x1) x1 = vx[i];
        if (vy[i] < y0) y0 = vy[i];
        if (vy[i] > y1) y1 = vy[i];
    }
    for (int py = (int)floorf(y0); py <= (int)ceilf(y1); ++py)
        for (int px = (int)floorf(x0); px <= (int)ceilf(x1); ++px) {
            int hit = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) hit += in_poly(vx, vy, n, px + (sx + 0.5f) / 4.0f, py + (sy + 0.5f) / 4.0f);
            if (hit) blend_px(c, px, py, col, hit / 16.0f * (float)alpha / 255.0f);
        }
}

static void dot(Ctx *c, float x, float y, float r, uint32_t col) { fill(c, x - r, y - r, 2 * r, 2 * r, r, col); }

/* a text with a dark rim, as map names are */
static void map_text(Ctx *c, const char *s, float x, float y, float w, float h, float size, int weight, uint32_t col, uint32_t rim, int align) {
    static const float o[4][2] = {{-0.9f, 0}, {0.9f, 0}, {0, -0.9f}, {0, 0.9f}};
    for (int i = 0; i < 4; ++i) text(c, s, x + o[i][0], y + o[i][1], w, h, size, weight, rim, align);
    text(c, s, x, y, w, h, size, weight, col, align);
}

/* the ground: the picture of the map (or a plain colour without one) */
static void map_ground(Ctx *c, const MapView *v, float r) {
    if (c->dry) return;
    float s = c->s, x = v->bx * s, y = v->by * s, w = v->bw * s, h = v->bh * s, rr = r * s;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    if (y1 > c->cv->h) y1 = c->cv->h;
    float mpp = v->mpu / s, mx = (v->bx + v->bw * 0.5f) * s, my = (v->by + v->bh * 0.5f) * s;
    int have = ts_map_ok();
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            float cov = rbox_cover(px + 0.5f, py + 0.5f, x, y, w, h, rr);
            if (cov <= 0.0f) continue;
            float ux = (px + 0.5f - mx) * mpp, uy = (py + 0.5f - my) * mpp;
            float wx = v->cx + ux * v->rc - uy * v->rs, wz = v->cz - ux * v->rs - uy * v->rc;
            uint32_t col = have ? ts_map_color(wx, wz, mpp) : 0x1E2A3A;
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, cov);
        }
}

/* the way between two pins along the roads; the leg from the car is kept until the car has moved on a bit */
#define WAY_MAX 400
static struct {
    float from[2], to[2];
    int n;
    float pts[WAY_MAX * 2];
} car_way;

static int map_way(const TsMapPin *a, const TsMapPin *b, float *out, int max) {
    float pa[2] = {a->x, a->z}, pb[2] = {b->x, b->z};
    if (!ts_map_ok()) {
        out[0] = pa[0], out[1] = pa[1], out[2] = pb[0], out[3] = pb[1];
        return 2;
    }
    if (a->kind != TS_PIN_CAR) return ts_map_route(pa, pb, out, max);
    float dx = car_way.from[0] - pa[0], dz = car_way.from[1] - pa[1];
    if (!car_way.n || car_way.to[0] != pb[0] || car_way.to[1] != pb[1] || dx * dx + dz * dz > 150.0f * 150.0f) {
        car_way.n = ts_map_route(pa, pb, car_way.pts, WAY_MAX);
        car_way.from[0] = pa[0], car_way.from[1] = pa[1], car_way.to[0] = pb[0], car_way.to[1] = pb[1];
    }
    /* from the car on: skip the points it has passed (the nearest point and those before it) */
    int near = 0;
    float bd = 1e18f;
    for (int i = 0; i < car_way.n - 1; ++i) {
        float ex = car_way.pts[2 * i] - pa[0], ez = car_way.pts[2 * i + 1] - pa[1], d = ex * ex + ez * ez;
        if (d < bd) bd = d, near = i;
    }
    int m = 0;
    out[m * 2] = pa[0], out[m * 2 + 1] = pa[1], m++;
    for (int i = near + 1; i < car_way.n && m < max; ++i) out[m * 2] = car_way.pts[2 * i], out[m * 2 + 1] = car_way.pts[2 * i + 1], m++;
    return m;
}

static void map_routes(Ctx *c, const MapView *v, const TsHud *h, int main_pass) {
    static float pts[WAY_MAX * 2];
    for (int layer = 0; layer < 2; ++layer) /* the dark edge under all of it, then the line */
        for (int i = 0; i < h->map_pin_count; ++i) {
            const TsMapPin *b = &h->map_pin[i];
            int r = b->route;
            if (r != TS_ROUTE_MAIN && r != TS_ROUTE_DASHED && r != TS_ROUTE_LATER) continue;
            if ((r == TS_ROUTE_MAIN) != main_pass) continue;
            int j = i - 1;
            while (j >= 0 && h->map_pin[j].route == TS_ROUTE_NONE) --j;
            if (j < 0) continue;
            const TsMapPin *a = &h->map_pin[j];
            uint32_t edge = r == TS_ROUTE_MAIN ? 0x1A3F8F : 0x232B38, line = r == TS_ROUTE_MAIN ? 0x5B9BFF : 0x8794AB;
            float wl = r == TS_ROUTE_MAIN ? 4.6f : 3.4f, we = wl + 2.6f;
            if (r == TS_ROUTE_DASHED) {
                float ax, ay, bx, by;
                map_xy(v, a->x, a->z, &ax, &ay);
                map_xy(v, b->x, b->z, &bx, &by);
                float len = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
                int dashes = (int)(len / 9.0f);
                if (layer == 1)
                    for (int k = 0; k < dashes; k += 2) {
                        float t0 = (float)k / dashes, t1 = (float)(k + 1) / dashes;
                        seg(c, ax + (bx - ax) * t0, ay + (by - ay) * t0, ax + (bx - ax) * t1, ay + (by - ay) * t1, 3.0f, line, 255);
                    }
                continue;
            }
            int n = map_way(a, b, pts, WAY_MAX);
            for (int k = 0; k + 1 < n; ++k) {
                float ax, ay, bx, by;
                map_xy(v, pts[2 * k], pts[2 * k + 1], &ax, &ay);
                map_xy(v, pts[2 * k + 2], pts[2 * k + 3], &bx, &by);
                seg(c, ax, ay, bx, by, layer ? wl : we, layer ? line : edge, 255);
            }
        }
}

/* the names drawn on the map so far: a name doesn't go over another */
static float labels[48][4];
static int label_count;
static int label_free(float x, float y, float w, float h) {
    for (int i = 0; i < label_count; ++i)
        if (x < labels[i][0] + labels[i][2] && labels[i][0] < x + w && y < labels[i][1] + labels[i][3] && labels[i][1] < y + h) return 0;
    if (label_count < 48) labels[label_count][0] = x, labels[label_count][1] = y, labels[label_count][2] = w, labels[label_count][3] = h, label_count++;
    return 1;
}

/* a name in a small dark plate beside a pin: right of it, or left when it doesn't fit */
static void map_plate(Ctx *c, const MapView *v, float px, float py, const char *s, uint32_t col) {
    if (!s || !s[0]) return;
    float tw = measure(c, s, 11.5f, 600) + 14, th = 20;
    if (tw > v->bw * 0.6f) tw = v->bw * 0.6f;
    /* right of the pin, left of it, under it, over it (centred, then to either side): the first place inside the map and free */
    float cand[8][2] = {{px + 13, py - th * 0.5f}, {px - 13 - tw, py - th * 0.5f}, {px - tw * 0.5f, py + 13}, {px - tw * 0.5f, py - 13 - th},
                        {px - 8, py - 13 - th}, {px + 8 - tw, py - 13 - th}, {px - 8, py + 13}, {px + 8 - tw, py + 13}};
    float x = 0, y = 0;
    int ok = 0;
    for (int k = 0; k < 8 && !ok; ++k) {
        x = cand[k][0], y = cand[k][1];
        if (k >= 2) x = clampf(x, v->bx + 4, v->bx + v->bw - 4 - tw);
        if (x < v->bx + 4 || x + tw > v->bx + v->bw - 4 || y < v->by + 2 || y + th > v->by + v->bh - 2) continue;
        ok = label_free(x, y, tw, th);
    }
    if (!ok) return;
    box(c, x, y, tw, th, 7, 0x15181F, 0x15181F, 1, 225);
    text(c, s, x + 7, y, tw - 14, th, 11.5f, 600, col, TS_TEXT_LEFT);
}

/* 0.9.1: where the last map was drawn, in pixels (the window keeps it still while the rest fades) */
int ts_map_rect_px[4];

/* a dark shade fading out from the map's top and bottom edges */
static void map_shade(Ctx *c, float bx, float by, float bw, float bh, float r) {
    if (c->dry) return;
    float s = c->s, x = bx * s, y = by * s, w = bw * s, h = bh * s, rr = r * s, band = 34.0f * s;
    int x0 = (int)floorf(x), x1 = (int)ceilf(x + w);
    if (x0 < 0) x0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    for (int py = (int)floorf(y); py < (int)ceilf(y + h); ++py) {
        if (py < 0 || py >= c->cv->h) continue;
        float d = py + 0.5f - y, e = y + h - (py + 0.5f), a;
        if (d < band) a = 0.30f * (1.0f - d / band) * (1.0f - d / band);
        else if (e < band) a = 0.26f * (1.0f - e / band) * (1.0f - e / band);
        else continue;
        for (int px = x0; px < x1; ++px) {
            float cov = rbox_cover(px + 0.5f, py + 0.5f, x, y, w, h, rr);
            if (cov > 0.0f) blend_px(c, px, py, 0x05080E, a * cov);
        }
    }
}

static int inside(const MapView *v, float x, float y, float m) {
    return x >= v->bx - m && y >= v->by - m && x <= v->bx + v->bw + m && y <= v->by + v->bh + m;
}

/* the map in a box: the ground, the hotspots, the route, the names, the stops and pins, you */
static void map_box(Ctx *c, const TsHud *h, float bx, float by, float bw, float bh, float r) {
    if (c->dry || bw < 20 || bh < 20) return;
    MapView v = {bx, by, bw, bh, h->map_x, h->map_z, 1.0f, cosf(h->map_rot), sinf(h->map_rot)};
    float nv = clampf(h->map_navi_t, 0.0f, 1.0f); /* 0.9.1: the dot (0) and the navi arrow (1), eased by the window */
    ts_map_rect_px[0] = (int)floorf(bx * c->s), ts_map_rect_px[1] = (int)floorf(by * c->s);
    ts_map_rect_px[2] = (int)ceilf((bx + bw) * c->s), ts_map_rect_px[3] = (int)ceilf((by + bh) * c->s);
    float mw = h->map_w > 100.0f ? h->map_w : 100.0f, mh = h->map_h > 100.0f ? h->map_h : 100.0f;
    v.mpu = mw / bw > mh / bh ? mw / bw : mh / bh;
    map_ground(c, &v, r);
    int saved = c->clip, sx0 = c->clip_x0, sy0 = c->clip_y0, sx1 = c->clip_x1, sy1 = c->clip_y1;
    c->clip = 1;
    c->clip_x0 = (int)floorf(bx * c->s), c->clip_y0 = (int)floorf(by * c->s);
    c->clip_x1 = (int)ceilf((bx + bw) * c->s), c->clip_y1 = (int)ceilf((by + bh) * c->s);
    float x, y;
    /* hotspots: a faint orange disc as big as the area */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_HOT) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        float rr = (float)p->num / v.mpu;
        if (rr < 10) rr = 10;
        box(c, x - rr, y - rr, 2 * rr, 2 * rr, rr, 0xFF7A2F, 0xFF7A2F, 1, 50);
        box(c, x - rr * 0.45f, y - rr * 0.45f, rr * 0.9f, rr * 0.9f, rr * 0.45f, 0xFF7A2F, 0xFF7A2F, 1, 40);
    }
    map_routes(c, &v, h, 0);
    map_routes(c, &v, h, 1);
    /* 0.9.1: a soft shade along the top and the bottom, so the compass and the scale sit on calm ground */
    map_shade(c, bx, by, bw, bh, r);
    label_count = 0;
    for (int i = 0; i < h->map_pin_count; ++i) { /* names keep off you and the route's pins */
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind < TS_PIN_STOP) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        if (p->kind == TS_PIN_DEST) label_free(x - 10, y - 26, 20, 28);
        else label_free(x - 10, y - 10, 20, 20);
    }

    /* the names of the route's places, on plates */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->route == TS_ROUTE_NONE || !p->label[0]) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        if (!inside(&v, x, y, 0)) continue;
        uint32_t col = p->kind == TS_PIN_PICKUP ? 0x7CE8B8 : p->kind == TS_PIN_RIDER2 ? 0xD6BDFF : p->kind == TS_PIN_DEST ? 0xFFB4AB : 0xFFFFFF;
        map_plate(c, &v, x, p->kind == TS_PIN_DEST ? y - 15 : y, p->label, col);
    }
    /* the hotspots' surcharge */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_HOT || !p->label[0]) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        float tw = measure(c, p->label, 11.5f, 700) + 14;
        if (!inside(&v, x, y, -12) || !label_free(x - tw / 2, y + 10, tw, 19)) continue;
        box(c, x - tw / 2, y + 10, tw, 19, 9.5f, 0xFF7A2F, 0xFF7A2F, 1, 235);
        text(c, p->label, x - tw / 2, y + 10, tw, 19, 11.5f, 700, 0xFFFFFF, TS_TEXT_CENTER);
    }
    /* the towns' names (not where a pin of the route has its own) */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_TOWN && p->kind != TS_PIN_FESTIVAL) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        int crowded = 0;
        for (int j = 0; j < h->map_pin_count && !crowded; ++j) {
            const TsMapPin *q = &h->map_pin[j];
            if (q->route == TS_ROUTE_NONE || q->kind == TS_PIN_CAR || !q->label[0]) continue;
            float qx, qy;
            map_xy(&v, q->x, q->z, &qx, &qy);
            crowded = fabsf(qx - x) < 60 && fabsf(qy - y) < 22;
        }
        if (crowded) continue;
        float tw = measure(c, p->label, 12, 700) + 6;
        int fest = p->kind == TS_PIN_FESTIVAL;
        float ty = fest ? y + 4 : y - 9;
        if (fest) dot(c, x, y, 4.5f, 0xFF7A2F), fill(c, x - 1.8f, y - 1.8f, 3.6f, 3.6f, 1.8f, 0xFFFFFF);
        float lx = clampf(x - tw / 2, bx + 3, bx + bw - 3 - tw); /* at the edge: pushed in */
        if (fabsf(lx + tw / 2 - x) > tw * 0.5f || ty < by + 2 || ty + 18 > by + bh - 2) continue;
        if (!label_free(lx, ty + 3, tw, 12)) {
            ty = fest ? y - 22 : y + 4; /* the other side of the place */
            if (ty < by + 2 || ty + 18 > by + bh - 2 || !label_free(lx, ty + 3, tw, 12)) continue;
        }
        x = lx + tw / 2;
        char up[40];
        snprintf(up, sizeof(up), "%s", p->label);
        if (!fest)
            for (char *q = up; *q; ++q)
                if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        map_text(c, up, x - tw / 2, ty, tw, 18, fest ? 11.5f : 11, 700, fest ? 0xFFC9A3 : 0xD5DCE8, 0x0B111C, TS_TEXT_CENTER);
    }
    /* the stops */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_STOP && p->kind != TS_PIN_STOP_DONE && p->kind != TS_PIN_STOP_NEXT) continue;
        if (p->num < 0) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        if (!inside(&v, x, y, 12)) continue;
        char num[8];
        snprintf(num, sizeof(num), "%d", p->num);
        if (p->kind == TS_PIN_STOP_DONE) {
            dot(c, x, y, 6, 0x232B38), dot(c, x, y, 4.5f, 0x6B7486);
            continue;
        }
        int next = p->kind == TS_PIN_STOP_NEXT;
        float rr = next ? 10.5f : 9;
        dot(c, x, y, rr + 1.6f, next ? 0xFFFFFF : 0x1A3F8F);
        dot(c, x, y, rr, next ? 0x2F6FE4 : 0xFFFFFF);
        text(c, num, x - rr, y - rr, 2 * rr, 2 * rr, next ? 11.5f : 10.5f, 700, next ? 0xFFFFFF : 0x1A3F8F, TS_TEXT_CENTER);
    }
    /* pickup, the second rider, destinations */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        map_xy(&v, p->x, p->z, &x, &y);
        if (!inside(&v, x, y, 16)) continue;
        if (p->kind == TS_PIN_PICKUP || p->kind == TS_PIN_RIDER2) {
            uint32_t col = p->kind == TS_PIN_PICKUP ? 0x1EB980 : 0xB07CFF;
            dot(c, x, y, 10, 0xFFFFFF);
            dot(c, x, y, 8, col);
            if (p->kind == TS_PIN_RIDER2) text(c, "2", x - 8, y - 8, 16, 16, 10.5f, 700, 0xFFFFFF, TS_TEXT_CENTER);
            else dot(c, x, y, 3.2f, 0xFFFFFF);
        } else if (p->kind == TS_PIN_DEST) {
            /* a drop pin: round head, point at the place */
            float hx = x, hy = y - 15;
            float tx[3] = {hx - 7.4f, hx + 7.4f, x}, ty[3] = {hy + 4.5f, hy + 4.5f, y};
            poly(c, tx, ty, 3, 0x7A1B14, 255);
            dot(c, hx, hy, 10, 0x7A1B14);
            float tx2[3] = {hx - 6.4f, hx + 6.4f, x}, ty2[3] = {hy + 4.2f, hy + 4.2f, y - 1.6f};
            poly(c, tx2, ty2, 3, 0xEA4335, 255);
            dot(c, hx, hy, 8.8f, 0xEA4335);
            dot(c, hx, hy, 3.4f, 0x7A1B14);
        }
    }
    /* you: the blue dot with its beam */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_CAR) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        if (!inside(&v, x, y, 20)) continue;
        if (nv >= 1.0f) continue; /* navi: an arrow instead (below) */
        float a = h->map_heading - h->map_rot; /* on the screen */
        float spread = 0.55f, len = 30;
        /* the beam: from bright at you to nothing at its end */
        for (int k = 0; k < 6; ++k) {
            float l0 = len * k / 6.0f, l1 = len * (k + 1) / 6.0f;
            float bxp[4] = {x + l0 * sinf(a - spread), x + l1 * sinf(a - spread), x + l1 * sinf(a + spread), x + l0 * sinf(a + spread)};
            float byp[4] = {y - l0 * cosf(a - spread), y - l1 * cosf(a - spread), y - l1 * cosf(a + spread), y - l0 * cosf(a + spread)};
            poly(c, bxp, byp, 4, 0x5B9BFF, (int)((120.0f - 95.0f * k / 5.0f) * (1.0f - nv)));
        }
        box(c, x - 16, y - 16, 32, 32, 16, 0x5B9BFF, 0x5B9BFF, 1, (int)(60.0f * (1.0f - nv)));
    }
    /* (the halo above was opaque: the dot is drawn with its ring over it) */
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind != TS_PIN_CAR) continue;
        map_xy(&v, p->x, p->z, &x, &y);
        if (!inside(&v, x, y, 20)) continue;
        if (nv > 0.0f) { /* the navigation arrow, pointing the way you drive (up, once the map has turned) */
            static const float ox0[4] = {0, 11.5f, 0, -11.5f}, oy0[4] = {-15.5f, 11.5f, 5.5f, 11.5f};
            static const float ix0[4] = {0, 8.0f, 0, -8.0f}, iy0[4] = {-11.0f, 7.5f, 2.5f, 7.5f};
            float a = h->map_heading - h->map_rot, ca = cosf(a), sa = sinf(a), g = 0.7f + 0.3f * nv; /* it grows in */
            float ox[4], oy[4], ix[4], iy[4];
            for (int k = 0; k < 4; ++k) {
                ox[k] = x + g * (ox0[k] * ca - oy0[k] * sa), oy[k] = y + g * (ox0[k] * sa + oy0[k] * ca);
                ix[k] = x + g * (ix0[k] * ca - iy0[k] * sa), iy[k] = y + g * (ix0[k] * sa + iy0[k] * ca);
            }
            int al = (int)(255.0f * nv);
            box(c, x - 20, y - 20, 40, 40, 20, 0x4285F4, 0x4285F4, 1, (int)(45.0f * nv));
            poly(c, ox, oy, 4, 0xFFFFFF, al);
            poly(c, ix, iy, 4, 0x4285F4, al);
        }
        if (nv < 1.0f) {
            int al = (int)(255.0f * (1.0f - nv));
            box(c, x - 9.5f, y - 9.5f, 19, 19, 9.5f, 0xFFFFFF, 0xFFFFFF, 1, al);
            box(c, x - 7, y - 7, 14, 14, 7, 0x4285F4, 0x4285F4, 1, al);
        }
    }
    /* navi: where north is, top right */
    if (nv > 0.0f) { /* it fades in with navi */
        float nx = bx + bw - 22, ny = by + 22, a = h->map_rot;
        int al = (int)(255.0f * nv);
        box(c, nx - 13, ny - 13, 26, 26, 13, 0x252A34, 0x15181F, 1, (int)(235.0f * nv));
        float sx = sinf(-a), sy = -cosf(-a); /* north on the screen */
        float tx[3] = {nx + sx * 9.5f, nx - sy * 3.6f, nx + sy * 3.6f}, ty[3] = {ny + sy * 9.5f, ny + sx * 3.6f, ny - sx * 3.6f};
        poly(c, tx, ty, 3, 0xEA4335, al);
        float tx2[3] = {nx - sx * 9.5f, nx - sy * 3.6f, nx + sy * 3.6f}, ty2[3] = {ny - sy * 9.5f, ny + sx * 3.6f, ny - sx * 3.6f};
        poly(c, tx2, ty2, 3, 0x9AA0A6, al);
    }
    /* the scale, bottom right: a round length about 60 units long */
    {
        static const int steps[] = {100, 200, 500, 1000, 2000, 5000};
        int m = 100;
        for (int i = 0; i < 6; ++i) if ((float)steps[i] / v.mpu <= 70.0f) m = steps[i];
        float len = (float)m / v.mpu, rx = bx + bw - 12 - len, ry = by + bh - 12;
        char lab[16];
        if (m >= 1000) snprintf(lab, sizeof(lab), "%d km", m / 1000);
        else snprintf(lab, sizeof(lab), "%d m", m);
        fill(c, rx, ry, len, 2.4f, 1.2f, 0xE8EAED);
        fill(c, rx, ry - 4, 2.4f, 6.4f, 1.2f, 0xE8EAED);
        fill(c, rx + len - 2.4f, ry - 4, 2.4f, 6.4f, 1.2f, 0xE8EAED);
        map_text(c, lab, rx - 60, ry - 19, len + 60, 16, 10.5f, 600, 0xE8EAED, 0x0B111C, TS_TEXT_RIGHT);
    }
    c->clip = saved, c->clip_x0 = sx0, c->clip_y0 = sy0, c->clip_x1 = sx1, c->clip_y1 = sy1;
}

static void detail_column(Ctx *c, const TsHud *h, float x0, float y, float w, float bottom) {
    const Theme *t = c->t;
    glass(c, x0, y, w, bottom - y, 20);
    float x = x0 + 16, cw = w - 32;
    y += 12;
    text(c, h->detail_title, x, y, cw, 26, 16.5f, 700, t->text, TS_TEXT_LEFT);
    y += 34;
    /* the route's line: from the first stop row to the last */
    int first = -1, last = -1;
    for (int i = 0; i < h->detail_count; ++i)
        if (h->detail_kind[i] >= TS_DET_STOP && h->detail_kind[i] <= TS_DET_STOP_DONE) {
            if (first < 0) first = i;
            last = i;
        }
    const float row_h = 30.0f;
    float ry = y;
    for (int i = 0; i < h->detail_count; ++i) {
        int k = h->detail_kind[i];
        if (k == TS_DET_TEXT) {
            text_wrap(c, h->detail_label[i], x, ry + 4, cw, 60, 13, t->dim);
            ry += 64;
            continue;
        }
        if (k == TS_DET_PAIR) {
            float vw = measure(c, h->detail_value[i], 13.5f, 700) + 4;
            if (vw <= 4) vw = 0;
            if (vw > cw * 0.5f) vw = cw * 0.5f;
            text(c, h->detail_label[i], x, ry, cw - vw - 8, row_h - 4, 13.5f, 600, t->dim, TS_TEXT_LEFT);
            if (vw > 0) text(c, h->detail_value[i], x + cw - vw, ry, vw, row_h - 4, 13.5f, 700, t->text, TS_TEXT_RIGHT);
            ry += row_h;
            continue;
        }
        /* a stop: the line through it, the dot (done: small and dim, now: big in the app's colour), its name */
        float cx = x + 9, cy = ry + (row_h - 4) / 2;
        uint32_t line_col = k == TS_DET_STOP_DONE ? t->dim : t->accent;
        if (i != first) box(c, cx - 2, cy - row_h, 4, row_h, 2, line_col, line_col, 1, k == TS_DET_STOP_DONE ? 120 : 255);
        if (i != last && first >= 0) {
            int next_done = i + 1 < h->detail_count && h->detail_kind[i + 1] == TS_DET_STOP_DONE;
            uint32_t nc = next_done ? t->dim : t->accent;
            box(c, cx - 2, cy, 4, row_h, 2, nc, nc, 1, next_done ? 120 : 255);
        }
        /* 0.5.1: as in ride apps, the destination (the last stop) is a square, the others are dots */
        float rr = i == last && k != TS_DET_STOP_DONE ? 2.0f : -1.0f;
        if (k == TS_DET_STOP_NOW) {
            fill(c, cx - 9, cy - 9, 18, 18, rr >= 0 ? 3.0f : 9, t->accent);
            fill(c, cx - 4.5f, cy - 4.5f, 9, 9, rr >= 0 ? 1.5f : 4.5f, 0xFFFFFF);
        } else if (k == TS_DET_STOP_DONE) {
            fill(c, cx - 5, cy - 5, 10, 10, 5, t->dim);
        } else {
            fill(c, cx - 7, cy - 7, 14, 14, rr >= 0 ? rr : 7, t->accent);
            fill(c, cx - 4, cy - 4, 8, 8, rr >= 0 ? 1.0f : 4, t->card_alpha < 255 ? t->top : t->card);
        }
        float vw = measure(c, h->detail_value[i], 12.5f, 600) + 4;
        if (vw <= 4) vw = 0;
        if (vw > cw * 0.35f) vw = cw * 0.35f;
        text(c, h->detail_label[i], x + 28, ry, cw - 28 - vw - 6, row_h - 4, 13.5f, k == TS_DET_STOP_NOW ? 700 : 600,
             k == TS_DET_STOP_DONE ? t->dim : t->text, TS_TEXT_LEFT);
        if (vw > 0) text(c, h->detail_value[i], x + cw - vw, ry, vw, row_h - 4, 12.5f, 600, t->dim, TS_TEXT_RIGHT);
        ry += row_h;
    }
}

/* ------------------------------------------------------------------ 0.12: the app's new look, as in the Android app:
   the head with the car and the company funds, a big page title, roomy glass cards (the chosen one with a rim in the
   app's colour; a ride request opens a "take the ride" pill), switches for On/Off, tiles for short numbers, a GO card
   for the shift, a sheet for the running job, and a floating tab bar with icons at the bottom */

enum { IC_CAR = 0, IC_BUS, IC_MAP, IC_PERSON, IC_BAG, IC_GEAR };

static void icon(Ctx *c, int kind, float cx, float cy, float s, uint32_t col) {
    float u = s / 20.0f; /* drawn on a 20 x 20 grid round (cx, cy) */
#define P(x, y) cx + ((x) - 10) * u, cy + ((y) - 10) * u
    switch (kind) {
    case IC_CAR:
        seg(c, P(5, 9), P(7, 4.5f), 1.8f * u, col, 255);
        seg(c, P(7, 4.5f), P(13, 4.5f), 1.8f * u, col, 255);
        seg(c, P(13, 4.5f), P(15, 9), 1.8f * u, col, 255);
        fill(c, cx - 8 * u, cy - 1.5f * u, 16 * u, 7 * u, 2.5f * u, col);
        fill(c, cx - 7 * u, cy + 4.5f * u, 3.4f * u, 4 * u, 1.2f * u, col);
        fill(c, cx + 3.6f * u, cy + 4.5f * u, 3.4f * u, 4 * u, 1.2f * u, col);
        break;
    case IC_BUS:
        outline(c, cx - 6.5f * u, cy - 8 * u, 13 * u, 14.5f * u, 3 * u, 1.8f * u, col, 255);
        fill(c, cx - 6.5f * u, cy - 2 * u, 13 * u, 1.8f * u, 0, col);
        fill(c, cx - 5 * u, cy + 6 * u, 3 * u, 3 * u, 1 * u, col);
        fill(c, cx + 2 * u, cy + 6 * u, 3 * u, 3 * u, 1 * u, col);
        break;
    case IC_MAP:
        seg(c, P(3, 5), P(8, 3), 1.8f * u, col, 255);
        seg(c, P(8, 3), P(12, 5), 1.8f * u, col, 255);
        seg(c, P(12, 5), P(17, 3), 1.8f * u, col, 255);
        seg(c, P(17, 3), P(17, 15), 1.8f * u, col, 255);
        seg(c, P(17, 15), P(12, 17), 1.8f * u, col, 255);
        seg(c, P(12, 17), P(8, 15), 1.8f * u, col, 255);
        seg(c, P(8, 15), P(3, 17), 1.8f * u, col, 255);
        seg(c, P(3, 17), P(3, 5), 1.8f * u, col, 255);
        seg(c, P(8, 3), P(8, 15), 1.6f * u, col, 255);
        seg(c, P(12, 5), P(12, 17), 1.6f * u, col, 255);
        break;
    case IC_PERSON:
        outline(c, cx - 3.6f * u, cy - 8 * u, 7.2f * u, 7.2f * u, 3.6f * u, 1.8f * u, col, 255);
        outline(c, cx - 7 * u, cy + 1.5f * u, 14 * u, 12 * u, 6 * u, 1.8f * u, col, 255);
        break;
    case IC_BAG:
        outline(c, cx - 7 * u, cy - 3 * u, 14 * u, 11 * u, 2.5f * u, 1.8f * u, col, 255);
        seg(c, P(7, 7), P(7, 5), 1.8f * u, col, 255);
        seg(c, P(7, 5), P(8.5f, 2.6f), 1.8f * u, col, 255);
        seg(c, P(8.5f, 2.6f), P(11.5f, 2.6f), 1.8f * u, col, 255);
        seg(c, P(11.5f, 2.6f), P(13, 5), 1.8f * u, col, 255);
        seg(c, P(13, 5), P(13, 7), 1.8f * u, col, 255);
        break;
    default: /* the gear: a ring, eight teeth */
        for (int i = 0; i < 8; ++i) {
            float a = i * 0.785398f, dx = sinf(a), dy = cosf(a);
            seg(c, cx + dx * 5.5f * u, cy + dy * 5.5f * u, cx + dx * 8.2f * u, cy + dy * 8.2f * u, 2.6f * u, col, 255);
        }
        outline(c, cx - 6 * u, cy - 6 * u, 12 * u, 12 * u, 6 * u, 2.2f * u, col, 255);
        outline(c, cx - 2.4f * u, cy - 2.4f * u, 4.8f * u, 4.8f * u, 2.4f * u, 1.6f * u, col, 255);
        break;
    }
#undef P
}

static int tab_icon(const TsHud *h, int i) {
    if (h->page >= TS_PAGE_JOB) return i == 0 ? IC_CAR : IC_MAP;
    static const int ic[TS_TABS] = {IC_CAR, IC_BUS, IC_MAP, IC_PERSON, IC_BAG, IC_GEAR};
    return i >= 0 && i < TS_TABS ? ic[i] : IC_CAR;
}

/* the head: logo, name, the car under it; the company funds in a glass pill on the right */
static float head2(Ctx *c, const TsHud *h, float x0, float y, float cw) {
    const Theme *t = c->t;
    logo(c, x0, y, 38);
    float mw = 0;
    if (h->money[0]) {
        float m1 = measure(c, h->money, 15.5f, 800), m2 = measure(c, h->money_label, 9.5f, 600);
        mw = (m1 > m2 ? m1 : m2) + 24;
        glass(c, x0 + cw - mw, y - 1, mw, 40, 20);
        text(c, h->money, x0 + cw - mw, y + 2, mw, 20, 15.5f, 800, t->text, TS_TEXT_CENTER);
        text(c, h->money_label, x0 + cw - mw, y + 21, mw, 14, 9.5f, 600, t->dim, TS_TEXT_CENTER);
    }
    float nw = cw - 48 - mw - 6;
    text(c, "Horizon Transit", x0 + 46, y + 1, nw + 2, 22, 15.5f, 800, t->text, TS_TEXT_LEFT);
    dot(c, x0 + 52, y + 29, 3.2f, 0x2FD27A);
    text(c, h->head_sub[0] ? h->head_sub : h->line2, x0 + 60, y + 21, nw - 12, 16, 11.5f, 500, t->dim, TS_TEXT_LEFT);
    return 50;
}

/* the floating tab bar: glass, the open tab under a glossy pill, an icon over each name */
static void tab_bar(Ctx *c, const TsHud *h, float x0, float y, float cw, float bh) {
    const Theme *t = c->t;
    int n = h->tab_count > 0 ? (h->tab_count < 8 ? h->tab_count : 8) : 1;
    ts_tab_rect_px[0] = (int)floorf(x0 * c->s), ts_tab_rect_px[1] = (int)floorf(y * c->s);
    ts_tab_rect_px[2] = (int)ceilf((x0 + cw) * c->s), ts_tab_rect_px[3] = (int)ceilf((y + bh) * c->s);
    glass(c, x0, y, cw, bh, bh * 0.5f);
    float tw = (cw - 10) / n;
    float pos = h->tab_slide > 0.0f ? clampf(h->tab_slide - 1.0f, 0.0f, (float)(n - 1)) : (float)h->tab;
    /* 0.13: a rounded box as wide as the tab (it used to be a circle taller than the tab, poking out of the bar) */
    if (pos >= 0.0f) glossy(c, x0 + 6 + pos * tw, y + 6, tw - 2, bh - 12, fminf(tw - 2, bh - 12) * 0.38f);
    int lit = (int)(pos + 0.5f);
    float isz = bh > 50 ? 19 : 16, lsz = n > 5 ? 9.5f : 10.5f;
    for (int i = 0; i < n; ++i) {
        uint32_t col = i == lit ? t->sel_text : t->dim;
        float tx = x0 + 5 + i * tw;
        icon(c, tab_icon(h, i), tx + tw / 2, y + bh * 0.38f, isz, col);
        float sz = lsz; /* 0.13: a long name ("Optionen" in bold) gets smaller instead of leaving its box */
        while (sz > 7.5f && measure(c, h->tabs[i], sz, i == lit ? 700 : 600) > tw - 8) sz -= 0.5f;
        text(c, h->tabs[i], tx, y + bh * 0.6f, tw, bh * 0.3f, sz, i == lit ? 700 : 600, col, TS_TEXT_CENTER);
    }
}

typedef struct {
    float one, gap, title, tile, go, two, pill, btn, hero;
} Metrics;
static const Metrics M_ROOMY = {44, 7, 38, 58, 80, 64, 42, 44, 100};
static const Metrics M_DENSE = {38, 5, 30, 50, 66, 54, 34, 38, 86};

static int is_switch(const char *v, int *on) {
    if (!strcmp(v, "An") || !strcmp(v, "Ein") || !strcmp(v, "On")) return *on = 1, 1;
    if (!strcmp(v, "Aus") || !strcmp(v, "Off")) return *on = 0, 1;
    return 0;
}

static void switch2(Ctx *c, float x, float y, int on) {
    const Theme *t = c->t;
    if (on) {
        box(c, x, y, 42, 24, 12, t->accent, t->accent2, 0, 255);
        box_fade(c, x, y, 42, 24, 12, 0xFFFFFF, 60, 0, 0.6f);
        dot(c, x + 30, y + 12, 9, t->sel_text);
    } else {
        box(c, x, y, 42, 24, 12, t->card, t->card, 1, 255);
        outline(c, x, y, 42, 24, 12, 2, t->dim, 200);
        dot(c, x + 12, y + 12, 6, t->dim);
    }
}

/* the chosen card: its rim and a light wash in the app's colour */
static void chosen(Ctx *c, float x, float y, float w, float h, float r) {
    const Theme *t = c->t;
    glass(c, x, y, w, h, r);
    box(c, x, y, w, h, r, t->accent, t->accent2, 0, theme_light(t) ? 26 : 40);
    outline(c, x, y, w, h, r, 2.0f, t->accent, 255);
}

static void chip(Ctx *c, const char *s, float x1, float y, float maxw, int hot) {
    const Theme *t = c->t;
    if (!s || !s[0]) return;
    float w = measure(c, s, 11, 700) + 20;
    if (w > maxw) w = maxw;
    if (hot) box(c, x1 - w, y, w, 22, 11, t->accent, t->accent2, 0, 255);
    else glass(c, x1 - w, y, w, 22, 11);
    text(c, s, x1 - w + 6, y, w - 12, 22, 11, 700, hot ? t->sel_text : t->dim, TS_TEXT_CENTER);
}

static int short_pair(Ctx *c, const TsHud *h, int r, float half) {
    return h->row_heading[r] == 0 && h->row_value[r][0] && !(r < 9 && (h->row_meter_on >> r & 1u)) && strlen(h->row_value[r]) <= 16 &&
           measure(c, h->row_label[r], 11, 600) < half - 22 && measure(c, h->row_value[r], 17, 800) < half - 22;
}

/* the page: returns the y after it */
static float page2(Ctx *c, const TsHud *h, float x0, float y, float cw, const Metrics *m) {
    const Theme *t = c->t;
    float x1 = x0 + cw;
    int first = h->row_count > 0 && h->row_heading[0] == 1 ? 1 : 0;
    int job = h->page >= TS_PAGE_JOB;
    int i = first;
    /* the title (or, on the job, the sheet) */
    if (job && h->page == TS_PAGE_JOB) {
        float hh = m->hero;
        glass(c, x0, y, cw, hh, 24);
        box_fade(c, x0, y, cw, hh, 24, t->accent, 60, 0, 0.9f);
        fill(c, x0 + cw / 2 - 18, y + 7, 36, 4, 2, t->dim);
        float sw = 0;
        if (h->speed_now >= 0) {
            char sp[16];
            snprintf(sp, sizeof(sp), "%d", h->speed_now);
            sw = measure(c, sp, 40, 800) + 6;
            float uw = measure(c, h->speed_unit, 10, 700) + 2;
            text(c, sp, x1 - 14 - uw - sw, y + 16, sw, 46, 40, 800, t->text, TS_TEXT_RIGHT);
            text(c, h->speed_unit, x1 - 12 - uw, y + 44, uw, 14, 10, 700, t->dim, TS_TEXT_LEFT);
            sw += uw + 8;
        }
        const char *lab = h->trip_on ? h->trip_label : h->tabs[0];
        const char *to = h->trip_on ? h->trip_to : (first < h->row_count ? h->row_label[first] : "");
        text(c, lab, x0 + 16, y + 18, cw - 32 - sw, 16, 11, 700, t->accent, TS_TEXT_LEFT);
        text(c, to, x0 + 16, y + 34, cw - 32 - sw, 24, 17, 800, t->text, TS_TEXT_LEFT);
        if (h->trip_on && first < h->row_count && h->row_heading[first] == 2)
            text(c, h->row_label[first], x0 + 16, y + 57, cw - 32 - sw, 16, 11, 500, t->dim, TS_TEXT_LEFT);
        float p = clampf(h->trip_on ? h->trip_progress : 0, 0, 1), by = y + hh - 16;
        fill(c, x0 + 16, by, cw - 32, 6, 3, mix(t->card_line, t->dim, 0.3f));
        if (p > 0.0f) box(c, x0 + 16, by, (cw - 32) * p < 6 ? 6 : (cw - 32) * p, 6, 3, t->accent, t->accent2, 0, 255);
        y += hh + m->gap + 2;
        if (first < h->row_count && h->row_heading[first] == 2) ++i;
    } else if (!job && h->page != TS_PAGE_MAP) {
        const char *title = h->tab >= 0 && h->tab < h->tab_count ? h->tabs[h->tab] : "";
        float tw = measure(c, title, 22, 800) + 8;
        text(c, title, x0 + 2, y + 2, tw, 30, 22, 800, t->text, TS_TEXT_LEFT);
        if (first < h->row_count && h->row_heading[first] == 2) {
            const char *cv = h->row_value[first][0] ? h->row_value[first] : h->page == TS_PAGE_RIDES ? "" : h->row_label[first];
            chip(c, cv, x1 - 2, y + 7, cw - tw - 12, strstr(cv, "\xC3\x97") != NULL); /* a hotspot's "1,3×" in colour */
            ++i;
        }
        y += m->title;
    }
    int last_row = h->row_count - 1;
    while (i < h->row_count) {
        int r = i;
        /* a heading inside the list */
        if (h->row_heading[r] == 1 || (h->row_heading[r] == 2 && h->page != TS_PAGE_BUS && h->page != TS_PAGE_RIDES)) {
            text(c, h->row_label[r], x0 + 4, y, cw * 0.6f, 22, 12.5f, 700, t->dim, TS_TEXT_LEFT);
            if (h->row_value[r][0]) text(c, h->row_value[r], x0 + cw * 0.6f, y, cw * 0.4f - 4, 22, 12.5f, 600, t->dim, TS_TEXT_RIGHT);
            y += 26;
            ++i;
            continue;
        }
        int sel = r == h->row_selected;
        int sub = (r + 1 < h->row_count && h->row_heading[r + 1] == 2 && h->row_heading[r] == 0 && h->page == TS_PAGE_RIDES) ? r + 1 : -1;
        if (sub >= 0 && sub == h->row_selected) sel = 1;
        int sw_on = 0;
        /* the shift: a GO card */
        if (r == h->go_row && !job) {
            float gh = m->go;
            if (sel) chosen(c, x0, y, cw, gh, 26);
            else glass(c, x0, y, cw, gh, 26);
            float d = gh - 22;
            float bx = x1 - 12 - d, byy = y + 11;
            if (h->go_on) {
                glass(c, bx, byy, d, d, d / 2);
                outline(c, bx, byy, d, d, d / 2, 2, t->accent, 255);
                text(c, pick(h->german, "Ende", "End"), bx, byy, d, d, 14, 800, t->text, TS_TEXT_CENTER);
            } else {
                dot(c, bx + d / 2, byy + d / 2, d / 2 + 4, mix(t->accent, t->bottom, 0.55f));
                glossy(c, bx, byy, d, d, d / 2);
                text(c, "GO", bx, byy, d, d, d * 0.32f, 900, t->sel_text, TS_TEXT_CENTER);
            }
            text(c, h->go_title, x0 + 16, y + 12, cw - d - 40, 26, 19, 800, t->text, TS_TEXT_LEFT);
            text(c, h->go_text, x0 + 16, y + 38, cw - d - 40, 18, 11.5f, 500, t->dim, TS_TEXT_LEFT);
            y += gh + m->gap + 2;
            ++i;
            continue;
        }
        /* a button: an entry without a value (the job's cancel too) */
        if (h->row_heading[r] == 0 && (!h->row_value[r][0] || (job && r == last_row))) {
            float bh = m->btn;
            int red = job && r == last_row;
            if (red) box(c, x0, y, cw, bh, bh / 2, sel ? 0xFF4D5E : mix(0xFF4D5E, t->bottom, 0.6f), sel ? 0xFF6A4D : mix(0xFF4D5E, t->bottom, 0.6f), 0, 255);
            else if (sel) glossy(c, x0, y, cw, bh, bh / 2);
            else glass(c, x0, y, cw, bh, bh / 2);
            if (red || sel) box_fade(c, x0, y, cw, bh, bh / 2, 0xFFFFFF, 60, 0, 0.6f);
            uint32_t tc = red ? 0xFFFFFF : sel ? t->sel_text : t->accent;
            if (h->row_value[r][0]) {
                text(c, h->row_label[r], x0 + 18, y, cw * 0.6f - 18, bh, 15, 800, tc, TS_TEXT_LEFT);
                text(c, h->row_value[r], x0 + cw * 0.6f, y, cw * 0.4f - 18, bh, 12, 700, tc, TS_TEXT_RIGHT);
            } else text(c, h->row_label[r], x0 + 18, y, cw - 36, bh, 15, 800, tc, TS_TEXT_CENTER);
            y += bh + m->gap;
            ++i;
            continue;
        }
        /* two short numbers side by side: tiles */
        float half = (cw - m->gap) / 2;
        if ((h->page == TS_PAGE_ACCOUNT || job) && short_pair(c, h, r, half) && r + 1 < h->row_count && short_pair(c, h, r + 1, half) &&
            r + 1 != last_row) {
            for (int k = 0; k < 2; ++k) {
                int rr = r + k;
                float tx = x0 + k * (half + m->gap);
                if (rr == h->row_selected) chosen(c, tx, y, half, m->tile, 20);
                else glass(c, tx, y, half, m->tile, 20);
                text(c, h->row_label[rr], tx + 12, y + 7, half - 24, 16, 11, 600, t->dim, TS_TEXT_LEFT);
                text(c, h->row_value[rr], tx + 12, y + m->tile - 32, half - 24, 26, 17, 800, t->text, TS_TEXT_LEFT);
            }
            y += m->tile + m->gap;
            i += 2;
            continue;
        }
        if (sub >= 0) { /* a ride request: where to and the pay big, who under it; chosen, a pill to take it */
            char v1[48], v2[48];
            snprintf(v1, sizeof(v1), "%s", h->row_value[r]);
            v2[0] = 0;
            char *dt = strstr(v1, " · ");
            if (dt) {
                snprintf(v2, sizeof(v2), "%.*s", (int)(dt - v1), v1);
                memmove(v1, dt + strlen(" · "), strlen(dt + strlen(" · ")) + 1);
            }
            char sb[112];
            if (h->row_value[sub][0]) snprintf(sb, sizeof(sb), "%s · %s", h->row_label[sub], h->row_value[sub]);
            else snprintf(sb, sizeof(sb), "%s", h->row_label[sub]);
            float ch = m->two + (sel ? m->pill + 8 : 0);
            if (sel) chosen(c, x0, y, cw, ch, 24);
            else glass(c, x0, y, cw, ch, 24);
            float pw = measure(c, v1, 19, 800) + 4, kw = measure(c, v2, 11.5f, 600) + 4;
            if (pw > cw * 0.45f) pw = cw * 0.45f;
            if (kw > cw * 0.35f) kw = cw * 0.35f;
            float mid = m->two * 0.5f;
            text(c, h->row_label[r], x0 + 14, y + mid - 22, cw - 40 - pw, 22, 15, 800, t->text, TS_TEXT_LEFT);
            text(c, v1, x1 - 14 - pw, y + mid - 24, pw, 26, 19, 800, t->value, TS_TEXT_RIGHT);
            text(c, sb, x0 + 14, y + mid + 2, cw - 40 - kw, 18, 11.5f, 500, t->dim, TS_TEXT_LEFT);
            if (v2[0]) text(c, v2, x1 - 14 - kw, y + mid + 2, kw, 18, 11.5f, 700, t->text, TS_TEXT_RIGHT);
            if (sel) {
                float py = y + m->two;
                glossy(c, x0 + 10, py, cw - 20, m->pill, m->pill / 2);
                text(c, pick(h->german, "Fahrt annehmen", "Accept ride"), x0 + 26, py, cw * 0.6f, m->pill, 14, 800, t->sel_text, TS_TEXT_LEFT);
                char pr[56];
                snprintf(pr, sizeof(pr), "%s  \xE2\x86\x92", v1);
                text(c, pr, x0 + cw * 0.5f, py, cw * 0.5f - 26, m->pill, 13.5f, 800, t->sel_text, TS_TEXT_RIGHT);
            }
            y += ch + m->gap;
            i += 2;
            continue;
        }
        /* one entry: name left, value (or a switch) right */
        {
            float ch = m->one;
            int dis = h->row_heading[r] == 2;
            if (sel) chosen(c, x0, y, cw, ch, 20);
            else glass(c, x0, y, cw, ch, 20);
            float inner = cw - 28;
            int isw = is_switch(h->row_value[r], &sw_on);
            float vw = isw ? 46 : measure(c, h->row_value[r], 13.5f, 800) + 4;
            if (vw <= 4) vw = 0;
            if (vw > inner * 0.5f) vw = inner * 0.5f;
            float ty = r < 9 && (h->row_meter_on >> r & 1u) ? -3 : 0;
            text(c, h->row_label[r], x0 + 14, y + ty, inner - vw - 8, ch, 14, 700, dis ? t->dim : t->text, TS_TEXT_LEFT);
            if (isw) switch2(c, x1 - 14 - 42, y + (ch - 24) / 2, sw_on);
            else if (vw > 0) text(c, h->row_value[r], x1 - 14 - vw, y + ty, vw, ch, 13.5f, 800, sel ? t->text : dis ? t->dim : t->value, TS_TEXT_RIGHT);
            if (r < 9 && (h->row_meter_on >> r & 1u)) {
                float mv = h->row_meter[r], mw = cw - 28, my = y + ch - 9;
                uint32_t col = !(h->row_meter_quality >> r & 1u) ? t->accent : mv >= 0.7f ? 0x2FD27A : mv >= 0.4f ? 0xFFC83D : 0xFF4D4D;
                fill(c, x0 + 14, my, mw, 4, 2, mix(t->card_line, t->dim, 0.25f));
                if (mv > 0.0f) fill(c, x0 + 14, my, mw * mv < 4 ? 4 : mw * mv, 4, 2, col);
            }
            y += ch + m->gap;
            ++i;
        }
    }
    return y;
}

/* draws the page, roomy if it fits, else closer together; returns the y after it */
static float page_fit(Ctx *c, const TsHud *h, float x0, float y, float cw, float bottom) {
    Ctx dry = *c;
    dry.dry = 1;
    const Metrics *m = page2(&dry, h, x0, y, cw, &M_ROOMY) <= bottom ? &M_ROOMY : &M_DENSE;
    return page2(c, h, x0, y, cw, m);
}

void ts_device_draw(const TsHud *h, int device, int theme, float scale, const char *clock, TsPhoneCanvas *cv) {
    Ctx c = {.cv = cv, .s = scale, .t = &themes[theme >= 0 && theme < TS_THEMES ? theme : 0]};
    const Theme *t = c.t;
    ts_map_rect_px[0] = ts_map_rect_px[1] = ts_map_rect_px[2] = ts_map_rect_px[3] = 0;
    memset(ts_tab_rect_px, 0, sizeof(ts_tab_rect_px));
    if (device < 0 || device >= TS_DEVICES) device = TS_DEVICE_PHONE;
    const Body *b = body_of(device);
    float sx0, sy0, sx1, sy1;
    device_body(&c, device, &sx0, &sy0, &sx1, &sy1);
    float X = PH_M;

    if (device == TS_DEVICE_TABLET) {
        /* a camera in the bezel, the status bar over the whole width */
        fill(&c, X + b->w / 2 - 4, PH_M + b->bezel / 2 - 4, 8, 8, 4, 0x10141E);
        text(&c, clock, sx0 + 14, sy0 + 4, 70, 22, 14, 600, t->text, TS_TEXT_LEFT);
        float bx = sx1 - 44;
        box(&c, bx, sy0 + 9, 25, 12, 4, t->text, t->text, 1, 110);
        fill(&c, bx + 2, sy0 + 11, 17, 8, 2.5f, t->text);
        box(&c, bx + 26, sy0 + 13, 2.2f, 4.5f, 1, t->text, t->text, 1, 140);
        float x0 = sx0 + 18, x1 = sx1 - 18, y = sy0 + 34;
        /* 0.12: the head over the list's width, the tab bar on the right */
        float lw = 330, rx = x0 + lw + 16, rw = x1 - rx;
        head2(&c, h, x0, y, lw);
        tab_bar(&c, h, rx, y - 4, rw, 48);
        y += 56;
        /* left: the list, right: the details, the footer and the keys */
        page_fit(&c, h, x0, y, lw, sy1 - 16);
        float foot_h = h->footer[0] ? 56 : 0;
        if (h->map_on) map_box(&c, h, rx, y, rw, sy1 - 40 - foot_h - (foot_h > 0 ? 8 : 0) - y, 16); /* 0.7 */
        else detail_column(&c, h, rx, y, rw, sy1 - 40 - foot_h - (foot_h > 0 ? 8 : 0));
        if (h->footer[0]) {
            glass(&c, rx, sy1 - 40 - foot_h, rw, foot_h, 16);
            text_wrap(&c, h->footer, rx + 10, sy1 - 40 - foot_h + 7, rw - 20, foot_h - 12, 12.5f, t->dim);
        }
        text(&c, keys_text(h), rx, sy1 - 36, rw, 20, 12, 600, t->dim, TS_TEXT_CENTER);
        box(&c, X + b->w / 2 - 70, sy1 - 12, 140, 5, 2.5f, t->text, t->text, 1, 200);
        return;
    }

    if (device == TS_DEVICE_EDGE) {
        /* the punch-hole camera, the clock on the left as on Android */
        fill(&c, X + b->w / 2 - 7.5f, sy0 + 11, 15, 15, 7.5f, 0x000000);
        fill(&c, X + b->w / 2 - 3, sy0 + 15.5f, 6, 6, 3, 0x1A2238);
        text(&c, clock, sx0 + 22, sy0 + 6, 70, 24, 14.5f, 600, t->text, TS_TEXT_LEFT);
        float x = sx1 - 22 - 24;
        box(&c, x, sy0 + 12, 24, 11, 3, t->text, t->text, 1, 110);
        fill(&c, x + 2, sy0 + 14, 16, 7, 2, t->text);
        x -= 26;
        for (int i = 0; i < 4; ++i) fill(&c, x + i * 5.0f, sy0 + 22 - (i + 1) * 2.6f, 3.4f, (i + 1) * 2.6f + 0.4f, 1, t->text);
    } else {
        /* camera island and status bar */
        fill(&c, X + b->w / 2 - 52, sy0 + 10, 104, 30, 15, 0x000000);
        fill(&c, X + b->w / 2 + 28, sy0 + 19, 12, 12, 6, 0x10141E);
        fill(&c, X + b->w / 2 + 31, sy0 + 22, 4, 4, 2, 0x26324A);
        status_bar(&c, sx0, sx1, sy0 + 7, clock, t->text);
    }

    float pad = device == TS_DEVICE_EDGE ? 14 : 16;
    float x0 = sx0 + pad, x1 = sx1 - pad, cw = x1 - x0;
    float y = sy0 + (device == TS_DEVICE_EDGE ? 44 : 52);
    y += head2(&c, h, x0, y, cw) + 4;
    float bottom = sy1 - (device == TS_DEVICE_EDGE ? 22 : 0);
    /* 0.12: the floating tab bar at the bottom, the keys over it, the footer over them if there is room */
    float bar_h = 54, bar_y = bottom - 20 - bar_h, keys_y = bar_y - 19, list_end = keys_y - 4;
    if (h->map_on) { /* 0.7: the map takes what the rows leave */
        Ctx dry = c;
        dry.dry = 1;
        float rows_h = page2(&dry, h, x0, 0, cw, &M_DENSE);
        float mh = list_end - y - rows_h - 6;
        if (mh > 40) {
            map_box(&c, h, x0, y, cw, mh, 22);
            y += mh + 8;
        }
        y = page2(&c, h, x0, y, cw, &M_DENSE);
    } else y = page_fit(&c, h, x0, y, cw, list_end - (h->footer[0] ? 50 : 0));
    if (h->footer[0] && y + 46 <= list_end) {
        float fy = list_end - 46;
        glass(&c, x0, fy, cw, 44, 18);
        text_wrap(&c, h->footer, x0 + 12, fy + 4, cw - 24, 36, 11.5f, t->dim);
    }
    text(&c, keys_text(h), x0, keys_y, cw, 16, 11, 600, t->dim, TS_TEXT_CENTER);
    tab_bar(&c, h, x0 - 4, bar_y, cw + 8, bar_h);
    if (device == TS_DEVICE_EDGE) {
        float ny = sy1 - 26, mid = X + b->w / 2;
        /* recent apps (three bars), home (a ring), back (a chevron) */
        for (int i = 0; i < 3; ++i) fill(&c, mid - 82 + i * 6.0f, ny + 4, 2.6f, 14, 1.3f, t->dim);
        ring(&c, mid, ny + 11, 8, 2.2f, t->dim, t->bottom);
        text(&c, "<", mid + 62, ny - 2, 26, 26, 19, 600, t->dim, TS_TEXT_CENTER);
    } else {
        box(&c, X + b->w / 2 - 60, bottom - 18, 120, 5, 2.5f, t->text, t->text, 1, 200);
    }
}

void ts_phone_draw(const TsHud *h, int theme, float scale, const char *clock, TsPhoneCanvas *cv) {
    ts_device_draw(h, TS_DEVICE_PHONE, theme, scale, clock, cv);
}

void ts_device_finish(int device, int theme, float scale, TsPhoneCanvas *cv) {
    (void)theme;
    const Body *b = body_of(device);
    float s = scale;
    float x = PH_M * s, y = PH_M * s, w = b->w * s, h = b->h * s, r = b->r * s;
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
                uint32_t rr = (rgb >> 16) & 255, g = (rgb >> 8) & 255, bb = rgb & 255, m = (uint32_t)ai;
                rgb = (rr > m ? m : rr) << 16 | (g > m ? m : g) << 8 | (bb > m ? m : bb);
            }
            *d = (uint32_t)ai << 24 | rgb;
        }
    }
}

void ts_phone_finish(int theme, float scale, TsPhoneCanvas *cv) { ts_device_finish(TS_DEVICE_PHONE, theme, scale, cv); }

const char *ts_device_name(int device, int german) {
    switch (device) {
    case TS_DEVICE_EDGE: return "Edge";
    case TS_DEVICE_TABLET: return "Tablet";
    default: return german ? "Handy" : "Phone";
    }
}

/* ------------------------------------------------------------------ the speed-limit sign */

#define SG_D 86.0f /* the disc, in units */
#define SG_M 7.0f  /* shadow margin */

void ts_sign_size(float scale, int *w, int *h) {
    *w = *h = (int)ceilf((SG_D + 2 * SG_M) * scale);
}

void ts_sign_draw(int number, const char *unit, int flash, float scale, TsPhoneCanvas *cv) {
    Ctx c = {.cv = cv, .s = scale, .t = &themes[0]};
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

#define DS_W 340.0f
#define DS_H 150.0f
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

/* 0.5.1: the ride panel and the feed follow the phone's design, as frosted glass: the theme's screen colours a
   little see-through, a light rim along the top edge, a soft sheen; text and meters in the theme's colours */
typedef struct {
    int light;                         /* a light design: dark text */
    uint32_t top, bottom, rim, sheen;  /* the glass */
    uint32_t text, dim, inner, grid, accent, good, mid, bad, dot;
    float body;                        /* how much of the glass covers the game */
} Glass;

static float lum(uint32_t c) { return (0.299f * (c >> 16 & 255) + 0.587f * (c >> 8 & 255) + 0.114f * (c & 255)) / 255.0f; }

static Glass glass_of(int theme) {
    const Theme *t = &themes[theme >= 0 && theme < TS_THEMES ? theme : 0];
    Glass g;
    g.light = lum(t->top) > 0.6f;
    if (theme == TS_THEME_FESTIVAL) g.light = 0; /* white text on the sunset */
    g.top = t->top, g.bottom = t->bottom;
    if (theme == TS_THEME_FESTIVAL) g.top = 0x4A2386, g.bottom = 0xB0386E; /* the sunset, deep enough for white text */
    g.rim = mix(g.top, 0xFFFFFF, g.light ? 0.9f : 0.32f);
    g.sheen = mix(g.top, 0xFFFFFF, g.light ? 0.6f : 0.10f);
    g.text = g.light ? t->text : 0xFFFFFF;
    g.dim = g.light ? t->dim : mix(t->dim, 0xFFFFFF, 0.15f);
    g.inner = mix(g.bottom, g.light ? 0xFFFFFF : 0x000000, g.light ? 0.55f : 0.35f);
    g.grid = mix(g.inner, g.text, 0.18f);
    g.accent = t->accent;
    if (!g.light && lum(g.accent) < 0.25f) g.accent = 0xFFFFFF;
    if (theme == TS_THEME_FESTIVAL) g.accent = 0xFFD21F;
    g.good = g.light ? 0x0A9F55 : 0x2FD27A;
    g.mid = g.light ? 0xB7791F : 0xFFC531;
    g.bad = g.light ? 0xD7261E : 0xFF5A4A;
    g.dot = g.light ? 0x111111 : 0xFFFFFF;
    g.body = g.light ? 0.95f : 0.93f; /* Marvin: "still see-through": milky, the game only a shimmer behind */
    return g;
}

static int dash_theme; /* the design the last ts_dash_draw used (for ts_dash_finish) */
static int feed_theme;

/* frosted glass has a grain: a fine one per pixel and a soft, large one (as if light behind it were blurred);
   both from the pixel's place only, so the same picture every time */
static float hash2(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFF) / 32767.5f - 1.0f;
}
static float soft_noise(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx), fy = fy * fy * (3 - 2 * fy);
    float a = hash2(ix, iy), b = hash2(ix + 1, iy), c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
/* adds the grain to an unpremultiplied colour where the glass covers (cov 0..1) */
static uint32_t frost(uint32_t rgb, int px, int py, float scale, float cov, int light) {
    if (cov <= 0.0f) return rgb;
    float n = hash2(px, py) * (light ? 6.0f : 8.0f) + soft_noise(px / (46.0f * scale), py / (46.0f * scale)) * (light ? 8.0f : 12.0f);
    n *= cov;
    int r = (int)((rgb >> 16) & 255) + (int)n, g = (int)((rgb >> 8) & 255) + (int)n, b = (int)(rgb & 255) + (int)n;
    r = r < 0 ? 0 : r > 255 ? 255 : r, g = g < 0 ? 0 : g > 255 ? 255 : g, b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

/* the glass panel: a rim, the body as a gradient, a sheen on the upper part */
static void glass_panel(Ctx *c, const Glass *g, float x, float y, float w, float h, float r) {
    box(c, x, y, w, h, r, g->rim, mix(g->rim, g->bottom, 0.85f), 1, 255);
    box(c, x + 1.2f, y + 1.2f, w - 2.4f, h - 2.4f, r - 1.2f, g->top, g->bottom, 1, 255);
    box(c, x + 1.2f, y + 1.2f, w - 2.4f, h * 0.42f, r - 1.2f, g->sheen, g->top, 1, 120);
}

/* the speed sign, d wide at sx, sy (a red ring, the limit; the end-of-restrictions sign for "floor it") */
static void dash_sign(Ctx *c, const TsHud *h, float sx, float sy, float d) {
    if (h->sign_free) { /* white, five black diagonal stripes */
        fill(c, sx, sy, d, d, d / 2, 0x111111);
        fill(c, sx + 2, sy + 2, d - 4, d - 4, d / 2 - 2, 0xFFFFFF);
        float r = d / 2 - 6, cx = sx + d / 2, cy = sy + d / 2, step = d / 13.0f;
        for (int k = -2; k <= 2; ++k) {
            float off = (float)k * step;
            for (float u = -r; u <= r; u += 0.8f) {
                float px = cx + u * 0.7071f + off * 0.7071f, py = cy - u * 0.7071f + off * 0.7071f;
                float dx = px - cx, dy = py - cy;
                if (dx * dx + dy * dy <= r * r) fill(c, px - 1.1f, py - 1.1f, 2.2f, 2.2f, 1.1f, 0x111111);
            }
        }
        return;
    }
    float ring_w = d * 0.11f;
    fill(c, sx, sy, d, d, d / 2, 0xD71920);
    fill(c, sx + ring_w, sy + ring_w, d - 2 * ring_w, d - 2 * ring_w, d / 2 - ring_w, h->sign_over == 2 ? 0xFF3B3B : h->sign_over ? 0xFFD21F : 0xFFFFFF);
    char n[16];
    snprintf(n, sizeof(n), "%d", h->sign_limit);
    float size = d * (h->sign_limit >= 100 ? 0.31f : 0.37f);
    int has_unit = h->sign_unit[0] != 0;
    uint32_t ink = h->sign_over == 2 ? 0xFFFFFF : 0x111111;
    text(c, n, sx, sy + d * (has_unit ? 0.19f : 0.26f), d, d * 0.44f, size, 700, ink, TS_TEXT_CENTER);
    if (has_unit) text(c, h->sign_unit, sx, sy + d * 0.62f, d, d * 0.17f, d * 0.12f, 700, h->sign_over == 2 ? 0xFFFFFF : 0x333333, TS_TEXT_CENTER);
}

/* the G-meter as a small glass bubble at cx, cy */
static void dash_g(Ctx *c, const Glass *g, const TsHud *h, float cx, float cy, float r) {
    uint32_t edge = h->g_over ? g->bad : g->good;
    fill(c, cx - r, cy - r, 2 * r, 2 * r, r, mix(g->inner, g->rim, 0.3f));
    box(c, cx - r + 1.2f, cy - r + 1.2f, 2 * r - 2.4f, 2 * r - 2.4f, r - 1.2f, mix(g->inner, g->sheen, 0.3f), g->inner, 1, 255);
    float inner = r / 1.45f;
    fill(c, cx - r + 5, cy - 0.6f, 2 * r - 10, 1.2f, 0.6f, g->grid);
    fill(c, cx - 0.6f, cy - r + 5, 1.2f, 2 * r - 10, 0.6f, g->grid);
    for (int k = 0; k < 360; k += 3) {
        float an = (float)k * 3.14159265f / 180.0f, px = cx + cosf(an) * inner, py = cy + sinf(an) * inner;
        fill(c, px - 1.2f, py - 1.2f, 2.4f, 2.4f, 1.2f, edge);
    }
    if (h->g_on) {
        float gx = h->g_lat, gy = h->g_long, len = sqrtf(gx * gx + gy * gy), maxlen = 1.38f;
        if (len > maxlen) gx *= maxlen / len, gy *= maxlen / len;
        float px = cx + gx * inner, py = cy + gy * inner;
        uint32_t dot = h->g_over ? g->bad : g->dot;
        fill(c, px - 5.5f, py - 5.5f, 11, 11, 5.5f, mix(dot, g->inner, 0.6f));
        fill(c, px - 4, py - 4, 8, 8, 4, dot);
    }
}

/* 0.6: a five-pointed star (points up), anti-aliased by 4x4 samples */
static int in_poly(const float *vx, const float *vy, int n, float px, float py) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((vy[i] > py) != (vy[j] > py) && px < (vx[j] - vx[i]) * (py - vy[i]) / (vy[j] - vy[i]) + vx[i]) in = !in;
    return in;
}
static void star(Ctx *c, float ucx, float ucy, float ur, uint32_t col) {
    float s = c->s, cx = ucx * s, cy = ucy * s, r = ur * s, vx[10], vy[10];
    for (int k = 0; k < 10; ++k) {
        float an = -1.5707963f + (float)k * 0.6283185f, rr = k % 2 ? r * 0.48f : r;
        vx[k] = cx + cosf(an) * rr, vy[k] = cy + sinf(an) * rr;
    }
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->cv->w) x1 = c->cv->w;
    if (y1 > c->cv->h) y1 = c->cv->h;
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            int hits = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) hits += in_poly(vx, vy, 10, px + (sx + 0.5f) / 4.0f, py + (sy + 0.5f) / 4.0f);
            if (!hits) continue;
            uint32_t *d = &c->cv->px[py * c->cv->w + px];
            *d = mix(*d & 0xFFFFFF, col, hits / 16.0f);
        }
}

static const uint32_t tier_col[TS_TIERS] = {0xC8813F, 0xC3CAD3, 0xF2C230, 0x9FE3EE};

/* a tier's medal: a coin with a rim and a star */
static void medal(Ctx *c, float cx, float cy, float r, int tier) {
    uint32_t base = tier_col[tier >= 0 && tier < TS_TIERS ? tier : 0];
    fill(c, cx - r, cy - r, 2 * r, 2 * r, r, mix(base, 0x000000, 0.35f));
    box(c, cx - r + r * 0.09f, cy - r + r * 0.09f, 2 * r * 0.91f, 2 * r * 0.91f, r * 0.91f, mix(base, 0xFFFFFF, 0.55f), mix(base, 0x000000, 0.12f), 1, 255);
    box(c, cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f, r * 0.72f, mix(base, 0x000000, 0.08f), mix(base, 0xFFFFFF, 0.35f), 1, 255);
    star(c, cx, cy + r * 0.04f, r * 0.5f, mix(base, 0xFFFFFF, 0.75f));
}

static float ease_out(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

/* 0.6: the card after a ride, a line, a shift; a new tier. h->card_anim: seconds since it came (the stars light up,
   the money counts up, the rest fades in) */
static void dash_card(Ctx *c, const Glass *g, const TsHud *h) {
    const TsCard *k = &h->card;
    float x = DS_M, y = DS_M, pad = 16, a = h->card_anim;
    uint32_t gold = g->light ? 0xE0A800 : 0xFFC531;
    float fade_in = ease_out((a - 0.8f) / 0.5f); /* the comment and the numbers along the bottom */
    if (k->kind == TS_CARD_TIER) {
        float pop = a < 0.45f ? 0.6f + 0.55f * ease_out(a / 0.45f) : a < 0.65f ? 1.15f - 0.15f * (a - 0.45f) / 0.2f : 1.0f;
        float r = 34 * pop, mcx = x + pad + 36, mcy = y + DS_H / 2;
        if (a > 0.3f) { /* a glow round the medal */
            uint32_t glow = mix(g->top, tier_col[k->tier >= 0 && k->tier < TS_TIERS ? k->tier : 0], 0.35f * ease_out((a - 0.3f) / 0.6f));
            fill(c, mcx - 46, mcy - 46, 92, 92, 46, glow);
        }
        medal(c, mcx, mcy, r, k->tier);
        float tx = x + pad + 92, tw = x + DS_W - pad - tx;
        text(c, k->title, tx, y + 22, tw, 14, 10.5f, 700, g->accent, TS_TEXT_LEFT);
        text(c, k->name, tx, y + 38, tw, 28, 21, 700, g->text, TS_TEXT_LEFT);
        char parts[2][TS_TEXT];
        int n = ts_feed_split(k->note, 30, parts, 2);
        uint32_t nc = mix(g->top, g->dim, fade_in);
        for (int i = 0; i < n; ++i) text(c, parts[i], tx, y + 74 + i * 18, tw, 18, 12, 600, nc, TS_TEXT_LEFT);
        return;
    }
    text(c, k->title, x + pad, y + 12, 200, 14, 10.5f, 700, g->accent, TS_TEXT_LEFT);
    text(c, k->name, x + pad, y + 27, 206, 24, 16.5f, 700, g->text, TS_TEXT_LEFT);
    if (k->money >= 0) { /* counts up */
        int shown = (int)((float)k->money * ease_out((a - 0.15f) / 1.0f) + 0.5f);
        if (shown != k->money) shown = shown / 5 * 5;
        char m[24];
        snprintf(m, sizeof(m), "%s%s", k->kind == TS_CARD_RIDE ? "+" : "", ts_cr(h->german, shown));
        text(c, m, x + DS_W - pad - 120, y + 10, 120, 34, 25, 700, g->good, TS_TEXT_RIGHT);
    }
    if (k->kind == TS_CARD_SHIFT && k->tier >= 0) medal(c, x + DS_W - pad - 12, y + 63, 12, k->tier);
    if (k->stars >= 0) {
        for (int i = 0; i < 5; ++i) {
            float t0 = 0.35f + 0.16f * (float)i, cx = x + pad + 10 + 25 * (float)i, cy = y + 63;
            int lit = i < k->stars && a >= t0;
            float sz = 10.5f;
            if (lit && a < t0 + 0.2f) sz *= 1.0f + 0.35f * (1.0f - (a - t0) / 0.2f); /* each pops as it lights */
            star(c, cx, cy, sz, lit ? gold : g->grid);
        }
    }
    if (k->note[0]) text(c, k->note, x + pad, y + 78, DS_W - 2 * pad, 18, 12.5f, 600, mix(g->top, g->dim, fade_in), TS_TEXT_LEFT);
    /* along the bottom: fare, tip, streak, on time */
    if (k->pairs > 0) {
        float by = y + DS_H - 44, bw = DS_W - 16, cw = bw / (float)k->pairs;
        fill(c, x + 8, by, bw, 36, 14, g->inner);
        for (int i = 0; i < k->pairs; ++i) {
            float cx = x + 8 + cw * (float)i;
            if (i) fill(c, cx, by + 8, 1.0f, 20, 0.5f, g->grid);
            text(c, k->label[i], cx + 4, by + 4, cw - 8, 13, 9.5f, 700, mix(g->inner, g->dim, fade_in), TS_TEXT_CENTER);
            uint32_t vc = k->value[i][0] == '+' ? g->good : k->value[i][0] == '-' ? g->bad : g->text;
            if (!strcmp(k->value[i], "zu spät") || !strcmp(k->value[i], "late")) vc = g->bad;
            text(c, k->value[i], cx + 4, by + 16, cw - 8, 18, 13, 700, mix(g->inner, vc, fade_in), TS_TEXT_CENTER);
        }
    }
}

/* 0.5.1 (Marvin chose "a ride card like a ride app"): the time left big, where to, a route from a dot to a square
   that fills as you go, the sign and the G-meter on the right, the wish and the taxi meter along the bottom */
void ts_dash_draw(const TsHud *h, int theme, float scale, TsPhoneCanvas *cv) {
    Ctx c = {.cv = cv, .s = scale, .t = &themes[theme >= 0 && theme < TS_THEMES ? theme : 0]};
    Glass g = glass_of(theme);
    dash_theme = theme;
    float x = DS_M, y = DS_M, pad = 16;
    glass_panel(&c, &g, x, y, DS_W, DS_H, DS_R);
    if (h->card.kind) { /* 0.6: a card after the ride instead of the ride */
        dash_card(&c, &g, h);
        return;
    }
    int de = h->german;

    /* the right column: the sign (or, before the passenger is in, the distance) over the G-meter */
    float d = 54, sx = x + DS_W - pad - d, sy = y + 10;
    if (h->sign_limit > 0) dash_sign(&c, h, sx, sy, d);
    else if (h->chip1[0]) {
        fill(&c, sx - 4, sy + 6, d + 8, d - 12, (d - 12) / 2, g.inner);
        text(&c, h->chip1, sx - 4, sy + 16, d + 8, 24, 16, 700, g.text, TS_TEXT_CENTER);
    }
    float gr = 20, gcx = sx + d / 2, gcy = sy + d + 6 + gr;
    dash_g(&c, &g, h, gcx, gcy, gr);
    float col_r = sx - 12; /* where the left part ends */

    /* the time left, big, and the mood */
    char a[32] = "", b[32] = "";
    const char *sep = strstr(h->corner, " · ");
    if (sep) {
        snprintf(a, sizeof(a), "%.*s", (int)(sep - h->corner), h->corner);
        snprintf(b, sizeof(b), "%s", sep + strlen(" · "));
    } else {
        snprintf(b, sizeof(b), "%s", h->corner);
    }
    float lx = x + pad;
    if (a[0]) {
        float aw = measure(&c, a, 30, 700) + 4;
        text(&c, a, lx, y + 10, aw, 38, 30, 700, a[0] == '+' ? g.bad : g.text, TS_TEXT_LEFT);
        text(&c, a[0] == '+' ? (de ? "ZU SPÄT" : "LATE") : (de ? "NOCH" : "LEFT"), lx, y + 46, aw + 20, 14, 10, 700, g.dim, TS_TEXT_LEFT);
        lx += aw + 12;
        fill(&c, lx - 7, y + 16, 1.2f, 40, 0.6f, g.grid); /* a hairline between time and place */
    }
    const char *label = h->trip_label[0] ? h->trip_label : h->chip0;
    text(&c, label, lx, y + 14, col_r - lx, 14, 10.5f, 700, g.accent, TS_TEXT_LEFT);
    {   /* 0.6: the ride's class and a hotspot's surcharge as small pills after the label */
        float px = lx + measure(&c, label, 10.5f, 700) + 6;
        char pill[3][16];
        int pills = 0;
        if (h->trip_pool) snprintf(pill[pills++], sizeof(pill[0]), "%s", h->german ? "Sammel" : "Shared");
        else if (h->trip_class >= 0 && h->trip_class != TS_CLASS_COMFORT)
            snprintf(pill[pills++], sizeof(pill[0]), "%s", ts_class_name(h->trip_class, h->german));
        for (int i = 0; i < pills; ++i) {
            float pw = measure(&c, pill[i], 9, 700) + 10;
            if (px + pw > col_r) break;
            int black = !strcmp(pill[i], "Black");
            uint32_t bg = black ? (g.light ? 0x111111 : 0xF2F2F2) : g.accent;
            uint32_t fg = black ? (g.light ? 0xFFFFFF : 0x111111) : (lum(bg) > 0.55f ? 0x111111 : 0xFFFFFF);
            fill(&c, px, y + 13, pw, 16, 8, bg);
            text(&c, pill[i], px, y + 13, pw, 16, 9, 700, fg, TS_TEXT_CENTER);
            px += pw + 4;
        }
    }
    if (h->trip_to[0]) { /* "San Giovanni · Spot 4": the town big, the spot under it */
        char town[64], spot[64] = "";
        snprintf(town, sizeof(town), "%s", h->trip_to);
        char *cut = strstr(town, " · ");
        if (cut) snprintf(spot, sizeof(spot), "%s", cut + strlen(" · ")), *cut = 0;
        text(&c, town, lx, y + 28, col_r - lx, 24, 17, 700, g.text, TS_TEXT_LEFT);
        if (spot[0]) text(&c, spot, lx, y + 50, col_r - lx, 16, 11.5f, 600, g.dim, TS_TEXT_LEFT);
    }

    /* the route: a dot where it started, a square where it ends, the car on the line */
    float rx0 = x + pad + 5, rx1 = col_r - 8, ry = y + 78, p = h->trip_on ? clampf(h->trip_progress, 0, 1) : 0;
    fill(&c, rx0, ry - 2, rx1 - rx0, 4, 2, g.grid);
    float cxp = rx0 + (rx1 - rx0) * p;
    if (cxp > rx0) fill(&c, rx0, ry - 2, cxp - rx0, 4, 2, g.accent);
    fill(&c, rx0 - 5, ry - 5, 10, 10, 5, g.accent);
    fill(&c, rx0 - 2.2f, ry - 2.2f, 4.4f, 4.4f, 2.2f, g.inner);
    fill(&c, rx1 - 5, ry - 5, 10, 10, 1.5f, g.text);
    fill(&c, rx1 - 2, ry - 2, 4, 4, 0.5f, g.inner);
    if (h->trip_on) {
        fill(&c, cxp - 8, ry - 8, 16, 16, 8, mix(g.accent, g.inner, 0.55f));
        fill(&c, cxp - 6, ry - 6, 12, 12, 6, g.accent);
        fill(&c, cxp - 2.5f, ry - 2.5f, 5, 5, 2.5f, g.light ? 0xFFFFFF : g.inner);
    }

    /* under the route: the passenger's mood */
    if (b[0]) {
        int mood = atoi(b);
        uint32_t mc = mood >= 75 ? g.good : mood >= 50 ? g.mid : g.bad;
        const char *who = de ? "Fahrgast" : "Passenger";
        float ww = measure(&c, who, 11, 600) + 4;
        fill(&c, rx0 - 4, ry + 17, 8, 8, 4, mc);
        text(&c, who, rx0 + 9, ry + 13, ww, 16, 11, 600, g.dim, TS_TEXT_LEFT);
        text(&c, b, rx0 + 9 + ww, ry + 12, col_r - rx0 - 9 - ww, 18, 12.5f, 700, mc, TS_TEXT_LEFT);
    }

    /* along the bottom: the wish, the timetable or the reply prompt on the left, the taxi meter on the right */
    float by = y + DS_H - 32, bw = DS_W - 16;
    fill(&c, x + 8, by, bw, 24, 12, g.inner);
    float mw = 0;
    if (h->meter >= 0) {
        char m[32];
        snprintf(m, sizeof(m), "%s", ts_cr(h->german, h->meter));
        float lw = measure(&c, h->meter_label, 9.5f, 700) + 6, vw = measure(&c, m, 14, 700) + 4;
        mw = lw + vw;
        text(&c, h->meter_label, x + DS_W - 20 - mw, by + 5, lw, 14, 9.5f, 700, g.dim, TS_TEXT_LEFT);
        text(&c, m, x + DS_W - 20 - vw, by + 2, vw, 20, 14, 700, g.good, TS_TEXT_RIGHT);
        mw += 10;
        if (h->trip_surge > 1.01f) { /* 0.6: a hotspot's surcharge, as a pill before the meter */
            char sx[16];
            snprintf(sx, sizeof(sx), "%.1fx", (double)h->trip_surge);
            if (h->german) for (char *p = sx; *p; ++p) if (*p == '.') *p = ',';
            float pw = measure(&c, sx, 10, 700) + 12;
            fill(&c, x + DS_W - 20 - mw - pw + 4, by + 4, pw, 16, 8, g.mid);
            text(&c, sx, x + DS_W - 20 - mw - pw + 4, by + 4, pw, 16, 10, 700, 0x111111, TS_TEXT_CENTER);
            mw += pw + 4;
        }
    }
    if (h->chip3[0]) text(&c, h->chip3, x + 20, by + 3, DS_W - 40 - mw, 18, 12.5f, 700, g.accent, TS_TEXT_LEFT);
}

void ts_dash_finish(float scale, TsPhoneCanvas *cv) {
    float s = scale, x = DS_M * s, y = DS_M * s, w = DS_W * s, hh = DS_H * s, r = DS_R * s, spread = DS_M * s;
    const float body = glass_of(dash_theme).body; /* frosted glass: the game only shimmers through */
    const int light = glass_of(dash_theme).light;
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
            uint32_t rgb = frost(*dp & 0xFFFFFF, px, py, s, cov, light);
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

void ts_feed_draw(const TsFeedLine *lines, int n, int theme, float scale, TsPhoneCanvas *cv) {
    Ctx c = {.cv = cv, .s = scale, .t = &themes[theme >= 0 && theme < TS_THEMES ? theme : 0]};
    Glass gl = glass_of(theme);
    feed_theme = theme;
    /* the name's colour: who (0 a passenger, 1 happy, 2 cross, 3 the bus's announcement); 0 = the design's colour */
    uint32_t name_col[4] = {gl.accent, gl.good, gl.light ? 0xC2410C : 0xFF9A3C, gl.light ? 0xA16207 : 0xFFD21F};
    if (theme == TS_THEME_NIGHT) name_col[0] = 0x8FD3FF; /* as before 0.5.1 */
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
        glass_panel(&c, &gl, 2, y, bw, bh, 12);
        feed_boxes[feed_box_count].x = 2 * scale, feed_boxes[feed_box_count].y = y * scale;
        feed_boxes[feed_box_count].w = bw * scale, feed_boxes[feed_box_count].h = bh * scale;
        feed_box_count++;
        int st = lines[i].style >= 0 && lines[i].style < 4 ? lines[i].style : 0;
        text(&c, lines[i].name, 14, y + 4, nw - 6, FD_ROW, 15, 700, name_col[st], TS_TEXT_LEFT);
        for (int p = 0; p < k; ++p) text(&c, parts[p], 14 + nw, y + 4 + p * FD_ROW, bw - nw - 26, FD_ROW, 15, 600, gl.text, TS_TEXT_LEFT);
        y -= FD_GAP;
    }
}

void ts_feed_finish(float scale, TsPhoneCanvas *cv) {
    Glass gl = glass_of(feed_theme);
    const float body = gl.body;
    for (int py = 0; py < cv->h; ++py)
        for (int px = 0; px < cv->w; ++px) {
            uint32_t *dp = &cv->px[py * cv->w + px];
            float cov = 0.0f;
            for (int b = 0; b < feed_box_count; ++b) {
                float r = 12 * scale;
                float v = rbox_cover(px + 0.5f, py + 0.5f, feed_boxes[b].x, feed_boxes[b].y, feed_boxes[b].w, feed_boxes[b].h, r);
                if (v > cov) cov = v;
            }
            uint32_t rgb = frost(*dp & 0xFFFFFF, px, py, scale, cov, gl.light);
            /* the text keeps full strength, the box behind it lets the game through a little */
            uint32_t rr = (rgb >> 16) & 255, g = (rgb >> 8) & 255, bb = rgb & 255;
            uint32_t lum = rr > g ? (rr > bb ? rr : bb) : (g > bb ? g : bb);
            float a = gl.light ? cov * body : cov * (body + (1.0f - body) * (lum / 255.0f)); /* light glass: dark text, even alpha */
            int ai = (int)(a * 255.0f + 0.5f);
            if (ai <= 0) {
                *dp = 0;
                continue;
            }
            if (gl.light) rr = (uint32_t)(rr * a + 0.5f), g = (uint32_t)(g * a + 0.5f), bb = (uint32_t)(bb * a + 0.5f);
            uint32_t m = (uint32_t)ai;
            rgb = (rr > m ? m : rr) << 16 | (g > m ? m : g) << 8 | (bb > m ? m : bb);
            *dp = m << 24 | rgb;
        }
}
