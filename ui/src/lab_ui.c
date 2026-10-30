/* lab_ui.c - see lab_ui.h. Layout numbers are display pixels taken from the
 * design canvas (the .dc.html artboards in ui-design/project). */
#include "lab_ui.h"

/* colours */
#define C_BG     0x0B0B0C
#define C_CHIP   0x1E1E21
#define C_LINE   0x2A2A2E
#define C_BORDER 0x3A3A3F
#define C_DECO   0x6A6A72
#define C_MUTED  0x9B9BA3
#define C_TEXT   0xF2F2F2
#define C_INK    0x0B0B0C          /* text on the accent */
#define C_CORAL  0xFF5A4E
#define C_ICE    0x7CC7FF

#define LIST_X   12                /* focus pill */
#define LIST_W   616
#define PAD_X    24                /* text start */
#define RIGHT    616               /* text end */
#define ROW_H    40
#define LEGEND_Y 450

/* values cached by lab_refresh */
enum { V_GD, V_ZEBRA, V_ZCS, V_ZHI, V_ZLO, V_FALSE, V_HIST, V_WAVE, V_PEAK,
       V_MZ, V_MZMODE, V_CROP, V_INTERVAL, V_COUNT };

typedef struct { const char *menu, *entry; } ref;
static const ref REFS[V_COUNT] = {
    {"Overlay", "Global Draw"}, {"Overlay", "Zebras"}, {"Zebras", "Color Space"},
    {"Zebras", "Overexposure"}, {"Zebras", "Underexposure"}, {"Overlay", "False color"},
    {"Overlay", "Histogram"}, {"Overlay", "Waveform"}, {"Overlay", "Focus Peak"},
    {"Overlay", "Magic Zoom"}, {"Magic Zoom", "Trigger mode"}, {"Overlay", "Cropmarks"},
    {"Shoot", "Intervalometer"},
};

/* ------------------------------------------------------------ screens */

typedef struct {
    const char *label;
    const lab_icon *icon;
    const char *menu;          /* classic ML menu; 0 = native screen */
    int section;
} root_row;

static const root_row ROOT[] = {
    {"Exposición",      &LAB_I_EXPO,    "Expo",    0},
    {"Superposiciones", &LAB_I_OVERLAY, 0,         0},
    {"Enfoque",         &LAB_I_FOCUS,   "Focus",   0},
    {"Disparo",         &LAB_I_SHOOT,   "Shoot",   0},
    {"Vídeo",           &LAB_I_VIDEO,   "Movie",   1},
    {"Audio",           &LAB_I_AUDIO,   "Audio",   1},
    {"Pantalla",        &LAB_I_DISPLAY, "Display", 1},
    {"Módulos",         &LAB_I_MODULES, "Modules", 2},
    {"Preferencias",    &LAB_I_PREFS,   "Prefs",   2},
    {"Menú clásico",    &LAB_I_CLASSIC, "",        2},
};
#define ROOT_N ((int)(sizeof(ROOT) / sizeof(ROOT[0])))
static const char *ROOT_SECTIONS[] = {"01 — CAPTURA", "02 — SALIDA", "03 — SISTEMA"};

enum { R_LINK, R_TOGGLE, R_ZOOM };
typedef struct {
    const char *label, *entry;
    int kind, value, section;
    const char *help;
} ov_row;

/* focus 0 is the Global Draw chip in the header; rows are 1..OV_N */
static const ov_row OV[] = {
    {"Zebras",             "Zebras",      R_LINK,   V_ZEBRA, 0,
     "Raya las zonas a punto de quemarse o empastarse. SET para ajustar."},
    {"Falso color",        "False color", R_TOGGLE, V_FALSE, 0,
     "Colorea la imagen por niveles de exposición."},
    {"Histograma",         "Histogram",   R_TOGGLE, V_HIST,  0,
     "Reparto de luces y sombras de la escena."},
    {"Forma de onda",      "Waveform",    R_TOGGLE, V_WAVE,  0,
     "Brillo de cada columna de la imagen, útil en vídeo."},
    {"Focus peaking",      "Focus Peak",  R_TOGGLE, V_PEAK,  1,
     "Resalta los bordes nítidos en la imagen en directo para enfocar a mano."},
    {"Zoom de enfoque",    "Magic Zoom",  R_ZOOM,   V_MZ,    1,
     "Ventana ampliada para comprobar el enfoque. ◀▶ cambia cuándo aparece."},
    {"Marcas de encuadre", "Cropmarks",   R_TOGGLE, V_CROP,  1,
     "Guías de encuadre superpuestas. Q elige el tipo en el menú clásico."},
};
#define OV_N ((int)(sizeof(OV) / sizeof(OV[0])))
static const char *OV_SECTIONS[] = {"01 — EXPOSICIÓN", "02 — ENFOQUE"};
static const char *GD_HELP = "Muestra u oculta todas las superposiciones (Global Draw).";

