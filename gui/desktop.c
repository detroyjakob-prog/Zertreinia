/* Window manager + desktop event loop.
 *
 * Draws a gradient desktop, draggable windows with title bars and close
 * buttons, a taskbar with application launchers and a clock, a start menu
 * that drops down from the start button (above the taskbar), desktop icons,
 * and a mouse pointer. Input from the keyboard and mouse IRQ ring buffers
 * is dispatched to the focused window. Everything is rendered to the gfx
 * back buffer and presented once per frame. */

#include "desktop.h"
#include "apps.h"
#include "gfx.h"
#include "mouse.h"
#include "keyboard.h"
#include "timer.h"
#include "string.h"
#include "printf.h"
#include "heap.h"
#include "ports.h"
#include "commands.h"
#include "kernel.h"
#include "vfs.h"

static window_t windows[WIN_MAX];
static int      order[WIN_MAX];
static int      win_count;

static bool     dragging;
static int      drag_idx;
static int      drag_dx, drag_dy;

static uint32_t scr_w, scr_h;
static int      cascade;
static volatile bool g_dirty = true;

static bool     menu_open;
static int      menu_hover = -1;


/* ---- geometry ------------------------------------------------------------- */

int win_client_x(window_t *w) { return w->x + 1; }
int win_client_y(window_t *w) { return w->y + TITLEBAR_H; }
int win_client_w(window_t *w) { return w->w - 2; }
int win_client_h(window_t *w) { return w->h - TITLEBAR_H - 1; }

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool in_close_button(window_t *w, int px, int py)
{
    return in_rect(px, py, w->x + w->w - 19, w->y + 4, 14, 14);
}

static bool in_start_button(int mx, int my)
{
    int y0 = (int)scr_h - TASKBAR_H;
    return in_rect(mx, my, 4, y0 + 4, START_BTN_W - 8, TASKBAR_H - 10);
}

static bool in_start_menu(int mx, int my)
{
    int y0 = (int)scr_h - TASKBAR_H;
    int menu_y = y0 - MENU_ITEM_H * APP_COUNT - 4;
    return in_rect(mx, my, 4, menu_y, START_MENU_W, MENU_ITEM_H * APP_COUNT);
}

/* ---- window list ---------------------------------------------------------- */

static int focus_idx(void)
{
    return win_count > 0 ? order[win_count - 1] : -1;
}

static window_t *focus_window(void)
{
    int i = focus_idx();
    return i >= 0 ? &windows[i] : NULL;
}

static int find_app_index(int app_id)
{
    for (int i = 0; i < WIN_MAX; i++) {
        if (windows[i].used && windows[i].app_id == app_id) {
            return i;
        }
    }
    return -1;
}

static int order_pos(int idx)
{
    for (int z = 0; z < win_count; z++) {
        if (order[z] == idx) return z;
    }
    return -1;
}

static void bring_to_front(int idx)
{
    int z = order_pos(idx);
    if (z < 0) return;
    for (int i = z; i < win_count - 1; i++) {
        order[i] = order[i + 1];
    }
    order[win_count - 1] = idx;
}

static void close_window(int idx)
{
    int z = order_pos(idx);
    if (z < 0) return;
    if (windows[idx].state) {
        kfree(windows[idx].state);
    }
    windows[idx].used = false;
    windows[idx].state = NULL;
    for (int i = z; i < win_count - 1; i++) {
        order[i] = order[i + 1];
    }
    win_count--;
}

window_t *desktop_spawn(int app_id, void *arg)
{
    int idx = -1;
    for (int i = 0; i < WIN_MAX; i++) {
        if (!windows[i].used) { idx = i; break; }
    }
    if (idx < 0) {
        return NULL;
    }

    window_t *w = &windows[idx];
    memset(w, 0, sizeof(*w));
    w->app_id = app_id;
    w->x = 70 + cascade * 26;
    w->y = 40 + cascade * 24;
    w->w = 480;
    w->h = 320;
    cascade = (cascade + 1) % 6;

    if (!app_create(w, app_id, arg)) {
        return NULL;
    }

    if (w->x + w->w > (int)scr_w) w->x = (int)scr_w - w->w;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;

    w->used = true;
    order[win_count++] = idx;
    g_dirty = true;
    return w;
}

