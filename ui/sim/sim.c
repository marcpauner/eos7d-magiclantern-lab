/* sim.c - run the 7D UI on the PC through the same code the camera runs.
 *
 * Draws in 48-row bands like the camera, quantises to the camera palette and
 * writes 720x480 PPMs (bitmap pixels, before the LCD squeezes them to 4:3).
 * Also drives key sequences against a fake settings store that keeps the
 * same ranges as Magic Lantern's menu entries, and checks the results.
 *
 * usage: sim OUTDIR
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lab_ui.h"

typedef struct { const char *menu, *entry; int v, lo, hi; } fake;

static fake F[] = {
    {"Overlay", "Global Draw", 3, 0, 3},   {"Overlay", "Zebras", 1, 0, 1},
    {"Zebras", "Color Space", 0, 0, 2},    {"Zebras", "Overexposure", 95, 0, 101},
    {"Zebras", "Underexposure", 5, 0, 20}, {"Overlay", "False color", 0, 0, 1},
    {"Overlay", "Histogram", 1, 0, 1},     {"Overlay", "Waveform", 0, 0, 1},
    {"Overlay", "Focus Peak", 1, 0, 1},    {"Overlay", "Magic Zoom", 1, 0, 1},
    {"Magic Zoom", "Trigger mode", 2, 1, 4}, {"Overlay", "Cropmarks", 0, 0, 1},
    {"Shoot", "Intervalometer", 1, 0, 1},
};

static fake *find(const char *m, const char *e)
{
    size_t i;
    for (i = 0; i < sizeof(F) / sizeof(F[0]); i++)
        if (!strcmp(F[i].menu, m) && !strcmp(F[i].entry, e)) return &F[i];
    return 0;
}

static int fget(const char *m, const char *e) { fake *f = find(m, e); return f ? f->v : LAB_MISSING; }

static int fset(const char *m, const char *e, int v)
{
    fake *f = find(m, e);
    if (!f || v < f->lo || v > f->hi) return 0;
    f->v = v;
    return 1;
}

static const lab_backend BE = {fget, fset};
static uint8_t lut[LAB_LUT_SIZE];
static uint16_t band_px[LAB_BW * 48];
static uint8_t frame[LAB_BW * LAB_BH * 3];
static const char *outdir;
static int failures;

static void render(lab_state *st, const char *name)
{
    lab_band b;
    char path[512];
    FILE *f;
    int y0, i;
    lab_refresh(st, &BE);
    b.px = band_px;
    b.h = 48;
    for (y0 = 0; y0 < LAB_BH; y0 += 48) {
        b.y0 = y0;
        lab_draw(&b, st);
        for (i = 0; i < LAB_BW * 48; i++) {
            uint32_t c = LAB_PAL_RGB[lut[LAB_LUT_KEY(band_px[i])]];
            uint8_t *p = frame + ((size_t)y0 * LAB_BW + i) * 3;
            p[0] = (uint8_t)(c >> 16); p[1] = (uint8_t)(c >> 8); p[2] = (uint8_t)c;
        }
    }
    snprintf(path, sizeof(path), "%s/%s.ppm", outdir, name);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", LAB_BW, LAB_BH);
    fwrite(frame, 1, sizeof(frame), f);
    fclose(f);
    printf("wrote %s\n", path);
}

static int press(lab_state *st, int key)
{
    int a;
    lab_refresh(st, &BE);
    a = lab_key(st, &BE, key);
    lab_refresh(st, &BE);
    return a;
}

#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
                              else printf("ok:   %s\n", msg); } while (0)

int main(int argc, char **argv)
{
    lab_state st;
    int a;
    outdir = argc > 1 ? argv[1] : ".";
    lab_build_lut(lut);

    /* screens, as in the design canvas */
    lab_init(&st);
    render(&st, "1_ajustes");
    st.screen = LAB_S_OVERLAY; st.focus[LAB_S_OVERLAY] = 5;
    render(&st, "2_superposiciones");
    st.screen = LAB_S_ZEBRAS; st.focus[LAB_S_ZEBRAS] = 2;
    render(&st, "3_zebras");
    st.screen = LAB_S_ROOT; st.focus[LAB_S_ROOT] = 9;
    render(&st, "4_ajustes_scroll");
    st.screen = LAB_S_OVERLAY; st.focus[LAB_S_OVERLAY] = 0;
    render(&st, "5_superposiciones_global");

    /* behaviour */
    lab_init(&st);
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.screen == LAB_S_OVERLAY, "SET on Superposiciones opens it");
    st.focus[LAB_S_OVERLAY] = 5;
    a = press(&st, LAB_K_SET);
    CHECK(a == LAB_A_REDRAW && fget("Overlay", "Focus Peak") == 0, "SET toggles Focus Peak off");
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Focus Peak") == 1, "SET toggles it back on");
    st.focus[LAB_S_OVERLAY] = 6;
    press(&st, LAB_K_RIGHT); press(&st, LAB_K_RIGHT); press(&st, LAB_K_RIGHT);
    CHECK(fget("Magic Zoom", "Trigger mode") == 1, "zoom trigger wraps 2 -> 3 -> 4 -> 1");
    st.focus[LAB_S_OVERLAY] = 0;
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Global Draw") == 0, "Global Draw chip turns overlays off");
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Global Draw") == 3, "and back on to the previous mode");
    st.focus[LAB_S_OVERLAY] = 1;
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.screen == LAB_S_ZEBRAS, "SET on Zebras opens its screen");
    st.focus[LAB_S_ZEBRAS] = 2;
    press(&st, LAB_K_RIGHT); press(&st, LAB_K_RIGHT);
    CHECK(fget("Zebras", "Overexposure") == 100, "highlights +5 clamps at 100");
    press(&st, LAB_K_DEC);
    CHECK(fget("Zebras", "Overexposure") == 99, "main dial -1");
    st.focus[LAB_S_ZEBRAS] = 3;
    { int i; for (i = 0; i < 30; i++) press(&st, LAB_K_RIGHT); }
    CHECK(fget("Zebras", "Underexposure") == 20, "shadows stop at 20");
    st.focus[LAB_S_ZEBRAS] = 1;
    press(&st, LAB_K_SET); press(&st, LAB_K_SET); press(&st, LAB_K_SET);
    CHECK(fget("Zebras", "Color Space") == 0, "mode cycles LUMA -> RGB -> RAPIDA -> LUMA");
    CHECK(press(&st, LAB_K_Q) == LAB_A_CLASSIC && !strcmp(st.classic_menu, "Overlay") &&
          !strcmp(st.classic_entry, "Zebras"), "Q opens the classic menu on the Zebras row");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_OVERLAY, "MENU goes back to Superposiciones");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_ROOT, "MENU goes back to Ajustes");
    CHECK(press(&st, LAB_K_BACK) == LAB_A_CLOSE, "MENU on Ajustes closes the UI");
    st.focus[LAB_S_ROOT] = 0;
    CHECK(press(&st, LAB_K_SET) == LAB_A_CLASSIC && !strcmp(st.classic_menu, "Expo"),
          "Exposicion opens the classic Expo tab");
    st.focus[LAB_S_ROOT] = 9;
    CHECK(press(&st, LAB_K_SET) == LAB_A_CLASSIC && st.classic_menu == 0, "Menu clasico keeps ML's own tab");
    {
        int i;
        st.focus[LAB_S_ROOT] = 0;
        for (i = 0; i < 20; i++) press(&st, LAB_K_DOWN);
        CHECK(st.focus[LAB_S_ROOT] == 9, "focus stops at the last row");
    }
    /* a camera without some entries must still draw */
    F[5].menu = "Nope";
    lab_init(&st);
    st.screen = LAB_S_OVERLAY;
    render(&st, "6_missing_entry");
    printf(failures ? "\n%d FAILED\n" : "\nall checks passed\n", failures);
    return failures != 0;
}
