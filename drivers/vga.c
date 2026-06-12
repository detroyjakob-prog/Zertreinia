/* VGA text-mode console: 80x25 cells at physical address 0xB8000.
 *
 * Each cell is two bytes: the ASCII character and an attribute byte
 * (high nibble = background colour, low nibble = foreground colour). */

#include "vga.h"
#include "ports.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)

#define CRTC_INDEX  0x3D4
#define CRTC_DATA   0x3D5

static size_t   cursor_row;
static size_t   cursor_col;
static uint8_t  term_color;

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

uint8_t vga_make_color(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)fg | (uint8_t)(bg << 4);
}

void vga_set_color(uint8_t color) { term_color = color; }
uint8_t vga_get_color(void)       { return term_color; }

/* Tell the VGA hardware where to draw the blinking cursor. */
static void vga_update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);

    outb(CRTC_INDEX, 0x0F);
    outb(CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(CRTC_INDEX, 0x0E);
    outb(CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

void vga_init(void)
{
    term_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

/* Scroll the screen up by one row when the cursor runs off the bottom. */
static void vga_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    /* Clear the now-duplicated last row. */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c)
{
    switch (c) {
    case '\n':
        cursor_col = 0;
        cursor_row++;
        break;
    case '\r':
        cursor_col = 0;
        break;
    case '\t':
        cursor_col = (cursor_col + 4) & ~(size_t)3;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
        break;
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', term_color);
        break;
    default:
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, term_color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
        break;
    }

    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
    vga_update_cursor();
}

void vga_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        vga_putchar(data[i]);
    }
}

void vga_puts(const char *str)
{
    while (*str) {
        vga_putchar(*str++);
    }
}
