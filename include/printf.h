/* Tiny formatted-output implementation for the kernel.
 *
 * Output is mirrored to both the VGA console and the serial port.
 * Supported conversions: %c %s %d %i %u %x %X %p %% with optional
 * field width and zero padding (e.g. %08x). */
#ifndef ZERT_PRINTF_H
#define ZERT_PRINTF_H

#include "types.h"

void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);

/* Format into a fixed-size buffer (always NUL-terminates when size > 0).
   Returns the number of characters written (excluding the NUL). */
int  snprintf(char *buf, size_t size, const char *fmt, ...);
int  vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif /* ZERT_PRINTF_H */
