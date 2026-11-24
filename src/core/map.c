/* 0.7: the phone's map, see map.h. The picture as colours (level 0, with a flag per pixel: road, water) and smaller
   levels (for zooming out); the ways along the roads by A* on a grid at half the picture's size (a road cell costs 1,
   land 10, water can't be crossed). 0.9: the picture can come from the game's own files (gamemap.c) and the roads as
   lines (drawn sharp when near, and marked on the grid). */
#include "map.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LEVELS 5        /* level 0 = the picture, then each half the size */
#define WATER_RGB 0x17263Cu
#define COST_ROAD 1
#define COST_LAND 10
#define SNAP_CELLS 30   /* the start and end look this far for a road */
#define CACHE 16
#define CACHE_PTS 768
#define ROAD_CELL 64.0f /* the roads' lookup grid, metres */

static uint32_t *img;                /* level 0: w*h colours 0xRRGGBB, top row first */
static uint8_t *flg;                 /* level 0: 1 road, 2 water */
static uint32_t *lev[LEVELS];        /* levels 1..: colours */
static int lw[LEVELS], lh[LEVELS];
static int W, H;
/* the world rectangle the picture covers (0.7's BMP: TS_MAP_*; 0.9: the game's picture, its own) */
static float RX0 = TS_MAP_X0, RX1 = TS_MAP_X1, RZ0 = TS_MAP_Z0, RZ1 = TS_MAP_Z1;
static uint32_t sea = 0x0C1422u; /* past the picture's edge: its water */
static int ready; /* 0.9: set last (the map may be made on another thread while the phone draws) */

/* the roads as lines (0.9) */
static float *rseg;                  /* x0, z0, x1, z1 */
static uint8_t *rkind;
static int rn;
static int *rcell_start, *rcell_list, rgw, rgh;
static const uint32_t ROAD_RGB[4] = {0x2F3847u, 0x464F5Eu, 0x5A6475u, 0x8A7A5Cu}; /* trail, dirt, street, motorway */
static const float ROAD_HALF[4] = {1.6f, 2.6f, 4.0f, 6.0f};                        /* metres */

/* the routing grid */
static uint8_t *cost;                /* gw*gh: 0 = can't pass */
static int gw, gh;
static float *gval;
static int32_t *from;
static uint32_t *stamp, gen;
typedef struct {
    float f;
    int32_t i;
} Node;
static Node *heap;
static int heap_n, heap_cap;

typedef struct {
    int used, a, b, n;
    unsigned age;
    float pts[CACHE_PTS * 2];
} Way;
static Way *cache;
static unsigned cache_age;

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void roads_free(void) {
    free(rseg), rseg = NULL;
    free(rkind), rkind = NULL;
    free(rcell_start), rcell_start = NULL;
    free(rcell_list), rcell_list = NULL;
    rn = rgw = rgh = 0;
}

void ts_map_free(void) {
    __atomic_store_n(&ready, 0, __ATOMIC_SEQ_CST);
    free(img), img = NULL;
    free(flg), flg = NULL;
    for (int i = 0; i < LEVELS; ++i) free(lev[i]), lev[i] = NULL;
    free(cost), cost = NULL;
    free(gval), gval = NULL;
    free(from), from = NULL;
    free(stamp), stamp = NULL;
    free(heap), heap = NULL, heap_n = heap_cap = 0;
    free(cache), cache = NULL;
    roads_free();
    W = H = gw = gh = 0;
}

int ts_map_ok(void) { return __atomic_load_n(&ready, __ATOMIC_ACQUIRE); }
int ts_map_width(void) { return W; }
int ts_map_height(void) { return H; }
int ts_map_road_count(void) { return ts_map_ok() ? rn : 0; }

static uint32_t at0(int x, int y) { return img[(size_t)y * W + x]; }

