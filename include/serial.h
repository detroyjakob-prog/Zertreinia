/* Serial port (COM1) driver - used for debug logging. */
#ifndef ZERT_SERIAL_H
#define ZERT_SERIAL_H

#include "types.h"

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *s);
int  serial_received(void);

#endif /* ZERT_SERIAL_H */
