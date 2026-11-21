/* 0.9: deflate (RFC 1951) and the zip's directory, see inflate.h. Written after the well-known canonical-Huffman
   decoder layout (counts per length + symbols sorted by code), checked by the tests against zlib's output. */
#include "inflate.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *in;
    size_t in_n, in_pos;
    uint32_t buf;
    int cnt, bad;
    uint8_t *out;
    size_t out_n, out_pos;
} St;

typedef struct {
    short count[16];
    short sym[320];
} Huff;

static int bits(St *s, int need) {
    uint32_t v = s->buf;
    while (s->cnt < need) {
        if (s->in_pos >= s->in_n) {
            s->bad = 1;
            return 0;
        }
        v |= (uint32_t)s->in[s->in_pos++] << s->cnt;
        s->cnt += 8;
    }
    s->buf = v >> need;
    s->cnt -= need;
    return (int)(v & ((1u << need) - 1));
}

static int build(Huff *h, const short *len, int n) {
    short offs[16];
    for (int i = 0; i < 16; ++i) h->count[i] = 0;
    for (int i = 0; i < n; ++i) h->count[len[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int i = 1; i < 16; ++i) {
        left <<= 1;
        left -= h->count[i];
        if (left < 0) return -1; /* too many codes */
    }
    offs[1] = 0;
    for (int i = 1; i < 15; ++i) offs[i + 1] = (short)(offs[i] + h->count[i]);
    for (int i = 0; i < n; ++i)
        if (len[i]) h->sym[offs[len[i]]++] = (short)i;
    return left; /* > 0: incomplete (allowed for a single code) */
}

static int decode(St *s, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; ++l) {
        code |= bits(s, 1);
        if (s->bad) return -1;
        int c = h->count[l];
        if (code - c < first) return h->sym[index + (code - first)];
        index += c, first += c;
        first <<= 1, code <<= 1;
    }
    return -1;
}