static uint32_t avg4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t r = ((a >> 16 & 255) + (b >> 16 & 255) + (c >> 16 & 255) + (d >> 16 & 255) + 2) / 4;
    uint32_t g = ((a >> 8 & 255) + (b >> 8 & 255) + (c >> 8 & 255) + (d >> 8 & 255) + 2) / 4;
    uint32_t bl = ((a & 255) + (b & 255) + (c & 255) + (d & 255) + 2) / 4;
    return r << 16 | g << 8 | bl;
}

/* the smaller levels and the routing grid from img/flg */
static int finish(void) {
    lw[0] = W, lh[0] = H;
    for (int l = 1; l < LEVELS; ++l) {
        lw[l] = lw[l - 1] / 2, lh[l] = lh[l - 1] / 2;
        lev[l] = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)lw[l] * lh[l]);
        if (!lev[l]) return ts_map_free(), 0;
        for (int y = 0; y < lh[l]; ++y)
            for (int x = 0; x < lw[l]; ++x) {
                uint32_t a, b, c, d;
                if (l == 1) a = at0(2 * x, 2 * y), b = at0(2 * x + 1, 2 * y), c = at0(2 * x, 2 * y + 1), d = at0(2 * x + 1, 2 * y + 1);
                else {
                    const uint32_t *s = lev[l - 1];
                    int sw = lw[l - 1];
                    a = s[(2 * y) * sw + 2 * x], b = s[(2 * y) * sw + 2 * x + 1], c = s[(2 * y + 1) * sw + 2 * x], d = s[(2 * y + 1) * sw + 2 * x + 1];
                }
                lev[l][(size_t)y * lw[l] + x] = avg4(a, b, c, d);
            }
    }
    /* the routing grid: a road anywhere in the 2x2 pixels makes the cell a road; water where all four are water */
    gw = W / 2, gh = H / 2;
    cost = (uint8_t *)malloc((size_t)gw * gh);
    if (!cost) return ts_map_free(), 0;
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            int road = 0, water = 0;
            for (int k = 0; k < 4; ++k) {
                int f = flg[(size_t)(2 * y + (k >> 1)) * W + 2 * x + (k & 1)];
                road |= f == 1, water += f == 2;
            }
            cost[(size_t)y * gw + x] = road ? COST_ROAD : water == 4 ? 0 : COST_LAND;
        }
    return 1;
}

static int publish(void) {
    __atomic_store_n(&ready, 1, __ATOMIC_RELEASE);
    return 1;
}

int ts_map_load_bmp(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    if (!p || size < 54 || p[0] != 'B' || p[1] != 'M') return 0;
    uint32_t off = rd32(p + 10), hsize = rd32(p + 14);
    int32_t w = (int32_t)rd32(p + 18), h = (int32_t)rd32(p + 22);
    int bits = rd16(p + 28);
    uint32_t comp = rd32(p + 30), used = hsize >= 36 ? rd32(p + 46) : 0;
    if (bits != 8 || comp != 0 || w < 64 || w > 16384 || h == 0) return 0;
    int bottom_up = h > 0;
    if (h < 0) h = -h;
    if (h < 32 || h > 16384) return 0;
    if (!used || used > 256) used = 256;
    size_t stride = ((size_t)w + 3) & ~(size_t)3;
    if (14 + (size_t)hsize + used * 4 > size || off > size || stride * (size_t)h > size - off) return 0;
    ts_map_free();
    W = w, H = h;
    uint32_t pal[256];
    uint8_t pal_flag[256];
    memset(pal, 0, sizeof(pal));
    memset(pal_flag, 0, sizeof(pal_flag));
    const uint8_t *q = p + 14 + hsize;
    for (uint32_t i = 0; i < used; ++i) {
        pal[i] = (uint32_t)q[i * 4 + 2] << 16 | (uint32_t)q[i * 4 + 1] << 8 | q[i * 4];
        pal_flag[i] = q[i * 4 + 3];
    }
    img = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)W * H);
    flg = (uint8_t *)malloc((size_t)W * H);
    if (!img || !flg) return ts_map_free(), 0;
    for (int y = 0; y < H; ++y) {
        const uint8_t *row = p + off + stride * (size_t)(bottom_up ? H - 1 - y : y);
        for (int x = 0; x < W; ++x) img[(size_t)y * W + x] = pal[row[x]], flg[(size_t)y * W + x] = pal_flag[row[x]];
    }
    sea = 0x0C1422u;
    RX0 = TS_MAP_X0, RX1 = TS_MAP_X1, RZ0 = TS_MAP_Z0, RZ1 = TS_MAP_Z1;
    return finish() && publish();
}

