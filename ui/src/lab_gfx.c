/* lab_gfx.c - see lab_gfx.h. */
#include "lab_gfx.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static uint16_t rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

/* Blend src over dst with coverage a in 0..16. */
static uint16_t blend(uint16_t dst, uint16_t src, int a)
{
    int r, g, bl;
    if (a >= 16) return src;
    if (a <= 0) return dst;
    r  = (((dst >> 11) & 31) * (16 - a) + ((src >> 11) & 31) * a + 8) >> 4;
    g  = (((dst >> 5) & 63) * (16 - a) + ((src >> 5) & 63) * a + 8) >> 4;
    bl = ((dst & 31) * (16 - a) + (src & 31) * a + 8) >> 4;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void put(lab_band *b, int x, int y, uint16_t c, int a)
{
    uint16_t *p;
    if (x < 0 || x >= LAB_BW || y < b->y0 || y >= b->y0 + b->h) return;
    p = b->px + (y - b->y0) * LAB_BW + x;
    *p = blend(*p, c, a);
}

void lab_clear(lab_band *b, uint32_t rgb)
{
    uint16_t c = rgb565(rgb);
    int i, n = LAB_BW * b->h;
    for (i = 0; i < n; i++) b->px[i] = c;
}

/* ------------------------------------------------------------- shapes */

/* A rounded rectangle in eighths of a bitmap pixel. Display x maps to
 * bitmap eighths as x * 9, display y as y * 8; corner radii likewise. */
typedef struct { int x0, y0, x1, y1, rx, ry; } rr8;

static rr8 rr_make(int x, int y, int w, int h, int r)
{
    rr8 q;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    q.x0 = x * 9; q.x1 = (x + w) * 9;
    q.y0 = y * 8; q.y1 = (y + h) * 8;
    q.rx = r * 9; q.ry = r * 8;
    return q;
}

static int rr_inside(const rr8 *q, int sx, int sy)
{
    int cx, cy, dx, dy;
    if (sx < q->x0 || sx >= q->x1 || sy < q->y0 || sy >= q->y1) return 0;
    if (q->rx == 0 || q->ry == 0) return 1;
    if (sx >= q->x0 + q->rx && sx < q->x1 - q->rx) return 1;
    if (sy >= q->y0 + q->ry && sy < q->y1 - q->ry) return 1;
    cx = sx < q->x0 + q->rx ? q->x0 + q->rx : q->x1 - q->rx;
    cy = sy < q->y0 + q->ry ? q->y0 + q->ry : q->y1 - q->ry;
    dx = sx - cx;
    dy = sy - cy;
    /* radii are at most ~200 eighths, so this stays inside 32 bits */
    return dx * dx * q->ry * q->ry + dy * dy * q->rx * q->rx <= q->rx * q->rx * q->ry * q->ry;
}

/* Coverage of bitmap pixel (px, py) by q, 0..16 (4x4 samples). */
static int rr_cover(const rr8 *q, int px, int py)
{
    int X = px * 8, Y = py * 8, i, j, n = 0;
    if (X >= q->x0 && X + 8 <= q->x1 && Y >= q->y0 && Y + 8 <= q->y1 &&
        ((X >= q->x0 + q->rx && X + 8 <= q->x1 - q->rx) ||
         (Y >= q->y0 + q->ry && Y + 8 <= q->y1 - q->ry)))
        return 16;
    if (X + 8 <= q->x0 || X >= q->x1 || Y + 8 <= q->y0 || Y >= q->y1) return 0;
    for (j = 0; j < 4; j++)
        for (i = 0; i < 4; i++)
            n += rr_inside(q, X + 2 * i + 1, Y + 2 * j + 1);
    return n;
}

static void shape(lab_band *b, const rr8 *outer, const rr8 *inner, uint32_t rgb)
{
    uint16_t c = rgb565(rgb);
    int px0 = MAX(0, outer->x0 / 8), px1 = MIN(LAB_BW, (outer->x1 + 7) / 8);
    int py0 = MAX(b->y0, outer->y0 / 8), py1 = MIN(b->y0 + b->h, (outer->y1 + 7) / 8);
    int x, y;
    for (y = py0; y < py1; y++) {
        uint16_t *row = b->px + (y - b->y0) * LAB_BW;
        for (x = px0; x < px1; x++) {
            int a = rr_cover(outer, x, y);
            if (inner && a) a -= rr_cover(inner, x, y);
            if (a > 0) row[x] = blend(row[x], c, a);
        }
    }
}

void lab_round(lab_band *b, int x, int y, int w, int h, int r, uint32_t rgb)
{
    rr8 q;
    if (w <= 0 || h <= 0) return;
    q = rr_make(x, y, w, h, r);
    shape(b, &q, 0, rgb);
}

void lab_frame(lab_band *b, int x, int y, int w, int h, int r, int t, uint32_t rgb)
{
    rr8 o, i;
    if (w <= 0 || h <= 0) return;
    o = rr_make(x, y, w, h, r);
    i = rr_make(x + t, y + t, w - 2 * t, h - 2 * t, MAX(0, r - t));
    shape(b, &o, (w > 2 * t && h > 2 * t) ? &i : 0, rgb);
}

void lab_disc(lab_band *b, int cx, int cy, int d, uint32_t rgb)
{
    rr8 q;
    /* centre on half pixels: work in doubled units then convert */
    q.x0 = (cx * 2 - d) * 9 / 2; q.x1 = (cx * 2 + d) * 9 / 2;
    q.y0 = (cy * 2 - d) * 4;     q.y1 = (cy * 2 + d) * 4;
    q.rx = d * 9 / 2;            q.ry = d * 4;
    shape(b, &q, 0, rgb);
}

void lab_ring(lab_band *b, int cx, int cy, int d, int t, uint32_t rgb)
{
    rr8 o, i;
    int di = d - 2 * t;
    o.x0 = (cx * 2 - d) * 9 / 2; o.x1 = (cx * 2 + d) * 9 / 2;
    o.y0 = (cy * 2 - d) * 4;     o.y1 = (cy * 2 + d) * 4;
    o.rx = d * 9 / 2;            o.ry = d * 4;
    i.x0 = (cx * 2 - di) * 9 / 2; i.x1 = (cx * 2 + di) * 9 / 2;
    i.y0 = (cy * 2 - di) * 4;     i.y1 = (cy * 2 + di) * 4;
    i.rx = di * 9 / 2;            i.ry = di * 4;
    shape(b, &o, di > 0 ? &i : 0, rgb);
}

void lab_poly(lab_band *b, const int16_t *xy, int n, uint32_t rgb, int stripe)
{
    uint16_t c = rgb565(rgb);
    int y, k, ymin = 1 << 30, ymax = -(1 << 30);
    for (k = 0; k < n; k++) {
        ymin = MIN(ymin, xy[2 * k + 1]);
        ymax = MAX(ymax, xy[2 * k + 1]);
    }
    ymin = MAX(ymin, b->y0);
    ymax = MIN(ymax, b->y0 + b->h);
    for (y = ymin; y < ymax; y++) {
        int xs[16], m = 0, i, j;
        int sy = y * 2 + 1;                       /* pixel centre, doubled */
        for (k = 0; k < n && m < 16; k++) {
            int ax = xy[2 * k], ay = xy[2 * k + 1] * 2;
            int bx = xy[2 * ((k + 1) % n)], by = xy[2 * ((k + 1) % n) + 1] * 2;
            if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                /* crossing in bitmap eighths */
                int xd = ax * 9 + (bx - ax) * 9 * (sy - ay) / (by - ay);
                xs[m++] = xd;
            }
        }
        for (i = 1; i < m; i++)                    /* sort crossings */
            for (j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
                int t = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = t;
            }
        for (i = 0; i + 1 < m; i += 2) {
            int x0 = MAX(0, (xs[i] + 4) / 8), x1 = MIN(LAB_BW, (xs[i + 1] + 4) / 8), x;
            uint16_t *row = b->px + (y - b->y0) * LAB_BW;
            for (x = x0; x < x1; x++) {
                if (stripe) {
                    int s = stripe > 0 ? stripe : -stripe;
                    int d = stripe > 0 ? x + y : x - y + 4096;
                    if ((d / s) & 1) continue;
                }
                row[x] = c;
            }
        }
    }
}

