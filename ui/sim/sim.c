/* sim.c - run the 7D UI on the PC through the same code the camera runs.
 *
 * Draws in 48-row bands like the camera, quantises to the camera palette and
 * writes 720x480 PPMs (bitmap pixels, before the LCD squeezes them to 4:3).
 * Also drives key sequences against fake settings: a (menu, entry) table
 * with Magic Lantern's ranges for the designed screens, and fake menus with
 * the same kinds of rows ML has (values, toggles, submenus, actions,
 * warnings) for the generic screens. Checks the results.
 *
 * usage: sim OUTDIR
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lab_ui.h"

/* ------------------------------------------- designed screens' settings */

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

/* --------------------------------------------------------- fake menus */

typedef struct fmenu fmenu;
typedef struct {
    const char *name;
    int kind;                  /* LAB_IT_* */
    int v, lo, hi;             /* hi < lo: no value (pure submenu / action) */
    const char **choices;
    fmenu *sub;
    const char *help, *warning;
    const char *top;           /* top-level entry for items inside submenus */
} fitem;
struct fmenu { const char *name; fitem *it; int n; };

#define C(...) (const char *[]){__VA_ARGS__}
static fitem ettr_items[] = {
    {"Trigger mode", LAB_IT_VALUE, 1, 0, 3, C("Always ON", "Auto Snap", "Press SET", "SET+HalfS"), 0,
     "When to meter for ETTR.", 0, "Auto ETTR"},
    {"Highlight ignore", LAB_IT_VALUE, 2, 0, 50, 0, 0, "Percent of highlights allowed to clip.", 0, "Auto ETTR"},
    {"Allow clipping", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, "Let specular highlights clip.", 0, "Auto ETTR"},
};
static fitem raw_items[] = {
    {"Resolution", LAB_IT_VALUE, 2, 0, 4, C("1280", "1600", "1728", "1920", "2048"), 0, 0, 0, "RAW video"},
    {"Preview", LAB_IT_VALUE, 0, 0, 2, C("Auto", "Canon", "ML Grayscale"), 0, 0, 0, "RAW video"},
};
static fmenu ettr_menu = {"Auto ETTR", ettr_items, 3};
static fmenu raw_menu = {"RAW video", raw_items, 2};
static fmenu empty_menu = {"Empty", 0, 0};

