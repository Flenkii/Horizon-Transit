#!/usr/bin/env python3
"""Makes the phone's map (0.7) from a picture of the whole FH2 map: a dark map in the style of a phone's maps app.

   python tools/make_map.py SOURCE.jpg OUT.bmp [--cal a b c d] [--dots]

The source is a picture of the game's map (satellite look with the white road lines). It is not part of the mod:
everyone uses their own picture; the mod reads OUT.bmp (8 bit, 256 colours) from its folder.
--cal: source pixel = (a*x + b, c*z + d) for game position x/z (default: the xboxdome picture, 4083 px wide,
fitted on the towns and ~200 race starts). Other pictures get their calibration by matching (--match REF.jpg).
--dots: paint over coloured collectible dots (the xboxdome picture)."""
import sys, argparse
import numpy as np
import cv2

# the world rectangle the output covers (must match TS_MAP_X0.. in src/core/map.c)
X0, X1, Z0, Z1 = -7500.0, 13500.0, 470.0, -9195.0
OUT_W = 4096
OUT_H = int(round(OUT_W * (Z0 - Z1) / (X1 - X0)))

XBOXDOME_CAL = (0.19445312, 1458.3259, -0.19423242, 91.1265)  # for 4083 px wide: ~200 race starts fitted onto the roads (0.7.1)

# colours (BGR for OpenCV) after a dark phone maps look
LAND = np.array([0x3a, 0x2a, 0x1e], float)     # #1e2a3a navy
LAND_LIGHT = np.array([0x5a, 0x40, 0x2e], float)  # open fields a little lighter
FOREST = np.array([0x38, 0x36, 0x1b], float)   # #1b3638 dark teal
WATER = np.array([0x22, 0x14, 0x0c], float)    # #0c1422
SHORE = np.array([0x40, 0x2a, 0x1a], float)
ROAD = np.array([0x8c, 0x76, 0x60], float)     # #60768c slate
ROAD_MAIN = np.array([0xb0, 0x9c, 0x88], float)
HIGHWAY = np.array([0x6e, 0x9c, 0xb8], float)  # #b89c6e soft amber
DIRT = np.array([0x5e, 0x5e, 0x6a], float)
CITY = np.array([0x6a, 0x4e, 0x3a], float)


def load(path):
    im = cv2.imread(path, cv2.IMREAD_COLOR)
    if im is None:
        sys.exit("can't read " + path)
    return im


def remove_dots(im):
    hsv = cv2.cvtColor(im, cv2.COLOR_BGR2HSV)
    h, s, v = hsv[..., 0].astype(int), hsv[..., 1].astype(int), hsv[..., 2].astype(int)
    green = (h > 45) & (h < 80) & (s > 150) & (v > 150)
    magenta = (h > 140) & (h < 170) & (s > 120) & (v > 140)
    yellow = (h > 22) & (h < 35) & (s > 170) & (v > 200)
    m = (green | magenta | yellow).astype(np.uint8) * 255
    m = cv2.dilate(m, np.ones((7, 7), np.uint8))
    return cv2.inpaint(im, m, 7, cv2.INPAINT_TELEA)


def masks(im):
    """the water and the roads (for the routing: the palette marks them)"""
    f = im.astype(float)
    b, g, r = f[..., 0], f[..., 1], f[..., 2]
    mx, mn = f.max(2), f.min(2)
    water = ((b > r + 4) & (mx < 95)) | (mx < 34)
    water = cv2.morphologyEx(water.astype(np.uint8), cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
    water = cv2.morphologyEx(water, cv2.MORPH_CLOSE, np.ones((9, 9), np.uint8))
    n, lab, stats, _ = cv2.connectedComponentsWithStats(water)
    keep = np.zeros(n, bool)
    keep[1:] = stats[1:, cv2.CC_STAT_AREA] > 400
    water = keep[lab]
    white = (mn > 175) & (mx - mn < 45)
    yellow = (r > 190) & (g > 180) & (b < 165) & (r - b > 55)
    dirt = (r > 160) & (r - g > 35) & (g - b > 0) & (r - b > 70) & (mx - mn > 60)
    road = (white | yellow | dirt) & ~water
    return water.astype(np.uint8), road.astype(np.uint8)


def style(im):
    f = im.astype(float)
    b, g, r = f[..., 0], f[..., 1], f[..., 2]
    mx, mn = f.max(2), f.min(2)
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    # water: dark, blue-green over red
    water = ((b > r + 4) & (mx < 95)) | (mx < 34)
    water = cv2.morphologyEx(water.astype(np.uint8), cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))
    water = cv2.morphologyEx(water, cv2.MORPH_CLOSE, np.ones((9, 9), np.uint8))
    n, lab, stats, _ = cv2.connectedComponentsWithStats(water)
    keep = np.zeros(n, bool)
    keep[1:] = stats[1:, cv2.CC_STAT_AREA] > 400  # no small dark spots
    water = keep[lab].astype(float)
    water = cv2.GaussianBlur(water, (5, 5), 0)
    # the map's lines
    white = ((mn > 175) & (mx - mn < 45)).astype(float)
    yellow = ((r > 190) & (g > 180) & (b < 165) & (r - b > 55)).astype(float)
    dirt = ((r > 160) & (r - g > 35) & (g - b > 0) & (r - b > 70) & (mx - mn > 60)).astype(float)
    city = cv2.GaussianBlur(white, (0, 0), 9)  # many white pixels together: a town
    city = np.clip((city - 0.18) * 3, 0, 1)
    soft = lambda m: cv2.GaussianBlur(m, (3, 3), 0)
    white, yellow, dirt = soft(white), soft(yellow), soft(dirt)
    # land: navy, forest teal, a little relief from the brightness
    veg = np.clip((g - r + 6) / 18.0, 0, 1) * np.clip((150 - lum) / 70.0, 0, 1)
    veg = cv2.GaussianBlur(veg, (0, 0), 2)
    bright = np.clip((lum - 60) / 120.0, 0, 1)[..., None]
    land = LAND * (1 - bright) + LAND_LIGHT * bright
    land = land * (1 - veg[..., None]) + FOREST * veg[..., None]
    shade = (0.85 + 0.3 * np.clip(lum / 200.0, 0, 1))[..., None]
    land = land * shade
    out = land
    out = out * (1 - city[..., None] * 0.6) + CITY * city[..., None] * 0.6
    w = water[..., None]
    out = out * (1 - w) + WATER * w
    for m, col in ((dirt, DIRT), (white, ROAD_MAIN), (yellow, HIGHWAY)):
        a = (m * (1 - water))[..., None]
        out = out * (1 - a) + col * a
    return np.clip(out, 0, 255).astype(np.uint8)


