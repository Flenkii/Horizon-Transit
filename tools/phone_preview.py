#!/usr/bin/env python3
"""Turns the phone pictures the tests write (PHONE_PREVIEW=<dir>) into PNGs, with the texts drawn by PIL.
Linux fonts stand in for Segoe UI, so letters look a little different from the game.
Usage: PHONE_PREVIEW=out/preview ./test_core && python tools/phone_preview.py out/preview"""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

FONTS = {400: "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 600: "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
         700: "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"}


def font(size, weight):
    return ImageFont.truetype(FONTS[700 if weight >= 600 else 400], max(6, int(size * 0.86)))


def fit(draw, text, f, width):
    if draw.textlength(text, font=f) <= width:
        return text
    while text and draw.textlength(text + "…", font=f) > width:
        text = text[:-1]
    return text + "…"


def main(d):
    for name in sorted(os.listdir(d)):
        if not name.endswith(".json"):
            continue
        base = os.path.join(d, name[:-5])
        meta = json.load(open(base + ".json", encoding="utf-8"))
        w, h = meta["w"], meta["h"]
        raw = open(base + ".bgra", "rb").read()
        img = Image.frombuffer("RGBA", (w, h), raw, "raw", "BGRA", 0, 1)
        # un-premultiply for PIL
        px = img.load()
        out = Image.new("RGBA", (w, h))
        op = out.load()
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                op[x, y] = (min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a), a) if a else (0, 0, 0, 0)
        draw = ImageDraw.Draw(out)
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
        # on a game-like grey backdrop
        bg = Image.new("RGBA", (w, h), (96, 110, 120, 255))
        bg.alpha_composite(out)
        bg.convert("RGB").save(base + ".png")


if __name__ == "__main__":
    main(sys.argv[1])
