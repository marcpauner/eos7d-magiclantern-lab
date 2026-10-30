/* lab_ui.h - screens and navigation of the 7D UI.
 *
 * Portable: the camera module and the PC simulator both drive it. Settings
 * live in Magic Lantern's menu entries, addressed by (menu, entry) names the
 * way scripts do; the backend wires that to the real camera or to a fake.
 */
#ifndef LAB_UI_H
#define LAB_UI_H

#include "lab_gfx.h"

#define LAB_MISSING (-2147483647 - 1)

typedef struct {
    int (*get)(const char *menu, const char *entry);              /* LAB_MISSING if absent */
    int (*set)(const char *menu, const char *entry, int value);   /* nonzero on success */
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

enum { LAB_S_ROOT, LAB_S_OVERLAY, LAB_S_ZEBRAS, LAB_S_COUNT };

typedef struct {
    int screen;
    int focus[LAB_S_COUNT];
    int scroll[LAB_S_COUNT];
    int gd_last;                  /* last Global Draw mode that was not OFF */
    const char *classic_menu;     /* where LAB_A_CLASSIC lands (0: keep ML's selection) */
    const char *classic_entry;
    uint32_t accent;
    int v[16];                    /* setting values, read by lab_refresh */
} lab_state;

void lab_init(lab_state *st);
/* Read every value the current screen shows. Call before drawing a frame. */
void lab_refresh(lab_state *st, const lab_backend *be);
int  lab_key(lab_state *st, const lab_backend *be, int key);
/* Draw the current screen into one band. Call for every band of a frame. */
void lab_draw(lab_band *b, lab_state *st);

#endif