static int set_roads(const float *seg, const uint8_t *kind, int n);

int ts_map_load_rgb(int w, int h, const uint32_t *rgb, const uint32_t *plain, const uint8_t *flags, const float *seg, const uint8_t *kind, int n, const float *rect) {
    if (!rgb || w < 64 || h < 32 || w > 16384 || h > 16384) return 0;
    if (rect && !(rect[1] > rect[0] && rect[2] > rect[3])) return 0;
    ts_map_free();
    if (rect) RX0 = rect[0], RX1 = rect[1], RZ0 = rect[2], RZ1 = rect[3];
    else RX0 = TS_MAP_X0, RX1 = TS_MAP_X1, RZ0 = TS_MAP_Z0, RZ1 = TS_MAP_Z1;
    W = w, H = h;
    img = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)W * H);
    flg = (uint8_t *)calloc((size_t)W * H, 1);
    if (!img || !flg) return ts_map_free(), 0;
    for (size_t i = 0; i < (size_t)W * H; ++i) img[i] = rgb[i] & 0xFFFFFFu;
    if (flags) memcpy(flg, flags, (size_t)W * H);
    if (!finish()) return 0;
    /* level 0 without the roads: near, map_color draws them sharp itself (the smaller levels keep them) */
    if (plain && seg && n > 0)
        for (size_t i = 0; i < (size_t)W * H; ++i) img[i] = plain[i] & 0xFFFFFFu;
    if (seg && kind && n > 0 && !set_roads(seg, kind, n)) return ts_map_free(), 0;
    sea = WATER_RGB;
    return publish();
}

/* marks the grid cells along a road as road cells */
static void grid_line(float ax, float az, float bx, float bz) {
    float sx = (float)gw / (RX1 - RX0), sy = (float)gh / (RZ0 - RZ1);
    float x0 = (ax - RX0) * sx, y0 = (RZ0 - az) * sy, x1 = (bx - RX0) * sx, y1 = (RZ0 - bz) * sy;
    float l = fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    int steps = (int)ceilf(l * 2.0f) + 1;
    for (int k = 0; k <= steps; ++k) {
        float t = (float)k / (float)steps;
        int x = (int)(x0 + (x1 - x0) * t), y = (int)(y0 + (y1 - y0) * t);
        if (x >= 0 && y >= 0 && x < gw && y < gh) cost[(size_t)y * gw + x] = COST_ROAD;
    }
}

