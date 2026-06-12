/* Interrupt service routines and the register state pushed by the stubs. */
#ifndef ZERT_ISR_H
#define ZERT_ISR_H

#include "types.h"

/* CPU/segment state captured by the assembly interrupt stubs.
   The field order MUST match the push order in cpu/interrupt.asm. */
typedef struct registers {
    uint32_t ds;                                     /* pushed by stub        */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pushed by `pusha`     */
    uint32_t int_no, err_code;                       /* pushed by the stub    */
    uint32_t eip, cs, eflags, useresp, ss;           /* pushed by the CPU      */
} registers_t;

/* Hardware IRQ numbers (after PIC remap to vectors 32..47). */
#define IRQ0  32   /* programmable interval timer */
#define IRQ1  33   /* PS/2 keyboard               */
#define IRQ2  34
#define IRQ3  35
#define IRQ4  36
#define IRQ5  37
#define IRQ6  38
#define IRQ7  39
#define IRQ8  40
#define IRQ9  41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

typedef void (*isr_t)(registers_t *);

void register_interrupt_handler(uint8_t n, isr_t handler);

/* Called from the assembly stubs. */
void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);

#endif /* ZERT_ISR_H */
