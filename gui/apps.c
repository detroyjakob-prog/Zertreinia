/* Built-in desktop applications: Terminal, File Manager, Text Editor,
 * System Monitor and About. Each app fills in a window's callbacks and a
 * private state object. */

#include "apps.h"
#include "gfx.h"
#include "commands.h"
#include "vfs.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "string.h"
#include "printf.h"
#include "ports.h"
#include "kernel.h"

/* Shared colours. */
#define COL_TEXT     rgb(0x10, 0x14, 0x1C)
#define COL_TERM_BG  rgb(0x12, 0x14, 0x1C)
#define COL_TERM_FG  rgb(0x76, 0xF1, 0x90)
#define COL_PANEL    rgb(0xF4, 0xF5, 0xF9)

static void app_reboot(void)
{
    uint8_t status = 0x02;
    while (status & 0x02) {
        status = inb(0x64);
    }
    outb(0x64, 0xFE);
    halt();
}

const char *app_name(int app_id)
{
    static const char *names[APP_COUNT] = {
        "Terminal", "Files", "Editor", "Monitor", "About"
    };
    return (app_id >= 0 && app_id < APP_COUNT) ? names[app_id] : "?";
}

/* ======================================================================== *
 *  Terminal
 * ======================================================================== */

#define T_COLS_MAX 110
#define T_ROWS_MAX 60

typedef struct {
    int        cols, rows;
    char       grid[T_ROWS_MAX * T_COLS_MAX];
    int        cx, cy;
    char       input[256];
    int        inlen;
    vfs_node_t *cwd;
} term_t;

static void term_clear(term_t *t)
{
    memset(t->grid, ' ', (size_t)t->rows * t->cols);
    t->cx = 0;
    t->cy = 0;
}

static void term_scroll(term_t *t)
{
    memmove(t->grid, t->grid + t->cols, (size_t)(t->rows - 1) * t->cols);
    memset(t->grid + (size_t)(t->rows - 1) * t->cols, ' ', t->cols);
    t->cy = t->rows - 1;
}

static void term_putc(term_t *t, char c)
{
    if (c == '\n') {
        t->cx = 0;
        if (++t->cy >= t->rows) term_scroll(t);
        return;
    }
    if (c == '\r') { t->cx = 0; return; }
    if (c == '\t') { int n = 4 - (t->cx & 3); while (n--) term_putc(t, ' '); return; }
    if (c == '\b') {
        if (t->cx > 0) { t->cx--; t->grid[t->cy * t->cols + t->cx] = ' '; }
        return;
    }
    t->grid[t->cy * t->cols + t->cx] = c;
    if (++t->cx >= t->cols) {
        t->cx = 0;
        if (++t->cy >= t->rows) term_scroll(t);
    }
}

static void term_puts(term_t *t, const char *s)
{
    while (*s) term_putc(t, *s++);
}

static void term_sink(void *ctx, const char *s)
{
    term_puts((term_t *)ctx, s);
}

static void term_prompt(term_t *t)
{
    char path[128];
    vfs_abspath(t->cwd, path, sizeof(path));
    term_puts(t, "zertreinia:");
    term_puts(t, path);
    term_puts(t, "$ ");
}

static void term_key(window_t *w, char c)
{
    term_t *t = (term_t *)w->state;

    if (c == '\n') {
        term_putc(t, '\n');
        t->input[t->inlen] = '\0';
        cmd_out_t out = { term_sink, t };
        cmd_result_t r = cmd_execute(t->input, &out, &t->cwd);
        t->inlen = 0;
        if (r == CMD_CLEAR)       term_clear(t);
        else if (r == CMD_REBOOT) app_reboot();
        else if (r == CMD_HALT)   halt();
        term_prompt(t);
    } else if (c == '\b') {
        if (t->inlen > 0) { t->inlen--; term_putc(t, '\b'); }
    } else if (c >= 32 && c < 127) {
        if (t->inlen < (int)sizeof(t->input) - 1) {
            t->input[t->inlen++] = c;
            term_putc(t, c);
        }
    }
}

