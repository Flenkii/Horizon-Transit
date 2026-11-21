#!/usr/bin/env python3
"""Cleans the xboxdome.de picture of the FH2 map (4083 px wide) for tools/make_map.py:

  python tools/clean_xboxdome.py XBOXDOME.jpg KUDOSPRIME.jpg CLEAN.png

- the collectible dots, the festival's logo, the rings, the arrow and the watermarks are filled from the second
  picture (kudosprime.com's, without dots), laid exactly over the first (SIFT + homography): the roads under them stay
- the names (towns, FRANCE, ITALIA, the sea, HORIZON FESTIVAL) are in both pictures: only their letters are
  painted over (inpaint), the ground and the roads round them stay."""
import sys
import numpy as np
import cv2

PREV = 1600.0  # the rectangles below are in a 1600-px-wide view of the picture


def register(a, b):
    """b warped onto a"""
    s = 0.5
    fb = s * a.shape[1] / b.shape[1]
    ga = cv2.cvtColor(cv2.resize(a, None, fx=s, fy=s), cv2.COLOR_BGR2GRAY)
    gb = cv2.cvtColor(cv2.resize(b, None, fx=fb, fy=fb), cv2.COLOR_BGR2GRAY)
    sift = cv2.SIFT_create(8000)
    ka, da = sift.detectAndCompute(ga, None)
    kb, db = sift.detectAndCompute(gb, None)
    good = [m for m, n in cv2.BFMatcher().knnMatch(db, da, k=2) if m.distance < 0.75 * n.distance]
    src = np.float32([kb[g.queryIdx].pt for g in good])
    dst = np.float32([ka[g.trainIdx].pt for g in good])
    H, inl = cv2.findHomography(src, dst, cv2.RANSAC, 3.0)
    print("matches", len(good), "inliers", int(inl.sum()))
    Hf = np.diag([1 / s, 1 / s, 1]) @ H @ np.diag([fb, fb, 1])
    w = cv2.warpPerspective(b, Hf, (a.shape[1], a.shape[0]), flags=cv2.INTER_CUBIC)
    cover = cv2.warpPerspective(np.full(b.shape[:2], 255, np.uint8), Hf, (a.shape[1], a.shape[0])) > 250
    return w, cover


def match_colours(src, ref, mask):
    out = src.astype(np.float32)
    for ch in range(3):
        s, r = src[..., ch][mask].astype(np.float32), ref[..., ch][mask].astype(np.float32)
        out[..., ch] = (out[..., ch] - s.mean()) / (s.std() + 1e-3) * r.std() + r.mean()
    return np.clip(out, 0, 255).astype(np.uint8)


