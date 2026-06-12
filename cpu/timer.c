/* Programmable Interval Timer (PIT) driver - the kernel's heartbeat. */

#include "timer.h"
#include "isr.h"
#include "ports.h"

#define PIT_CHANNEL0   0x40
#define PIT_COMMAND    0x43
#define PIT_BASE_FREQ  1193182u   /* input clock to the PIT, in Hz */

static volatile uint32_t ticks = 0;
static uint32_t frequency = 100;

static void timer_callback(registers_t *regs)
{
    UNUSED(regs);
    ticks++;
}

void timer_init(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        frequency_hz = 100;
    }
    frequency = frequency_hz;

    register_interrupt_handler(IRQ0, timer_callback);

    /* Program channel 0 in mode 3 (square wave) with a 16-bit divisor. */
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;   /* clamp to 16 bits */

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_ticks(void)     { return ticks; }
uint32_t timer_frequency(void) { return frequency; }
uint32_t timer_seconds(void)   { return ticks / frequency; }

/* Busy-wait (with HLT) for the given number of milliseconds. */
void sleep(uint32_t milliseconds)
{
    uint32_t target = ticks + (milliseconds * frequency) / 1000u;
    while (ticks < target) {
        __asm__ volatile("hlt");
    }
}