/* -------------------------------------------------------------- text */

static uint32_t utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t cp;
    if (s[0] < 0x80) { cp = s[0]; *ps += 1; }
    else if ((s[0] & 0xE0) == 0xC0 && s[1]) { cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); *ps += 2; }
    else if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2]) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); *ps += 3;
    } else { cp = '?'; *ps += 1; }
    return cp;
}

static const lab_glyph *glyph(const lab_font *f, uint32_t cp)
{
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return cp == '?' ? 0 : glyph(f, '?');
}

/* tenths of a display pixel -> bitmap pixels * 256 */
#define TRACK256(t) ((t) * 9 * 256 / 80)

int lab_text_w(const lab_font *f, const char *s, int track)
{
    int pen = 0, n = 0;
    while (*s) {
        const lab_glyph *g = glyph(f, utf8_next(&s));
        if (g) pen += g->adv;
        n++;
    }
    if (n > 1) pen += TRACK256(track) * (n - 1);
    return (pen * 8 / 9 + 128) >> 8;              /* back to display pixels */
}

static void draw_bits(lab_band *b, const uint8_t *bits, uint32_t off, int x0, int y0, int w, int h, uint16_t c)
{
    int x, y;
    if (y0 + h <= b->y0 || y0 >= b->y0 + b->h) return;
    for (y = 0; y < h; y++) {
        int yy = y0 + y;
        if (yy < b->y0 || yy >= b->y0 + b->h) continue;
        for (x = 0; x < w; x++) {
            uint32_t i = off + (uint32_t)(y * w + x);
            int v = (bits[i >> 1] >> ((i & 1) * 4)) & 15;
            if (v) put(b, x0 + x, yy, c, (v * 16 + 7) / 15);
        }
    }
}

