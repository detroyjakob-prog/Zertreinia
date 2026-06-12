/* Zertreinia OS - kernel entry point.
 *
 * Called by the boot stub (boot/boot.asm) with the Multiboot magic value and
 * a pointer to the Multiboot information structure. This function brings up
 * every subsystem in dependency order and then hands control to the shell. */

#include "kernel.h"
#include "multiboot.h"
#include "vga.h"
#include "serial.h"
#include "printf.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "shell.h"
#include "gfx.h"
#include "mouse.h"
#include "vfs.h"
#include "desktop.h"

void halt(void)
{
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
    __builtin_unreachable();
}

void panic(const char *message)
{
    __asm__ volatile("cli");

    /* Show the panic on the framebuffer if graphics are up, otherwise fall
       back to the VGA text console. Always log to serial. */
    if (gfx_available()) {
        gfx_clip_reset();
        gfx_clear(rgb(0x88, 0x12, 0x12));
        gfx_draw_string(40, 50, "KERNEL PANIC", rgb(255, 255, 255), 0, 4, false);
        gfx_draw_string(40, 110, message, rgb(255, 255, 255), 0, 2, false);
        gfx_draw_string(40, 150, "System halted.", rgb(255, 220, 220), 0, 2, false);
        gfx_present();
    } else {
        vga_set_color(vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    }
    kprintf("\n\n*** KERNEL PANIC ***\n%s\nSystem halted.\n", message);
    halt();
}

static void print_banner(void)
{
    vga_set_color(vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("================================================================\n");
    kprintf("            %s  v%s\n", KERNEL_NAME, KERNEL_VERSION);
    kprintf("     A small 32-bit x86 operating system in C and Assembly\n");
    kprintf("================================================================\n");
    vga_set_color(vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

static void log_step(const char *name)
{
    vga_set_color(vga_make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("  [ OK ] ");
    vga_set_color(vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    kprintf("%s\n", name);
}

void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    /* Earliest possible output: serial first (works even if VGA is odd),
       then the VGA text console. */
    serial_init();
    vga_init();

    print_banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("WARNING: bad Multiboot magic 0x%08x (expected 0x%08x)\n",
                magic, MULTIBOOT_BOOTLOADER_MAGIC);
    }

    kprintf("\nBringing up subsystems:\n");

    gdt_init();        log_step("Global Descriptor Table");
    idt_init();        log_step("Interrupt Descriptor Table + PIC remap");
    timer_init(100);   log_step("PIT timer @ 100 Hz");
    keyboard_init();   log_step("PS/2 keyboard");
    pmm_init(mbi);     log_step("Physical memory manager");
    paging_init();     log_step("Paging (identity map)");
    heap_init();       log_step("Kernel heap");
    vfs_init();        log_step("Virtual filesystem");

    /* Everything is ready: enable hardware interrupts. */
    __asm__ volatile("sti");
    log_step("Interrupts enabled");

    kprintf("\nDetected %u KiB (%u MiB) of RAM across %u frames.\n",
            pmm_total_memory_kb(), pmm_total_memory_kb() / 1024,
            pmm_total_frames());

    /* Prefer the graphical desktop; fall back to the text shell if GRUB did
       not give us a usable linear framebuffer. */
    if (gfx_init(mbi)) {
        log_step("Linear framebuffer graphics");
        mouse_init(gfx_width(), gfx_height());
        log_step("PS/2 mouse");
        desktop_run();
    } else {
        kprintf("No graphical framebuffer; starting the text shell.\n");
        kprintf("Type 'help' for a list of commands.\n");
        shell_run();
    }

    /* Neither path returns, but just in case: */
    panic("kernel_main: main loop returned");
}