static void desktop_launch(int app_id)
{
    int idx = find_app_index(app_id);
    if (idx >= 0) {
        bring_to_front(idx);
    } else {
        desktop_spawn(app_id, NULL);
    }
    menu_open = false;
    menu_hover = -1;
    g_dirty = true;
}

void desktop_request_redraw(void) { g_dirty = true; }

/* ---- drawing -------------------------------------------------------------- */

static void draw_desktop_icons(void)
{
    int icons_y[] = { 24, 144, 264 };
    const char *icons_lbl[] = { "Terminal", "Files", "Editor" };
    int icon_count = 3;

    for (int i = 0; i < icon_count; i++) {
        int y = icons_y[i];
        gfx_fill_rect(24, y, DESK_ICON_W, DESK_ICON_H, rgb(0x24, 0x38, 0x56));
        gfx_draw_rect(24, y, DESK_ICON_W, DESK_ICON_H, rgb(0x3A, 0x46, 0x60));
        int lx = 24 + (DESK_ICON_W - (int)strlen(icons_lbl[i]) * 8) / 2;
        gfx_draw_string(lx, y + DESK_ICON_H + 4, icons_lbl[i],
                        rgb(0xE6, 0xEC, 0xF6), 0, 1, false);
    }
}

static bool in_desktop_icon(int mx, int my, int *icon_idx)
{
    int icons_y[] = { 24, 144, 264 };
    int icon_count = 3;
    for (int i = 0; i < icon_count; i++) {
        if (in_rect(mx, my, 24, icons_y[i], DESK_ICON_W, DESK_ICON_H)) {
            *icon_idx = i;
            return true;
        }
    }
    return false;
}

static void draw_background(void)
{
    for (uint32_t y = 0; y < scr_h; y++) {
        int r = 38 + (-22 * (int)y) / (int)scr_h;
        int g = 72 + (-46 * (int)y) / (int)scr_h;
        int b = 122 + (-70 * (int)y) / (int)scr_h;
        gfx_fill_rect(0, (int)y, (int)scr_w, 1, rgb(r, g, b));
    }
    draw_desktop_icons();
}

static void draw_window(window_t *w, bool focused)
{
    gfx_fill_rect(w->x, w->y, w->w, w->h, rgb(0xEC, 0xEC, 0xEC));
    gfx_draw_rect(w->x, w->y, w->w, w->h, rgb(0x10, 0x18, 0x28));

    uint32_t tb = focused ? rgb(0x2D, 0x6C, 0xDF) : rgb(0x6A, 0x73, 0x86);
    gfx_fill_rect(w->x + 1, w->y + 1, w->w - 2, TITLEBAR_H - 1, tb);
    gfx_draw_string(w->x + 8, w->y + 7, w->title, rgb(255, 255, 255), 0, 1, false);

    int bx = w->x + w->w - 19, by = w->y + 4;
    gfx_fill_rect(bx, by, 14, 14, rgb(0xD0, 0x45, 0x3C));
    gfx_draw_string(bx + 3, by + 3, "x", rgb(255, 255, 255), 0, 1, false);

    gfx_clip_set(win_client_x(w), win_client_y(w), win_client_w(w), win_client_h(w));
    if (w->on_draw) {
        w->on_draw(w);
    }
    gfx_clip_reset();
}

static const char *cursor_bitmap[] = {
    "X         ", "XX        ", "X.X       ", "X..X      ",
    "X...X     ", "X....X    ", "X.....X   ", "X......X  ",
    "X.......X ", "X........X", "X.....XXXX", "X..X..X   ",
    "X.X X..X  ", "XX  X..X  ", "X    X..X ", "     X..X ",
    "      XX  ",
};

static void draw_cursor(int mx, int my)
{
    int rows = (int)(sizeof(cursor_bitmap) / sizeof(cursor_bitmap[0]));
    for (int r = 0; r < rows; r++) {
        const char *line = cursor_bitmap[r];
        for (int c = 0; line[c]; c++) {
            if (line[c] == 'X') gfx_putpixel(mx + c, my + r, rgb(0, 0, 0));
            else if (line[c] == '.') gfx_putpixel(mx + c, my + r, rgb(255, 255, 255));
        }
    }
}