static void term_draw(window_t *w)
{
    term_t *t = (term_t *)w->state;
    int cx = win_client_x(w), cy = win_client_y(w);
    gfx_fill_rect(cx, cy, win_client_w(w), win_client_h(w), COL_TERM_BG);

    for (int row = 0; row < t->rows; row++) {
        for (int col = 0; col < t->cols; col++) {
            char ch = t->grid[row * t->cols + col];
            if (ch != ' ') {
                gfx_draw_char(cx + col * 8, cy + row * 8, ch, COL_TERM_FG, 0, 1, false);
            }
        }
    }
    /* cursor */
    gfx_fill_rect(cx + t->cx * 8, cy + t->cy * 8 + 7, 7, 2, COL_TERM_FG);
}

static bool term_init(window_t *w)
{
    w->w = 624;
    w->h = 404;
    strcpy(w->title, "Terminal");

    term_t *t = (term_t *)kmalloc(sizeof(term_t));
    if (!t) return false;
    memset(t, 0, sizeof(*t));

    t->cols = win_client_w(w) / 8;
    t->rows = win_client_h(w) / 8;
    if (t->cols > T_COLS_MAX) t->cols = T_COLS_MAX;
    if (t->rows > T_ROWS_MAX) t->rows = T_ROWS_MAX;
    term_clear(t);
    t->cwd = vfs_root();

    w->state = t;
    w->on_key = term_key;
    w->on_draw = term_draw;

    term_puts(t, "Zertreinia OS Terminal.  Type 'help' for commands.\n\n");
    term_prompt(t);
    return true;
}

/* ======================================================================== *
 *  File Manager
 * ======================================================================== */

#define FM_ROW_H 12

typedef struct {
    vfs_node_t *dir;
} fm_t;

static void fm_draw(window_t *w)
{
    fm_t *fm = (fm_t *)w->state;
    int cx = win_client_x(w), cy = win_client_y(w);
    int cw = win_client_w(w), ch = win_client_h(w);

    gfx_fill_rect(cx, cy, cw, ch, COL_PANEL);

    char path[160];
    vfs_abspath(fm->dir, path, sizeof(path));
    gfx_fill_rect(cx, cy, cw, 18, rgb(0xDD, 0xE3, 0xEE));
    gfx_draw_string(cx + 6, cy + 5, path, rgb(0x22, 0x2A, 0x3A), 0, 1, false);

    int y = cy + 22;
    gfx_draw_string(cx + 8, y + 2, "[..]", rgb(0x2A, 0x44, 0x9A), 0, 1, false);
    y += FM_ROW_H;

    for (vfs_node_t *c = fm->dir->children; c; c = c->next) {
        if (y > cy + ch - FM_ROW_H) break;
        bool isdir = (c->type == VFS_DIR);
        char line[96];
        snprintf(line, sizeof(line), "%s %s", isdir ? "[D]" : "   ", c->name);
        gfx_draw_string(cx + 8, y + 2, line,
                        isdir ? rgb(0x1E, 0x40, 0x9E) : COL_TEXT, 0, 1, false);
        if (!isdir) {
            char sz[16];
            snprintf(sz, sizeof(sz), "%u B", c->size);
            gfx_draw_string(cx + cw - 70, y + 2, sz, rgb(0x70, 0x78, 0x88), 0, 1, false);
        }
        y += FM_ROW_H;
    }
}

static void fm_click(window_t *w, int mx, int my, uint8_t buttons)
{
    fm_t *fm = (fm_t *)w->state;
    int listtop = win_client_y(w) + 22;
    int idx = (my - listtop) / FM_ROW_H;
    if (idx < 0) return;

    if (idx == 0) {
        if (fm->dir->parent) fm->dir = fm->dir->parent;
        desktop_request_redraw();
        return;
    }

    int ci = idx - 1;
    vfs_node_t *c = fm->dir->children;
    while (ci-- > 0 && c) c = c->next;
    if (!c) return;

    if (c->type == VFS_DIR) {
        fm->dir = c;
    } else {
        desktop_spawn(APP_EDITOR, c);
    }
    desktop_request_redraw();
}

static bool fm_init(window_t *w)
{
    w->w = 440;
    w->h = 360;
    strcpy(w->title, "File Manager");

    fm_t *fm = (fm_t *)kmalloc(sizeof(fm_t));
    if (!fm) return false;
    fm->dir = vfs_root();

    w->state = fm;
    w->on_draw = fm_draw;
    w->on_click = fm_click;
    return true;
}

/* ======================================================================== *
 *  Text Editor
 * ======================================================================== */