static int set_roads(const float *seg, const uint8_t *kind, int n) {
    roads_free();
    rseg = (float *)malloc(sizeof(float) * 4 * (size_t)n);
    rkind = (uint8_t *)malloc((size_t)n);
    rgw = (int)ceilf((RX1 - RX0) / ROAD_CELL), rgh = (int)ceilf((RZ0 - RZ1) / ROAD_CELL);
    rcell_start = (int *)calloc((size_t)rgw * rgh + 1, sizeof(int));
    if (!rseg || !rkind || !rcell_start) return roads_free(), 0;
    memcpy(rseg, seg, sizeof(float) * 4 * (size_t)n);
    memcpy(rkind, kind, (size_t)n);
    rn = n;
    /* each segment into the cells its box touches (two passes: count, then fill) */
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            for (int i = 0, sum = 0; i <= rgw * rgh; ++i) {
                int c = rcell_start[i];
                rcell_start[i] = sum, sum += c;
            }
            rcell_list = (int *)malloc(sizeof(int) * (size_t)(rcell_start[rgw * rgh] + 1));
            if (!rcell_list) return roads_free(), 0;
        }
        int *fill = NULL;
        if (pass == 1) {
            fill = (int *)malloc(sizeof(int) * (size_t)rgw * rgh);
            if (!fill) return roads_free(), 0;
            memcpy(fill, rcell_start, sizeof(int) * (size_t)rgw * rgh);
        }
        for (int i = 0; i < n; ++i) {
            const float *s = rseg + 4 * i;
            int cx0 = (int)floorf((fminf(s[0], s[2]) - RX0) / ROAD_CELL), cx1 = (int)floorf((fmaxf(s[0], s[2]) - RX0) / ROAD_CELL);
            int cy0 = (int)floorf((RZ0 - fmaxf(s[1], s[3])) / ROAD_CELL), cy1 = (int)floorf((RZ0 - fminf(s[1], s[3])) / ROAD_CELL);
            if (cx0 < 0) cx0 = 0;
            if (cy0 < 0) cy0 = 0;
            if (cx1 >= rgw) cx1 = rgw - 1;
            if (cy1 >= rgh) cy1 = rgh - 1;
            for (int cy = cy0; cy <= cy1; ++cy)
                for (int cx = cx0; cx <= cx1; ++cx) {
                    if (pass == 0) rcell_start[cy * rgw + cx]++;
                    else rcell_list[fill[cy * rgw + cx]++] = i;
                }
        }
        free(fill);
    }
    /* the ways keep to streets and dirt roads (not to the trails) */
    for (int i = 0; i < n; ++i)
        if (rkind[i] != TS_ROAD_TRAIL) grid_line(rseg[4 * i], rseg[4 * i + 1], rseg[4 * i + 2], rseg[4 * i + 3]);
    if (cache) memset(cache, 0, sizeof(Way) * CACHE);
    return 1;
}

/* ------------------------------------------------------------------ colours */

static uint32_t lerp(uint32_t a, uint32_t b, float t) {
    int ar = a >> 16 & 255, ag = a >> 8 & 255, ab = a & 255, br = b >> 16 & 255, bg = b >> 8 & 255, bb = b & 255;
    return (uint32_t)((int)(ar + (br - ar) * t + 0.5f) << 16 | (int)(ag + (bg - ag) * t + 0.5f) << 8 | (int)(ab + (bb - ab) * t + 0.5f));
}

static uint32_t texel(int l, int x, int y) {
    /* past the edge: the edge's colour (ts_map_color fades it into the sea) */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= lw[l]) x = lw[l] - 1;
    if (y >= lh[l]) y = lh[l] - 1;
    return l == 0 ? at0(x, y) : lev[l][(size_t)y * lw[l] + x];
}

static float seg_dist(const float *s, float x, float z) {
    float vx = s[2] - s[0], vz = s[3] - s[1], px = x - s[0], pz = z - s[1], l2 = vx * vx + vz * vz;
    float t = l2 > 1e-6f ? (px * vx + pz * vz) / l2 : 0.0f;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    float dx = px - t * vx, dz = pz - t * vz;
    return sqrtf(dx * dx + dz * dz);
}

