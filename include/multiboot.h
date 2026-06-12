/* Multiboot v1 information structures, as passed by GRUB in EBX. */
#ifndef ZERT_MULTIBOOT_H
#define ZERT_MULTIBOOT_H

#include "types.h"

/* Value placed in EAX by a Multiboot-compliant loader. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Selected bits of the `flags` field that we care about. */
#define MULTIBOOT_INFO_MEMORY      (1u << 0)  /* mem_lower / mem_upper valid */
#define MULTIBOOT_INFO_MMAP        (1u << 6)  /* mmap_* fields valid         */
#define MULTIBOOT_INFO_FRAMEBUFFER (1u << 12) /* framebuffer_* fields valid  */

/* Memory map entry type codes. */
#define MULTIBOOT_MEMORY_AVAILABLE 1

/* framebuffer_type values. */
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA     2

/* The information structure GRUB hands to the kernel. */
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;       /* KiB of lower memory  (< 1 MiB)            */
    uint32_t mem_upper;       /* KiB of upper memory  (>= 1 MiB)           */
    uint32_t boot_device;
    uint32_t cmdline;         /* physical address of the command line      */
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;     /* size in bytes of the memory map buffer    */
    uint32_t mmap_addr;       /* physical address of the first mmap entry  */
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;     /* physical address of the linear FB    */
    uint32_t framebuffer_pitch;    /* bytes per scanline                   */
    uint32_t framebuffer_width;    /* pixels                               */
    uint32_t framebuffer_height;   /* pixels                               */
    uint8_t  framebuffer_bpp;      /* bits per pixel                       */
    uint8_t  framebuffer_type;     /* MULTIBOOT_FRAMEBUFFER_TYPE_*         */
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

/* One entry in the BIOS memory map. NOTE: `size` does NOT include itself,
   so the next entry is at (uint8_t*)entry + entry->size + 4. */
typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif /* ZERT_MULTIBOOT_H */