static void draw_taskbar(void)
{
    int y0 = (int)scr_h - TASKBAR_H;
    gfx_fill_rect(0, y0, (int)scr_w, TASKBAR_H, rgb(0x18, 0x20, 0x30));
    gfx_hline(0, y0, (int)scr_w, rgb(0x3A, 0x46, 0x60));

    bool start_hover = in_rect(mouse_x(), mouse_y(), 4, y0 + 4, START_BTN_W - 8, TASKBAR_H - 10);
    gfx_fill_rect(4, y0 + 4, START_BTN_W - 8, TASKBAR_H - 10,
                  start_hover ? rgb(0x39, 0x4C, 0x70) : rgb(0x24, 0x38, 0x56));
    gfx_draw_rect(4, y0 + 4, START_BTN_W - 8, TASKBAR_H - 10, rgb(0x12, 0x18, 0x28));
    gfx_draw_string(12, y0 + (TASKBAR_H - 8) / 2, "Start",
                    rgb(0xE6, 0xEC, 0xF6), 0, 1, false);

    for (int i = 0; i < APP_COUNT; i++) {
        int bx = START_BTN_W + 14 + i * 90;
        bool open = find_app_index(i) >= 0;
        gfx_fill_rect(bx, y0 + 5, 86, TASKBAR_H - 10,
                      open ? rgb(0x39, 0x4C, 0x70) : rgb(0x2C, 0x38, 0x50));
        gfx_draw_rect(bx, y0 + 5, 86, TASKBAR_H - 10, rgb(0x12, 0x18, 0x28));
        gfx_draw_string(bx + 8, y0 + (TASKBAR_H - 8) / 2, app_name(i),
                        rgb(0xE6, 0xEC, 0xF6), 0, 1, false);
    }

    char clk[16];
    uint32_t s = timer_seconds();
    snprintf(clk, sizeof(clk), "%u:%02u", s / 60, s % 60);
    gfx_draw_string((int)scr_w - 56, y0 + (TASKBAR_H - 8) / 2, clk,
                    rgb(255, 255, 255), 0, 1, false);
}

static int start_menu_app_id[] = { APP_TERMINAL, APP_FILES, APP_EDITOR, APP_MONITOR, APP_ABOUT };

static void draw_start_menu(void)
{
    int y0 = (int)scr_h - TASKBAR_H;
    int mx = mouse_x(), my = mouse_y();

    int menu_y = y0 - MENU_ITEM_H * APP_COUNT - 4;
    gfx_fill_rect(4, menu_y, START_MENU_W, MENU_ITEM_H * APP_COUNT,
                  rgb(0x3A, 0x46, 0x60));

    menu_hover = -1;
    for (int i = 0; i < APP_COUNT; i++) {
        int iy = menu_y + i * MENU_ITEM_H;
        bool hover = in_rect(mx, my, 4, iy, START_MENU_W, MENU_ITEM_H);
        if (hover) menu_hover = i;
        gfx_fill_rect(4, iy, START_MENU_W, MENU_ITEM_H,
                      hover ? rgb(0x39, 0x4C, 0x70) : rgb(0x24, 0x38, 0x56));
        gfx_draw_string(16, iy + 6, app_name(i),
                        rgb(0xE6, 0xEC, 0xF6), 0, 1, false);
    }
}

static void desktop_draw(void)
{
    gfx_clip_reset();
    draw_background();
    for (int z = 0; z < win_count; z++) {
        draw_window(&windows[order[z]], order[z] == focus_idx());
    }
    draw_taskbar();
    if (menu_open) draw_start_menu();
    draw_cursor(mouse_x(), mouse_y());
    gfx_present();
}

/* ---- input ---------------------------------------------------------------- */

static int topmost_at(int px, int py)
{
    for (int z = win_count - 1; z >= 0; z--) {
        window_t *w = &windows[order[z]];
        if (in_rect(px, py, w->x, w->y, w->w, w->h)) {
            return order[z];
        }
    }
    return -1;
}

#define MENU_Y() ((int)scr_h - TASKBAR_H - MENU_ITEM_H * APP_COUNT - 4)
#define MENU_BOTTOM_Y() ((int)scr_h - TASKBAR_H - 4)

