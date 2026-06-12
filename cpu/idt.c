/* Interrupt Descriptor Table and 8259 PIC initialisation. */

#include "idt.h"
#include "ports.h"

/* One 8-byte IDT gate descriptor. */
struct idt_entry {
    uint16_t base_low;     /* handler address bits 0..15  */
    uint16_t selector;     /* code segment selector       */
    uint8_t  always0;      /* reserved, must be 0         */
    uint8_t  flags;        /* type and attributes         */
    uint16_t base_high;    /* handler address bits 16..31 */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_pointer;

/* Implemented in cpu/flush.asm. */
extern void idt_flush(uint32_t idt_ptr_address);

/* The 32 CPU exception stubs (cpu/interrupt.asm). */
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void);
extern void isr4(void),  isr5(void),  isr6(void),  isr7(void);
extern void isr8(void),  isr9(void),  isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);

/* The 16 hardware IRQ stubs. */
extern void irq0(void),  irq1(void),  irq2(void),  irq3(void);
extern void irq4(void),  irq5(void),  irq6(void),  irq7(void);
extern void irq8(void),  irq9(void),  irq10(void), irq11(void);
extern void irq12(void), irq13(void), irq14(void), irq15(void);

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low  = (uint16_t)(base & 0xFFFF);
    idt[num].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/* Remap the master/slave 8259 PICs so hardware IRQs 0..15 arrive as
   interrupt vectors 32..47 (vectors 0..31 are reserved for exceptions). */
static void pic_remap(void)
{
    /* ICW1: begin initialisation (cascade mode, expect ICW4). */
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    /* ICW2: vector offsets. */
    outb(0x21, 0x20); io_wait();   /* master -> 0x20 (32) */
    outb(0xA1, 0x28); io_wait();   /* slave  -> 0x28 (40) */

    /* ICW3: wire the slave to the master's IRQ2 line. */
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();

    /* ICW4: 8086/88 mode. */
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    /* Masks: enable IRQ0 (timer), IRQ1 (keyboard) and IRQ2 (cascade);
       mask everything else for now. */
    outb(0x21, 0xF8);   /* 1111 1000 -> IRQ0,1,2 unmasked */
    outb(0xA1, 0xFF);   /* all slave IRQs masked          */
}

void idt_init(void)
{
    idt_pointer.limit = (uint16_t)(sizeof(idt) - 1);
    idt_pointer.base  = (uint32_t)&idt;

    /* idt is in zeroed BSS, so all gates start "not present". */

    pic_remap();

    /* 0x8E = present, ring 0, 32-bit interrupt gate. */
    const uint8_t F = 0x8E;
    const uint16_t S = 0x08;   /* kernel code selector */

    idt_set_gate(0,  (uint32_t)isr0,  S, F);
    idt_set_gate(1,  (uint32_t)isr1,  S, F);
    idt_set_gate(2,  (uint32_t)isr2,  S, F);
    idt_set_gate(3,  (uint32_t)isr3,  S, F);
    idt_set_gate(4,  (uint32_t)isr4,  S, F);
    idt_set_gate(5,  (uint32_t)isr5,  S, F);
    idt_set_gate(6,  (uint32_t)isr6,  S, F);
    idt_set_gate(7,  (uint32_t)isr7,  S, F);
    idt_set_gate(8,  (uint32_t)isr8,  S, F);
    idt_set_gate(9,  (uint32_t)isr9,  S, F);
    idt_set_gate(10, (uint32_t)isr10, S, F);
    idt_set_gate(11, (uint32_t)isr11, S, F);
    idt_set_gate(12, (uint32_t)isr12, S, F);
    idt_set_gate(13, (uint32_t)isr13, S, F);
    idt_set_gate(14, (uint32_t)isr14, S, F);
    idt_set_gate(15, (uint32_t)isr15, S, F);
    idt_set_gate(16, (uint32_t)isr16, S, F);
    idt_set_gate(17, (uint32_t)isr17, S, F);
    idt_set_gate(18, (uint32_t)isr18, S, F);
    idt_set_gate(19, (uint32_t)isr19, S, F);
    idt_set_gate(20, (uint32_t)isr20, S, F);
    idt_set_gate(21, (uint32_t)isr21, S, F);
    idt_set_gate(22, (uint32_t)isr22, S, F);
    idt_set_gate(23, (uint32_t)isr23, S, F);
    idt_set_gate(24, (uint32_t)isr24, S, F);
    idt_set_gate(25, (uint32_t)isr25, S, F);
    idt_set_gate(26, (uint32_t)isr26, S, F);
    idt_set_gate(27, (uint32_t)isr27, S, F);
    idt_set_gate(28, (uint32_t)isr28, S, F);
    idt_set_gate(29, (uint32_t)isr29, S, F);
    idt_set_gate(30, (uint32_t)isr30, S, F);
    idt_set_gate(31, (uint32_t)isr31, S, F);

    idt_set_gate(32, (uint32_t)irq0,  S, F);
    idt_set_gate(33, (uint32_t)irq1,  S, F);
    idt_set_gate(34, (uint32_t)irq2,  S, F);
    idt_set_gate(35, (uint32_t)irq3,  S, F);
    idt_set_gate(36, (uint32_t)irq4,  S, F);
    idt_set_gate(37, (uint32_t)irq5,  S, F);
    idt_set_gate(38, (uint32_t)irq6,  S, F);
    idt_set_gate(39, (uint32_t)irq7,  S, F);
    idt_set_gate(40, (uint32_t)irq8,  S, F);
    idt_set_gate(41, (uint32_t)irq9,  S, F);
    idt_set_gate(42, (uint32_t)irq10, S, F);
    idt_set_gate(43, (uint32_t)irq11, S, F);
    idt_set_gate(44, (uint32_t)irq12, S, F);
    idt_set_gate(45, (uint32_t)irq13, S, F);
    idt_set_gate(46, (uint32_t)irq14, S, F);
    idt_set_gate(47, (uint32_t)irq15, S, F);

    idt_flush((uint32_t)&idt_pointer);
}
