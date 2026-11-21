/* 0.9: BC7 block decoding, see bc7.h. Taken from Pillow's src/libImaging/BcnDecode.c (Pillow 11.0.0), which says:
   "The contents of this file are in the public domain (CC0)" (https://creativecommons.org/publicdomain/zero/1.0/).
   Only the BC7 part, with the types renamed; see third_party/README.md. */
#include "bc7.h"

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef struct {
    UINT8 r, g, b, a;
} rgba;

static UINT8
get_bits(const UINT8 *src, int bit, int count) {
    UINT8 v;
    int x;
    int by = bit >> 3;
    bit &= 7;
    if (!count) {
        return 0;
    }
    if (bit + count <= 8) {
        v = (src[by] >> bit) & ((1 << count) - 1);
    } else {
        x = src[by] | (src[by + 1] << 8);
        v = (x >> bit) & ((1 << count) - 1);
    }
    return v;
}

/* BC7 */
typedef struct {
    char ns;
    char pb;
    char rb;
    char isb;
    char cb;
    char ab;
    char epb;
    char spb;
    char ib;
    char ib2;
} bc7_mode_info;

static const bc7_mode_info bc7_modes[] = {
    {3, 4, 0, 0, 4, 0, 1, 0, 3, 0},
    {2, 6, 0, 0, 6, 0, 0, 1, 3, 0},
    {3, 6, 0, 0, 5, 0, 0, 0, 2, 0},
    {2, 6, 0, 0, 7, 0, 1, 0, 2, 0},
    {1, 0, 2, 1, 5, 6, 0, 0, 2, 3},
    {1, 0, 2, 0, 7, 8, 0, 0, 2, 2},
    {1, 0, 0, 0, 7, 7, 1, 0, 4, 0},
    {2, 6, 0, 0, 5, 5, 1, 0, 2, 0}
};

/* Subset indices:
   Table.P2, 1 bit per index */
static const UINT16 bc7_si2[] = {
    0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xfec8, 0xec80, 0xc800, 0xffec,
    0xfe80, 0xe800, 0xffe8, 0xff00, 0xfff0, 0xf000, 0xf710, 0x008e, 0x7100, 0x08ce,
    0x008c, 0x7310, 0x3100, 0x8cce, 0x088c, 0x3110, 0x6666, 0x366c, 0x17e8, 0x0ff0,
    0x718e, 0x399c, 0xaaaa, 0xf0f0, 0x5a5a, 0x33cc, 0x3c3c, 0x55aa, 0x9696, 0xa55a,
    0x73ce, 0x13c8, 0x324c, 0x3bdc, 0x6996, 0xc33c, 0x9966, 0x0660, 0x0272, 0x04e4,
    0x4e40, 0x2720, 0xc936, 0x936c, 0x39c6, 0x639c, 0x9336, 0x9cc6, 0x817e, 0xe718,
    0xccf0, 0x0fcc, 0x7744, 0xee22
};

/* Table.P3, 2 bits per index */
static const UINT32 bc7_si3[] = {
    0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8, 0xa5a50000, 0xa0a05050, 0x5555a0a0,
    0x5a5a5050, 0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090, 0x94949494, 0xa4a4a4a4,
    0xa9a59450, 0x2a0a4250, 0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0, 0xa8a85454,
    0x6a6a4040, 0xa4a45000, 0x1a1a0500, 0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400,
    0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200, 0xa9a58000, 0x5090a0a8, 0xa8a09050,
    0x24242424, 0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50, 0x500aa550, 0xaaaa4444,
    0x66660000, 0xa5a0a5a0, 0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600, 0xaa444444,
    0x54a854a8, 0x95809580, 0x96969600, 0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
    0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000, 0x40804080, 0xa9a8a9a8, 0xaaaaaa44,
    0x2a4a5254
};

/* Anchor indices:
   Table.A2 */
