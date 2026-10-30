#!/usr/bin/env python3
"""Generate ui/src/lab_assets.{c,h}: fonts, icons and palette for the 7D UI.

Glyphs and icons are rasterised 8x oversampled (FreeType through Pillow for
text), then reduced with a box filter to 4-bit coverage. They are stretched
horizontally by 9/8: Magic Lantern draws into a 720x480 bitmap that the 7D
shows on a 4:3 (640x480) LCD, so bitmap pixels are 9/8 as narrow as tall.

The palette is the set of colours the renderer quantises to. Its YUV values
use the conversion fitted to the 16 RGB/hardware pairs in ML's
src/disp_direct.c (Y = 0.9216 * BT.601 luma, U * 0.79, V * 1.11, clamped).
"""
import os
from PIL import Image, ImageDraw, ImageFont

SS = 8                 # oversampling factor
HERE = os.path.dirname(os.path.abspath(__file__))
FONT_DIR = os.path.join(HERE, "..", "fonts")
OUT_DIR = os.path.join(HERE, "..", "src")

CHARS = (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
         "abcdefghijklmnopqrstuvwxyz{|}~"
         "ÁÉÍÓÚÜÑáéíóúüñ"
         "¿¡·—→←±°◀▶▲▼")

# C name, font file, weight, size in display pixels
FONTS = [
    ("TITLE30", "SpaceGrotesk.ttf", 600, 30),
    ("TITLE28", "SpaceGrotesk.ttf", 600, 28),
    ("ROW16",   "SpaceGrotesk.ttf", 500, 16),
    ("ROW16B",  "SpaceGrotesk.ttf", 600, 16),
    ("LABEL15", "SpaceGrotesk.ttf", 500, 15),
    ("HELP13",  "SpaceGrotesk.ttf", 400, 13),
    ("MONO22B", "JetBrainsMono.ttf", 600, 22),
    ("MONO13",  "JetBrainsMono.ttf", 500, 13),
    ("MONO12",  "JetBrainsMono.ttf", 500, 12),
    ("MONO12B", "JetBrainsMono.ttf", 600, 12),
    ("MONO11",  "JetBrainsMono.ttf", 500, 11),
    ("MONO10B", "JetBrainsMono.ttf", 600, 10),
    ("MONO9B",  "JetBrainsMono.ttf", 700, 9),
]


def stretch_reduce(img_ss, out_w, out_h):
    """Box-filter an oversampled 8-bit image to out_w x out_h (x stretched 9/8)."""
    return img_ss.resize((out_w, out_h), Image.BOX)


