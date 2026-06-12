/* PS/2 keyboard driver (scancode set 1, US layout). */
#ifndef ZERT_KEYBOARD_H
#define ZERT_KEYBOARD_H

#include "types.h"

void keyboard_init(void);

/* Blocking read of one translated ASCII character from the input buffer.
   Returns control characters '\n' (Enter) and '\b' (Backspace) too. */
char keyboard_getchar(void);

/* Non-blocking: returns 0 if no character is currently buffered. */
char keyboard_trygetchar(void);

#endif /* ZERT_KEYBOARD_H */
