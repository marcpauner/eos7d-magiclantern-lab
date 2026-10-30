/* lab_gfx.h - drawing primitives for the 7D UI.
 *
 * Layout is done in display pixels: the 640x480 the eye sees on the 7D's 4:3
 * LCD. Magic Lantern draws into a 720x480 bitmap whose pixels are 9/8 as
 * narrow, so horizontal positions are scaled by 9/8 on the way in. Shapes are
 * evaluated in eighths of a bitmap pixel, which keeps that scaling exact and
 * gives anti-aliased edges and corners.
 *
 * Pixels are RGB565 while drawing; lab_quantize maps them to the palette in
 * lab_assets.c when a band is copied to the screen, so anti-aliasing can blend
 * any two colours. The screen is drawn in horizontal bands to keep memory low.
 *
 * Freestanding C99: no libc, no floating point.
 */
#ifndef LAB_GFX_H
#define LAB_GFX_H

#include <stdint.h>
#include "lab_assets.h"

#define LAB_BW 720              /* bitmap width */
#define LAB_BH 480              /* bitmap height */
#define LAB_DW 640              /* display width the layout is written in */

typedef struct {
    uint16_t *px;               /* LAB_BW * h pixels, RGB565 */
    int y0, h;                  /* bitmap rows y0 .. y0 + h - 1 */
} lab_band;

void lab_clear(lab_band *b, uint32_t rgb);
/* Filled rectangle with radius r (0 = square corners). Display pixels. */
void lab_round(lab_band *b, int x, int y, int w, int h, int r, uint32_t rgb);
/* Outline of thickness t (display pixels) along the inside of the shape. */
void lab_frame(lab_band *b, int x, int y, int w, int h, int r, int t, uint32_t rgb);
/* Filled circle / ring of diameter d centred at (cx, cy). */
void lab_disc(lab_band *b, int cx, int cy, int d, uint32_t rgb);
void lab_ring(lab_band *b, int cx, int cy, int d, int t, uint32_t rgb);
/* Polygon (display coordinates, n points), no anti-aliasing. stripe != 0 fills
 * only diagonal stripes |stripe| bitmap pixels wide, slanted by its sign. */
void lab_poly(lab_band *b, const int16_t *xy, int n, uint32_t rgb, int stripe);

/* UTF-8 text with its baseline at y. track = extra spacing per character in
 * tenths of a display pixel. Returns the width in display pixels. */
int lab_text(lab_band *b, const lab_font *f, int x, int y, const char *s, uint32_t rgb, int track);
int lab_text_w(const lab_font *f, const char *s, int track);
/* Icon with its nominal box at (x, y), display pixels. */
void lab_icon_draw(lab_band *b, const lab_icon *ic, int x, int y, uint32_t rgb);

/* Palette index (0 .. LAB_PAL_N-1) for every RGB555 value: lut must hold
 * LAB_LUT_SIZE bytes; fill it once with lab_build_lut, then look a pixel up
 * with lut[LAB_LUT_KEY(p)] (the lowest green bit is dropped). */
#define LAB_LUT_SIZE 32768
#define LAB_LUT_KEY(p) ((((p) >> 1) & 0x7FE0) | ((p) & 0x1F))
void lab_build_lut(uint8_t *lut);

#endif
