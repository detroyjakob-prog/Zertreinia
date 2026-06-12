/* ============================================================================
 * Zertreinia OS - Kernel linker script
 *
 * The kernel is loaded by a Multiboot-compliant boot loader (GRUB) at the
 * physical address 1 MiB (0x00100000), which is the conventional location for
 * 32-bit x86 kernels. Lower memory is reserved for the BIOS, real-mode IVT,
 * VGA memory and other legacy structures.
 * ========================================================================== */

ENTRY(_start)

SECTIONS
{
    /* Load the kernel at the 1 MiB mark. */
    . = 1M;
    __kernel_start = .;

    /* Executable code. The Multiboot header MUST be in the first 8 KiB of the
       binary, so we force the .multiboot section to the very beginning. */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    /* Read-only data. */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write initialized data. */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* Read-write zero-initialized data (BSS) and the kernel stack.
       The boot stub zeroes everything between __bss_start and __bss_end. */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        *(.bootstrap_stack)
        __bss_end = .;
    }

    __kernel_end = .;

    /* Discard linker/debug noise we do not need in the final image. */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}