static const short LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short LEXT[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const short DEXT[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(St *s, const Huff *lc, const Huff *dc) {
    for (;;) {
        int sym = decode(s, lc);
        if (sym < 0) return 0;
        if (sym < 256) {
            if (s->out_pos >= s->out_n) return 0;
            s->out[s->out_pos++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 1;
        } else {
            sym -= 257;
            if (sym >= 29) return 0;
            int len = LBASE[sym] + bits(s, LEXT[sym]);
            int ds = decode(s, dc);
            if (ds < 0 || ds >= 30) return 0;
            size_t dist = (size_t)DBASE[ds] + (size_t)bits(s, DEXT[ds]);
            if (s->bad || dist > s->out_pos || s->out_pos + (size_t)len > s->out_n) return 0;
            uint8_t *o = s->out + s->out_pos;
            for (int i = 0; i < len; ++i) o[i] = o[(ptrdiff_t)i - (ptrdiff_t)dist];
            s->out_pos += (size_t)len;
        }
    }
}

static int stored(St *s) {
    s->buf = 0, s->cnt = 0; /* to a byte */
    if (s->in_pos + 4 > s->in_n) return 0;
    unsigned len = s->in[s->in_pos] | s->in[s->in_pos + 1] << 8, nlen = s->in[s->in_pos + 2] | s->in[s->in_pos + 3] << 8;
    s->in_pos += 4;
    if ((len ^ 0xFFFFu) != nlen || s->in_pos + len > s->in_n || s->out_pos + len > s->out_n) return 0;
    memcpy(s->out + s->out_pos, s->in + s->in_pos, len);
    s->in_pos += len, s->out_pos += len;
    return 1;
}

static int fixed(St *s) {
    static Huff lc, dc;
    static int made;
    if (!made) {
        short len[320];
        int i = 0;
        for (; i < 144; ++i) len[i] = 8;
        for (; i < 256; ++i) len[i] = 9;
        for (; i < 280; ++i) len[i] = 7;
        for (; i < 288; ++i) len[i] = 8;
        build(&lc, len, 288);
        for (i = 0; i < 30; ++i) len[i] = 5;
        build(&dc, len, 30);
        made = 1;
    }
    return codes(s, &lc, &dc);
}

static int dynamic(St *s) {
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short len[320];
    Huff lc, dc;
    int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    if (s->bad || nlen > 286 || ndist > 30) return 0;
    int i = 0;
    for (; i < ncode; ++i) len[order[i]] = (short)bits(s, 3);
    for (; i < 19; ++i) len[order[i]] = 0;
    if (s->bad || build(&lc, len, 19) != 0) return 0;
    i = 0;
    while (i < nlen + ndist) {
        int sym = decode(s, &lc);
        if (sym < 0) return 0;
        if (sym < 16) {
            len[i++] = (short)sym;
        } else {
            short v = 0;
            int rep;
            if (sym == 16) {
                if (i == 0) return 0;
                v = len[i - 1];
                rep = 3 + bits(s, 2);
            } else if (sym == 17) rep = 3 + bits(s, 3);
            else rep = 11 + bits(s, 7);
            if (s->bad || i + rep > nlen + ndist) return 0;
            while (rep--) len[i++] = v;
        }
    }
    if (len[256] == 0) return 0;
    int e = build(&lc, len, nlen);
    if (e < 0 || (e > 0 && nlen - lc.count[0] != 1)) return 0;
    e = build(&dc, len + nlen, ndist);
    if (e < 0 || (e > 0 && ndist - dc.count[0] != 1)) return 0;
    return codes(s, &lc, &dc);
}

int ts_inflate(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
    St s;
    memset(&s, 0, sizeof(s));
    s.in = src, s.in_n = src_size, s.out = dst, s.out_n = dst_size;
    int last;
    do {
        last = bits(&s, 1);
        int type = bits(&s, 2);
        if (s.bad) return 0;
        int ok = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 ? dynamic(&s) : 0;
        if (!ok || s.bad) return 0;
    } while (!last);
    return s.out_pos == dst_size;
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

uint8_t *ts_unzip(const uint8_t *zip, size_t n, const char *name, size_t *out_size) {
    if (!zip || n < 22) return NULL;
    size_t e = n - 22, stop = n > 22 + 65535 ? n - 22 - 65535 : 0;
    for (;; --e) {
        if (rd32(zip + e) == 0x06054b50u) break;
        if (e == stop) return NULL;
    }
    uint32_t count = rd16(zip + e + 10), cd = rd32(zip + e + 16);
    size_t nl = strlen(name), p = cd;
    for (uint32_t k = 0; k < count; ++k) {
        if (p + 46 > n || rd32(zip + p) != 0x02014b50u) return NULL;
        unsigned method = rd16(zip + p + 10), fl = rd16(zip + p + 28), xl = rd16(zip + p + 30), cl = rd16(zip + p + 32);
        uint32_t csize = rd32(zip + p + 20), usize = rd32(zip + p + 24), lho = rd32(zip + p + 42);
        if (p + 46 + fl > n) return NULL;
        if (fl == nl && !memcmp(zip + p + 46, name, nl)) {
            if ((size_t)lho + 30 > n || rd32(zip + lho) != 0x04034b50u) return NULL;
            size_t data = (size_t)lho + 30 + rd16(zip + lho + 26) + rd16(zip + lho + 28);
            if (data > n || csize > n - data || usize == 0 || usize > 512u * 1024 * 1024) return NULL;
            uint8_t *out = (uint8_t *)malloc(usize);
            if (!out) return NULL;
            int ok = method == 0 ? (csize == usize ? (memcpy(out, zip + data, usize), 1) : 0)
                   : method == 8 ? ts_inflate(zip + data, csize, out, usize) : 0;
            if (!ok) return free(out), NULL;
            *out_size = usize;
            return out;
        }
        p += 46 + fl + xl + cl;
    }
    return NULL;
}
