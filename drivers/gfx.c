/* Linear-framebuffer graphics driver.
 *
 * GRUB sets a 32-bpp linear video mode for us (requested via the Multiboot
 * header) and reports the framebuffer in the Multiboot info. We draw into a
 * back buffer in RAM and copy it to the framebuffer in one shot
 * (gfx_present), which is flicker-free and keeps per-pixel writes off the
 * slow MMIO framebuffer. */

#include "gfx.h"
#include "font.h"
#include "paging.h"
#include "string.h"

static volatile uint8_t *fb;      /* linear framebuffer (identity mapped) */
static uint32_t fb_pitch;         /* framebuffer bytes per scanline       */
static uint32_t scr_w, scr_h;     /* active resolution                    */
static bool     available = false;

/* Back buffer: tightly packed at the active width. */
static uint32_t backbuffer[GFX_MAX_WIDTH * GFX_MAX_HEIGHT];

/* Current clip rectangle. */
static int clip_x0, clip_y0, clip_x1, clip_y1;

bool gfx_init(multiboot_info_t *mbi)
{
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER)) {
        return false;
    }
    if (mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB ||
        mbi->framebuffer_bpp != 32) {
        return false;   /* we only support 32-bpp RGB */
    }

    uint32_t addr = (uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFFu);
    scr_w   = mbi->framebuffer_width;
    scr_h   = mbi->framebuffer_height;
    fb_pitch = mbi->framebuffer_pitch;

    if (scr_w > GFX_MAX_WIDTH)  scr_w = GFX_MAX_WIDTH;
    if (scr_h > GFX_MAX_HEIGHT) scr_h = GFX_MAX_HEIGHT;

    /* Make the framebuffer reachable (it lives at a high physical address). */
    paging_identity_map_4mb(addr, fb_pitch * mbi->framebuffer_height);
    fb = (volatile uint8_t *)addr;

    available = true;
    gfx_clip_reset();
    gfx_clear(0x000000);
    gfx_present();
    return true;
}

bool     gfx_available(void) { return available; }
uint32_t gfx_width(void)     { return scr_w; }
uint32_t gfx_height(void)    { return scr_h; }

void gfx_clip_set(int x, int y, int w, int h)
{
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = x + w; if (clip_x1 > (int)scr_w) clip_x1 = scr_w;
    clip_y1 = y + h; if (clip_y1 > (int)scr_h) clip_y1 = scr_h;
}

void gfx_clip_reset(void)
{
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = (int)scr_w;
    clip_y1 = (int)scr_h;
}

void gfx_putpixel(int x, int y, uint32_t color)
{
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) {
        return;
    }
    backbuffer[(uint32_t)y * scr_w + (uint32_t)x] = color;
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < clip_x0 ? clip_x0 : x;
    int y0 = y < clip_y0 ? clip_y0 : y;
    int x1 = x + w; if (x1 > clip_x1) x1 = clip_x1;
    int y1 = y + h; if (y1 > clip_y1) y1 = clip_y1;

    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = &backbuffer[(uint32_t)yy * scr_w];
        for (int xx = x0; xx < x1; xx++) {
            row[xx] = color;
        }
    }
}

void gfx_hline(int x, int y, int w, uint32_t color) { gfx_fill_rect(x, y, w, 1, color); }
void gfx_vline(int x, int y, int h, uint32_t color) { gfx_fill_rect(x, y, 1, h, color); }

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color)
{
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_clear(uint32_t color)
{
    for (uint32_t i = 0; i < scr_w * scr_h; i++) {
        backbuffer[i] = color;
    }
}

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg,
                   int scale, bool bg_opaque)
{
    unsigned char uc = (unsigned char)c;
    if (uc >= 128) uc = '?';
    const uint8_t *glyph = font8x8_basic[uc];

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            bool on = (bits >> col) & 1u;
            if (on) {
                gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, fg);
            } else if (bg_opaque) {
                gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, bg);
            }
        }
    }
}

void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg,
                     int scale, bool bg_opaque)
{
    int cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += FONT_HEIGHT * scale;
        } else {
            gfx_draw_char(cx, y, *s, fg, bg, scale, bg_opaque);
            cx += FONT_WIDTH * scale;
        }
        s++;
    }
}

void gfx_present(void)
{
    if (!available) {
        return;
    }
    /* Copy each scanline of the back buffer to the framebuffer, honouring the
       framebuffer pitch (which may exceed width * 4). */
    for (uint32_t y = 0; y < scr_h; y++) {
        volatile uint8_t *dst = fb + (uint32_t)y * fb_pitch;
        const uint32_t *src = &backbuffer[(uint32_t)y * scr_w];
        memcpy((void *)dst, src, scr_w * 4u);
    }
}
