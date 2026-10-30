#!/usr/bin/env python3
"""Contact sheet: one row per input group, thumbnails side by side.

usage: contact.py OUT.jpg ROW1_IMG [ROW1_IMG ...] [-- ROW2_IMG ...]
"""
import sys
from PIL import Image, ImageDraw

THUMB_W = 360


def thumb(path):
    im = Image.open(path)
    im.draft("RGB", (THUMB_W * 2, THUMB_W * 2))  # fast JPEG downscale on load
    im = im.convert("RGB")
    im.thumbnail((THUMB_W, THUMB_W))
    d = ImageDraw.Draw(im)
    label = path.rsplit("/", 1)[-1]
    d.rectangle([0, 0, 8 + 7 * len(label), 14], fill=(0, 0, 0))
    d.text((4, 2), label, fill=(255, 255, 255))
    return im


def main():
    out, args = sys.argv[1], sys.argv[2:]
    rows, cur = [], []
    for a in args:
        if a == "--":
            rows.append(cur)
            cur = []
        else:
            cur.append(a)
    if cur:
        rows.append(cur)
    grid = [[thumb(p) for p in row] for row in rows]
    w = max(sum(t.width for t in r) for r in grid)
    h = sum(max(t.height for t in r) for r in grid)
    sheet = Image.new("RGB", (w, h), (40, 40, 40))
    y = 0
    for r in grid:
        x = 0
        for t in r:
            sheet.paste(t, (x, y))
            x += t.width
        y += max(t.height for t in r)
    sheet.save(out, quality=85)
    print(out, sheet.size)


if __name__ == "__main__":
    main()
