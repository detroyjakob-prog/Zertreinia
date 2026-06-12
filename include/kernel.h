/* Kernel-wide helpers. */
#ifndef ZERT_KERNEL_H
#define ZERT_KERNEL_H

#include "types.h"

/* Symbols provided by the linker script (linker.ld). */
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

/* Halt the CPU permanently after printing a message. Never returns. */
void panic(const char *message) __attribute__((noreturn));

/* Disable interrupts and stop the CPU. Never returns. */
void halt(void) __attribute__((noreturn));

#endif /* ZERT_KERNEL_H */
