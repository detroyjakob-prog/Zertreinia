/* Linear-framebuffer graphics with a double buffer and a clip rectangle. */
#ifndef ZERT_GFX_H
#define ZERT_GFX_H

#include "types.h"
#include "multiboot.h"

/* Maximum back-buffer we reserve (must hold the chosen mode). */
#define GFX_MAX_WIDTH  1024
#define GFX_MAX_HEIGHT 768

/* Pack an 0x00RRGGBB colour. */
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Returns true if a usable 32-bpp RGB framebuffer was set up. */
bool     gfx_init(multiboot_info_t *mbi);
bool     gfx_available(void);
uint32_t gfx_width(void);
uint32_t gfx_height(void);

/* All drawing goes to the back buffer; gfx_present() blits it to the screen. */
void gfx_present(void);

void gfx_clear(uint32_t color);
void gfx_putpixel(int x, int y, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);

/* Draw one glyph / string at scale `scale` (1 = 8x8). When `bg_opaque` is
   false the background pixels are left untouched (transparent text). */
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg,
                   int scale, bool bg_opaque);
void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg,
                     int scale, bool bg_opaque);

/* Clip rectangle: drawing is restricted to it until gfx_clip_reset(). */
void gfx_clip_set(int x, int y, int w, int h);
void gfx_clip_reset(void);

#endif /* ZERT_GFX_H */
