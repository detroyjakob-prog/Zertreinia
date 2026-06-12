/* Window manager / desktop environment. */
#ifndef ZERT_DESKTOP_H
#define ZERT_DESKTOP_H

#include "types.h"

#define WIN_MAX        16
#define TITLEBAR_H     22
#define TASKBAR_H      34
#define START_BTN_W    48
#define START_MENU_W   180
#define MENU_ITEM_H    26
#define DESK_ICON_W    80
#define DESK_ICON_H    64

#define APP_TERMINAL   0
#define APP_FILES      1
#define APP_EDITOR     2
#define APP_MONITOR    3
#define APP_ABOUT      4
#define APP_COUNT      5

typedef struct window window_t;

struct window {
    int   x, y, w, h;
    char  title[48];
    bool  used;
    int   app_id;
    void *state;
    void (*on_draw)(window_t *win);
    void (*on_key)(window_t *win, char c);
    void (*on_click)(window_t *win, int cx, int cy, uint8_t buttons);
    void (*on_tick)(window_t *win);
};

int win_client_x(window_t *win);
int win_client_y(window_t *win);
int win_client_w(window_t *win);
int win_client_h(window_t *win);

window_t *desktop_spawn(int app_id, void *arg);
void      desktop_request_redraw(void);

void desktop_run(void);

#endif /* ZERT_DESKTOP_H */
