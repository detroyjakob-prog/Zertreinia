/* Formatted output core.
 *
 * A single character-sink core drives kprintf (to VGA + serial) and the
 * snprintf family (to a caller-provided buffer), so the GUI can format text
 * into strings without touching the console. */

#include "printf.h"
#include "vga.h"
#include "serial.h"
#include "string.h"

typedef void (*putc_fn)(void *ctx, char c);

static void p_str(putc_fn put, void *ctx, const char *s)
{
    while (*s) {
        put(ctx, *s++);
    }
}

static void p_pad(putc_fn put, void *ctx, int n, char pad)
{
    while (n-- > 0) {
        put(ctx, pad);
    }
}

/* Convert an unsigned value to a string in the given base. */
static int format_uint(char *buf, uint32_t value, uint32_t base, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[32];
    int i = 0;

    if (value == 0) {
        tmp[i++] = '0';
    }
    while (value != 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    int len = i;
    for (int j = 0; j < len; j++) {
        buf[j] = tmp[len - 1 - j];
    }
    buf[len] = '\0';
    return len;
}

static void core_vprintf(putc_fn put, void *ctx, const char *fmt, va_list args)
{
    char numbuf[34];

    while (*fmt) {
        if (*fmt != '%') {
            put(ctx, *fmt++);
            continue;
        }
        fmt++;   /* skip '%' */

        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            put(ctx, (char)va_arg(args, int));
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            p_pad(put, ctx, width - (int)strlen(s), ' ');
            p_str(put, ctx, s);
            break;
        }
        case 'd':
        case 'i': {
            int v = va_arg(args, int);
            uint32_t mag = (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
            int len = format_uint(numbuf, mag, 10, 0);
            int total = len + (v < 0 ? 1 : 0);
            if (pad == '0' && v < 0) {
                put(ctx, '-');
                p_pad(put, ctx, width - total, '0');
            } else {
                p_pad(put, ctx, width - total, pad);
                if (v < 0) put(ctx, '-');
            }
            p_str(put, ctx, numbuf);
            break;
        }
        case 'u': {
            uint32_t v = va_arg(args, uint32_t);
            int len = format_uint(numbuf, v, 10, 0);
            p_pad(put, ctx, width - len, pad);
            p_str(put, ctx, numbuf);
            break;
        }
        case 'x':
        case 'X': {
            uint32_t v = va_arg(args, uint32_t);
            int len = format_uint(numbuf, v, 16, (*fmt == 'X'));
            p_pad(put, ctx, width - len, pad);
            p_str(put, ctx, numbuf);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(args, void *);
            p_str(put, ctx, "0x");
            int len = format_uint(numbuf, (uint32_t)v, 16, 0);
            p_pad(put, ctx, 8 - len, '0');
            p_str(put, ctx, numbuf);
            break;
        }
        case '%':
            put(ctx, '%');
            break;
        case '\0':
            return;
        default:
            put(ctx, '%');
            put(ctx, *fmt);
            break;
        }
        fmt++;
    }
}

/* ---- console sink (VGA + serial) ------------------------------------------ */

static void console_put(void *ctx, char c)
{
    UNUSED(ctx);
    vga_putchar(c);
    serial_putchar(c);
}

void kvprintf(const char *fmt, va_list args)
{
    core_vprintf(console_put, NULL, fmt, args);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    core_vprintf(console_put, NULL, fmt, args);
    va_end(args);
}

/* ---- buffer sink (snprintf) ----------------------------------------------- */

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} buf_sink_t;

static void buf_put(void *ctx, char c)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    if (s->cap != 0 && s->len + 1 < s->cap) {
        s->buf[s->len++] = c;
    }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    buf_sink_t s = { buf, size, 0 };
    core_vprintf(buf_put, &s, fmt, args);
    if (size != 0) {
        buf[s.len] = '\0';
    }
    return (int)s.len;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return n;
}
