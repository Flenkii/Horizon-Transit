/* The phone's animations, see phone_anim.h. */
#include "phone_anim.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f

float ts_ease_inout(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

float ts_ease_out(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

static float wrap(float a) {
    while (a > PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}

/* eases v towards target (k of the way), lands on it when close; 1 = still moving */
static int follow(float *v, float target, float k, float close, int angle) {
    float d = angle ? wrap(target - *v) : target - *v;
    if (fabsf(d) <= close) {
        *v = target;
        return 0;
    }
    *v += d * k;
    if (angle) *v = wrap(*v);
    return 1;
}

int ts_map_anim(TsMapAnim *a, TsHud *h, double now) {
    if (!h->map_on) {
        a->on = 0, a->last = now;
        return 0;
    }
    float tx = h->map_x, tz = h->map_z, tw = h->map_w > 1.0f ? h->map_w : 1.0f, th = h->map_h > 1.0f ? h->map_h : 1.0f;
    if (!a->on || a->keys != h->map_keys) { /* a new map: it comes as it is (the screen fades it in) */
        a->on = 1, a->keys = h->map_keys, a->gliding = 0, a->last = now;
        a->x = a->tx = tx, a->z = a->tz = tz, a->w = a->tw = tw, a->h = a->th = th;
        a->rot = h->map_rot, a->head = h->map_heading, a->navi = h->map_navi_t;
        return 0;
    }
    float dt = (float)(now - a->last);
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    a->last = now;

    /* the car's heading, the turn of the map, the dot becoming the arrow: eased (no jumps of the rounded heading) */
    int busy = follow(&a->head, h->map_heading, 1.0f - expf(-dt / 0.10f), 0.002f, 1);
    float kr = 1.0f - expf(-dt / 0.16f);
    busy |= follow(&a->rot, h->map_navi ? a->head : 0.0f, kr, 0.002f, 1);
    busy |= follow(&a->navi, h->map_navi_t, kr, 0.004f, 0);
    if (h->map_navi) /* navi looks ahead along the eased turn, so the view doesn't swing with each jolt */
        for (int i = 0; i < h->map_pin_count; ++i)
            if (h->map_pin[i].kind == TS_PIN_CAR) {
                float ahead = 0.25f * tw;
                tx = h->map_pin[i].x + ahead * sinf(a->rot), tz = h->map_pin[i].z + ahead * cosf(a->rot);
                break;
            }

    /* a jump of the view (another zoom, the route's next target): it glides there */
    float small = a->tw < a->th ? a->tw : a->th;
    if (fabsf(tx - a->tx) + fabsf(tz - a->tz) > 0.04f * small || fabsf(logf(tw / a->tw)) > 0.02f || fabsf(logf(th / a->th)) > 0.02f) {
        a->fx = a->x, a->fz = a->z, a->fw = a->w, a->fh = a->h;
        a->t0 = now, a->gliding = 1;
    }
    a->tx = tx, a->tz = tz, a->tw = tw, a->th = th;
    if (a->gliding) {
        float t = (float)(now - a->t0) / TS_MAP_GLIDE_S;
        if (t >= 1.0f) a->gliding = 0;
        else {
            float e = ts_ease_inout(t);
            a->w = a->fw * expf(logf(tw / a->fw) * e), a->h = a->fh * expf(logf(th / a->fh) * e);
            /* the middle moves with the size: a zoom keeps one spot of the map in its place (no swinging out) */
            float u = fabsf(tw - a->fw) > 0.05f * tw ? (a->w - a->fw) / (tw - a->fw) : e;
            a->x = a->fx + (tx - a->fx) * u, a->z = a->fz + (tz - a->fz) * u;
            busy = 1;
        }
    }
    if (!a->gliding) a->x = tx, a->z = tz, a->w = tw, a->h = th;

    h->map_x = a->x, h->map_z = a->z, h->map_w = a->w, h->map_h = a->h;
    h->map_rot = a->rot, h->map_heading = a->head, h->map_navi_t = a->navi;
    return busy;
}

int ts_tab_anim(TsMapAnim *a, TsHud *h, double now) {
    float dt = (float)(now - a->tab_last);
    a->tab_last = now;
    if (!a->tab_on || a->tabs != h->tab_count || dt > 0.5f) {
        a->tab_on = 1, a->tabs = h->tab_count, a->tab = (float)h->tab;
        h->tab_slide = 0.0f;
        return 0;
    }
    if (dt < 0.0f) dt = 0.0f;
    int busy = follow(&a->tab, (float)h->tab, 1.0f - expf(-dt / 0.07f), 0.01f, 0);
    h->tab_slide = busy ? a->tab + 1.0f : 0.0f;
    return busy;
}

/* FNV-1a */
static uint32_t mixin(uint32_t k, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) k = (k ^ b[i]) * 16777619u;
    return k;
}
static uint32_t mix_int(uint32_t k, int v) { return mixin(k, &v, sizeof(v)); }
static uint32_t mix_str(uint32_t k, const char *s) { return mixin(k, s, strlen(s) + 1); }

uint32_t ts_screen_key(const TsHud *h) {
    uint32_t k = 2166136261u;
    k = mix_int(k, h->panel), k = mix_int(k, h->tab), k = mix_int(k, h->tab_count);
    k = mix_int(k, h->row_count), k = mix_int(k, h->row_selected);
    for (int r = 0; r < h->row_count && r < 9; ++r) k = mix_str(k, h->row_label[r]), k = mix_int(k, h->row_heading[r]);
    if (h->row_selected >= 0 && h->row_selected < 9) k = mix_str(k, h->row_value[h->row_selected]);
    k = mix_int(k, h->map_on), k = mix_int(k, h->map_keys);
    k = mix_str(k, h->detail_title), k = mix_int(k, h->detail_count);
    k = mix_int(k, h->countdown > 0);
    return k;
}

uint32_t ts_pins_key(const TsHud *h) {
    uint32_t k = 2166136261u;
    for (int i = 0; i < h->map_pin_count; ++i) {
        const TsMapPin *p = &h->map_pin[i];
        if (p->kind == TS_PIN_CAR) continue;
        k = mix_int(k, p->kind), k = mix_int(k, p->route), k = mix_int(k, p->num), k = mix_str(k, p->label);
        k = mix_int(k, (int)floorf(p->x / 10.0f)), k = mix_int(k, (int)floorf(p->z / 10.0f));
    }
    return k;
}

uint32_t ts_dash_key(const TsHud *h) {
    uint32_t k = 2166136261u;
    k = mix_str(k, h->trip_label), k = mix_str(k, h->trip_to), k = mix_str(k, h->chip3);
    k = mix_int(k, h->sign_limit), k = mix_int(k, h->sign_free), k = mix_int(k, h->g_on);
    k = mix_int(k, h->trip_class), k = mix_int(k, h->trip_pool), k = mix_int(k, h->meter >= 0), k = mix_str(k, h->meter_label);
    return k;
}

static int room(TsFader *f, int w, int h) {
    size_t n = (size_t)w * h;
    if (w <= 0 || h <= 0) return 0;
    if ((size_t)f->cap < n) {
        uint32_t *a = (uint32_t *)realloc(f->from, n * 4), *b;
        if (!a) return 0;
        f->from = a;
        b = (uint32_t *)realloc(f->raw, n * 4);
        if (!b) return 0;
        f->raw = b, f->cap = (int)n;
    }
    return 1;
}

int ts_fader_start(TsFader *f, const uint32_t *shown, int w, int h, double now, float secs) {
    if (!shown || !room(f, w, h)) {
        f->on = 0;
        return 0;
    }
    memcpy(f->from, shown, (size_t)w * h * 4);
    memcpy(f->raw, shown, (size_t)w * h * 4);
    f->w = w, f->h = h, f->on = 1, f->t0 = now, f->secs = secs > 0.01f ? secs : 0.01f;
    memset(f->keep, 0, sizeof(f->keep));
    return 1;
}

void ts_fader_drawn(TsFader *f, const uint32_t *px, int w, int h) {
    if (!f->on) return;
    if (w != f->w || h != f->h) {
        f->on = 0;
        return;
    }
    memcpy(f->raw, px, (size_t)w * h * 4);
}

int ts_fader_apply(TsFader *f, uint32_t *px, int w, int h, double now) {
    if (!f->on) return 0;
    if (w != f->w || h != f->h) {
        f->on = 0;
        return 0;
    }
    float t = (float)(now - f->t0) / f->secs;
    if (t >= 1.0f) {
        memcpy(px, f->raw, (size_t)w * h * 4);
        f->on = 0;
        return 0;
    }
    int m = (int)(ts_ease_out(t) * 256.0f + 0.5f);
    for (int y = 0; y < h; ++y) {
        uint32_t *d = px + (size_t)y * w;
        const uint32_t *a = f->from + (size_t)y * w, *b = f->raw + (size_t)y * w;
        for (int x = 0; x < w; ++x) d[x] = ts_px_mix(a[x], b[x], m);
        for (int k = 0; k < 2; ++k) { /* the kept parts: new at once */
            const int *r = f->keep[k];
            if (r[2] <= r[0] || y < r[1] || y >= r[3]) continue;
            int x0 = r[0] < 0 ? 0 : r[0], x1 = r[2] > w ? w : r[2];
            if (x1 > x0) memcpy(d + x0, b + x0, (size_t)(x1 - x0) * 4);
        }
    }
    return 1;
}

void ts_fader_free(TsFader *f) {
    free(f->from), free(f->raw);
    memset(f, 0, sizeof(*f));
}
