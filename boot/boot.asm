; ============================================================================
; Zertreinia OS - Boot stub (Multiboot v1)
;
; This is the very first code that runs after GRUB hands control to the
; kernel. It declares a Multiboot header so GRUB recognises the binary as a
; bootable kernel, sets up a stack, clears the BSS and jumps into the C
; kernel entry point `kernel_main`.
; ============================================================================

bits 32

; ---- Multiboot v1 header constants -----------------------------------------
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide a memory map
VIDEO       equ 1 << 2              ; request a graphics video mode
MBFLAGS     equ MBALIGN | MEMINFO | VIDEO
MAGIC       equ 0x1BADB002          ; magic number that lets GRUB find header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; checksum: header fields must sum to zero

; ---- Multiboot header ------------------------------------------------------
; Because the VIDEO flag is set, GRUB reads the video fields at the fixed
; offsets that follow. The five address fields are only used with the a.out
; kludge (flag bit 16, not set), but must still be present as placeholders.
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd 0            ; header_addr     (unused)
    dd 0            ; load_addr       (unused)
    dd 0            ; load_end_addr   (unused)
    dd 0            ; bss_end_addr    (unused)
    dd 0            ; entry_addr      (unused)
    dd 0            ; mode_type: 0 = linear graphics framebuffer
    dd 1024         ; preferred width
    dd 768          ; preferred height
    dd 32           ; preferred bits per pixel

; ---- Kernel stack (16 KiB) -------------------------------------------------
; Lives in BSS (NOBITS), so it takes no space in the binary. It is bracketed
; by __bss_start/__bss_end and therefore gets zeroed by the loop below.
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ---- Entry point -----------------------------------------------------------
section .text
global _start
extern kernel_main
extern __bss_start
extern __bss_end

_start:
    ; Set up the stack pointer (stack grows downward from stack_top).
    mov esp, stack_top

    ; GRUB leaves the magic value in EAX and the info pointer in EBX. Preserve
    ; them in registers that the BSS-clearing loop does NOT touch, because that
    ; loop overwrites EAX (the store value) and EDI/ECX (dest/count).
    mov esi, eax                    ; esi = multiboot magic
    mov ebp, ebx                    ; ebp = multiboot info pointer

    ; Zero the BSS so all uninitialised globals start at 0. The Multiboot
    ; spec does NOT guarantee this, so we do it ourselves.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi                    ; ecx = number of bytes in BSS
    xor eax, eax
    cld
    rep stosb

    ; Re-establish the stack (it lives in BSS which we just zeroed).
    mov esp, stack_top

    ; Pass the Multiboot information to the kernel:
    ;   kernel_main(uint32_t magic, multiboot_info_t *mbi)
    push ebp                        ; 2nd argument: multiboot info pointer
    push esi                        ; 1st argument: multiboot magic value
    call kernel_main

    ; kernel_main should never return; if it does, halt the CPU forever.
.hang:
    cli
    hlt
    jmp .hang

; Mark the stack as non-executable (silences linker warnings).
section .note.GNU-stack noalloc noexec nowrite progbits
