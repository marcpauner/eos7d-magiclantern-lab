/** \file
 * labui: a redesigned Magic Lantern menu for the EOS 7D.
 *
 * Trash opens ML's menu as usual (that hides Canon's GUI). This module then
 * blocks ML's menu redraw, the way the Arkanoid module does, and draws its
 * own screens on top with the portable renderer in lab_gfx.c / lab_ui.c.
 * Module key handlers run before ML's menu handler (gui-common.c), so the
 * buttons come here first. Settings are ML's own menu entries, read and
 * written with the functions Lua scripts use.
 *
 * While the screens are up, LAB_PAL_N palette entries from LAB_PAL_BASE are
 * redefined (hardware YUV, see lab_assets.c) and put back on the way out.
 * Everything not redesigned yet stays in the classic menu, one key away.
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <module.h>
#include <dryos.h>
#include <menu.h>
#include <bmp.h>
#include <config.h>
#include <tasks.h>
#include "lab_ui.h"

#define LAB_PAL_BASE 0xA0          /* first palette entry we borrow */
#define BAND_H 48                   /* rows drawn per pass */
#define K_CLOSE 100                 /* internal key: Trash while open */

extern int menu_redraw_blocked;     /* menu.c */

static CONFIG_INT("labui.enabled", labui_enabled, 1);

static lab_state st;
static uint8_t lut[LAB_LUT_SIZE];   /* RGB555 -> palette, built once */
static uint16_t band_px[LAB_BW * BAND_H];
static uint32_t pal_saved[LAB_PAL_N];

static volatile int ready, active, want_open;
static volatile int kq[16], kq_head, kq_tail;

/* ---------------------------------------------------------- settings */

static int be_get(const char *menu, const char *entry)
{
    return menu_get_value_from_script(menu, entry);
}

static int be_set(const char *menu, const char *entry, int value)
{
    int r = menu_set_value_from_script(menu, entry, value);
    return r != 0 && r != LAB_MISSING;
}

/* ------------------------------------------------ Magic Lantern menus
 *
 * ML's list of menus is private to menu.c, but the structures are public
 * (menu.h) and menu_add links our own Prefs entry into that list: its
 * parent_menu and the menus' next/prev pointers reach every menu. The rules
 * below follow menu.c: is_visible, entry_guess_enabled,
 * entry_default_display_info and menu_entry_select.
 */

static struct menu_entry labui_menu[];
extern int raw2iso(int raw_iso);
extern void menu_open_submenu();

static struct menu *ml_first(void)
{
    struct menu *m = labui_menu[0].parent_menu;
    if (!m) return 0;
    while (m->prev) m = m->prev;
    return m;
}

static int visible(struct menu *m, struct menu_entry *e)
{
    if (MENU_IS_PLACEHOLDER(e) || e->shidden || e->hidden) return 0;
    return !e->advanced || m->advanced || e->selected;
}

static int has_children(struct menu_entry *e)
{
    return e->children && !MENU_IS_EOL(e->children);
}

static int be_tabs(void **h, const char **names, int max)
{
    struct menu *m;
    struct menu_entry *e;
    int n = 0;
    for (m = ml_first(); m && n < max; m = m->next) {
        if (IS_SUBMENU(m) || m->no_name_lookup || !m->name) continue;
        if (!strcmp(m->name, "MyMenu") || !strcmp(m->name, "Modified")) continue;
        for (e = m->children; e && !visible(m, e); e = e->next) ;
        if (!e) continue;                          /* nothing to show */
        h[n] = m;
        names[n] = m->name;
        n++;
    }
    return n;
}

static int guess_on(struct menu_entry *e)
{
    struct menu_entry *c;
    switch (e->icon_type) {
    case IT_BOOL: case IT_DICE_OFF: case IT_PERCENT_OFF: case IT_PERCENT_LOG_OFF:
        return MENU_INT(e) != 0;
    case IT_BOOL_NEG:
        return !MENU_INT(e);
    case IT_SUBMENU:
        if (IS_ML_PTR(e->priv)) return MENU_INT(e) != 0;
        for (c = has_children(e) ? e->children : 0; c; c = c->next)
            if (MENU_INT(c) && (c->icon_type == IT_BOOL || c->icon_type == IT_DICE_OFF)) return 1;
        return 0;
    }
    return 1;
}

