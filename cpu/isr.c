/* C-side interrupt dispatch: CPU exceptions and hardware IRQs. */

#include "isr.h"
#include "ports.h"
#include "printf.h"
#include "kernel.h"

/* One slot per interrupt vector. NULL means "no registered handler". */
static isr_t interrupt_handlers[256];

/* Human-readable names for the 32 CPU exceptions. */
static const char *exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Security exception", "Reserved",
};

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

/* Called from isr_common_stub for CPU exceptions (vectors 0..31). */
void isr_handler(registers_t *regs)
{
    isr_t handler = interrupt_handlers[regs->int_no];
    if (handler) {
        handler(regs);
        return;
    }

    if (regs->int_no < 32) {
        kprintf("\n[CPU EXCEPTION] %s (int %u, err 0x%x)\n",
                exception_messages[regs->int_no], regs->int_no, regs->err_code);
        kprintf("  eip=0x%08x cs=0x%x eflags=0x%08x\n",
                regs->eip, regs->cs, regs->eflags);
        kprintf("  eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx);

        /* A page fault stores the offending address in CR2. */
        if (regs->int_no == 14) {
            uint32_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            kprintf("  faulting address (cr2) = 0x%08x\n", cr2);
        }

        panic("Unhandled CPU exception");
    }
}

/* Called from irq_common_stub for hardware IRQs (vectors 32..47). */
void irq_handler(registers_t *regs)
{
    isr_t handler = interrupt_handlers[regs->int_no];
    if (handler) {
        handler(regs);
    }

    /* Send End-Of-Interrupt. If the IRQ came from the slave PIC
       (vectors 40..47) the slave must be acknowledged too. */
    if (regs->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