#define ZB_N 4                     /* chip, mode, highlights, shadows */
static const char *ZCS_NAMES[] = {"LUMA", "RGB", "RÁPIDA"};
static const char *MZ_NAMES[] = {"MEDIO DISPARO", "ANILLO", "ANILLO+DISPARO", "SIEMPRE"};

/* ------------------------------------------------------------- helpers */

static char *put_str(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

static char *put_int(char *p, int v)
{
    char t[12];
    int n = 0;
    if (v < 0) { *p++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

static int on(const lab_state *st, int v) { return st->v[v] != LAB_MISSING && st->v[v] != 0; }
static int missing(const lab_state *st, int v) { return st->v[v] == LAB_MISSING; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void text_right(lab_band *b, const lab_font *f, int right, int y, const char *s, uint32_t c, int track)
{
    lab_text(b, f, right - lab_text_w(f, s, track), y, s, c, track);
}

/* "02 / 10" */
static void counter2(char *p, int a, int n)
{
    if (a < 10) *p++ = '0';
    p = put_str(put_int(p, a), " / ");
    if (n < 10) *p++ = '0';
    put_int(p, n);
}

/* ----------------------------------------------------------- refresh */

void lab_init(lab_state *st)
{
    int i;
    for (i = 0; i < LAB_S_COUNT; i++) { st->focus[i] = 0; st->scroll[i] = 0; }
    st->screen = LAB_S_ROOT;
    st->focus[LAB_S_ROOT] = 1;            /* Superposiciones: the native screen */
    st->focus[LAB_S_OVERLAY] = 1;
    st->focus[LAB_S_ZEBRAS] = 2;
    st->gd_last = 3;                      /* ML's default: ON, all modes */
    st->classic_menu = 0;
    st->classic_entry = 0;
    st->accent = 0xC8F03C;
    for (i = 0; i < V_COUNT; i++) st->v[i] = LAB_MISSING;
}

void lab_refresh(lab_state *st, const lab_backend *be)
{
    int i;
    for (i = 0; i < V_COUNT; i++) st->v[i] = be->get(REFS[i].menu, REFS[i].entry);
    if (on(st, V_GD)) st->gd_last = st->v[V_GD];
}

/* -------------------------------------------------------------- keys */

static void toggle(lab_state *st, const lab_backend *be, int v)
{
    if (missing(st, v)) return;
    if (v == V_GD) {
        be->set(REFS[v].menu, REFS[v].entry, st->v[v] ? 0 : (st->gd_last ? st->gd_last : 3));
        return;
    }
    be->set(REFS[v].menu, REFS[v].entry, st->v[v] ? 0 : 1);
}

static void step(lab_state *st, const lab_backend *be, int v, int d, int lo, int hi, int wrap)
{
    int x;
    if (missing(st, v)) return;
    x = st->v[v] + d;
    if (wrap) x = lo + ((x - lo) % (hi - lo + 1) + (hi - lo + 1)) % (hi - lo + 1);
    else x = clampi(x, lo, hi);
    if (x != st->v[v]) be->set(REFS[v].menu, REFS[v].entry, x);
}

static int classic(lab_state *st, const char *menu, const char *entry)
{
    st->classic_menu = menu;
    st->classic_entry = entry;
    return LAB_A_CLASSIC;
}

static int key_root(lab_state *st, int key)
{
    int *f = &st->focus[LAB_S_ROOT];
    switch (key) {
    case LAB_K_UP: case LAB_K_DEC:   if (*f > 0) (*f)--; return LAB_A_REDRAW;
    case LAB_K_DOWN: case LAB_K_INC: if (*f < ROOT_N - 1) (*f)++; return LAB_A_REDRAW;
    case LAB_K_SET: case LAB_K_RIGHT:
        if (!ROOT[*f].menu) { st->screen = LAB_S_OVERLAY; return LAB_A_REDRAW; }
        return classic(st, ROOT[*f].menu[0] ? ROOT[*f].menu : 0, "");
    case LAB_K_BACK: return LAB_A_CLOSE;
    case LAB_K_Q:    return classic(st, 0, 0);
    }
    return LAB_A_NONE;
}

static int key_overlay(lab_state *st, const lab_backend *be, int key)
{
    int *f = &st->focus[LAB_S_OVERLAY];
    const ov_row *r = *f ? &OV[*f - 1] : 0;
    switch (key) {
    case LAB_K_UP:   if (*f > 0) (*f)--; return LAB_A_REDRAW;
    case LAB_K_DOWN: if (*f < OV_N) (*f)++; return LAB_A_REDRAW;
    case LAB_K_BACK: st->screen = LAB_S_ROOT; return LAB_A_REDRAW;
    case LAB_K_Q:    return classic(st, "Overlay", r ? r->entry : "Global Draw");
    case LAB_K_SET:
        if (!r) toggle(st, be, V_GD);
        else if (r->kind == R_LINK) st->screen = LAB_S_ZEBRAS;
        else toggle(st, be, r->value);
        return LAB_A_REDRAW;
    case LAB_K_LEFT: case LAB_K_RIGHT: case LAB_K_DEC: case LAB_K_INC: {
        int d = (key == LAB_K_LEFT || key == LAB_K_DEC) ? -1 : 1;
        if (!r) toggle(st, be, V_GD);
        else if (r->kind == R_TOGGLE) toggle(st, be, r->value);
        else if (r->kind == R_ZOOM) step(st, be, V_MZMODE, d, 1, 4, 1);
        else if (d > 0) st->screen = LAB_S_ZEBRAS;
        return LAB_A_REDRAW;
    }
    }
    return LAB_A_NONE;
}

static int key_zebras(lab_state *st, const lab_backend *be, int key)
{
    int *f = &st->focus[LAB_S_ZEBRAS];
    int d = 0;
    switch (key) {
    case LAB_K_UP:   if (*f > 0) (*f)--; return LAB_A_REDRAW;
    case LAB_K_DOWN: if (*f < ZB_N - 1) (*f)++; return LAB_A_REDRAW;
    case LAB_K_BACK: st->screen = LAB_S_OVERLAY; return LAB_A_REDRAW;
    /* the classic menu cannot land inside a submenu: select the Zebras row */
    case LAB_K_Q:    return classic(st, "Overlay", "Zebras");
    case LAB_K_SET:
        if (*f == 0) toggle(st, be, V_ZEBRA);
        else if (*f == 1) step(st, be, V_ZCS, 1, 0, 2, 1);
        return LAB_A_REDRAW;
    case LAB_K_LEFT: d = -5; break;
    case LAB_K_RIGHT: d = 5; break;
    case LAB_K_DEC: d = -1; break;
    case LAB_K_INC: d = 1; break;
    default: return LAB_A_NONE;
    }
    if (*f == 0) toggle(st, be, V_ZEBRA);
    else if (*f == 1) step(st, be, V_ZCS, d > 0 ? 1 : -1, 0, 2, 1);
    else if (*f == 2) step(st, be, V_ZHI, d, 1, 100, 0);
    else step(st, be, V_ZLO, d > 0 ? 1 : -1, 0, 20, 0);
    return LAB_A_REDRAW;
}

int lab_key(lab_state *st, const lab_backend *be, int key)
{
    switch (st->screen) {
    case LAB_S_ROOT:    return key_root(st, key);
    case LAB_S_OVERLAY: return key_overlay(st, be, key);
    case LAB_S_ZEBRAS:  return key_zebras(st, be, key);
    }
    return LAB_A_NONE;
}

/* ---------------------------------------------------------- drawing */

static void header(lab_band *b, lab_state *st, const char *const *crumbs, int n, int title_big,
                   const char *title, const char *counter)
{
    int x = 56, i;
    int y = title_big ? 16 : 14;
    lab_round(b, PAD_X, y, 24, 16, 4, st->accent);
    lab_text(b, &LAB_F_MONO9B, PAD_X + (24 - lab_text_w(&LAB_F_MONO9B, "ML", 0)) / 2, y + 11, "ML", C_INK, 0);
    for (i = 0; i < n; i++) {
        if (i) x += lab_text(b, &LAB_F_MONO11, x, y + 12, " / ", C_MUTED, 10);
        x += lab_text(b, &LAB_F_MONO11, x, y + 12, crumbs[i], i == n - 1 && n > 1 ? C_TEXT : C_MUTED, 10);
    }
    if (counter) text_right(b, &LAB_F_MONO11, RIGHT, y + 12, counter, C_MUTED, 10);
    if (title_big) lab_text(b, &LAB_F_TITLE30, PAD_X, 64, title, C_TEXT, -8);
    else lab_text(b, &LAB_F_TITLE28, PAD_X, 60, title, C_TEXT, -7);
}

static void legend(lab_band *b, const char *const *keys, const char *const *labels, int n)
{
    int x = PAD_X, i;
    if (b->y0 + b->h <= LEGEND_Y) return;
    lab_round(b, 0, LEGEND_Y, LAB_DW, LAB_BH - LEGEND_Y, 0, C_BG);
    lab_round(b, 0, LEGEND_Y, LAB_DW, 1, 0, C_CHIP);
    for (i = 0; i < n; i++) {
        int tw = lab_text_w(&LAB_F_MONO10B, keys[i], 0), cw = tw + 12;
        lab_frame(b, x, 457, cw, 16, 4, 1, C_BORDER);
        lab_text(b, &LAB_F_MONO10B, x + 6, 469, keys[i], C_TEXT, 0);
        x += cw + 6;
        x += lab_text(b, &LAB_F_MONO11, x, 469, labels[i], C_MUTED, 0) + 14;
    }
    text_right(b, &LAB_F_MONO11, RIGHT, 469, "ML lua_fix · 7D 2.0.3", C_MUTED, 0);
}

static int section_header(lab_band *b, int y, const char *s, int first)
{
    int base = y + (first ? 19 : 25);
    int w = lab_text(b, &LAB_F_MONO11, PAD_X, base, s, C_MUTED, 12);
    lab_round(b, PAD_X + w + 10, base - 4, RIGHT - (PAD_X + w + 10), 1, 0, C_LINE);
    return first ? 28 : 34;
}

static void arrow(lab_band *b, int y, uint32_t c, int *right)
{
    *right -= lab_text_w(&LAB_F_MONO13, "→", 0);
    lab_text(b, &LAB_F_MONO13, *right, y + 25, "→", c, 0);
    *right -= 12;
}

/* LED chip, right-aligned at *right; moves *right left past it. */
static void led_chip(lab_band *b, lab_state *st, int y, int state, int focused, int *right)
{
    const char *s = state < 0 ? "N/D" : state ? "ON" : "OFF";
    int tw = lab_text_w(&LAB_F_MONO12B, s, 6), w = 9 + 8 + 7 + tw + 9, x;
    if (w < 64) w = 64;
    x = *right - w;
    if (focused) lab_round(b, x, y + 8, w, 24, 6, C_BG);
    else {
        if (state > 0) lab_round(b, x, y + 8, w, 24, 6, C_CHIP);
        lab_frame(b, x, y + 8, w, 24, 6, 1, C_BORDER);
    }
    if (state > 0) lab_disc(b, x + 13, y + 20, 8, st->accent);
    else lab_ring(b, x + 13, y + 20, 8, 1, focused ? C_MUTED : C_DECO);
    lab_text(b, &LAB_F_MONO12B, x + 24, y + 24, s, state > 0 ? st->accent : C_MUTED, 6);
    *right = x - 12;
}

static int ensure_visible(int scroll, int top, int bottom, int view_h, int content_h)
{
    if (top < scroll) scroll = top;
    if (bottom > scroll + view_h) scroll = bottom - view_h;
    if (scroll > content_h - view_h) scroll = content_h - view_h;
    if (scroll < 0) scroll = 0;
    return scroll;
}

/* --- root */

static void root_value(const lab_state *st, int i, char *buf)
{
    buf[0] = 0;
    if (i == 1 && !missing(st, V_GD)) {
        int n = on(st, V_ZEBRA) + on(st, V_FALSE) + on(st, V_HIST) + on(st, V_WAVE) +
                on(st, V_PEAK) + on(st, V_MZ) + on(st, V_CROP);
        if (!st->v[V_GD]) put_str(buf, "OCULTAS");
        else if (n == 0) put_str(buf, "NINGUNA");
        else put_str(put_int(buf, n), n == 1 ? " ACTIVA" : " ACTIVAS");
    }
    if (i == 3 && on(st, V_INTERVAL)) put_str(buf, "INTERVALO");
}

static void draw_root(lab_band *b, lab_state *st)
{
    const int top = 82, bottom = LEGEND_Y;
    int ys[ROOT_N], y = 0, i, sec = -1, focus = st->focus[LAB_S_ROOT], content;
    int sec_top[3] = {0, 0, 0};
    char buf[24], counter[12];
    static const char *crumbs[] = {"EOS 7D", "AJUSTES"};
    static const char *keys[] = {"SET", "MENU", "Q"};
    static const char *labels[] = {"Entrar", "Salir", "Menú clásico"};

    /* layout */
    for (i = 0; i < ROOT_N; i++) {
        if (ROOT[i].section != sec) {
            sec = ROOT[i].section;
            sec_top[sec] = y;
            y += sec == 0 ? 28 : 34;
        } else y += 1;                               /* separator */
        ys[i] = y;
        y += ROW_H;
    }
    content = y + 8;
    {
        int row_top = ys[focus];
        if (focus == 0 || ROOT[focus].section != ROOT[focus - 1].section)
            row_top = sec_top[ROOT[focus].section];
        st->scroll[LAB_S_ROOT] = ensure_visible(st->scroll[LAB_S_ROOT], row_top, ys[focus] + ROW_H + 8,
                                                bottom - top, content);
    }

    lab_clear(b, C_BG);
    sec = -1;
    for (i = 0; i < ROOT_N; i++) {
        int ry = top + ys[i] - st->scroll[LAB_S_ROOT], foc = i == focus, right = RIGHT;
        uint32_t fg = foc ? C_INK : C_TEXT, mu = foc ? C_INK : C_MUTED;
        if (ROOT[i].section != sec) {
            sec = ROOT[i].section;
            section_header(b, top + sec_top[sec] - st->scroll[LAB_S_ROOT], ROOT_SECTIONS[sec], sec == 0);
        } else if (!foc && i - 1 != focus) {
            lab_round(b, 58, ry - 1, RIGHT - 58, 1, 0, C_CHIP);
        }
        if (ry > bottom || ry + ROW_H < top) continue;
        if (foc) lab_round(b, LIST_X, ry, LIST_W, ROW_H, 6, st->accent);
        lab_icon_draw(b, ROOT[i].icon, PAD_X, ry + 10, mu);
        lab_text(b, foc ? &LAB_F_ROW16B : &LAB_F_ROW16, 58, ry + 26, ROOT[i].label, fg, 0);
        arrow(b, ry, mu, &right);
        root_value(st, i, buf);
        if (buf[0]) text_right(b, foc ? &LAB_F_MONO12B : &LAB_F_MONO12, right, ry + 25, buf, mu, 6);
    }

    lab_round(b, 0, 0, LAB_DW, top, 0, C_BG);
    counter2(counter, focus + 1, ROOT_N);
    header(b, st, crumbs, 2, 1, "Ajustes", counter);
    legend(b, keys, labels, 3);
}

/* --- overlay */

static void zebra_value(const lab_state *st, char *buf)
{
    char *p = buf;
    int cs = st->v[V_ZCS], hi = st->v[V_ZHI];
    p = put_str(p, (cs >= 0 && cs <= 2) ? ZCS_NAMES[cs] : "—");
    p = put_str(p, " · ");
    if (hi >= 1 && hi <= 100) put_str(put_int(p, hi), "%");
    else put_str(p, "OFF");
}

static void draw_overlay(lab_band *b, lab_state *st)
{
    const int top = 76, bottom = 422;
    int ys[OV_N], y = 0, i, sec = -1, focus = st->focus[LAB_S_OVERLAY], content;
    int sec_top[2] = {0, 0};
    char buf[32], counter[12];
    static const char *crumbs[] = {"AJUSTES", "SUPERPOSICIONES"};
    static const char *keys[] = {"SET", "MENU", "Q"};
    static const char *labels[] = {"Cambiar", "Atrás", "Opciones"};

    for (i = 0; i < OV_N; i++) {
        if (OV[i].section != sec) {
            sec = OV[i].section;
            sec_top[sec] = y;
            y += sec == 0 ? 28 : 34;
        } else y += 1;
        ys[i] = y;
        y += ROW_H;
    }
    content = y + 4;
    if (focus > 0) {
        int fi = focus - 1, row_top = ys[fi];
        if (fi == 0 || OV[fi].section != OV[fi - 1].section) row_top = sec_top[OV[fi].section];
        st->scroll[LAB_S_OVERLAY] = ensure_visible(st->scroll[LAB_S_OVERLAY], row_top, ys[fi] + ROW_H,
                                                   bottom - top, content);
    } else st->scroll[LAB_S_OVERLAY] = 0;

    lab_clear(b, C_BG);
    sec = -1;
    for (i = 0; i < OV_N; i++) {
        const ov_row *r = &OV[i];
        int ry = top + ys[i] - st->scroll[LAB_S_OVERLAY], foc = focus == i + 1, right = RIGHT;
        uint32_t fg = foc ? C_INK : C_TEXT, mu = foc ? C_INK : C_MUTED;
        int state = missing(st, r->value) ? -1 : on(st, r->value);
        if (r->section != sec) {
            sec = r->section;
            section_header(b, top + sec_top[sec] - st->scroll[LAB_S_OVERLAY], OV_SECTIONS[sec], sec == 0);
        } else if (!foc && focus != i) {
            lab_round(b, PAD_X, ry - 1, RIGHT - PAD_X, 1, 0, C_CHIP);
        }
        if (ry > bottom || ry + ROW_H < top) continue;
        if (foc) lab_round(b, LIST_X, ry, LIST_W, ROW_H, 6, st->accent);
        lab_text(b, foc ? &LAB_F_ROW16B : &LAB_F_ROW16, PAD_X, ry + 26, r->label, fg, 0);
        if (r->kind == R_LINK) {
            arrow(b, ry, mu, &right);
            zebra_value(st, buf);
            right -= lab_text(b, foc ? &LAB_F_MONO12B : &LAB_F_MONO12,
                              right - lab_text_w(foc ? &LAB_F_MONO12B : &LAB_F_MONO12, buf, 6),
                              ry + 25, buf, mu, 6) + 10;
            if (state > 0) lab_disc(b, right - 3, ry + 20, 7, foc ? C_INK : st->accent);
            else lab_ring(b, right - 3, ry + 20, 7, 1, foc ? C_INK : C_DECO);
        } else {
            led_chip(b, st, ry, state, foc, &right);
            if (r->kind == R_ZOOM) {
                int m = st->v[V_MZMODE];
                text_right(b, foc ? &LAB_F_MONO12B : &LAB_F_MONO12, right, ry + 25,
                           (m >= 1 && m <= 4) ? MZ_NAMES[m - 1] : "—", mu, 6);
            }
        }
    }

    /* help line */
    lab_round(b, 0, bottom, LAB_DW, LEGEND_Y - bottom, 0, C_BG);
    lab_icon_draw(b, &LAB_I_INFO, PAD_X, 430, C_MUTED);
    lab_text(b, &LAB_F_HELP13, PAD_X + 22, 441, focus ? OV[focus - 1].help : GD_HELP, C_MUTED, 0);

    /* header with the Global Draw chip */
    lab_round(b, 0, 0, LAB_DW, top, 0, C_BG);
    counter2(counter, focus + 1, OV_N + 1);
    header(b, st, crumbs, 2, 0, "Superposiciones", counter);
    {
        int right = RIGHT, gd = missing(st, V_GD) ? -1 : on(st, V_GD);
        int foc = focus == 0;
        if (foc) {
            const char *s = gd < 0 ? "N/D" : gd ? "ON" : "OFF";
            int tw = lab_text_w(&LAB_F_MONO12B, s, 6), w = 9 + 8 + 7 + tw + 9, x;
            if (w < 64) w = 64;
            x = right - w;
            lab_round(b, x, 30, w, 24, 6, st->accent);
            if (gd > 0) lab_disc(b, x + 13, 42, 8, C_INK); else lab_ring(b, x + 13, 42, 8, 1, C_INK);
            lab_text(b, &LAB_F_MONO12B, x + 24, 46, s, C_INK, 6);
            right = x - 12;
        } else {
            led_chip(b, st, 22, gd, 0, &right);
        }
        text_right(b, &LAB_F_MONO11, right, 46, "MOSTRAR", foc ? C_TEXT : C_MUTED, 12);
    }
    legend(b, keys, labels, 3);
}

/* --- zebras */

static void ruler(lab_band *b, lab_state *st, int x, int y, int v, int lo, int hi, const char *const *labels,
                  int focused, int off)
{
    int i, mx;
    uint32_t fill = focused ? st->accent : C_MUTED, mark = focused ? st->accent : C_TEXT;
    for (i = 0; i <= 20; i++)
        lab_round(b, x + i * 28, y + 2, 2, i % 5 ? 6 : 12, 0, C_DECO);
    lab_round(b, x, y + 18, 562, 2, 0, C_BORDER);
    mx = x + (clampi(v, lo, hi) - lo) * 560 / (hi - lo);
    if (!off) {
        lab_round(b, x, y + 18, mx - x + 1, 2, 0, fill);
        lab_round(b, mx - 1, y, 3, 23, 0, mark);
    }
    lab_text(b, &LAB_F_MONO11, x, y + 33, labels[0], C_MUTED, 0);
    for (i = 1; i < 4; i++) {
        int w = lab_text_w(&LAB_F_MONO11, labels[i], 0);
        lab_text(b, &LAB_F_MONO11, x + i * 140 - w / 2, y + 33, labels[i], C_MUTED, 0);
    }
    text_right(b, &LAB_F_MONO11, x + 562, y + 33, labels[4], C_MUTED, 0);
}

/* vi: V_ZHI (OFF outside 1..100) or V_ZLO (OFF at 0), as zebra.c reads them */
static void threshold(lab_band *b, lab_state *st, int y, const char *label, int vi, int lo, int hi,
                      const char *const *labels, int focused)
{
    char buf[12];
    int v = st->v[vi];
    int off = missing(st, vi) || (vi == V_ZLO ? v == 0 : (v < 1 || v > 100));
    if (focused) lab_frame(b, PAD_X, y, RIGHT - PAD_X, 76, 8, 2, st->accent);
    lab_text(b, &LAB_F_LABEL15, PAD_X + 12, y + 25, label, C_TEXT, 0);
    if (off) put_str(buf, "OFF"); else put_str(put_int(buf, v), "%");
    text_right(b, &LAB_F_MONO22B, RIGHT - 12, y + 28, buf, focused ? st->accent : C_TEXT, 0);
    ruler(b, st, PAD_X + 13, y + 34, v, lo, hi, labels, focused, off);
}

static void preview(lab_band *b, lab_state *st, int y)
{
    static const int16_t hills[] = {29,393, 139,361, 239,385, 349,355, 469,389, 611,367, 611,411, 29,411};
    static const int16_t ground[] = {29,403, 179,385, 319,401, 459,383, 611,399, 611,411, 29,411};
    static const int16_t sun[] = {499,365, 497,373, 491,380, 482,383, 473,380, 467,373, 465,365,
                                  467,357, 473,350, 482,347, 491,350, 497,357};
    int zebras = on(st, V_ZEBRA);
    int x0 = PAD_X, x1 = RIGHT, y1 = y + 80, k;
    lab_round(b, x0 + 5, y + 5, x1 - x0 - 10, 70, 0, 0x6B7480);
    lab_disc(b, 482, 365, 38, 0xECEAE3);
    if (zebras && st->v[V_ZHI] >= 1 && st->v[V_ZHI] <= 100) lab_poly(b, sun, 12, C_CORAL, 4);
    lab_poly(b, hills, 8, 0x3A4048, 0);
    lab_poly(b, ground, 7, 0x121417, 0);
    if (zebras && st->v[V_ZLO] > 0) lab_poly(b, ground, 7, C_ICE, -4);
    for (k = 0; k < 4; k++) {          /* viewfinder corners */
        int cx = k & 1 ? x1 - 12 : x0, cy = k & 2 ? y1 - 12 : y;
        lab_round(b, cx, k & 2 ? y1 - 2 : cy, 12, 2, 0, C_MUTED);
        lab_round(b, k & 1 ? x1 - 2 : cx, cy, 2, 12, 0, C_MUTED);
    }
}

static void draw_zebras(lab_band *b, lab_state *st)
{
    static const char *crumbs[] = {"AJUSTES", "SUPERPOSICIONES", "ZEBRAS"};
    static const char *keys[] = {"◀ ▶", "SET", "MENU"};
    static const char *labels[] = {"Ajustar", "ON/OFF", "Atrás"};
    static const char *hi_labels[] = {"0", "25", "50", "75", "100"};
    static const char *lo_labels[] = {"0", "5", "10", "15", "20"};
    int focus = st->focus[LAB_S_ZEBRAS], i, x, w;
    int z = missing(st, V_ZEBRA) ? -1 : on(st, V_ZEBRA);

    lab_clear(b, C_BG);
    header(b, st, crumbs, 3, 0, "Zebras", 0);

    /* main switch, in the header */
    {
        const char *s = z < 0 ? "N/D" : z ? "ON" : "OFF";
        int tw = lab_text_w(&LAB_F_MONO12B, s, 6);
        w = 12 + 9 + 8 + tw + 12;
        x = RIGHT - w;
        if (focus == 0) lab_round(b, x, 36, w, 28, 6, st->accent);
        else {
            if (z > 0) lab_round(b, x, 36, w, 28, 6, C_CHIP);
            lab_frame(b, x, 36, w, 28, 6, 1, C_BORDER);
        }
        if (z > 0) lab_disc(b, x + 17, 50, 9, focus == 0 ? C_INK : st->accent);
        else lab_ring(b, x + 17, 50, 9, 1, focus == 0 ? C_INK : C_DECO);
        lab_text(b, &LAB_F_MONO12B, x + 29, 54, s, focus == 0 ? C_INK : z > 0 ? st->accent : C_MUTED, 6);
    }

    /* mode */
    if (focus == 1) lab_frame(b, PAD_X, 80, RIGHT - PAD_X, 44, 8, 2, st->accent);
    lab_text(b, &LAB_F_MONO11, PAD_X + 12, 106, "MODO", C_MUTED, 12);
    x = PAD_X + 96;
    for (i = 0; i < 3; i++) {
        int sel = st->v[V_ZCS] == i, tw = lab_text_w(&LAB_F_MONO12B, ZCS_NAMES[i], 8);
        w = tw + 32;
        if (sel) lab_round(b, x, 88, w, 28, 6, st->accent);
        else lab_frame(b, x, 88, w, 28, 6, 1, C_BORDER);
        lab_text(b, &LAB_F_MONO12B, x + 16, 106, ZCS_NAMES[i], sel ? C_INK : C_MUTED, 8);
        x += w + 8;
    }

    section_header(b, 120, "01 — UMBRALES", 1);
    threshold(b, st, 150, "Altas luces", V_ZHI, 0, 100, hi_labels, focus == 2);
    threshold(b, st, 230, "Sombras", V_ZLO, 0, 20, lo_labels, focus == 3);

    /* preview */
    {
        int base = 326, lw;
        lw = lab_text(b, &LAB_F_MONO11, PAD_X, base, "02 — VISTA PREVIA", C_MUTED, 12);
        x = RIGHT - lab_text_w(&LAB_F_MONO11, "SOMBRAS", 10);
        lab_text(b, &LAB_F_MONO11, x, base, "SOMBRAS", C_MUTED, 10);
        lab_round(b, x - 13, base - 8, 8, 8, 0, C_ICE);
        x -= 13 + 14 + lab_text_w(&LAB_F_MONO11, "ALTAS", 10);
        lab_text(b, &LAB_F_MONO11, x, base, "ALTAS", C_MUTED, 10);
        lab_round(b, x - 13, base - 8, 8, 8, 0, C_CORAL);
        lab_round(b, PAD_X + lw + 10, base - 4, x - 13 - 10 - (PAD_X + lw + 10), 1, 0, C_LINE);
        preview(b, st, 336);
    }
    legend(b, keys, labels, 3);
}

void lab_draw(lab_band *b, lab_state *st)
{
    switch (st->screen) {
    case LAB_S_ROOT:    draw_root(b, st); break;
    case LAB_S_OVERLAY: draw_overlay(b, st); break;
    case LAB_S_ZEBRAS:  draw_zebras(b, st); break;
    }
}
