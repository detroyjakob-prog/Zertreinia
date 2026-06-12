/* Paging.
 *
 * We set up a simple identity map: virtual address X maps to physical
 * address X for the first PAGING_IDENTITY_MB megabytes. That covers the
 * kernel, the heap and every early structure, so enabling the MMU does not
 * move anything around. This gives us a working, fault-handled address space
 * to build on, without the complexity of a higher-half remap. */

#include "paging.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2

#define ENTRIES_PER_TABLE 1024
#define PAGE_SIZE         4096
#define NUM_TABLES        (PAGING_IDENTITY_MB / 4u)   /* one table maps 4 MiB */

/* Page directory and tables must be 4 KiB aligned. They live in BSS, which
   the boot stub has already zeroed (so unused entries are "not present"). */
static uint32_t page_directory[ENTRIES_PER_TABLE]      __attribute__((aligned(PAGE_SIZE)));
static uint32_t page_tables[NUM_TABLES][ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));

static bool enabled = false;

void paging_init(void)
{
    /* Build NUM_TABLES page tables that identity-map the low memory. */
    for (uint32_t t = 0; t < NUM_TABLES; t++) {
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
            uint32_t phys = (t * ENTRIES_PER_TABLE + i) * PAGE_SIZE;
            page_tables[t][i] = phys | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[t] = ((uint32_t)&page_tables[t][0]) | PAGE_PRESENT | PAGE_RW;
    }

    /* Load CR3 with the page directory and turn on the paging bit in CR0. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(&page_directory[0]));

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;   /* set PG (paging enable) */
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    enabled = true;
}

bool paging_enabled(void) { return enabled; }

#define PAGE_4MB     0x80u           /* PS bit in a page-directory entry */
#define SIZE_4MB     0x00400000u
#define MASK_4MB     0xFFC00000u

/* Identity-map [phys_start, phys_start+size) with 4 MiB pages. The linear
   framebuffer typically lives at a high physical address (e.g. 0xFD000000)
   that is far outside the low identity map, so we add big page-directory
   entries to reach it without allocating extra page tables. */
void paging_identity_map_4mb(uint32_t phys_start, uint32_t size)
{
    /* Enable 4 MiB pages (Page Size Extension) in CR4. */
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1u << 4);   /* CR4.PSE */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    uint32_t start = phys_start & MASK_4MB;
    uint32_t end   = (phys_start + size + (SIZE_4MB - 1)) & MASK_4MB;

    for (uint32_t addr = start; addr < end; addr += SIZE_4MB) {
        uint32_t pde_index = addr >> 22;
        page_directory[pde_index] = addr | PAGE_PRESENT | PAGE_RW | PAGE_4MB;
    }

    /* Flush the TLB by reloading CR3. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(&page_directory[0]));
}