static void default_value(struct menu_entry *e, char *v, int n)
{
    int x, a, den, r, len;
    v[0] = 0;
    if (e->choices && SELECTED_INDEX(e) >= 0 && SELECTED_INDEX(e) < NUM_CHOICES(e)) {
        snprintf(v, n, "%s", e->choices[SELECTED_INDEX(e)]);
        return;
    }
    if (!IS_ML_PTR(e->priv) || e->icon_type == IT_ACTION || e->icon_type == IT_SUBMENU) return;
    x = MENU_INT(e);
    if (e->min == 0 && e->max == 1) { snprintf(v, n, "%s", x ? "ON" : "OFF"); return; }
    switch (e->unit) {
    case UNIT_1_8_EV: case UNIT_x10: case UNIT_PERCENT_x10:
        den = e->unit == UNIT_1_8_EV ? 8 : 10;
        a = x < 0 ? -x : x;
        r = (a % den) * 10 / den;
        snprintf(v, n, "%s%d", x < 0 ? "-" : "", a / den);
        len = strlen(v);
        if (r) { snprintf(v + len, n - len, ".%d", r); len = strlen(v); }
        snprintf(v + len, n - len, "%s", e->unit == UNIT_1_8_EV ? " EV" : e->unit == UNIT_PERCENT_x10 ? "%" : "");
        break;
    case UNIT_PERCENT: snprintf(v, n, "%d%%", x); break;
    case UNIT_ISO:     if (x) snprintf(v, n, "%d", raw2iso(x)); else snprintf(v, n, "Auto"); break;
    case UNIT_HEX:     snprintf(v, n, "0x%x", x); break;
    case UNIT_TIME:
        if (x >= 3600) snprintf(v, n, "%dh%02dm%02ds", x / 3600, x / 60 % 60, x % 60);
        else if (x >= 60) snprintf(v, n, "%dm%02ds", x / 60, x % 60);
        else snprintf(v, n, "%ds", x);
        break;
    case UNIT_TIME_MS: snprintf(v, n, "%d ms", x); break;
    case UNIT_TIME_US: snprintf(v, n, "%d us", x); break;
    default:           snprintf(v, n, "%d", x); break;
    }
}

/* help strings may hold one line per choice, separated by '\n' */
static void help_line_of(const char *src, int idx, char *dst, int n)
{
    const char *s = src;
    int i;
    for (i = 0; i < idx && s; i++) {
        while (*s && *s != '\n') s++;
        s = *s ? s + 1 : 0;
    }
    if (!s || !*s) s = src;
    for (i = 0; s[i] && s[i] != '\n' && i < n - 1; i++) dst[i] = s[i];
    dst[i] = 0;
}

static void format(struct menu_entry *e, lab_item *it, char *help, int *warn)
{
    static char name[MENU_MAX_NAME_LEN], value[MENU_MAX_VALUE_LEN], sname[MENU_MAX_SHORT_NAME_LEN],
                svalue[MENU_MAX_SHORT_VALUE_LEN], hl[MENU_MAX_HELP_LEN], wn[MENU_MAX_WARNING_LEN],
                ri[MENU_MAX_RINFO_LEN];
    struct menu_display_info info;
    int onoff;

    memset(&info, 0, sizeof(info));
    name[0] = value[0] = sname[0] = svalue[0] = hl[0] = wn[0] = ri[0] = 0;
    info.name = name; info.value = value; info.short_name = sname; info.short_value = svalue;
    info.help = hl; info.warning = wn; info.rinfo = ri;
    info.can_custom_draw = 0;
    info.enabled = guess_on(e);
    snprintf(name, sizeof(name), "%s", e->name);
    default_value(e, value, sizeof(value));
    if (e->update) {
        /* help and warning texts are only filled in for the selected entry */
        unsigned sel = e->selected;
        if (help) e->selected = 1;
        e->update(e, &info);
        e->selected = sel;
    }

    snprintf(it->name, sizeof(it->name), "%s", name);
    snprintf(it->value, sizeof(it->value), "%s", value);
    onoff = !strcmp(value, "ON") || !strcmp(value, "OFF");
    if (has_children(e)) {
        it->kind = LAB_IT_SUBMENU;
        it->state = IS_ML_PTR(e->priv) && e->icon_type != IT_ACTION ? (onoff ? !strcmp(value, "ON") : info.enabled) : -1;
        if (onoff && it->state >= 0) it->value[0] = 0;          /* the dot says it */
    } else if (e->icon_type == IT_ACTION) {
        it->kind = LAB_IT_ACTION;
        it->state = -1;
    } else if ((e->icon_type == IT_BOOL || e->icon_type == IT_BOOL_NEG) && (onoff || !value[0])) {
        it->kind = LAB_IT_TOGGLE;
        it->state = onoff ? !strcmp(value, "ON") : info.enabled;
    } else {
        it->kind = LAB_IT_VALUE;
        it->state = -1;
    }
    it->dim = info.warning_level == MENU_WARN_NOT_WORKING;
    it->h = e;

    if (help) {
        if (wn[0]) { snprintf(help, LAB_HELP_LEN, "%s", wn); *warn = 1; }
        else if (hl[0]) snprintf(help, LAB_HELP_LEN, "%s", hl);
        else if (e->help) help_line_of(e->help, e->choices ? SELECTED_INDEX(e) : 0, help, LAB_HELP_LEN);
    }
}