static const char bc7_ai0[] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                               15, 15, 15, 15, 2,  8,  2,  2,  8,  8,  15, 2,  8,
                               2,  2,  8,  8,  2,  2,  15, 15, 6,  8,  2,  8,  15,
                               15, 2,  8,  2,  2,  2,  15, 15, 6,  6,  2,  6,  8,
                               15, 15, 2,  2,  15, 15, 15, 15, 15, 2,  2,  15};

/* Table.A3a */
static const char bc7_ai1[] = {3,  3,  15, 15, 8,  3,  15, 15, 8,  8,  6,  6,  6,
                               5,  3,  3,  3,  3,  8,  15, 3,  3,  6,  10, 5,  8,
                               8,  6,  8,  5,  15, 15, 8,  15, 3,  5,  6,  10, 8,
                               15, 15, 3,  15, 5,  15, 15, 15, 15, 3,  15, 5,  5,
                               5,  8,  5,  10, 5,  10, 8,  13, 15, 12, 3,  3};

/* Table.A3b */
static const char bc7_ai2[] = {15, 8,  8,  3,  15, 15, 3,  8,  15, 15, 15, 15, 15,
                               15, 15, 8,  15, 8,  15, 3,  15, 8,  15, 8,  3,  15,
                               6,  10, 15, 15, 10, 8,  15, 3,  15, 10, 10, 8,  9,
                               10, 6,  15, 8,  15, 3,  6,  6,  8,  15, 3,  15, 15,
                               15, 15, 15, 15, 15, 15, 15, 15, 3,  15, 15, 8};

/* Interpolation weights */
static const char bc7_weights2[] = {0, 21, 43, 64};
static const char bc7_weights3[] = {0, 9, 18, 27, 37, 46, 55, 64};
static const char bc7_weights4[] = {
    0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64
};

static const char *
bc7_get_weights(int n) {
    if (n == 2) {
        return bc7_weights2;
    }
    if (n == 3) {
        return bc7_weights3;
    }
    return bc7_weights4;
}

static int
bc7_get_subset(int ns, int partition, int n) {
    if (ns == 2) {
        return 1 & (bc7_si2[partition] >> n);
    }
    if (ns == 3) {
        return 3 & (bc7_si3[partition] >> (2 * n));
    }
    return 0;
}

static UINT8
expand_quantized(UINT8 v, int bits) {
    v = v << (8 - bits);
    return v | (v >> bits);
}

static void
bc7_lerp(rgba *dst, const rgba *e, int s0, int s1) {
    int t0 = 64 - s0;
    int t1 = 64 - s1;
    dst->r = (UINT8)((t0 * e[0].r + s0 * e[1].r + 32) >> 6);
    dst->g = (UINT8)((t0 * e[0].g + s0 * e[1].g + 32) >> 6);
    dst->b = (UINT8)((t0 * e[0].b + s0 * e[1].b + 32) >> 6);
    dst->a = (UINT8)((t1 * e[0].a + s1 * e[1].a + 32) >> 6);
}

