#!/usr/bin/env python3
"""GIFs of the phone's animations (0.9.1), from the frames the tests write:
  GIFS=1 SCREENS=1 PHONE_PREVIEW=<dir> MAP_ZIP=... MAP_NAV=... ./test_core && python tools/anim_gifs.py <dir> <out>
The crossfades are done here as the window does them (phone_anim.c): from the picture shown last to the new one,
the map kept unfaded while only the rows change. Opening/closing and the ride panel are composed from still pictures."""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from phone_preview import font, fit  # noqa: E402

FPS = 25
BG = (96, 110, 120)


def render(d, base):
    """the picture with its texts, un-premultiplied RGBA"""
    meta = json.load(open(os.path.join(d, base + ".json"), encoding="utf-8"))
    w, h = meta["w"], meta["h"]
    raw = np.frombuffer(open(os.path.join(d, base + ".bgra"), "rb").read(), np.uint8).reshape(h, w, 4).astype(np.float32)
    a = raw[..., 3:4]
    rgb = np.where(a > 0, np.minimum(255, raw[..., 2::-1] * 255 / np.maximum(a, 1)), 0)
    img = Image.fromarray(np.concatenate([rgb, a], 2).astype(np.uint8), "RGBA")
    draw = ImageDraw.Draw(img)
    for t in meta["texts"]:
        f = font(t["size"], t["weight"])
        c = t["color"]
        col = ((c >> 16) & 255, (c >> 8) & 255, c & 255, 255)
        if t["wrap"]:
            words, lines, line = t["text"].split(), [], ""
            for word in words:
                test = (line + " " + word).strip()
                if draw.textlength(test, font=f) > t["w"] and line:
                    lines.append(line)
                    line = word
                else:
                    line = test
            lines.append(line)
            y = t["y"]
            for ln in lines:
                if y + t["size"] > t["y"] + t["h"]:
                    break
                lw = draw.textlength(ln, font=f)
                draw.text((t["x"] + (t["w"] - lw) / 2, y), ln, font=f, fill=col)
                y += t["size"] * 1.25
            continue
        s = fit(draw, t["text"], f, t["w"])
        lw = draw.textlength(s, font=f)
        x = t["x"] if t["align"] == 0 else t["x"] + (t["w"] - lw) / 2 if t["align"] == 1 else t["x"] + t["w"] - lw
        draw.text((x, t["y"] + t["h"] / 2), s, font=f, fill=col, anchor="lm")
    return img


def on_bg(img, size=None, at=(0, 0), alpha=1.0):
    bg = Image.new("RGBA", size or img.size, BG + (255,))
    if alpha < 1.0:
        a = np.asarray(img).copy()
        a[..., 3] = (a[..., 3] * alpha).astype(np.uint8)
        img = Image.fromarray(a, "RGBA")
    bg.alpha_composite(img, at)
    return np.asarray(bg.convert("RGB")).astype(np.float32)


def ease_out(t):
    t = min(max(t, 0.0), 1.0)
    return 1 - (1 - t) ** 3