/* the roads over a colour at x/z: each kind's nearest line, drawn trails first, motorways last */
static uint32_t road_over(uint32_t col, float x, float z, float mpp) {
    float best[4] = {1e9f, 1e9f, 1e9f, 1e9f}, reach = ROAD_HALF[3] + 1.5f * mpp + 1.0f;
    int cx0 = (int)floorf((x - reach - RX0) / ROAD_CELL), cx1 = (int)floorf((x + reach - RX0) / ROAD_CELL);
    int cy0 = (int)floorf((RZ0 - z - reach) / ROAD_CELL), cy1 = (int)floorf((RZ0 - z + reach) / ROAD_CELL);
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 >= rgw) cx1 = rgw - 1;
    if (cy1 >= rgh) cy1 = rgh - 1;
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx)
            for (int k = rcell_start[cy * rgw + cx]; k < rcell_start[cy * rgw + cx + 1]; ++k) {
                int i = rcell_list[k];
                float d = seg_dist(rseg + 4 * i, x, z);
                if (d < best[rkind[i]]) best[rkind[i]] = d;
            }
    for (int kd = 0; kd < 4; ++kd) {
        if (best[kd] > 1e8f) continue;
        float hw = ROAD_HALF[kd], min_hw = (kd == TS_ROAD_TRAIL ? 0.35f : kd == TS_ROAD_DIRT ? 0.5f : 0.7f) * mpp, a = kd == TS_ROAD_TRAIL ? 0.7f : 1.0f;
        if (hw < min_hw) a *= hw / min_hw, hw = min_hw;
        float cov = (hw + 0.5f * mpp - best[kd]) / mpp;
        if (cov > 0.0f) col = lerp(col, ROAD_RGB[kd], (cov > 1.0f ? 1.0f : cov) * a);
    }
    return col;
}

uint32_t ts_map_color(float x, float z, float mpp) {
    if (!ts_map_ok()) return 0x1E2A3A;
    float m0 = (RX1 - RX0) / (float)W;
    int l = 0;
    while (l + 1 < LEVELS && mpp > m0 * (float)(2 << l) * 0.75f) ++l;
    int outside = x < RX0 || x > RX1 || z > RZ0 || z < RZ1;
    if (outside) l = LEVELS - 1; /* past the edge: the smallest level, soft */
    float u = (x - RX0) / (RX1 - RX0) * (float)lw[l] - 0.5f;
    float v = (RZ0 - z) / (RZ0 - RZ1) * (float)lh[l] - 0.5f;
    int x0 = (int)floorf(u), y0 = (int)floorf(v);
    float fx = u - (float)x0, fy = v - (float)y0;
    uint32_t top = lerp(texel(l, x0, y0), texel(l, x0 + 1, y0), fx);
    uint32_t bot = lerp(texel(l, x0, y0 + 1), texel(l, x0 + 1, y0 + 1), fx);
    uint32_t col = lerp(top, bot, fy);
    /* outside the picture: its edge fades into the sea over 1.5 km */
    float out = 0.0f;
    if (x < RX0) out = RX0 - x;
    if (x > RX1 && x - RX1 > out) out = x - RX1;
    if (z > RZ0 && z - RZ0 > out) out = z - RZ0;
    if (z < RZ1 && RZ1 - z > out) out = RZ1 - z;
    if (out > 0.0f) col = lerp(col, sea, out >= 1500.0f ? 1.0f : out / 1500.0f);
    /* near: the roads as sharp lines (the picture has them only at its own size) */
    if (rn && mpp < m0 * 1.5f && !outside) col = road_over(col, x, z, mpp);
    return col;
}

/* ------------------------------------------------------------------ ways */

static int cell_of(const float w[2]) {
    int x = (int)((w[0] - RX0) / (RX1 - RX0) * (float)gw);
    int y = (int)((RZ0 - w[1]) / (RZ0 - RZ1) * (float)gh);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gw) x = gw - 1;
    if (y >= gh) y = gh - 1;
    return y * gw + x;
}

static void cell_world(int i, float out[2]) {
    out[0] = RX0 + ((float)(i % gw) + 0.5f) * (RX1 - RX0) / (float)gw;
    out[1] = RZ0 - ((float)(i / gw) + 0.5f) * (RZ0 - RZ1) / (float)gh;
}

/* the nearest road cell (or the cell itself) */
static int snap(int i) {
    if (cost[i] == COST_ROAD) return i;
    int cx = i % gw, cy = i / gw, best = -1, bd = 1 << 30;
    int r = cost[i] ? SNAP_CELLS : 3 * SNAP_CELLS; /* in the water (a place by a lake): look further */
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int x = cx + dx, y = cy + dy, d = dx * dx + dy * dy;
            if (x < 0 || y < 0 || x >= gw || y >= gh || d >= bd || d > r * r) continue;
            if (cost[y * gw + x] == COST_ROAD) bd = d, best = y * gw + x;
        }
    return best >= 0 ? best : i;
}