#define ED_MAX 8192

typedef struct {
    vfs_node_t *file;
    int         len;
    char        buf[ED_MAX];
} ed_t;

static void ed_key(window_t *w, char c)
{
    ed_t *e = (ed_t *)w->state;
    if (c == '\b') {
        if (e->len > 0) e->len--;
    } else if (c == '\n' || (c >= 32 && c < 127)) {
        if (e->len < ED_MAX - 1) e->buf[e->len++] = c;
    }
    desktop_request_redraw();
}

static void ed_click(window_t *w, int mx, int my, uint8_t buttons)
{
    ed_t *e = (ed_t *)w->state;
    int cx = win_client_x(w), cy = win_client_y(w);
    /* Save button. */
    if (mx >= cx + 6 && mx < cx + 60 && my >= cy + 3 && my < cy + 17) {
        if (e->file) {
            vfs_write(e->file, e->buf, (uint32_t)e->len);
        }
        desktop_request_redraw();
    }
}

static void ed_draw(window_t *w)
{
    ed_t *e = (ed_t *)w->state;
    int cx = win_client_x(w), cy = win_client_y(w);
    int cw = win_client_w(w), ch = win_client_h(w);

    gfx_fill_rect(cx, cy, cw, ch, rgb(0xFF, 0xFF, 0xFF));

    /* Toolbar. */
    gfx_fill_rect(cx, cy, cw, 20, rgb(0xDD, 0xE2, 0xEA));
    gfx_fill_rect(cx + 6, cy + 3, 54, 14, rgb(0x37, 0x86, 0x44));
    gfx_draw_string(cx + 15, cy + 6, "Save", rgb(255, 255, 255), 0, 1, false);
    char info[80];
    snprintf(info, sizeof(info), "%s  (%d bytes)",
             e->file ? e->file->name : "untitled", e->len);
    gfx_draw_string(cx + 70, cy + 6, info, rgb(0x33, 0x3A, 0x48), 0, 1, false);

    /* Text area with wrapping. */
    int tx = cx + 4, ty = cy + 24;
    int maxcols = (cw - 8) / 8;
    int maxrows = (ch - 26) / 8;
    if (maxcols < 1) maxcols = 1;
    int col = 0, row = 0;
    for (int i = 0; i < e->len; i++) {
        char c = e->buf[i];
        if (c == '\n') { col = 0; row++; continue; }
        if (col >= maxcols) { col = 0; row++; }
        if (row >= maxrows) break;
        gfx_draw_char(tx + col * 8, ty + row * 8, c, COL_TEXT, 0, 1, false);
        col++;
    }
    if (row < maxrows) {
        gfx_fill_rect(tx + col * 8, ty + row * 8, 2, 8, rgb(0x20, 0x40, 0xA0));
    }
}

static bool ed_init(window_t *w, vfs_node_t *file)
{
    w->w = 540;
    w->h = 400;
    snprintf(w->title, sizeof(w->title), "Editor - %s",
             file ? file->name : "untitled");

    ed_t *e = (ed_t *)kmalloc(sizeof(ed_t));
    if (!e) return false;
    e->file = file;
    e->len = 0;
    if (file && file->data && file->size > 0) {
        e->len = (int)(file->size < ED_MAX - 1 ? file->size : ED_MAX - 1);
        memcpy(e->buf, file->data, (size_t)e->len);
    }

    w->state = e;
    w->on_key = ed_key;
    w->on_click = ed_click;
    w->on_draw = ed_draw;
    return true;
}

/* ======================================================================== *
 *  System Monitor
 * ======================================================================== */

static void bar(int x, int y, int w, int h, uint32_t used, uint32_t total,
                uint32_t color)
{
    gfx_fill_rect(x, y, w, h, rgb(0x2C, 0x34, 0x42));
    gfx_draw_rect(x, y, w, h, rgb(0x10, 0x16, 0x20));
    if (total > 0) {
        uint32_t fillw = (uint32_t)(w - 2) * used / total;
        gfx_fill_rect(x + 1, y + 1, (int)fillw, h - 2, color);
    }
}

