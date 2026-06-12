/* Programmable Interval Timer (PIT, channel 0) driver. */
#ifndef ZERT_TIMER_H
#define ZERT_TIMER_H

#include "types.h"

void     timer_init(uint32_t frequency_hz);
uint32_t timer_ticks(void);
uint32_t timer_frequency(void);
uint32_t timer_seconds(void);
void     sleep(uint32_t milliseconds);

#endif /* ZERT_TIMER_H */