def main():
    a = cv2.imread(sys.argv[1])
    b = cv2.imread(sys.argv[2])
    k = a.shape[1] / PREV
    R = lambda x, y, w, h: (int(x * k), int(y * k), int((x + w) * k), int((y + h) * k))
    kud, cover = register(a, b)
    kud = match_colours(kud, a, cover)
    f = a.astype(np.float32)
    hsv = cv2.cvtColor(a, cv2.COLOR_BGR2HSV)
    h, s, v = hsv[..., 0].astype(int), hsv[..., 1].astype(int), hsv[..., 2].astype(int)
    # 1. from the second picture: dots, the logo, rings, the arrow, xboxdome's marks
    dots = ((h > 45) & (h < 80) & (s > 150) & (v > 150)) | ((h > 140) & (h < 170) & (s > 120) & (v > 140)) | \
           ((h > 22) & (h < 35) & (s > 170) & (v > 200))
    fill = dots.astype(np.uint8) * 255
    x0, y0, x1, y1 = R(1012, 466, 56, 50)  # the festival's logo (orange, white)
    logo = ((f[..., 2] - f[..., 0] > 50) | (f.min(2) > 185))[y0:y1, x0:x1]
    fill[y0:y1, x0:x1] |= logo.astype(np.uint8) * 255
    for r in [(440, 295, 112, 18), (735, 384, 110, 20), (1480, 650, 120, 60), (1490, 700, 110, 24), (1146, 528, 30, 28)]:
        x0, y0, x1, y1 = R(*r)
        fill[y0:y1, x0:x1] = 255
    ang = np.linspace(0, 2 * np.pi, 90, endpoint=False)
    lum = f.mean(2)
    rings = np.zeros_like(fill)
    for cx, cy in [(190, 35), (1015, 35), (1448, 35), (190, 258), (190, 410), (1448, 355), (190, 605), (997, 341)]:
        best = None
        for r in np.arange(10, 14.5, 0.5):
            for dy in range(-14, 15):
                for dx in range(-14, 15):
                    X = ((cx + dx + r * np.cos(ang)) * k).astype(int)
                    Y = ((cy + dy + r * np.sin(ang)) * k).astype(int)
                    ok = (X >= 0) & (X < lum.shape[1]) & (Y >= 0) & (Y < lum.shape[0])
                    if ok.sum() < 45:
                        continue
                    sc = lum[Y[ok], X[ok]].mean()
                    if best is None or sc > best[0]:
                        best = (sc, cx + dx, cy + dy, r)
        _, bx, by, r = best
        cv2.circle(rings, (int(bx * k), int(by * k)), int(r * k), 255, int(2.6 * k))
        for t in (0, np.pi / 2, np.pi, 1.5 * np.pi):
            cv2.circle(rings, (int((bx + r * np.cos(t)) * k), int((by + r * np.sin(t)) * k)), int(2 * k), 255, -1)
        if (cx, cy) == (997, 341):
            cv2.circle(rings, (int(bx * k), int(by * k)), int(5 * k), 255, -1)
    fill |= rings
    fill = cv2.dilate(fill, np.ones((7, 7), np.uint8))
    # where the second picture has its own marks (its watermark at the top right, its K at the bottom) or doesn't
    # reach: paint over instead
    kmarks = np.zeros_like(fill)
    for r in [(1180, 0, 420, 48), (750, 690, 60, 50)]:
        x0, y0, x1, y1 = R(*r)
        kmarks[y0:y1, x0:x1] = 255
    use_k = (fill > 0) & cover & (kmarks == 0)
    soft = cv2.GaussianBlur(use_k.astype(np.float32), (0, 0), 1.5)[..., None]
    out = (a * (1 - soft) + kud * soft).astype(np.uint8)
    paint = ((fill > 0) & ~use_k).astype(np.uint8) * 255
    # 2. the names: only the letters
    bg = cv2.medianBlur(a, 31).astype(np.float32).mean(2)
    for r in [(220, 222, 68, 22), (655, 188, 98, 24), (840, 308, 98, 24), (1258, 250, 90, 24), (528, 408, 40, 22),
              (1182, 598, 94, 24), (440, 310, 120, 42), (1018, 276, 96, 40), (480, 570, 180, 30), (600, 140, 55, 20),
              (1112, 118, 40, 18), (265, 312, 50, 28), (612, 315, 48, 18), (220, 442, 52, 16), (1205, 388, 42, 16),
              (990, 505, 110, 45)]:
        x0, y0, x1, y1 = R(*r)
        letters = lum[y0:y1, x0:x1] > bg[y0:y1, x0:x1] + 14
        paint[y0:y1, x0:x1] |= letters.astype(np.uint8) * 255
    # HORIZON: big shaded white letters on the festival's light ground: everything whitish there, wider
    x0, y0, x1, y1 = R(1000, 506, 92, 26)
    big = (f[y0:y1, x0:x1].min(2) > 140) & (f[y0:y1, x0:x1].max(2) - f[y0:y1, x0:x1].min(2) < 40)
    big = cv2.dilate(big.astype(np.uint8) * 255, np.ones((11, 11), np.uint8))
    paint = cv2.dilate(paint, np.ones((5, 5), np.uint8))
    paint[y0:y1, x0:x1] |= big
    out = cv2.inpaint(out, paint, 5, cv2.INPAINT_TELEA)
    cv2.imwrite(sys.argv[3], out)
    print(sys.argv[3], "from the second picture %.2f %%, painted %.2f %%" % (use_k.mean() * 100, (paint > 0).mean() * 100))


if __name__ == "__main__":
    main()
