; ============================================================================
; Zertreinia OS - GDT / IDT loaders
;
; These small helpers load the descriptor tables and (for the GDT) reload the
; segment registers so the new descriptors take effect.
; ============================================================================

bits 32

global gdt_flush
global idt_flush

; void gdt_flush(uint32_t gdt_ptr_address);
gdt_flush:
    mov eax, [esp + 4]      ; first argument: address of the GDT pointer
    lgdt [eax]              ; load the new GDT

    mov ax, 0x10            ; 0x10 = kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with the kernel code selector (0x08).
    jmp 0x08:.flush
.flush:
    ret

; void idt_flush(uint32_t idt_ptr_address);
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; Mark the stack as non-executable (silences linker warnings).
section .note.GNU-stack noalloc noexec nowrite progbits