static void close_start_menu(void)
{
    if (menu_open) {
        menu_open = false;
        menu_hover = -1;
        g_dirty = true;
    }
}

static void handle_mouse(uint8_t prev)
{
    int mx = mouse_x(), my = mouse_y();
    uint8_t b = mouse_buttons();
    bool left = (b & MOUSE_LEFT) != 0;
    bool prev_left = (prev & MOUSE_LEFT) != 0;

    if (dragging) {
        if (left) {
            window_t *w = &windows[drag_idx];
            w->x = mx - drag_dx;
            w->y = my - drag_dy;
            if (w->x < -(w->w - 60)) w->x = -(w->w - 60);
            if (w->x > (int)scr_w - 60) w->x = (int)scr_w - 60;
            if (w->y < 0) w->y = 0;
            if (w->y > (int)scr_h - TASKBAR_H - TITLEBAR_H)
                w->y = (int)scr_h - TASKBAR_H - TITLEBAR_H;
        } else {
            dragging = false;
        }
        return;
    }

    if (left && !prev_left) {
        int y0 = (int)scr_h - TASKBAR_H;

        if (my >= y0) {
            if (in_start_button(mx, my)) {
                menu_open = !menu_open;
                menu_hover = -1;
                g_dirty = true;
                return;
            }
            if (menu_open && in_start_menu(mx, my)) {
                if (menu_hover >= 0) {
                    desktop_launch(start_menu_app_id[menu_hover]);
                }
                g_dirty = true;
                return;
            }
            close_start_menu();
            for (int i = 0; i < APP_COUNT; i++) {
                int bx = START_BTN_W + 14 + i * 90;
                if (mx >= bx && mx < bx + 86) {
                    desktop_launch(i);
                    return;
                }
            }
            return;
        }

        int icon_idx;
        if (in_desktop_icon(mx, my, &icon_idx)) {
            int icons_app[] = { APP_TERMINAL, APP_FILES, APP_EDITOR };
            desktop_launch(icons_app[icon_idx]);
            return;
        }

        if (menu_open && !in_start_menu(mx, my)) {
            close_start_menu();
            return;
        }

        int idx = topmost_at(mx, my);
        if (idx >= 0) {
            bring_to_front(idx);
            window_t *w = &windows[idx];
            if (in_close_button(w, mx, my)) {
                close_window(idx);
            } else if (my < w->y + TITLEBAR_H) {
                dragging = true;
                drag_idx = idx;
                drag_dx = mx - w->x;
                drag_dy = my - w->y;
            } else if (w->on_click) {
                w->on_click(w, mx, my, b);
            }
        }
    }
}

static void open_editor(vfs_node_t *file)
{
    desktop_spawn(APP_EDITOR, file);
}

void desktop_run(void)
{
    scr_w = gfx_width();
    scr_h = gfx_height();

    cmd_set_editor_hook(open_editor);

    window_t *fm = desktop_spawn(APP_FILES, NULL);
    if (fm) { fm->x = 16; fm->y = 56; fm->w = 352; fm->h = 668; }
    window_t *mon = desktop_spawn(APP_MONITOR, NULL);
    if (mon) { mon->x = 384; mon->y = 476; mon->w = 624; mon->h = 248; }
    window_t *tm = desktop_spawn(APP_TERMINAL, NULL);
    if (tm) { tm->x = 384; tm->y = 56; }

    uint8_t prev_buttons = 0;
    uint32_t last_sec = timer_seconds();

    for (;;) {
        bool need = g_dirty;
        g_dirty = false;

        char c;
        while ((c = keyboard_trygetchar()) != 0) {
            window_t *f = focus_window();
            if (f && f->on_key) f->on_key(f, c);
            need = true;
        }

        if (mouse_poll_changed()) {
            handle_mouse(prev_buttons);
            prev_buttons = mouse_buttons();
            need = true;
        }

        uint32_t s = timer_seconds();
        if (s != last_sec) {
            last_sec = s;
            for (int i = 0; i < WIN_MAX; i++) {
                if (windows[i].used && windows[i].on_tick) {
                    windows[i].on_tick(&windows[i]);
                }
            }
            need = true;
        }

        if (need) desktop_draw();
        __asm__ volatile("hlt");
    }
}