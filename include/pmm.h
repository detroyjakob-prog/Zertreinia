/* Physical Memory Manager - a bitmap frame allocator (4 KiB frames). */
#ifndef ZERT_PMM_H
#define ZERT_PMM_H

#include "types.h"
#include "multiboot.h"

#define PMM_FRAME_SIZE 4096u

void       pmm_init(multiboot_info_t *mbi);

/* Allocate / free a single 4 KiB physical frame.
   pmm_alloc_frame() returns the physical address, or 0 on failure. */
physaddr_t pmm_alloc_frame(void);
void       pmm_free_frame(physaddr_t addr);

/* Statistics (in 4 KiB frames). */
uint32_t   pmm_total_frames(void);
uint32_t   pmm_used_frames(void);
uint32_t   pmm_free_frames(void);
uint32_t   pmm_total_memory_kb(void);

#endif /* ZERT_PMM_H */