def to_nibbles(img):
    w, h = img.size
    px = img.load()
    return [min(15, (px[x, y] * 15 + 127) // 255) for y in range(h) for x in range(w)]


def crop_nonzero(img):
    box = img.getbbox()
    return (img.crop(box), box) if box else (None, None)


def raster_glyph(font, ch):
    """Return (advance_out_px_float, bitmap_or_None, xoff, yoff)."""
    adv = font.getlength(ch) / SS * 9 / 8
    l, t, r, b = font.getbbox(ch, anchor="ls")
    if r <= l or b <= t:
        return adv, None, 0, 0
    origin = 8 * SS                       # maps to output x = 9 exactly
    top_pad = -t + SS
    base_in = ((top_pad + SS - 1) // SS) * SS
    h_in = base_in + b + SS
    h_in = ((h_in + SS - 1) // SS) * SS
    w_in = origin + r + 2 * SS
    w_in = ((w_in + 8 * SS - 1) // (8 * SS)) * (8 * SS)
    im = Image.new("L", (w_in, h_in), 0)
    ImageDraw.Draw(im).text((origin, base_in), ch, font=font, fill=255, anchor="ls")
    out = stretch_reduce(im, w_in * 9 // (8 * SS), h_in // SS)
    cropped, box = crop_nonzero(out)
    if cropped is None:
        return adv, None, 0, 0
    return adv, cropped, box[0] - 9, box[1] - base_in // SS


def glyph_mask(font, ch):
    """Rendered pixels of one character, to tell real glyphs from .notdef."""
    n = font.size * 2
    im = Image.new("L", (n, n), 0)
    ImageDraw.Draw(im).text((n // 4, n // 4), ch, font=font, fill=255)
    return im.tobytes()


def missing_glyph_mask(font):
    return glyph_mask(font, "")


def gen_font(cname, path, weight, size):
    font = ImageFont.truetype(os.path.join(FONT_DIR, path), size * SS)
    font.set_variation_by_axes([weight])
    notdef = missing_glyph_mask(font)
    glyphs, bits = [], []
    for ch in CHARS:
        if ch != " " and glyph_mask(font, ch) == notdef:
            continue                      # not in this font
        adv, bm, xoff, yoff = raster_glyph(font, ch)
        w, h = (bm.size if bm else (0, 0))
        glyphs.append((ord(ch), xoff, yoff, w, h, round(adv * 256), len(bits)))
        if bm:
            bits.extend(to_nibbles(bm))
    asc, desc = font.getmetrics()
    cap = -font.getbbox("H", anchor="ls")[1] / SS
    return glyphs, bits, round(asc / SS), round(desc / SS), round(cap)


# ---------------------------------------------------------------- icons
# Line icons on a 24-unit grid (same drawings as the mockups' inline SVG).
ICONS = {
    "EXPO": [("circle", 12, 12, 4)] + [("line", *p) for p in [
        (12, 2, 12, 4.5), (12, 19.5, 12, 22), (2, 12, 4.5, 12), (19.5, 12, 22, 12),
        (4.9, 4.9, 6.7, 6.7), (17.3, 17.3, 19.1, 19.1), (4.9, 19.1, 6.7, 17.3), (17.3, 6.7, 19.1, 4.9)]],
    "OVERLAY": [("poly", True, [(12, 3), (21, 8), (12, 13), (3, 8)]),
                ("poly", False, [(3, 13), (12, 18), (21, 13)])],
    "FOCUS": [("circle", 12, 12, 6.5), ("line", 12, 2, 12, 6), ("line", 12, 18, 12, 22),
              ("line", 2, 12, 6, 12), ("line", 18, 12, 22, 12)],
    "SHOOT": [("rrect", 3, 7, 21, 20, 3), ("poly", False, [(8, 7), (10, 4), (14, 4), (16, 7)]),
              ("circle", 12, 13.5, 3.5)],
    "VIDEO": [("rrect", 3, 6, 15, 18, 2.5), ("poly", True, [(15, 10.5), (21, 7), (21, 17), (15, 13.5)])],
    "AUDIO": [("line", 4, 10, 4, 14), ("line", 8, 7, 8, 17), ("line", 12, 4, 12, 20),
              ("line", 16, 8, 16, 16), ("line", 20, 11, 20, 13)],
    "DISPLAY": [("rrect", 3, 4, 21, 16, 2), ("line", 8.5, 20, 15.5, 20), ("line", 12, 16, 12, 20)],
    "MODULES": [("rrect", 4, 4, 11, 11, 1.5), ("rrect", 13, 4, 20, 11, 1.5),
                ("rrect", 4, 13, 11, 20, 1.5), ("rrect", 13, 13, 20, 20, 1.5)],
    "PREFS": [("line", 3, 6, 21, 6), ("line", 3, 12, 21, 12), ("line", 3, 18, 21, 18),
              ("disc", 9, 6, 2.6), ("disc", 15, 12, 2.6), ("disc", 7, 18, 2.6)],
    "CLASSIC": [("line", 9, 6, 21, 6), ("line", 9, 12, 21, 12), ("line", 9, 18, 21, 18),
                ("disc", 4.5, 6, 1.4), ("disc", 4.5, 12, 1.4), ("disc", 4.5, 18, 1.4)],
    "INFO": [("circle", 12, 12, 9.5), ("line", 12, 11, 12, 17), ("disc", 12, 7.5, 1.3)],
}
ICON_SIZES = {"INFO": (14, 2.2)}           # size in display px, stroke in grid units
DEFAULT_ICON = (20, 1.8)


def draw_icon(name, ops):
    size, stroke = ICON_SIZES.get(name, DEFAULT_ICON)
    k = size / 24 * SS                    # grid unit -> oversampled px
    pad = 2 * SS
    w_in = ((int(size * SS) + 2 * pad + 8 * SS - 1) // (8 * SS)) * (8 * SS)
    h_in = int(size * SS) + 2 * pad
    im = Image.new("L", (w_in, h_in), 0)
    d = ImageDraw.Draw(im)
    sw = stroke * k
    P = lambda x, y: (pad + x * k, pad + y * k)

    def cap(x, y):
        cx, cy = P(x, y)
        d.ellipse([cx - sw / 2, cy - sw / 2, cx + sw / 2, cy + sw / 2], fill=255)

    def seg(a, b):
        d.line([P(*a), P(*b)], fill=255, width=max(1, round(sw)))
        cap(*a)
        cap(*b)

    for op in ops:
        kind = op[0]
        if kind == "line":
            seg(op[1:3], op[3:5])
        elif kind == "circle":
            cx, cy = P(op[1], op[2])
            r = op[3] * k + sw / 2
            d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=max(1, round(sw)))
        elif kind == "disc":
            cx, cy = P(op[1], op[2])
            r = op[3] * k
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        elif kind == "rrect":
            x0, y0 = P(op[1], op[2])
            x1, y1 = P(op[3], op[4])
            d.rounded_rectangle([x0 - sw / 2, y0 - sw / 2, x1 + sw / 2, y1 + sw / 2],
                                radius=op[5] * k + sw / 2, outline=255, width=max(1, round(sw)))
        elif kind == "poly":
            closed, pts = op[1], op[2]
            seq = pts + ([pts[0]] if closed else [])
            for a, b in zip(seq, seq[1:]):
                seg(a, b)
    out = stretch_reduce(im, w_in * 9 // (8 * SS), h_in // SS)
    cropped, box = crop_nonzero(out)
    # offsets relative to the icon's top-left corner in bitmap pixels
    return cropped, box[0] - (pad * 9 // (8 * SS)), box[1] - pad // SS, size


# ---------------------------------------------------------------- palette
def hexrgb(v):
    return (v >> 16) & 255, (v >> 8) & 255, v & 255


def mix(a, b, t):
    ra, ga, ba = hexrgb(a)
    rb, gb, bb = hexrgb(b)
    return (round(ra + (rb - ra) * t) << 16) | (round(ga + (gb - ga) * t) << 8) | round(ba + (bb - ba) * t)


def gray_ramp(anchors, n):
    lum = lambda v: sum(c * w for c, w in zip(hexrgb(v), (0.299, 0.587, 0.114)))
    segs = [lum(b) - lum(a) for a, b in zip(anchors, anchors[1:])]
    total = sum(segs)
    want = [s / total * (n - 1) for s in segs]
    steps = [max(1, int(x)) for x in want]
    while sum(steps) < n - 1:              # largest remainder
        i = max(range(len(want)), key=lambda j: want[j] - steps[j])
        steps[i] += 1
    while sum(steps) > n - 1:
        i = max(range(len(want)), key=lambda j: steps[j] - want[j])
        steps[i] -= 1
    out = [anchors[0]]
    for (a, b), s in zip(zip(anchors, anchors[1:]), steps):
        out += [mix(a, b, (i + 1) / s) for i in range(s)]
    return out


BG = 0x0B0B0C
PALETTE = gray_ramp([0x0B0B0C, 0x151517, 0x1E1E21, 0x2A2A2E, 0x3A3A3F,
                     0x6A6A72, 0x9B9BA3, 0xC8C8CE, 0xF2F2F2], 32)
for accent in (0xC8F03C,):
    PALETTE += [accent] + [mix(BG, accent, t) for t in (0.2, 0.4, 0.6, 0.8)]
for signal in (0xFF5A4E, 0x7CC7FF):
    PALETTE += [signal] + [mix(BG, signal, t) for t in (0.33, 0.66)]


def yuv(v):
    r, g, b = hexrgb(v)
    y = 0.9216 * (0.299 * r + 0.587 * g + 0.114 * b)
    u = 0.79 * (-0.1687 * r - 0.3313 * g + 0.5 * b)
    w = 1.11 * (0.5 * r - 0.4187 * g - 0.0813 * b)
    y = max(0, min(255, round(y)))
    u = max(-128, min(127, round(u))) & 0xFF
    w = max(-128, min(127, round(w))) & 0xFF
    return (y << 16) | (u << 8) | w


# ---------------------------------------------------------------- C output
def c_bytes(nibbles):
    packed = [(nibbles[i] | ((nibbles[i + 1] if i + 1 < len(nibbles) else 0) << 4))
              for i in range(0, len(nibbles), 2)]
    lines = []
    for i in range(0, len(packed), 24):
        lines.append("    " + ",".join("0x%02x" % b for b in packed[i:i + 24]) + ",")
    return "\n".join(lines) if lines else "    0"


def main():
    h, c = [], []
    h.append("/* Generated by ui/tools/gen_assets.py - do not edit. */")
    h.append("#ifndef LAB_ASSETS_H\n#define LAB_ASSETS_H\n\n#include <stdint.h>\n")
    h.append("typedef struct {\n    uint16_t cp;        /* code point */\n"
             "    int8_t xoff, yoff;  /* bitmap top-left from pen position on the baseline */\n"
             "    uint8_t w, h;       /* bitmap size, bitmap pixels */\n"
             "    uint16_t adv;       /* advance, bitmap pixels * 256 */\n"
             "    uint32_t off;       /* first pixel in the font's 4-bit coverage data */\n} lab_glyph;\n")
    h.append("typedef struct {\n    const lab_glyph *glyphs;   /* sorted by code point */\n    int count;\n"
             "    const uint8_t *bits;       /* 4-bit coverage, two pixels per byte, low nibble first */\n"
             "    int ascent, descent, cap;  /* display pixels */\n} lab_font;\n")
    h.append("typedef struct {\n    uint8_t w, h;\n    int8_t xoff, yoff;\n    uint8_t size;   /* nominal size, display pixels */\n"
             "    const uint8_t *bits;\n} lab_icon;\n")
    c.append("/* Generated by ui/tools/gen_assets.py - do not edit.\n"
             " * Glyph shapes: Space Grotesk and JetBrains Mono, SIL Open Font License 1.1\n"
             " * (see ui/fonts/OFL-*.txt). */")
    c.append('#include "lab_assets.h"\n')
    total = 0
    for cname, path, weight, size in FONTS:
        glyphs, bits, asc, desc, cap = gen_font(cname, path, weight, size)
        glyphs.sort()
        total += len(bits) // 2
        h.append("extern const lab_font LAB_F_%s;" % cname)
        c.append("static const uint8_t %s_bits[] = {\n%s\n};" % (cname, c_bytes(bits)))
        c.append("static const lab_glyph %s_glyphs[] = {" % cname)
        for g in glyphs:
            c.append("    {0x%04x,%d,%d,%d,%d,%d,%d}," % g)
        c.append("};")
        c.append("const lab_font LAB_F_%s = {%s_glyphs, %d, %s_bits, %d, %d, %d};\n"
                 % (cname, cname, len(glyphs), cname, asc, desc, cap))
    h.append("")
    for name, ops in ICONS.items():
        bm, xoff, yoff, size = draw_icon(name, ops)
        nib = to_nibbles(bm)
        total += len(nib) // 2
        h.append("extern const lab_icon LAB_I_%s;" % name)
        c.append("static const uint8_t ICON_%s_bits[] = {\n%s\n};" % (name, c_bytes(nib)))
        c.append("const lab_icon LAB_I_%s = {%d, %d, %d, %d, %d, ICON_%s_bits};\n"
                 % (name, bm.size[0], bm.size[1], xoff, yoff, size, name))
    h.append("\n#define LAB_PAL_N %d" % len(PALETTE))
    h.append("extern const uint32_t LAB_PAL_RGB[LAB_PAL_N];  /* 0xRRGGBB */")
    h.append("extern const uint32_t LAB_PAL_YUV[LAB_PAL_N];  /* 0x00YYUUVV, display hardware format */")
    c.append("const uint32_t LAB_PAL_RGB[LAB_PAL_N] = {\n    " +
             ",".join("0x%06x" % v for v in PALETTE) + "\n};")
    c.append("const uint32_t LAB_PAL_YUV[LAB_PAL_N] = {\n    " +
             ",".join("0x%06x" % yuv(v) for v in PALETTE) + "\n};")
    h.append("\n#endif")
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "lab_assets.h"), "w") as f:
        f.write("\n".join(h) + "\n")
    with open(os.path.join(OUT_DIR, "lab_assets.c"), "w") as f:
        f.write("\n".join(c) + "\n")
    print("fonts: %d, icons: %d, palette: %d colours, glyph+icon data: %d KB"
          % (len(FONTS), len(ICONS), len(PALETTE), total // 1024))


if __name__ == "__main__":
    main()
