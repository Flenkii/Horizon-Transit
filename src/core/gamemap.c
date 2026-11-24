/* 0.9: the phone's map from the game's own files, see gamemap.h. */
#include "gamemap.h"
#include "bc7.h"
#include "inflate.h"
#include "map.h"
#include "stops.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* the picture: 3584 x 2048 pixels, the world from x -7500 (21000 m) and z +1000 down to -11000 */
#define SRC_W 3584
#define SRC_H 2048
#define SRC_HEAD 83
#define SRC_X0 (-7500.0f)
#define SRC_Z0 (1000.0f)
#define SRC_MPP (12000.0f / 2048.0f)
#define MAX_SEG 40000
static const float RECT[4] = {SRC_X0, SRC_X0 + SRC_W * SRC_MPP, SRC_Z0, SRC_Z0 - SRC_H * SRC_MPP}; /* x0, x1, z0, z1 */

/* the colours: a dark phone map (0xRRGGBB) */
#define C_LAND 0x242F3Eu
#define C_FIELD 0x2E3A4Bu
#define C_FOREST 0x223A3Du
#define C_TOWN 0x343E4Fu
#define C_WATER 0x17263Cu
static const uint32_t ROAD_COL[4] = {0x2F3847u, 0x464F5Eu, 0x5A6475u, 0x8A7A5Cu}; /* trail, dirt, street, motorway */
static const float ROAD_HW[4] = {1.6f, 2.6f, 4.0f, 6.0f};                         /* half widths in metres */

uint32_t ts_gamemap_tile_index(uint32_t x, uint32_t y, uint32_t blocks_w) {
#define B(v, k) (((v) >> (k)) & 1u)
    uint32_t i = B(x, 0) | B(y, 0) << 1 | B(x, 1) << 2 | B(y, 1) << 3;
    i |= (B(x, 3) ^ B(y, 3) ^ B(x, 4)) << 4 | (B(x, 4) ^ B(y, 4)) << 5 | (B(x, 5) ^ B(y, 5)) << 6; /* pipe */
    i |= (B(x, 6) ^ B(y, 4) ^ B(y, 5)) << 7 | (B(x, 7) ^ B(y, 3)) << 8;                           /* bank */
    i |= B(x, 2) << 9 | B(y, 2) << 10;
#undef B
    return i | ((y >> 5) * (blocks_w >> 6) + (x >> 6)) << 11; /* the 64 x 32 block macro tiles, row by row */
}