static fitem expo_items[] = {
    {"ISO", LAB_IT_VALUE, 3, 0, 5, C("Auto", "100", "200", "400", "800", "1600"), 0, "Sensor sensitivity.", 0, 0},
    {"Shutter", LAB_IT_VALUE, 1, 0, 2, C("1/60", "1/125", "1/250"), 0, "Exposure time.", 0, 0},
    {"Auto ETTR", LAB_IT_SUBMENU, 0, 0, 1, 0, &ettr_menu, "Expose to the right automatically.", 0, 0},
    {"Dual ISO", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, "Two ISOs in one picture.", "Dual ISO needs photo mode.", 0},
    {"Expo. Lock", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, "Keep exposure when changing ISO.", 0, 0},
    {"Exposure Presets", LAB_IT_ACTION, 0, 0, -1, 0, 0, "Opens the preset dialog.", 0, 0},
};
static fitem overlay_items[] = {
    {"Global Draw", LAB_IT_VALUE, 3, 0, 3, C("OFF", "LiveView", "QuickReview", "ON, all modes"), 0, 0, 0, 0},
    {"Zebras", LAB_IT_SUBMENU, 1, 0, 1, 0, &empty_menu, 0, 0, 0},
    {"Focus Peak", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, 0, 0, 0},
    {"Magic Zoom", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, 0, 0, 0},
    {"Cropmarks", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Ghost image", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, "Transparent overlay of a previous picture.", 0, 0},
    {"Spotmeter", LAB_IT_VALUE, 1, 0, 2, C("OFF", "Percent", "IRE"), 0, 0, 0, 0},
    {"False color", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Histogram", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, 0, 0, 0},
    {"Waveform", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Vectorscope", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
};
static fitem movie_items[] = {
    {"Bit Rate", LAB_IT_VALUE, 0, 0, 2, C("CBR 1.0x", "CBR 1.5x", "VBR -16"), 0, 0, 0, 0},
    {"RAW video", LAB_IT_SUBMENU, 0, 0, 1, 0, &raw_menu, "Record 14-bit RAW video.", 0, 0},
    {"Movie Logging", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
};
static fitem shoot_items[] = {
    {"Intervalometer", LAB_IT_SUBMENU, 1, 0, 1, 0, &empty_menu, 0, 0, 0},
    {"Bulb Timer", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Silent Picture", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
};
static fitem focus_items[] = {{"Trap Focus", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0}};
static fitem display_items[] = {{"Clear overlays", LAB_IT_VALUE, 0, 0, 1, C("OFF", "HalfShutter"), 0, 0, 0, 0}};
static fitem audio_items[] = {{"Audio Meters", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, 0, 0, 0}};
static fitem prefs_items[] = {
    {"Lab UI", LAB_IT_TOGGLE, 1, 0, 1, 0, 0, "Trash opens the redesigned menu (Ajustes).", 0, 0},
    {"Powersave in LiveView", LAB_IT_SUBMENU, -1, 0, -1, 0, &empty_menu, 0, 0, 0},
};
static fitem modules_items[] = {
    {"soup", LAB_IT_VALUE, 0, 0, -1, C("Loaded"), 0, "Film looks for CR2s.", 0, 0},
    {"labui", LAB_IT_VALUE, 0, 0, -1, C("Loaded"), 0, 0, 0, 0},
    {"dual_iso", LAB_IT_VALUE, 0, 0, -1, C("OFF"), 0, 0, 0, 0},
};
static fitem debug_items[] = {
    {"Dump ROM and RAM", LAB_IT_ACTION, 0, 0, -1, 0, 0, "ROM0.BIN:F0000000, ROM1.BIN:F8000000, RAM4.BIN", 0, 0},
    {"Show tasks", LAB_IT_ACTION, 0, 0, -1, 0, 0, 0, 0, 0},
    {"An unusually long debug entry name that cannot fit on one row", LAB_IT_VALUE, 0, 0, 0,
     C("and an equally long value string here"), 0,
     "A help line that is much longer than the screen can show, so it has to be shortened on the right end.", 0, 0},
};
static fitem help_items[] = {{"About Magic Lantern", LAB_IT_ACTION, 0, 0, -1, 0, 0, 0, 0, 0}};
static fitem film_items[] = {
    {"Film", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, "Off leaves every file alone.", 0, 0},
    {"Delete CR2", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Rewrite CR2", LAB_IT_TOGGLE, 0, 0, 1, 0, 0, 0, 0, 0},
    {"Stock", LAB_IT_VALUE, 12, 0, 12, C("Acros", "Astia", "Bleach Bypass", "Classic Chrome", "Classic Neg",
     "Eterna", "Gold", "Nostalgic Neg", "Portra", "Provia", "Real Ace", "Soup 800", "Velvia"), 0,
     "Look applied when developing. From ML/FILM.", 0, 0},
};

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))
static fmenu TABS[] = {          /* Magic Lantern's order */
    {"Audio", audio_items, N(audio_items)},   {"Expo", expo_items, N(expo_items)},
    {"Overlay", overlay_items, N(overlay_items)}, {"Movie", movie_items, N(movie_items)},
    {"Shoot", shoot_items, N(shoot_items)},   {"Focus", focus_items, N(focus_items)},
    {"Display", display_items, N(display_items)}, {"Prefs", prefs_items, N(prefs_items)},
    {"Modules", modules_items, N(modules_items)}, {"Debug", debug_items, N(debug_items)},
    {"Help", help_items, N(help_items)},      {"Film", film_items, N(film_items)},
};

static const char *tab_of_item(const fitem *f)
{
    int t, i;
    for (t = 0; t < N(TABS); t++)
        for (i = 0; i < TABS[t].n; i++)
            if (&TABS[t].it[i] == f) return TABS[t].name;
    for (t = 0; t < N(TABS); t++)
        for (i = 0; i < TABS[t].n; i++)
            if (TABS[t].it[i].sub && f->top && !strcmp(TABS[t].it[i].name, f->top)) return TABS[t].name;
    return 0;
}

static int ftabs(void **h, const char **names, int max)
{
    int i, n = 0;
    for (i = 0; i < N(TABS) && n < max; i++) { h[n] = &TABS[i]; names[n] = TABS[i].name; n++; }
    return n;
}

static int fitems(void *menu, lab_item *out, int max, int focus, char *help, int *warn)
{
    fmenu *m = menu;
    int i;
    for (i = 0; i < m->n && i < max; i++) {
        fitem *f = &m->it[i];
        lab_item *o = &out[i];
        snprintf(o->name, sizeof(o->name), "%s", f->name);
        o->value[0] = 0;
        if (f->choices && f->hi >= f->lo) snprintf(o->value, sizeof(o->value), "%s", f->choices[f->v - f->lo]);
        else if (f->choices) snprintf(o->value, sizeof(o->value), "%s", f->choices[0]);
        else if (f->kind == LAB_IT_VALUE && f->hi > f->lo) snprintf(o->value, sizeof(o->value), "%d.%d%%", f->v / 10, f->v % 10);
        o->kind = (uint8_t)f->kind;
        o->state = (int8_t)((f->kind == LAB_IT_TOGGLE || (f->kind == LAB_IT_SUBMENU && f->hi == 1)) ? f->v != 0 : -1);
        o->dim = f->warning != 0;
        o->h = f;
        if (i == focus) {
            if (f->warning) { snprintf(help, LAB_HELP_LEN, "%s", f->warning); *warn = 1; }
            else if (f->help) snprintf(help, LAB_HELP_LEN, "%s", f->help);
        }
    }
    return i;
}

static void *fsubmenu(void *h) { return ((fitem *)h)->sub; }

static void fchange(void *h, int how)
{
    fitem *f = h;
    int d = how == LAB_CH_SET ? 1 : how, span = f->hi - f->lo + 1;
    if (f->hi < f->lo) return;
    f->v = f->lo + ((f->v - f->lo + d) % span + span) % span;
}

static void fwhere(void *h, const char **menu, const char **entry)
{
    fitem *f = h;
    *menu = tab_of_item(f);
    *entry = f->top ? f->top : f->name;
}

static const lab_backend BE = {fget, fset, ftabs, fitems, fsubmenu, fchange, fwhere};

/* ------------------------------------------------------------- driver */

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

static void presses(lab_state *st, int key, int n) { while (n-- > 0) press(st, key); }

#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
                              else printf("ok:   %s\n", msg); } while (0)

static void open_root_row(lab_state *st, const char *ml)
{
    int i;
    lab_refresh(st, &BE);
    st->screen = LAB_S_ROOT;
    for (i = 0; i < st->n_root; i++)
        if (!strcmp(st->root_ml[i], ml)) st->focus[LAB_S_ROOT] = i;
    press(st, LAB_K_SET);
}

int main(int argc, char **argv)
{
    lab_state st;
    int a;
    outdir = argc > 1 ? argv[1] : ".";
    lab_build_lut(lut);

    /* ---- designed screens */
    lab_init(&st);
    render(&st, "01_ajustes");
    st.screen = LAB_S_OVERLAY; st.focus[LAB_S_OVERLAY] = 5;
    render(&st, "02_superposiciones");
    st.screen = LAB_S_ZEBRAS; st.focus[LAB_S_ZEBRAS] = 2;
    render(&st, "03_zebras");
    lab_refresh(&st, &BE);
    st.screen = LAB_S_ROOT; st.focus[LAB_S_ROOT] = st.n_root;
    render(&st, "04_ajustes_final");

    /* ---- generic screens */
    lab_init(&st);
    open_root_row(&st, "Expo");
    render(&st, "05_exposicion");
    st.gfocus[0] = 3;
    render(&st, "06_exposicion_aviso");
    open_root_row(&st, "Debug");
    render(&st, "07_depuracion");
    open_root_row(&st, "Film");
    st.gfocus[0] = 3;
    render(&st, "08_film_soup");
    lab_init(&st);
    open_root_row(&st, "Expo");
    st.gfocus[0] = 2;
    press(&st, LAB_K_SET);
    render(&st, "09_auto_ettr");

    /* ---- behaviour: designed screens */
    lab_init(&st);
    lab_refresh(&st, &BE);
    CHECK(st.n_root == 12 && !strcmp(st.root_ml[0], "Expo") && !strcmp(st.root_ml[1], "Overlay") &&
          !strcmp(st.root_ml[11], "Film"), "root: known menus in design order, module menus last");
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.screen == LAB_S_OVERLAY, "SET on Superposiciones opens it");
    st.focus[LAB_S_OVERLAY] = 5;
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Focus Peak") == 0, "SET toggles Focus Peak off");
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Focus Peak") == 1, "SET toggles it back on");
    st.focus[LAB_S_OVERLAY] = 6;
    presses(&st, LAB_K_RIGHT, 3);
    CHECK(fget("Magic Zoom", "Trigger mode") == 1, "zoom trigger wraps 2 -> 3 -> 4 -> 1");
    st.focus[LAB_S_OVERLAY] = 0;
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Global Draw") == 0, "Global Draw chip turns overlays off");
    press(&st, LAB_K_SET);
    CHECK(fget("Overlay", "Global Draw") == 3, "and back on to the previous mode");
    st.focus[LAB_S_OVERLAY] = 1;
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.screen == LAB_S_ZEBRAS, "SET on Zebras opens its screen");
    st.focus[LAB_S_ZEBRAS] = 2;
    presses(&st, LAB_K_RIGHT, 2);
    CHECK(fget("Zebras", "Overexposure") == 100, "highlights +5 clamps at 100");
    press(&st, LAB_K_DEC);
    CHECK(fget("Zebras", "Overexposure") == 99, "main dial -1");
    st.focus[LAB_S_ZEBRAS] = 3;
    presses(&st, LAB_K_RIGHT, 30);
    CHECK(fget("Zebras", "Underexposure") == 20, "shadows stop at 20");
    st.focus[LAB_S_ZEBRAS] = 1;
    presses(&st, LAB_K_SET, 3);
    CHECK(fget("Zebras", "Color Space") == 0, "mode cycles LUMA -> RGB -> RAPIDA -> LUMA");
    CHECK(press(&st, LAB_K_Q) == LAB_A_CLASSIC && !strcmp(st.classic_menu, "Overlay") &&
          !strcmp(st.classic_entry, "Zebras"), "Q opens the classic menu on the Zebras row");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_OVERLAY, "MENU goes back to Superposiciones");
    st.focus[LAB_S_OVERLAY] = 8;
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.screen == LAB_S_MENU && st.n_items == N(overlay_items),
          "'Todas las opciones' lists the whole Overlay menu");
    render(&st, "10_superposiciones_todas");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_OVERLAY, "and MENU returns to Superposiciones");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_ROOT, "MENU goes back to Ajustes");
    CHECK(press(&st, LAB_K_BACK) == LAB_A_CLOSE, "MENU on Ajustes closes the UI");

    /* ---- behaviour: generic screens */
    lab_init(&st);
    open_root_row(&st, "Expo");
    CHECK(st.screen == LAB_S_MENU && st.depth == 1 && !strcmp(st.title[0], "Exposición") &&
          st.n_items == N(expo_items), "Exposicion opens as a Lab screen with all its rows");
    press(&st, LAB_K_RIGHT);
    CHECK(expo_items[0].v == 4, "right changes ISO 400 -> 800");
    press(&st, LAB_K_DEC);
    CHECK(expo_items[0].v == 3, "main dial back to 400");
    presses(&st, LAB_K_DOWN, 2);
    CHECK(press(&st, LAB_K_SET) == LAB_A_REDRAW && st.depth == 2 && !strcmp(st.title[1], "Auto ETTR"),
          "SET on a submenu row opens it");
    press(&st, LAB_K_RIGHT);
    CHECK(ettr_items[0].v == 2, "values change inside the submenu");
    press(&st, LAB_K_DOWN); press(&st, LAB_K_DOWN);
    press(&st, LAB_K_SET);
    CHECK(ettr_items[2].v == 1, "SET toggles a toggle row");
    a = press(&st, LAB_K_Q);
    CHECK(a == LAB_A_CLASSIC && !strcmp(st.classic_menu, "Expo") && !strcmp(st.classic_entry, "Auto ETTR"),
          "Q in a submenu lands on its top-level entry in the classic menu");
    press(&st, LAB_K_BACK);
    CHECK(st.depth == 1 && st.gfocus[0] == 2, "MENU goes up one level, focus kept");
    press(&st, LAB_K_BACK);
    CHECK(st.screen == LAB_S_ROOT, "and up to Ajustes");
    open_root_row(&st, "Debug");
    a = press(&st, LAB_K_SET);
    CHECK(a == LAB_A_CLASSIC && !strcmp(st.classic_menu, "Debug") && !strcmp(st.classic_entry, "Dump ROM and RAM"),
          "actions open in the classic menu");
    open_root_row(&st, "Film");
    press(&st, LAB_K_SET);
    CHECK(film_items[0].v == 1, "a module's own menu (Soup's Film) works too");
    presses(&st, LAB_K_DOWN, 50);
    CHECK(st.gfocus[0] == N(film_items) - 1, "focus stops at the last row");
    open_root_row(&st, "Prefs");
    press(&st, LAB_K_DOWN);
    press(&st, LAB_K_SET);
    CHECK(st.depth == 2 && st.n_items == 0, "an empty submenu opens without rows");
    render(&st, "11_submenu_vacio");
    st.screen = LAB_S_ROOT;
    st.focus[LAB_S_ROOT] = st.n_root;
    CHECK(press(&st, LAB_K_SET) == LAB_A_CLASSIC && st.classic_menu == 0, "Menu clasico keeps ML's own tab");

    /* a camera without some designed-screen entries must still draw */
    F[5].menu = "Nope";
    lab_init(&st);
    st.screen = LAB_S_OVERLAY;
    render(&st, "12_missing_entry");
    printf(failures ? "\n%d FAILED\n" : "\nall checks passed\n", failures);
    return failures != 0;
}
