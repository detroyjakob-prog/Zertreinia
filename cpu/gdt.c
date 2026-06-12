/* Global Descriptor Table.
 *
 * We use a classic "flat" memory model: a kernel/user code and data segment
 * that each span the entire 4 GiB address space. Real protection comes later
 * from paging; the GDT mostly exists because x86 requires valid segment
 * descriptors to run in protected mode. */

#include "gdt.h"

/* One 8-byte GDT entry. */
struct gdt_entry {
    uint16_t limit_low;    /* limit bits 0..15                         */
    uint16_t base_low;     /* base  bits 0..15                         */
    uint8_t  base_mid;     /* base  bits 16..23                        */
    uint8_t  access;       /* access flags (present, ring, type)       */
    uint8_t  granularity;  /* limit bits 16..19 + flags (granularity)  */
    uint8_t  base_high;    /* base  bits 24..31                        */
} __attribute__((packed));

/* The value loaded by `lgdt`. */
struct gdt_ptr {
    uint16_t limit;        /* size of the table - 1   */
    uint32_t base;         /* linear address of entry 0 */
} __attribute__((packed));

#define GDT_ENTRIES 5

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_pointer;

/* Implemented in cpu/flush.asm. */
extern void gdt_flush(uint32_t gdt_ptr_address);

static void gdt_set_gate(int idx, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran)
{
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[idx].granularity |= (uint8_t)(gran & 0xF0);

    gdt[idx].access      = access;
}

void gdt_init(void)
{
    gdt_pointer.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_pointer.base  = (uint32_t)&gdt;

    /* 0x00 - null descriptor (required). */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Access byte: P=1, DPL, S=1 (code/data), then type bits.
       Granularity: G=1 (4 KiB), D/B=1 (32-bit) -> 0xCF. */

    /* 0x08 - kernel code: present, ring 0, executable, readable. */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    /* 0x10 - kernel data: present, ring 0, writable. */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);
    /* 0x18 - user code: present, ring 3, executable, readable. */
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);
    /* 0x20 - user data: present, ring 3, writable. */
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

    gdt_flush((uint32_t)&gdt_pointer);
}
