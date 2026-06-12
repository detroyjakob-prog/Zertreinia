/* Low-level port I/O primitives (x86 IN/OUT instructions). */
#ifndef ZERT_PORTS_H
#define ZERT_PORTS_H

#include "types.h"

/* Write a byte to an I/O port. */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a byte from an I/O port. */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Write a 16-bit word to an I/O port. */
static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a 16-bit word from an I/O port. */
static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Write a 32-bit dword to an I/O port. */
static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a 32-bit dword from an I/O port. */
static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Short, well-defined delay by writing to an unused diagnostic port.
   Useful when programming the PIC/PIT, which need a moment to settle. */
static inline void io_wait(void)
{
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

#endif /* ZERT_PORTS_H */