/* ------------------------------------------------------------------ the roads */

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static float rdf(const uint8_t *p) {
    uint32_t v = rd32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

/* motorways: the nav has the streets' lanes; a motorway is a street with a twin beside it, running the same way
   (the other carriageway), for most of its length */
static void find_motorways(const float *seg, uint8_t *kind, const int *road_of, int n, int roads) {
    float *len = (float *)calloc((size_t)roads, sizeof(float)), *twin = (float *)calloc((size_t)roads, sizeof(float));
    /* a grid of the street segments' middles, 40 m cells */
    const float cell = 40.0f, x0 = -8000.0f, z0 = -12000.0f;
    const int gw = 560, gh = 360;
    int *head = (int *)malloc(sizeof(int) * gw * gh), *next = (int *)malloc(sizeof(int) * (size_t)n);
    if (!len || !twin || !head || !next) goto done;
    for (int i = 0; i < gw * gh; ++i) head[i] = -1;
    for (int i = 0; i < n; ++i) {
        next[i] = -1;
        if (kind[i] != TS_ROAD_STREET) continue;
        int cx = (int)((0.5f * (seg[4 * i] + seg[4 * i + 2]) - x0) / cell), cz = (int)((0.5f * (seg[4 * i + 1] + seg[4 * i + 3]) - z0) / cell);
        if (cx < 0 || cz < 0 || cx >= gw || cz >= gh) continue;
        next[i] = head[cz * gw + cx], head[cz * gw + cx] = i;
    }
    for (int i = 0; i < n; ++i) {
        if (kind[i] != TS_ROAD_STREET) continue;
        float mx = 0.5f * (seg[4 * i] + seg[4 * i + 2]), mz = 0.5f * (seg[4 * i + 1] + seg[4 * i + 3]);
        float dx = seg[4 * i + 2] - seg[4 * i], dz = seg[4 * i + 3] - seg[4 * i + 1], l = sqrtf(dx * dx + dz * dz);
        if (l < 0.5f) continue;
        len[road_of[i]] += l;
        int cx = (int)((mx - x0) / cell), cz = (int)((mz - z0) / cell), hit = 0;
        for (int yy = cz - 1; yy <= cz + 1 && !hit; ++yy)
            for (int xx = cx - 1; xx <= cx + 1 && !hit; ++xx) {
                if (xx < 0 || yy < 0 || xx >= gw || yy >= gh) continue;
                for (int j = head[yy * gw + xx]; j >= 0 && !hit; j = next[j]) {
                    if (road_of[j] == road_of[i]) continue;
                    float ex = seg[4 * j + 2] - seg[4 * j], ez = seg[4 * j + 3] - seg[4 * j + 1], el = sqrtf(ex * ex + ez * ez);
                    if (el < 0.5f) continue;
                    float c = (dx * ex + dz * ez) / (l * el);
                    if (fabsf(c) < 0.96f) continue;
                    /* the distance across: from this middle to the other line */
                    float px = mx - seg[4 * j], pz = mz - seg[4 * j + 1], t = (px * ex + pz * ez) / (el * el);
                    if (t < -0.5f || t > 1.5f) continue;
                    float across = fabsf(px * ez - pz * ex) / el;
                    if (across > 5.0f && across < 28.0f) hit = 1;
                }
            }
        if (hit) twin[road_of[i]] += l;
    }
    /* a town's boulevards (Nice's promenade) have twins too: no motorway within 900 m of a town's centre */
    for (int i = 0; i < n; ++i) {
        int r = road_of[i];
        if (kind[i] != TS_ROAD_STREET || len[r] <= 700.0f || twin[r] <= 0.7f * len[r]) continue;
        float mx = 0.5f * (seg[4 * i] + seg[4 * i + 2]), mz = 0.5f * (seg[4 * i + 1] + seg[4 * i + 3]);
        int town = 0;
        for (unsigned p = 0; p < ts_place_count && !town; ++p) {
            if (ts_places[p].kind != TS_PLACE_OUTPOST) continue;
            float dx = ts_places[p].pos[0] - mx, dz = ts_places[p].pos[2] - mz;
            town = dx * dx + dz * dz < 900.0f * 900.0f;
        }
        if (town) twin[r] = 0.0f; /* the whole road stays a street */
        else kind[i] = TS_ROAD_MOTORWAY;
    }
    for (int i = 0; i < n; ++i)
        if (kind[i] == TS_ROAD_MOTORWAY && twin[road_of[i]] == 0.0f) kind[i] = TS_ROAD_STREET;
done:
    free(len), free(twin), free(head), free(next);
}

int ts_gamemap_roads(const uint8_t *nav, size_t size, float *seg, uint8_t *kind, int max) {
    if (!nav || size < 0x88 || memcmp(nav, "WVAN", 4)) return 0;
    uint32_t nodes = rd32(nav + 0x58), roads = rd32(nav + 0x5C), list = rd32(nav + 0x60);
    if (!nodes || nodes > 1000000 || !roads || roads > 100000 || list > 2000000) return 0;
    size_t node_at = 0x88, road_at = node_at + (size_t)nodes * 40, list_at = road_at + (size_t)roads * 24;
    if (list_at + (size_t)list * 8 > size) return 0;
    int n = 0, *road_of = (int *)malloc(sizeof(int) * (size_t)max);
    if (!road_of) return 0;
    for (uint32_t r = 0; r < roads; ++r) {
        const uint8_t *rec = nav + road_at + (size_t)r * 24;
        uint32_t cnt = rd32(rec + 8), type = rd32(rec + 12), end = rd32(rec + 16);
        if (cnt < 2 || end > list || cnt > end) continue;
        /* 0.13: types 5 and 6 are real roads too (the festival's entrance, Castelletto's coast road: Marvin's
           screenshots against the game's map); before they were drawn as faint dirt tracks and the GPS left the 6s out.
           For the motorway search they stay apart (TRAIL), so two of them side by side don't make a motorway. */
        uint8_t k = type == 4 ? TS_ROAD_STREET : type == 5 || type == 6 ? TS_ROAD_TRAIL : TS_ROAD_DIRT;
        for (uint32_t i = end - cnt; i + 1 < end && n < max; ++i) {
            uint32_t a = rd32(nav + list_at + (size_t)i * 8), b = rd32(nav + list_at + (size_t)(i + 1) * 8);
            if (a >= nodes || b >= nodes) continue;
            const uint8_t *pa = nav + node_at + (size_t)a * 40 + 12, *pb = nav + node_at + (size_t)b * 40 + 12;
            float ax = rdf(pa), az = rdf(pa + 8), bx = rdf(pb), bz = rdf(pb + 8);
            if (!(fabsf(ax) < 30000.0f && fabsf(az) < 30000.0f && fabsf(bx) < 30000.0f && fabsf(bz) < 30000.0f)) continue;
            if ((ax - bx) * (ax - bx) + (az - bz) * (az - bz) > 300.0f * 300.0f) continue; /* not one road's step */
            seg[4 * n] = ax, seg[4 * n + 1] = az, seg[4 * n + 2] = bx, seg[4 * n + 3] = bz;
            kind[n] = k, road_of[n] = (int)r, n++;
        }
    }
    find_motorways(seg, kind, road_of, n, (int)roads);
    for (int i = 0; i < n; ++i)
        if (kind[i] == TS_ROAD_TRAIL) kind[i] = TS_ROAD_STREET;
    free(road_of);
    return n;
}

/* ------------------------------------------------------------------ the picture */

static uint32_t mixc(uint32_t a, uint32_t b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = a >> 16 & 255, ag = a >> 8 & 255, ab = a & 255, br = b >> 16 & 255, bg = b >> 8 & 255, bb = b & 255;
    return (uint32_t)((int)(ar + (br - ar) * t + 0.5f) << 16 | (int)(ag + (bg - ag) * t + 0.5f) << 8 | (int)(ab + (bb - ab) * t + 0.5f));
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* the game's picture as RGB (SRC_W x SRC_H x 3) from low-0-0.swatchbin */
static uint8_t *decode_picture(const uint8_t *zip, size_t zip_size) {
    size_t n = 0;
    uint8_t *tex = ts_unzip(zip, zip_size, "low-0-0.swatchbin", &n);
    const uint32_t bw = SRC_W / 4, bh = SRC_H / 4;
    if (!tex) return NULL;
    if (n != SRC_HEAD + (size_t)bw * bh * 16) return free(tex), NULL;
    uint8_t *rgb = (uint8_t *)malloc((size_t)SRC_W * SRC_H * 3);
    if (!rgb) return free(tex), NULL;
    uint8_t px[64];
    for (uint32_t by = 0; by < bh; ++by)
        for (uint32_t bx = 0; bx < bw; ++bx) {
            ts_bc7_block(tex + SRC_HEAD + (size_t)ts_gamemap_tile_index(bx, by, bw) * 16, px);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    uint8_t *d = rgb + ((size_t)(by * 4 + y) * SRC_W + bx * 4 + x) * 3;
                    d[0] = px[(y * 4 + x) * 4], d[1] = px[(y * 4 + x) * 4 + 1], d[2] = px[(y * 4 + x) * 4 + 2];
                }
        }
    free(tex);
    return rgb;
}

/* box sums over (2r+1)^2 of a w*h float plane, in place (two passes with a running sum) */
static void box(float *p, int w, int h, int r, float *tmp) {
    for (int y = 0; y < h; ++y) {
        float *row = p + (size_t)y * w, s = 0.0f;
        for (int x = -r; x <= r; ++x) s += row[x < 0 ? 0 : x >= w ? w - 1 : x];
        for (int x = 0; x < w; ++x) {
            tmp[x] = s;
            int a = x - r, b = x + r + 1;
            s += row[b >= w ? w - 1 : b] - row[a < 0 ? 0 : a];
        }
        memcpy(row, tmp, sizeof(float) * (size_t)w);
    }
    for (int x = 0; x < w; ++x) {
        float s = 0.0f;
        for (int y = -r; y <= r; ++y) s += p[(size_t)(y < 0 ? 0 : y >= h ? h - 1 : y) * w + x];
        for (int y = 0; y < h; ++y) {
            tmp[y] = s;
            int a = y - r, b = y + r + 1;
            s += p[(size_t)(b >= h ? h - 1 : b) * w + x] - p[(size_t)(a < 0 ? 0 : a) * w + x];
        }
        for (int y = 0; y < h; ++y) p[(size_t)y * w + x] = tmp[y];
    }
}

/* the picture's colour (bilinear) at the map pixel x/y (of w x h over the map's rectangle) */
static void sample(const uint8_t *src, int w, int h, int x, int y, float c[3]) {
    c[0] = c[1] = c[2] = 0.0f;
    if (!src) return;
    float wx = RECT[0] + ((float)x + 0.5f) * (RECT[1] - RECT[0]) / (float)w;
    float wz = RECT[2] - ((float)y + 0.5f) * (RECT[2] - RECT[3]) / (float)h;
    float u = (wx - SRC_X0) / SRC_MPP - 0.5f, v = (SRC_Z0 - wz) / SRC_MPP - 0.5f;
    int u0 = (int)floorf(u), v0 = (int)floorf(v);
    float fu = u - (float)u0, fv = v - (float)v0;
    for (int k = 0; k < 4; ++k) {
        int uu = u0 + (k & 1), vv = v0 + (k >> 1);
        if (uu < 0) uu = 0;
        if (vv < 0) vv = 0;
        if (uu >= SRC_W) uu = SRC_W - 1;
        if (vv >= SRC_H) vv = SRC_H - 1;
        float wgt = ((k & 1) ? fu : 1.0f - fu) * ((k >> 1) ? fv : 1.0f - fv);
        const uint8_t *s = src + ((size_t)vv * SRC_W + uu) * 3;
        c[0] += wgt * s[0], c[1] += wgt * s[1], c[2] += wgt * s[2];
    }
}

/* the dark map for the picture's rectangle (RECT), w x h, from the picture (or plain land without one) */
static int style(const uint8_t *src, int w, int h, uint32_t *out, uint8_t *flag) {
    size_t n = (size_t)w * h;
    float *water = (float *)malloc(sizeof(float) * n), *veg = (float *)malloc(sizeof(float) * n), *lit = (float *)malloc(sizeof(float) * n),
          *town = (float *)malloc(sizeof(float) * n), *ok = (float *)malloc(sizeof(float) * n),
          *tmp = (float *)malloc(sizeof(float) * (size_t)(w > h ? w : h));
    if (!water || !veg || !lit || !town || !ok || !tmp) {
        free(water), free(veg), free(lit), free(town), free(ok), free(tmp);
        return 0;
    }
    /* the picture's own names (white letters, and a pixel round them) don't count: the phone writes its own */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float c[3];
            sample(src, w, h, x, y, c);
            float mx = fmaxf(c[0], fmaxf(c[1], c[2])), mn = fminf(c[0], fminf(c[1], c[2]));
            ok[(size_t)y * w + x] = src && mn > 115.0f && mx - mn < 45.0f ? 1.0f : 0.0f;
        }
    box(ok, w, h, 2, tmp);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = (size_t)y * w + x;
            int label = ok[i] > 0.5f;
            ok[i] = label ? 0.0f : 1.0f;
            if (!src) {
                water[i] = 0, veg[i] = 0, lit[i] = 0.3f, town[i] = 0;
                continue;
            }
            float c[3];
            sample(src, w, h, x, y, c);
            float r = c[0], g = c[1], b = c[2];
            float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
            float lum = 0.3f * r + 0.59f * g + 0.11f * b;
            water[i] = ((b > r + 4.0f && mx < 95.0f) || mx < 34.0f) ? 1.0f : 0.0f;
            veg[i] = label ? 0.0f : clamp01((g - r + 6.0f) / 18.0f) * clamp01((150.0f - lum) / 70.0f);
            lit[i] = label ? 0.0f : clamp01((lum - 60.0f) / 110.0f);
            /* towns: grey (roofs, streets) or the old towns' orange roofs, brighter than the land */
            town[i] = label ? 0.0f : clamp01((lum - 95.0f) / 50.0f) * (clamp01((40.0f - (mx - mn)) / 25.0f) + clamp01((r - g - 25.0f) / 20.0f));
        }
    /* smooth: water by a 9x9 majority (no stray dark spots, no letters in the sea), the rest over 5x5 without the
       letters */
    box(water, w, h, 4, tmp);
    /* where the letters were: the land round them, wider (12 px) */
    float *wide[4] = {NULL, NULL, NULL, NULL}, *plane[4] = {ok, veg, lit, town};
    for (int k = 0; k < 4; ++k) {
        wide[k] = (float *)malloc(sizeof(float) * n);
        if (wide[k]) memcpy(wide[k], plane[k], sizeof(float) * n), box(wide[k], w, h, 12, tmp);
    }
    box(ok, w, h, 2, tmp), box(veg, w, h, 2, tmp), box(lit, w, h, 2, tmp), box(town, w, h, 2, tmp);
    for (size_t i = 0; i < n; ++i) {
        if (ok[i] > 12.5f || !wide[0] || !wide[1] || !wide[2] || !wide[3] || wide[0][i] < 1.0f) continue;
        float k = 25.0f / wide[0][i]; /* to the 5x5's scale */
        ok[i] = 25.0f, veg[i] = wide[1][i] * k, lit[i] = wide[2][i] * k, town[i] = wide[3][i] * k;
    }
    for (int k = 0; k < 4; ++k) free(wide[k]);
    for (size_t i = 0; i < n; ++i) {
        int wet = water[i] > 81.0f * 0.5f;
        flag[i] = wet ? 2 : 0;
        if (wet) {
            out[i] = C_WATER;
            continue;
        }
        float k = ok[i] > 0.5f ? 1.0f / ok[i] : 0.0f;
        float vg = veg[i] * k, lt = ok[i] > 0.5f ? lit[i] * k : 0.35f, tw = town[i] * k;
        uint32_t col = mixc(C_LAND, C_FIELD, clamp01((lt - 0.25f) * 2.2f));
        col = mixc(col, C_FOREST, clamp01(vg * 1.6f));
        col = mixc(col, C_TOWN, clamp01((tw - 0.25f) * 2.0f));
        out[i] = col;
    }
    /* the shore: a slightly lighter rim on the water */
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
            size_t i = (size_t)y * w + x;
            if (flag[i] != 2) continue;
            int land = (flag[i - 1] != 2) + (flag[i + 1] != 2) + (flag[i - w] != 2) + (flag[i + w] != 2);
            if (land) out[i] = 0x1C2D45u;
        }
    free(water), free(veg), free(lit), free(town), free(ok), free(tmp);
    return 1;
}

