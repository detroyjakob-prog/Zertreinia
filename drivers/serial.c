/* 16550 UART serial driver for COM1 (0x3F8).
 *
 * Extremely handy for debugging: with `-serial stdio` in QEMU, everything the
 * kernel prints also appears in your terminal, even before the screen works. */

#include "serial.h"
#include "ports.h"

#define COM1 0x3F8

static bool initialised = false;

void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* disable interrupts                       */
    outb(COM1 + 3, 0x80);   /* enable DLAB (set baud rate divisor)      */
    outb(COM1 + 0, 0x03);   /* divisor low byte  -> 38400 baud          */
    outb(COM1 + 1, 0x00);   /* divisor high byte                        */
    outb(COM1 + 3, 0x03);   /* 8 bits, no parity, one stop bit          */
    outb(COM1 + 2, 0xC7);   /* enable FIFO, clear, 14-byte threshold    */
    outb(COM1 + 4, 0x0B);   /* IRQs enabled, RTS/DSR set                */
    initialised = true;
}

static int transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c)
{
    if (!initialised) {
        return;
    }
    if (c == '\n') {
        /* Most terminals want CRLF. */
        while (!transmit_empty()) { }
        outb(COM1, '\r');
    }
    while (!transmit_empty()) { }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s) {
        serial_putchar(*s++);
    }
}

int serial_received(void)
{
    return inb(COM1 + 5) & 1;
}
