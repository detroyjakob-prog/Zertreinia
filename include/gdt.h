/* Global Descriptor Table (flat memory model). */
#ifndef ZERT_GDT_H
#define ZERT_GDT_H

#include "types.h"

/* Segment selectors (offsets into the GDT). */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20

void gdt_init(void);

#endif /* ZERT_GDT_H */