static int push(float f, int32_t i) {
    if (heap_n == heap_cap) {
        int cap = heap_cap ? heap_cap * 2 : 65536;
        Node *n = (Node *)realloc(heap, sizeof(Node) * (size_t)cap);
        if (!n) return 0;
        heap = n, heap_cap = cap;
    }
    int k = heap_n++;
    while (k > 0) {
        int up = (k - 1) / 2;
        if (heap[up].f <= f) break;
        heap[k] = heap[up];
        k = up;
    }
    heap[k].f = f, heap[k].i = i;
    return 1;
}

static Node pop(void) {
    Node top = heap[0], last = heap[--heap_n];
    int k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= heap_n) break;
        if (c + 1 < heap_n && heap[c + 1].f < heap[c].f) ++c;
        if (heap[c].f >= last.f) break;
        heap[k] = heap[c];
        k = c;
    }
    if (heap_n > 0) heap[k] = last;
    return top;
}

static float octile(int a, int b) {
    float dx = fabsf((float)(a % gw - b % gw)), dy = fabsf((float)(a / gw - b / gw));
    return dx > dy ? dx + 0.41421356f * dy : dy + 0.41421356f * dx;
}

/* A* from s to t; fills `from`; 1 = found */
static int astar(int s, int t) {
    if (!gval) {
        size_t n = (size_t)gw * gh;
        gval = (float *)malloc(sizeof(float) * n);
        from = (int32_t *)malloc(sizeof(int32_t) * n);
        stamp = (uint32_t *)calloc(n, sizeof(uint32_t));
        if (!gval || !from || !stamp) return 0;
    }
    if (++gen == 0) memset(stamp, 0, sizeof(uint32_t) * (size_t)gw * gh), gen = 1;
    heap_n = 0;
    stamp[s] = gen, gval[s] = 0.0f, from[s] = -1;
    if (!push(octile(s, t), s)) return 0;
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const float len[8] = {1, 1, 1, 1, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};
    long budget = 6000000;
    while (heap_n > 0 && budget-- > 0) {
        Node n = pop();
        int i = n.i;
        if (i == t) return 1;
        float g = gval[i];
        if (n.f - octile(i, t) > g + 1e-3f) continue; /* an old entry */
        int x = i % gw, y = i / gw;
        for (int k = 0; k < 8; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
            int j = ny * gw + nx;
            int cst = cost[j];
            if (!cst && j != t) continue;
            if (!cst) cst = COST_LAND;
            float ng = g + len[k] * (float)cst;
            if (stamp[j] == gen && gval[j] <= ng) continue;
            stamp[j] = gen, gval[j] = ng, from[j] = i;
            if (!push(ng + octile(j, t), j)) return 0;
        }
    }
    return 0;
}

/* Ramer-Douglas-Peucker on cell coordinates: keeps the corners */
static int simplify(const float *pts, int n, float eps, unsigned char *keep) {
    memset(keep, 0, (size_t)n);
    keep[0] = keep[n - 1] = 1;
    int (*stack)[2] = (int (*)[2])malloc(sizeof(int) * 2 * (size_t)(n + 1)), sp = 0, kept = 2;
    if (!stack) return kept;
    stack[sp][0] = 0, stack[sp][1] = n - 1, sp++;
    while (sp > 0) {
        sp--;
        int a = stack[sp][0], b = stack[sp][1];
        float ax = pts[2 * a], ay = pts[2 * a + 1], bx = pts[2 * b], by = pts[2 * b + 1];
        float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
        float best = -1.0f;
        int bi = -1;
        for (int i = a + 1; i < b; ++i) {
            float px = pts[2 * i] - ax, py = pts[2 * i + 1] - ay, d;
            if (l2 < 1e-6f) d = px * px + py * py;
            else {
                float cr = px * vy - py * vx;
                d = cr * cr / l2;
            }
            if (d > best) best = d, bi = i;
        }
        if (bi >= 0 && best > eps * eps) {
            keep[bi] = 1, kept++;
            stack[sp][0] = a, stack[sp][1] = bi, sp++;
            stack[sp][0] = bi, stack[sp][1] = b, sp++;
        }
    }
    free(stack);
    return kept;
}