/* the roads into the picture, smooth edges (for the zoomed-out views; map.c draws them sharp when near) */
static void bake_roads(uint32_t *img, int w, int h, const float *seg, const uint8_t *kind, int n) {
    float sx = (float)w / (RECT[1] - RECT[0]), sy = (float)h / (RECT[2] - RECT[3]);
    for (int pass = 0; pass < 4; ++pass)
        for (int i = 0; i < n; ++i) {
            if (kind[i] != pass) continue;
            float hw = ROAD_HW[pass] * sx;
            float min_hw = pass == TS_ROAD_TRAIL ? 0.3f : pass == TS_ROAD_DIRT ? 0.45f : 0.6f;
            float alpha = pass == TS_ROAD_TRAIL ? 0.6f : 1.0f;
            if (hw < min_hw) alpha *= hw / min_hw, hw = min_hw;
            float ax = (seg[4 * i] - RECT[0]) * sx, ay = (RECT[2] - seg[4 * i + 1]) * sy;
            float bx = (seg[4 * i + 2] - RECT[0]) * sx, by = (RECT[2] - seg[4 * i + 3]) * sy;
            int x0 = (int)floorf((ax < bx ? ax : bx) - hw - 1), x1 = (int)ceilf((ax > bx ? ax : bx) + hw + 1);
            int y0 = (int)floorf((ay < by ? ay : by) - hw - 1), y1 = (int)ceilf((ay > by ? ay : by) + hw + 1);
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > w - 1) x1 = w - 1;
            if (y1 > h - 1) y1 = h - 1;
            float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    float px = (float)x + 0.5f - ax, py = (float)y + 0.5f - ay;
                    float t = l2 > 1e-6f ? (px * vx + py * vy) / l2 : 0.0f;
                    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
                    float dx = px - t * vx, dy = py - t * vy, d = sqrtf(dx * dx + dy * dy);
                    float cov = clamp01(hw + 0.5f - d) * alpha;
                    if (cov > 0.0f) img[(size_t)y * w + x] = mixc(img[(size_t)y * w + x], ROAD_COL[pass], cov);
                }
        }
}