static void
decode_bc7_block(rgba *col, const UINT8 *src) {
    rgba endpoints[6];
    int bit = 0, cibit, aibit;
    int mode = src[0];
    int i, j;
    int numep, cb, ab, ib, ib2, i0, i1, s;
    UINT8 index_sel, partition, rotation, val;
    const char *cw, *aw;
    const bc7_mode_info *info;

    /* mode is the number of unset bits before the first set bit: */
    if (!mode) {
        /* degenerate case when no bits set */
        for (i = 0; i < 16; i++) {
            col[i].r = col[i].g = col[i].b = 0;
            col[i].a = 255;
        }
        return;
    }
    while (!(mode & (1 << bit++)));
    mode = bit - 1;
    info = &bc7_modes[mode];
    /* color selection bits: {subset}{endpoint} */
    cb = info->cb;
    ab = info->ab;
    cw = bc7_get_weights(info->ib);
    aw = bc7_get_weights((ab && info->ib2) ? info->ib2 : info->ib);

#define LOAD(DST, N)             \
    DST = get_bits(src, bit, N); \
    bit += N;
    LOAD(partition, info->pb);
    LOAD(rotation, info->rb);
    LOAD(index_sel, info->isb);
    numep = info->ns << 1;

    /* red */
    for (i = 0; i < numep; i++) {
        LOAD(val, cb);
        endpoints[i].r = val;
    }

    /* green */
    for (i = 0; i < numep; i++) {
        LOAD(val, cb);
        endpoints[i].g = val;
    }

    /* blue */
    for (i = 0; i < numep; i++) {
        LOAD(val, cb);
        endpoints[i].b = val;
    }

    /* alpha */
    for (i = 0; i < numep; i++) {
        if (ab) {
            LOAD(val, ab);
        } else {
            val = 255;
        }
        endpoints[i].a = val;
    }

    /* p-bits */
#define ASSIGN_P(x) x = (x << 1) | val
    if (info->epb) {
        /* per endpoint */
        cb++;
        if (ab) {
            ab++;
        }
        for (i = 0; i < numep; i++) {
            LOAD(val, 1);
            ASSIGN_P(endpoints[i].r);
            ASSIGN_P(endpoints[i].g);
            ASSIGN_P(endpoints[i].b);
            if (ab) {
                ASSIGN_P(endpoints[i].a);
            }
        }
    }
    if (info->spb) {
        /* per subset */
        cb++;
        if (ab) {
            ab++;
        }
        for (i = 0; i < numep; i += 2) {
            LOAD(val, 1);
            for (j = 0; j < 2; j++) {
                ASSIGN_P(endpoints[i + j].r);
                ASSIGN_P(endpoints[i + j].g);
                ASSIGN_P(endpoints[i + j].b);
                if (ab) {
                    ASSIGN_P(endpoints[i + j].a);
                }
            }
        }
    }
#undef ASSIGN_P
#define EXPAND(x, b) x = expand_quantized(x, b)
    for (i = 0; i < numep; i++) {
        EXPAND(endpoints[i].r, cb);
        EXPAND(endpoints[i].g, cb);
        EXPAND(endpoints[i].b, cb);
        if (ab) {
            EXPAND(endpoints[i].a, ab);
        }
    }
#undef EXPAND
#undef LOAD
    cibit = bit;
    aibit = cibit + 16 * info->ib - info->ns;
    for (i = 0; i < 16; i++) {
        s = bc7_get_subset(info->ns, partition, i) << 1;
        ib = info->ib;
        if (i == 0) {
            ib--;
        } else if (info->ns == 2) {
            if (i == bc7_ai0[partition]) {
                ib--;
            }
        } else if (info->ns == 3) {
            if (i == bc7_ai1[partition]) {
                ib--;
            } else if (i == bc7_ai2[partition]) {
                ib--;
            }
        }
        i0 = get_bits(src, cibit, ib);
        cibit += ib;

        if (ab && info->ib2) {
            ib2 = info->ib2;
            if (ib2 && i == 0) {
                ib2--;
            }
            i1 = get_bits(src, aibit, ib2);
            aibit += ib2;
            if (index_sel) {
                bc7_lerp(&col[i], &endpoints[s], aw[i1], cw[i0]);
            } else {
                bc7_lerp(&col[i], &endpoints[s], cw[i0], aw[i1]);
            }
        } else {
            bc7_lerp(&col[i], &endpoints[s], cw[i0], cw[i0]);
        }
#define ROTATE(x, y) \
    val = x;         \
    x = y;           \
    y = val
        if (rotation == 1) {
            ROTATE(col[i].r, col[i].a);
        } else if (rotation == 2) {
            ROTATE(col[i].g, col[i].a);
        } else if (rotation == 3) {
            ROTATE(col[i].b, col[i].a);
        }
#undef ROTATE
    }
}

void ts_bc7_block(const uint8_t *src, uint8_t out[64]) {
    rgba col[16];
    decode_bc7_block(col, src);
    for (int i = 0; i < 16; ++i) out[4 * i] = col[i].r, out[4 * i + 1] = col[i].g, out[4 * i + 2] = col[i].b, out[4 * i + 3] = col[i].a;
}