def to_world(im, cal):
    a, b, c, d = cal
    # output pixel -> world -> source pixel
    xs = X0 + (np.arange(OUT_W) + 0.5) * (X1 - X0) / OUT_W
    zs = Z0 + (np.arange(OUT_H) + 0.5) * (Z1 - Z0) / OUT_H
    mapx = np.tile((a * xs + b).astype(np.float32), (OUT_H, 1))
    mapy = np.tile((c * zs + d).astype(np.float32)[:, None], (1, OUT_W))
    return cv2.remap(im, mapx, mapy, cv2.INTER_AREA if a < 1 else cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT,
                     borderValue=tuple(int(v) for v in WATER))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--cal", nargs=4, type=float)
    ap.add_argument("--dots", action="store_true")
    ap.add_argument("--png", help="also a full-colour PNG (preview)")
    o = ap.parse_args()
    im = load(o.src)
    cal = o.cal
    if not cal:
        k = im.shape[1] / 4083.0
        cal = (XBOXDOME_CAL[0] * k, XBOXDOME_CAL[1] * k, XBOXDOME_CAL[2] * k, XBOXDOME_CAL[3] * k)
    if o.dots:
        im = remove_dots(im)
    out = to_world(style(im), cal)
    water, road = masks(im)
    water = to_world(water * 255, cal)[..., 0] > 127 if water.ndim == 3 else to_world(np.dstack([water * 255] * 3), cal)[..., 0] > 127
    road = to_world(np.dstack([road * 255] * 3), cal)[..., 0] > 100
    road &= ~water
    if o.png:
        cv2.imwrite(o.png, out)
    write_bmp(o.out, cv2.cvtColor(out, cv2.COLOR_BGR2RGB), water, road)
    print(o.out, OUT_W, OUT_H, "roads %.1f %%" % (road.mean() * 100), "water %.1f %%" % (water.mean() * 100))


def write_bmp(path, rgb, water, road):
    """8 bit, 256 colours: 192 for the land, 48 for the roads, 16 for the water; a colour's 4th byte: 1 road, 2 water"""
    from PIL import Image
    h, w = water.shape
    index = np.zeros((h, w), np.uint8)
    pal = np.zeros((256, 4), np.uint8)
    land = ~water & ~road
    base = 0
    for mask, count, flag in ((land, 192, 0), (road, 48, 1), (water, 16, 2)):
        px = rgb[mask]
        if len(px) == 0:
            base += count
            continue
        side = int(np.ceil(np.sqrt(len(px))))
        sq = np.zeros((side * side, 3), np.uint8)
        sq[:len(px)] = px
        sq[len(px):] = px[0]
        q = Image.fromarray(sq.reshape(side, side, 3)).quantize(count, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        qi = np.asarray(q).reshape(-1)[:len(px)]
        qp = np.array(q.getpalette()[:count * 3], np.uint8).reshape(-1, 3)
        index[mask] = qi + base
        pal[base:base + len(qp), 0] = qp[:, 2]
        pal[base:base + len(qp), 1] = qp[:, 1]
        pal[base:base + len(qp), 2] = qp[:, 0]
        pal[base:base + count, 3] = flag
        base += count
    stride = (w + 3) & ~3
    rows = np.zeros((h, stride), np.uint8)
    rows[:, :w] = index[::-1]  # bottom-up
    import struct
    off = 14 + 40 + 1024
    with open(path, "wb") as f:
        f.write(b"BM" + struct.pack("<IHHI", off + rows.size, 0, 0, off))
        f.write(struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, rows.size, 2835, 2835, 256, 256))
        f.write(pal.tobytes())
        f.write(rows.tobytes())


if __name__ == "__main__":
    main()