static int be_items(void *menu, lab_item *out, int max, int focus, char *help, int *warn)
{
    struct menu *m = menu;
    struct menu_entry *e;
    int n = 0;
    for (e = m->children; e && n < max; e = e->next) {
        if (!visible(m, e)) continue;
        format(e, &out[n], n == focus ? help : 0, warn);
        n++;
    }
    return n;
}

static void *be_submenu(void *h)
{
    struct menu_entry *e = h;
    return has_children(e) ? e->children->parent_menu : 0;
}

static void be_change(void *h, int how)
{
    struct menu_entry *e = h;
    int d = how == LAB_CH_SET ? 1 : how;
    if ((void *)e->select == (void *)menu_open_submenu) return;   /* would open ML's own submenu */
    if (e->select) { e->select(e->priv, d); return; }
    if (!IS_ML_PTR(e->priv) || e->max <= e->min) return;
    if (e->max - e->min < 50 || e->unit == UNIT_HEX) {
        menu_numeric_toggle(e->priv, d, e->min, e->max);
    } else {                        /* long ranges: bigger steps for bigger values */
        int *p = e->priv, a = *p < 0 ? -*p : *p;
        int x = *p + d * (a >= 1000 ? 100 : a >= 100 ? 10 : 1);
        *p = x < e->min ? e->min : x > e->max ? e->max : x;
    }
}

static void be_where(void *h, const char **menu, const char **entry)
{
    struct menu_entry *e = h;
    while (e->parent) e = e->parent;
    *menu = e->parent_menu ? e->parent_menu->name : 0;
    *entry = e->name;
}

static const lab_backend be = {be_get, be_set, be_tabs, be_items, be_submenu, be_change, be_where};

/* ----------------------------------------------------------- palette */

static void palette_apply(void)
{
    uint32_t opaque = LCD_Palette[3 * COLOR_WHITE + 2] & 0xFF000000;
    int i;
    for (i = 0; i < LAB_PAL_N; i++) {
        int idx = LAB_PAL_BASE + i;
        uint32_t v = opaque | LAB_PAL_YUV[i];
        pal_saved[i] = LCD_Palette[3 * idx + 2];
        EngDrvOut(LCD_Palette[3 * idx], v);
        EngDrvOut(LCD_Palette[3 * idx + 0x300], v);
    }
}

static void palette_restore(void)
{
    int i;
    for (i = 0; i < LAB_PAL_N; i++) {
        int idx = LAB_PAL_BASE + i;
        EngDrvOut(LCD_Palette[3 * idx], pal_saved[i]);
        EngDrvOut(LCD_Palette[3 * idx + 0x300], pal_saved[i]);
    }
}

/* ------------------------------------------------------------ render */

static void render(void)
{
    lab_band b;
    uint8_t *bmp;
    int y0, r, x;

    lab_refresh(&st, &be);
    palette_apply();                /* cheap; survives Canon reloading it */
    bmp_draw_to_idle(1);
    bmp = bmp_vram();
    if (!bmp) { bmp_draw_to_idle(0); return; }
    b.px = band_px;
    b.h = BAND_H;
    for (y0 = 0; y0 < LAB_BH; y0 += BAND_H) {
        b.y0 = y0;
        lab_draw(&b, &st);
        for (r = 0; r < BAND_H; r++) {
            uint8_t *dst = bmp + (y0 + r) * BMPPITCH;
            const uint16_t *src = band_px + r * LAB_BW;
            for (x = 0; x < LAB_BW; x++) dst[x] = (uint8_t)(LAB_PAL_BASE + lut[LAB_LUT_KEY(src[x])]);
        }
    }
    bmp_draw_to_idle(0);
    bmp_idle_copy(1, 0);
}

/* ---------------------------------------------------- open and close */

static void ui_open(void)
{
    active = 1;
    kq_tail = kq_head;              /* drop keys from before */
    render();
}

/* ML's menu is still open: leave it showing the classic screens. */
static void ui_to_classic(void)
{
    active = 0;
    palette_restore();
    if (st.classic_menu)
        select_menu_by_name((char *)st.classic_menu, st.classic_entry ? st.classic_entry : "");
    menu_redraw_blocked = 0;
    menu_redraw();
}