def save(frames, path, scale=0.7):
    ims = []
    for f in frames:
        im = Image.fromarray(np.clip(f + 0.5, 0, 255).astype(np.uint8), "RGB")
        if scale != 1:
            im = im.resize((int(im.width * scale), int(im.height * scale)), Image.LANCZOS)
        ims.append(im.quantize(colors=192, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=int(1000 / FPS), loop=0, optimize=True)
    print(path, len(ims), os.path.getsize(path) // 1024, "KB")


def sequence(d, rows):
    """the window's crossfade (0.22 s), frame by frame"""
    out, shown, prev, fade = [], None, None, None
    for i, r in enumerate(rows):
        img = on_bg(render(d, r["name"]))
        if prev and shown is not None and (r["skey"] != prev["skey"] or r["pkey"] != prev["pkey"]):
            keep = []
            if r["map"] and prev["map"] and r["pkey"] == prev["pkey"] and r["rect"] == prev["rect"]:
                keep.append(r["rect"])
            if r["tabs"] == prev["tabs"] and r["trect"] == prev["trect"] and r["trect"][2] > r["trect"][0]:
                keep.append(r["trect"])
            fade = (shown, i, keep)
        frame = img
        if fade:
            src, i0, keep = fade
            t = (i - i0) / FPS / 0.22
            if t >= 1:
                fade = None
            else:
                e = ease_out(t)
                frame = src * (1 - e) + img * e
                for x0, y0, x1, y1 in keep:
                    frame[y0:y1, x0:x1] = img[y0:y1, x0:x1]
        out.append(frame)
        shown, prev = frame, r
    return out


def main(d, outdir):
    os.makedirs(outdir, exist_ok=True)
    seqs = {}
    for line in open(os.path.join(d, "gifs.txt")):
        seq, name, sk, pk, mp, x0, y0, x1, y1, tc, t0, t1, t2, t3 = line.split()
        seqs.setdefault(seq, []).append({"name": name, "skey": sk, "pkey": pk, "map": int(mp), "rect": (int(x0), int(y0), int(x1), int(y1)),
                                         "tabs": int(tc), "trect": (int(t0), int(t1), int(t2), int(t3))})
    titles = {"zoom": "map-zoom", "tabs": "tabs", "auswahl": "selection", "vorschau": "ride-preview", "auftrag": "job-zoom",
              "navi": "navi-ride"}
    for seq, rows in seqs.items():
        save(sequence(d, rows), os.path.join(outdir, titles.get(seq, seq) + ".gif"))

    # opening and closing the phone: slides in from the left as it fades in (0.25 s), fades out to the left (0.2 s)
    phone = render(d, seqs["auswahl"][0]["name"])
    size = (phone.width + 60, phone.height)
    frames = [on_bg(phone, size, (-1000, 0))] * 8
    for i in range(int(0.25 * FPS) + 1):
        e = ease_out(i / FPS / 0.25)
        frames.append(on_bg(phone, size, (int(40 - (1 - e) * 28), 0), e))
    frames += [on_bg(phone, size, (40, 0))] * int(1.2 * FPS)
    for i in range(int(0.2 * FPS) + 1):
        e = ease_out(i / FPS / 0.2)
        frames.append(on_bg(phone, size, (int(40 - e * 20), 0), 1 - e))
    frames += [on_bg(phone, size, (-1000, 0))] * 10
    save(frames, os.path.join(outdir, "phone-open-close.gif"))

    # the ride panel: comes up and fades in (0.35 s), a wish fades in (0.3 s), it goes down and fades out (0.25 s)
    a, b = render(d, "gifdash_a_0"), render(d, "gifdash_b_0")
    size = (a.width + 40, a.height + 40)
    fa, fb = on_bg(a, size, (20, 20)), on_bg(b, size, (20, 20))
    frames = [on_bg(a, size, (-2000, 0))] * 6
    for i in range(int(0.35 * FPS) + 1):
        e = ease_out(i / FPS / 0.35)
        frames.append(on_bg(a, size, (20, 20 + int((1 - e) * 18)), e))
    frames += [fa] * int(1.0 * FPS)
    for i in range(int(0.3 * FPS) + 1):
        e = ease_out(i / FPS / 0.3)
        frames.append(fa * (1 - e) + fb * e)
    frames += [fb] * int(1.5 * FPS)
    for i in range(int(0.25 * FPS) + 1):
        k = i / FPS / 0.25
        frames.append(on_bg(b, size, (20, 20 + int(10 * k)), 1 - min(k, 1)))
    frames += [on_bg(a, size, (-2000, 0))] * 6
    save(frames, os.path.join(outdir, "ride-panel.gif"), 1.0)

    # the passengers' lines: the first fades in, the next ones fade over (0.3 s), the last fades out
    lines = [render(d, "giffeed1_0"), render(d, "giffeed2_0"), render(d, "feed_0")]
    size = (lines[0].width + 40, lines[0].height + 40)
    empty = on_bg(lines[0], size, (-3000, 0))
    fl = [on_bg(x, size, (20, 20)) for x in lines]
    frames = [empty] * 6
    for i in range(int(0.3 * FPS) + 1):
        frames.append(on_bg(lines[0], size, (20, 20), ease_out(i / FPS / 0.3)))
    frames += [fl[0]] * int(1.0 * FPS)
    for k in (1, 2):
        for i in range(int(0.3 * FPS) + 1):
            e = ease_out(i / FPS / 0.3)
            frames.append(fl[k - 1] * (1 - e) + fl[k] * e)
        frames += [fl[k]] * int(1.0 * FPS)
    for i in range(int(0.3 * FPS) + 1):
        frames.append(on_bg(lines[2], size, (20, 20), 1 - ease_out(i / FPS / 0.3)))
    frames += [empty] * 8
    save(frames, os.path.join(outdir, "passenger-lines.gif"), 1.0)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
