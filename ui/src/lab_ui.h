/* lab_ui.h - screens and navigation of the 7D UI.
 *
 * Portable: the camera module and the PC simulator both drive it.
 *
 * Two kinds of screens:
 *  - designed ones (Superposiciones, Zebras), bound to settings by
 *    (menu, entry) name the way scripts address Magic Lantern's menu;
 *  - a generic screen that shows any Magic Lantern menu or submenu, rows
 *    supplied by the backend (on the camera: ML's own menu entries).
 */
#ifndef LAB_UI_H
#define LAB_UI_H

#include "lab_gfx.h"

#define LAB_MISSING (-2147483647 - 1)

/* One row of a generic screen. */
enum { LAB_IT_VALUE, LAB_IT_TOGGLE, LAB_IT_SUBMENU, LAB_IT_ACTION };
typedef struct {
    char name[36];
    char value[36];
    void *h;                /* backend handle (the ML menu entry on the camera) */
    uint8_t kind;
    int8_t state;           /* toggles and switchable submenus: 1 on, 0 off; -1 none */
    uint8_t dim;            /* does not work in the current mode */
} lab_item;

#define LAB_ITEMS_MAX 64
#define LAB_TABS_MAX 24
#define LAB_DEPTH_MAX 4
#define LAB_HELP_LEN 104
#define LAB_CH_SET 0        /* change(): SET instead of -1 / +1 */

typedef struct {
    /* settings of the designed screens, by name */
    int (*get)(const char *menu, const char *entry);              /* LAB_MISSING if absent */
    int (*set)(const char *menu, const char *entry, int value);   /* nonzero on success */
    /* generic screens */
    int (*tabs)(void **h, const char **names, int max);           /* top-level menus */
    /* Visible rows of a menu; help (and *warn) describe row `focus`. */
    int (*items)(void *menu, lab_item *out, int max, int focus, char *help, int *warn);
    void *(*submenu)(void *item);                                 /* 0 if none */
    void (*change)(void *item, int how);                          /* -1, +1 or LAB_CH_SET */
    /* where the classic menu should land for a row (its top-level entry) */
    void (*where)(void *item, const char **menu, const char **entry);
} lab_backend;

/* Keys, already translated from the camera's buttons. */
enum {
    LAB_K_UP = 1, LAB_K_DOWN,     /* rear dial, joystick up/down: move focus */
    LAB_K_LEFT, LAB_K_RIGHT,      /* joystick left/right: change value, coarse */
    LAB_K_DEC, LAB_K_INC,         /* main dial: change value, fine */
    LAB_K_SET,                    /* open / toggle */
    LAB_K_BACK,                   /* MENU: back, or close from the root */
    LAB_K_Q                       /* Q: open this setting in the classic ML menu */
};

/* What the caller should do after a key. */
enum { LAB_A_NONE, LAB_A_REDRAW, LAB_A_CLOSE, LAB_A_CLASSIC };

enum { LAB_S_ROOT, LAB_S_OVERLAY, LAB_S_ZEBRAS, LAB_S_MENU, LAB_S_COUNT };

typedef struct {
    int screen;
    int focus[LAB_S_COUNT];
    int scroll[LAB_S_COUNT];
    int gd_last;                  /* last Global Draw mode that was not OFF */
    const char *classic_menu;     /* where LAB_A_CLASSIC lands (0: keep ML's selection) */
    const char *classic_entry;
    uint32_t accent;
    int v[16];                    /* values of the designed screens */

    /* root: ML's top-level menus as found on this camera */
    int n_root;
    void *root_h[LAB_TABS_MAX];
    const char *root_ml[LAB_TABS_MAX];

    /* generic screen: a stack of menus */
    int depth, menu_from;         /* menu_from: screen MENU returns to */
    void *menu[LAB_DEPTH_MAX];
    char title[LAB_DEPTH_MAX][36];
    int gfocus[LAB_DEPTH_MAX], gscroll[LAB_DEPTH_MAX];
    int n_items;
    lab_item items[LAB_ITEMS_MAX];
    char help[LAB_HELP_LEN];
    int help_warn;
} lab_state;

void lab_init(lab_state *st);
/* Read everything the current screen shows. Call before handling a key and
 * before drawing a frame. */
void lab_refresh(lab_state *st, const lab_backend *be);
int  lab_key(lab_state *st, const lab_backend *be, int key);
/* Draw the current screen into one band. Call for every band of a frame. */
void lab_draw(lab_band *b, lab_state *st);

#endif