/* Close ML's menu too. */
static void ui_close(void)
{
    int i;
    active = 0;
    palette_restore();
    clrscr();
    gui_stop_menu();
    for (i = 0; i < 25 && gui_menu_shown(); i++) msleep(20);
    menu_redraw_blocked = 0;
}

/* ML's menu went away on its own (half-shutter, playback...). */
static void ui_lost(void)
{
    active = 0;
    palette_restore();
    menu_redraw_blocked = 0;
}

/* -------------------------------------------------------------- keys */

static void kq_push(int k)
{
    int n = (kq_head + 1) % 16;
    if (n != kq_tail) { kq[kq_head] = k; kq_head = n; }
}

static int kq_pop(void)
{
    int k;
    if (kq_tail == kq_head) return 0;
    k = kq[kq_tail];
    kq_tail = (kq_tail + 1) % 16;
    return k;
}

static int translate(unsigned int key)
{
    switch (key) {
    case MODULE_KEY_WHEEL_UP:     case MODULE_KEY_PRESS_UP:    return LAB_K_UP;
    case MODULE_KEY_WHEEL_DOWN:   case MODULE_KEY_PRESS_DOWN:  return LAB_K_DOWN;
    case MODULE_KEY_PRESS_LEFT:   return LAB_K_LEFT;
    case MODULE_KEY_PRESS_RIGHT:  return LAB_K_RIGHT;
    case MODULE_KEY_WHEEL_LEFT:   return LAB_K_DEC;
    case MODULE_KEY_WHEEL_RIGHT:  return LAB_K_INC;
    case MODULE_KEY_PRESS_SET:    case MODULE_KEY_JOY_CENTER:  return LAB_K_SET;
    case MODULE_KEY_MENU:         return LAB_K_BACK;
    case MODULE_KEY_Q:            return LAB_K_Q;
    case MODULE_KEY_TRASH:        return K_CLOSE;
    }
    return 0;
}

static unsigned int labui_keypress(unsigned int key)
{
    int k;
    if (!active) {
        /* Trash from Canon's screens: let ML open its menu, then take over */
        if (key == MODULE_KEY_TRASH && labui_enabled && ready && !gui_menu_shown()) {
            want_open = 1;
            menu_redraw_blocked = 1;
        }
        return 1;
    }
    k = translate(key);
    if (k) { kq_push(k); return 0; }
    /* releases of keys we handle would otherwise reach ML's menu */
    if (key == MODULE_KEY_UNPRESS_SET || key == MODULE_KEY_UNPRESS_UDLR) return 0;
    return 1;                       /* half-shutter, PLAY...: ML closes its menu */
}

/* -------------------------------------------------------------- task */

static void labui_task(void *unused)
{
    int wait = 0;
    (void)unused;
    lab_init(&st);
    lab_build_lut(lut);
    ready = 1;

    TASK_LOOP
    {
        if (want_open) {
            if (gui_menu_shown()) { want_open = 0; wait = 0; ui_open(); }
            else if (++wait > 40) { want_open = 0; wait = 0; menu_redraw_blocked = 0; }
        }
        if (active) {
            int key, act = LAB_A_NONE;
            if (!gui_menu_shown()) { ui_lost(); continue; }
            while ((key = kq_pop()) != 0) {
                int a;
                if (key == K_CLOSE) { act = LAB_A_CLOSE; break; }
                lab_refresh(&st, &be);
                a = lab_key(&st, &be, key);
                if (a == LAB_A_CLOSE || a == LAB_A_CLASSIC) { act = a; break; }
                if (a == LAB_A_REDRAW) act = LAB_A_REDRAW;
            }
            if (act == LAB_A_CLOSE) ui_close();
            else if (act == LAB_A_CLASSIC) ui_to_classic();
            else if (act == LAB_A_REDRAW) render();
        }
        msleep(active ? 20 : 50);
    }
}

/* ------------------------------------------------------------ module */

static struct menu_entry labui_menu[] = {
    {
        .name = "Lab UI",
        .priv = &labui_enabled,
        .max = 1,
        .help = "Trash opens the redesigned menu (Ajustes).",
        .help2 = "OFF: Trash opens this classic menu. Q there returns here.",
    },
};

static unsigned int labui_init()
{
    menu_add("Prefs", labui_menu, COUNT(labui_menu));
    task_create("labui_task", 0x1d, 0x4000, labui_task, 0);
    return 0;
}

static unsigned int labui_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(labui_init)
    MODULE_DEINIT(labui_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_KEYPRESS, labui_keypress, 0)
MODULE_CBRS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(labui_enabled)
MODULE_CONFIGS_END()
