/* Physical Memory Manager.
 *
 * A bitmap tracks every 4 KiB physical frame (1 = used, 0 = free). The bitmap
 * is sized statically to cover up to 4 GiB of RAM, which keeps allocation
 * simple and avoids a chicken-and-egg bootstrap problem. The available regions
 * come from the Multiboot memory map; the kernel image and low memory are then
 * reserved so we never hand them out. */

#include "pmm.h"
#include "kernel.h"
#include "printf.h"
#include "string.h"

/* Cover up to 4 GiB: 4 GiB / 4 KiB = 1,048,576 frames -> 32,768 dwords. */
#define MAX_FRAMES (1u << 20)
#define BITMAP_DWORDS (MAX_FRAMES / 32u)

static uint32_t frame_bitmap[BITMAP_DWORDS];
static uint32_t total_frames;
static uint32_t used_frames;
static uint32_t total_memory_kb;

static inline void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame >> 5] |= (1u << (frame & 31u));
}

static inline void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame >> 5] &= ~(1u << (frame & 31u));
}

static inline int bitmap_test(uint32_t frame)
{
    return (frame_bitmap[frame >> 5] >> (frame & 31u)) & 1u;
}

/* Mark [start, end) as used (rounding outward to whole frames). */
static void reserve_region(uint64_t start, uint64_t end)
{
    uint32_t first = (uint32_t)(start / PMM_FRAME_SIZE);
    uint32_t last  = (uint32_t)((end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE);
    for (uint32_t f = first; f < last && f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }
}

/* Mark [start, end) as free (rounding inward to whole frames). */
static void free_region(uint64_t start, uint64_t end)
{
    uint32_t first = (uint32_t)((start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE);
    uint32_t last  = (uint32_t)(end / PMM_FRAME_SIZE);
    for (uint32_t f = first; f < last && f < total_frames; f++) {
        if (bitmap_test(f)) {
            bitmap_clear(f);
            used_frames--;
        }
    }
}

void pmm_init(multiboot_info_t *mbi)
{
    /* 1. Determine how much RAM exists (highest available address). */
    uint64_t highest = 0;

    if (mbi->flags & MULTIBOOT_INFO_MMAP) {
        uint32_t off = 0;
        while (off < mbi->mmap_length) {
            multiboot_mmap_entry_t *e =
                (multiboot_mmap_entry_t *)(uintptr_t)(mbi->mmap_addr + off);
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t top = e->addr + e->len;
                if (top > highest) highest = top;
            }
            off += e->size + sizeof(e->size);
        }
    } else if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
        /* Fallback: mem_upper is KiB above 1 MiB. */
        highest = (uint64_t)0x100000 + (uint64_t)mbi->mem_upper * 1024u;
    } else {
        highest = 16u * 1024u * 1024u;   /* assume 16 MiB if nothing known */
    }

    total_memory_kb = (uint32_t)(highest / 1024u);
    total_frames    = (uint32_t)(highest / PMM_FRAME_SIZE);
    if (total_frames > MAX_FRAMES) {
        total_frames = MAX_FRAMES;
    }

    /* 2. Start with everything marked used. */
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
    used_frames = total_frames;

    /* 3. Free the regions the BIOS reports as available RAM. */
    if (mbi->flags & MULTIBOOT_INFO_MMAP) {
        uint32_t off = 0;
        while (off < mbi->mmap_length) {
            multiboot_mmap_entry_t *e =
                (multiboot_mmap_entry_t *)(uintptr_t)(mbi->mmap_addr + off);
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                free_region(e->addr, e->addr + e->len);
            }
            off += e->size + sizeof(e->size);
        }
    } else {
        free_region(0x100000, highest);
    }

    /* 4. Re-reserve things we must never allocate:
       - the first 1 MiB (BIOS, IVT, VGA, etc.)
       - the kernel image, including BSS and the statically-sized bitmap. */
    reserve_region(0, 0x100000);
    reserve_region((uint64_t)(uintptr_t)__kernel_start,
                   (uint64_t)(uintptr_t)__kernel_end);
}

physaddr_t pmm_alloc_frame(void)
{
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            return (physaddr_t)(f * PMM_FRAME_SIZE);
        }
    }
    return 0;   /* out of physical memory */
}

void pmm_free_frame(physaddr_t addr)
{
    uint32_t f = addr / PMM_FRAME_SIZE;
    if (f < total_frames && bitmap_test(f)) {
        bitmap_clear(f);
        used_frames--;
    }
}

uint32_t pmm_total_frames(void)    { return total_frames; }
uint32_t pmm_used_frames(void)     { return used_frames; }
uint32_t pmm_free_frames(void)     { return total_frames - used_frames; }
uint32_t pmm_total_memory_kb(void) { return total_memory_kb; }