int lab_text(lab_band *b, const lab_font *f, int x, int y, const char *s, uint32_t rgb, int track)
{
    uint16_t c = rgb565(rgb);
    int pen = ((x * 9) << 8) / 8, start = pen, first = 1;
    while (*s) {
        const lab_glyph *g = glyph(f, utf8_next(&s));
        if (!first) pen += TRACK256(track);
        first = 0;
        if (!g) continue;
        if (g->w) draw_bits(b, f->bits, g->off, ((pen + 128) >> 8) + g->xoff, y + g->yoff, g->w, g->h, c);
        pen += g->adv;
    }
    return ((pen - start) * 8 / 9 + 128) >> 8;
}

void lab_icon_draw(lab_band *b, const lab_icon *ic, int x, int y, uint32_t rgb)
{
    draw_bits(b, ic->bits, 0, (x * 9 + 4) / 8 + ic->xoff, y + ic->yoff, ic->w, ic->h, rgb565(rgb));
}

/* ----------------------------------------------------------- palette */

void lab_build_lut(uint8_t *lut)
{
    int v, i;
    for (v = 0; v < LAB_LUT_SIZE; v++) {
        int r = ((v >> 10) & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, bl = (v & 31) * 255 / 31;
        int best = 0, bestd = 1 << 30;
        for (i = 0; i < LAB_PAL_N; i++) {
            int pr = (LAB_PAL_RGB[i] >> 16) & 255, pg = (LAB_PAL_RGB[i] >> 8) & 255, pb = LAB_PAL_RGB[i] & 255;
            int d = 2 * (r - pr) * (r - pr) + 4 * (g - pg) * (g - pg) + 3 * (bl - pb) * (bl - pb);
            if (d < bestd) { bestd = d; best = i; }
        }
        lut[v] = (uint8_t)best;
    }
}