static int straight(const float a[2], const float b[2], float *out, int max) {
    if (max < 2) return 0;
    out[0] = a[0], out[1] = a[1], out[2] = b[0], out[3] = b[1];
    return 2;
}

static int route_locked(const float a[2], const float b[2], float *out, int max) {
    if (!ts_map_ok() || max < 2) return 0;
    int s = snap(cell_of(a)), t = snap(cell_of(b));
    if (!cache) cache = (Way *)calloc(CACHE, sizeof(Way));
    if (cache)
        for (int k = 0; k < CACHE; ++k)
            if (cache[k].used && cache[k].a == s && cache[k].b == t) {
                Way *w = &cache[k];
                w->age = ++cache_age;
                int n = w->n < max ? w->n : max;
                memcpy(out, w->pts, sizeof(float) * 2 * (size_t)n);
                out[0] = a[0], out[1] = a[1];
                out[2 * (n - 1)] = b[0], out[2 * (n - 1) + 1] = b[1];
                return n;
            }
    if (s == t || !astar(s, t)) return straight(a, b, out, max);
    /* the cells, start first */
    int n = 0;
    for (int i = t; i >= 0; i = from[i]) ++n;
    float *pts = (float *)malloc(sizeof(float) * 2 * (size_t)(n + 2));
    unsigned char *keep = (unsigned char *)malloc((size_t)n + 2);
    if (!pts || !keep) {
        free(pts), free(keep);
        return straight(a, b, out, max);
    }
    int k = n;
    for (int i = t; i >= 0; i = from[i]) {
        --k;
        pts[2 * k] = (float)(i % gw), pts[2 * k + 1] = (float)(i / gw);
    }
    int lim = max < CACHE_PTS ? max : CACHE_PTS;
    float eps = 0.7f;
    int kept = simplify(pts, n, eps, keep);
    while (kept + 2 > lim && eps < 50.0f) eps *= 1.6f, kept = simplify(pts, n, eps, keep);
    /* a, the way, b */
    int m = 0;
    out[m * 2] = a[0], out[m * 2 + 1] = a[1], m++;
    for (int i = 0; i < n && m < lim - 1; ++i) {
        if (!keep[i]) continue;
        float w[2];
        cell_world((int)pts[2 * i + 1] * gw + (int)pts[2 * i], w);
        out[m * 2] = w[0], out[m * 2 + 1] = w[1], m++;
    }
    out[m * 2] = b[0], out[m * 2 + 1] = b[1], m++;
    free(pts), free(keep);
    if (cache) {
        int slot = 0;
        for (int j = 1; j < CACHE; ++j)
            if (!cache[j].used || (cache[slot].used && cache[j].age < cache[slot].age)) slot = j;
        Way *w = &cache[slot];
        w->used = 1, w->a = s, w->b = t, w->n = m, w->age = ++cache_age;
        memcpy(w->pts, out, sizeof(float) * 2 * (size_t)m);
    }
    return m;
}

/* 0.10: the phone on screen and the phone app (another thread) both ask for ways: one at a time */
static volatile int route_busy;
int ts_map_route(const float a[2], const float b[2], float *out, int max) {
    while (__atomic_exchange_n(&route_busy, 1, __ATOMIC_ACQUIRE)) {
    }
    int n = route_locked(a, b, out, max);
    __atomic_store_n(&route_busy, 0, __ATOMIC_RELEASE);
    return n;
}