static void sm_draw(window_t *w)
{
    int cx = win_client_x(w), cy = win_client_y(w);
    int cw = win_client_w(w), ch = win_client_h(w);
    gfx_fill_rect(cx, cy, cw, ch, rgb(0x1B, 0x21, 0x2C));

    uint32_t total = pmm_total_frames();
    uint32_t used  = pmm_used_frames();
    size_t ht, hu, hf;
    heap_stats(&ht, &hu, &hf);

    char line[96];
    int y = cy + 8;
    gfx_draw_string(cx + 10, y, "System Monitor", rgb(0x9F, 0xC4, 0xFF), 0, 2, false);
    y += 26;

    snprintf(line, sizeof(line), "RAM  %u / %u KiB", used * 4, total * 4);
    gfx_draw_string(cx + 10, y, line, rgb(0xDD, 0xE4, 0xF0), 0, 1, false);
    y += 12;
    bar(cx + 10, y, cw - 20, 16, used, total, rgb(0x46, 0xC0, 0x6A));
    y += 26;

    snprintf(line, sizeof(line), "Heap %u / %u KiB",
             (uint32_t)hu / 1024, (uint32_t)ht / 1024);
    gfx_draw_string(cx + 10, y, line, rgb(0xDD, 0xE4, 0xF0), 0, 1, false);
    y += 12;
    bar(cx + 10, y, cw - 20, 16, (uint32_t)hu, (uint32_t)ht, rgb(0x4C, 0x9A, 0xF0));
    y += 28;

    snprintf(line, sizeof(line), "Uptime : %u s  (%u ticks)",
             timer_seconds(), timer_ticks());
    gfx_draw_string(cx + 10, y, line, rgb(0xCF, 0xD6, 0xE2), 0, 1, false);
    y += 14;
    snprintf(line, sizeof(line), "Frames : %u used / %u total", used, total);
    gfx_draw_string(cx + 10, y, line, rgb(0xCF, 0xD6, 0xE2), 0, 1, false);
    y += 14;
    snprintf(line, sizeof(line), "Tick Hz: %u", timer_frequency());
    gfx_draw_string(cx + 10, y, line, rgb(0xCF, 0xD6, 0xE2), 0, 1, false);
}

static bool sm_init(window_t *w)
{
    w->w = 360;
    w->h = 260;
    strcpy(w->title, "System Monitor");
    w->state = NULL;
    w->on_draw = sm_draw;
    return true;
}

/* ======================================================================== *
 *  About
 * ======================================================================== */

static void about_draw(window_t *w)
{
    int cx = win_client_x(w), cy = win_client_y(w);
    int cw = win_client_w(w), ch = win_client_h(w);
    gfx_fill_rect(cx, cy, cw, ch, rgb(0xF7, 0xF9, 0xFF));

    gfx_draw_string(cx + 20, cy + 18, "Zertreinia OS", rgb(0x21, 0x3A, 0x8F), 0, 3, false);
    gfx_draw_string(cx + 22, cy + 52, "version " KERNEL_VERSION, rgb(0x66, 0x6E, 0x80), 0, 1, false);

    const char *lines[] = {
        "A 32-bit x86 operating system written from",
        "scratch in C and Assembly.",
        "",
        "  - Multiboot/GRUB boot",
        "  - GDT, IDT, PIC, PIT timer",
        "  - PS/2 keyboard + mouse",
        "  - Paging + physical allocator + heap",
        "  - Linear-framebuffer graphics",
        "  - In-memory filesystem (VFS)",
        "  - Windowed desktop + applications",
        "",
        "Use the taskbar to launch apps. Drag",
        "title bars; click [x] to close.",
    };
    int y = cy + 74;
    for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        gfx_draw_string(cx + 20, y, lines[i], rgb(0x2A, 0x30, 0x3E), 0, 1, false);
        y += 12;
    }
}

static bool about_init(window_t *w)
{
    w->w = 420;
    w->h = 320;
    strcpy(w->title, "About Zertreinia OS");
    w->state = NULL;
    w->on_draw = about_draw;
    return true;
}

/* ======================================================================== */

bool app_create(window_t *w, int app_id, void *arg)
{
    switch (app_id) {
    case APP_TERMINAL: return term_init(w);
    case APP_FILES:    return fm_init(w);
    case APP_EDITOR:   return ed_init(w, (vfs_node_t *)arg);
    case APP_MONITOR:  return sm_init(w);
    case APP_ABOUT:    return about_init(w);
    default:           return false;
    }
}
