; ============================================================================
; Zertreinia OS - Interrupt / IRQ stubs
;
; The CPU does not tell a handler which interrupt fired, and only some
; exceptions push an error code. These stubs normalise the stack so that the C
; handlers always receive a consistent `registers_t` structure:
;
;   - For exceptions WITHOUT an error code we push a dummy 0.
;   - We always push the interrupt number.
;   - The common stub then saves the general-purpose and segment registers.
; ============================================================================

bits 32

extern isr_handler
extern irq_handler

; ---- Macros ----------------------------------------------------------------

; Exception that does NOT push an error code.
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0            ; dummy error code
    push dword %1           ; interrupt number
    jmp isr_common_stub
%endmacro

; Exception that DOES push an error code (the CPU pushed it for us).
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1           ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Hardware IRQ. %1 = IRQ number, %2 = remapped interrupt vector.
%macro IRQ 2
global irq%1
irq%1:
    push dword 0            ; dummy error code
    push dword %2           ; interrupt number (vector)
    jmp irq_common_stub
%endmacro

; ---- CPU exceptions 0..31 --------------------------------------------------
ISR_NOERR 0     ; divide-by-zero
ISR_NOERR 1     ; debug
ISR_NOERR 2     ; non-maskable interrupt
ISR_NOERR 3     ; breakpoint
ISR_NOERR 4     ; overflow
ISR_NOERR 5     ; bound range exceeded
ISR_NOERR 6     ; invalid opcode
ISR_NOERR 7     ; device not available
ISR_ERR   8     ; double fault
ISR_NOERR 9     ; coprocessor segment overrun (legacy)
ISR_ERR   10    ; invalid TSS
ISR_ERR   11    ; segment not present
ISR_ERR   12    ; stack-segment fault
ISR_ERR   13    ; general protection fault
ISR_ERR   14    ; page fault
ISR_NOERR 15    ; reserved
ISR_NOERR 16    ; x87 floating-point exception
ISR_ERR   17    ; alignment check
ISR_NOERR 18    ; machine check
ISR_NOERR 19    ; SIMD floating-point exception
ISR_NOERR 20    ; virtualization exception
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ---- Hardware IRQs 0..15 (remapped to vectors 32..47) ----------------------
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ---- Common stubs ----------------------------------------------------------

isr_common_stub:
    pusha                   ; push edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds
    push eax                ; save the data segment selector

    mov ax, 0x10            ; load the kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax                ; pass pointer to registers_t
    call isr_handler
    add esp, 4

    pop eax                 ; restore the original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8              ; discard pushed error code and interrupt number
    iret                    ; restores eip, cs, eflags (and ss:esp if needed)

irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

; Mark the stack as non-executable (silences linker warnings).
section .note.GNU-stack noalloc noexec nowrite progbits