int ts_gamemap_load(const uint8_t *zip, size_t zip_size, const uint8_t *nav, size_t nav_size) {
    uint8_t *src = zip ? decode_picture(zip, zip_size) : NULL;
    float *seg = nav ? (float *)malloc(sizeof(float) * 4 * MAX_SEG) : NULL;
    uint8_t *kind = nav ? (uint8_t *)malloc(MAX_SEG) : NULL;
    int n = seg && kind ? ts_gamemap_roads(nav, nav_size, seg, kind, MAX_SEG) : 0;
    int got = (src ? TS_GAMEMAP_PICTURE : 0) | (n > 0 ? TS_GAMEMAP_ROADS : 0);
    if (got) {
        /* the picture's own scale over the map's rectangle */
        int w = (int)lroundf((RECT[1] - RECT[0]) / SRC_MPP), h = (int)lroundf((RECT[2] - RECT[3]) / SRC_MPP);
        uint32_t *img = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h);
        uint8_t *flag = (uint8_t *)malloc((size_t)w * h);
        if (img && flag && style(src, w, h, img, flag)) {
            uint32_t *plain = n > 0 ? (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * h) : NULL;
            if (plain) memcpy(plain, img, sizeof(uint32_t) * (size_t)w * h);
            if (n > 0) bake_roads(img, w, h, seg, kind, n);
            if (!ts_map_load_rgb(w, h, img, plain, flag, seg, kind, n, RECT)) got = 0;
            free(plain);
        } else got = 0;
        free(img), free(flag);
    }
    free(src), free(seg), free(kind);
    return got;
}
