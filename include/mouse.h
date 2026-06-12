/* PS/2 mouse driver (IRQ12). */
#ifndef ZERT_MOUSE_H
#define ZERT_MOUSE_H

#include "types.h"

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

/* Initialise the mouse and clamp the pointer to [0,w) x [0,h). */
void mouse_init(uint32_t screen_w, uint32_t screen_h);

/* Current pointer position and pressed-button bitmask. */
int     mouse_x(void);
int     mouse_y(void);
uint8_t mouse_buttons(void);

/* True (and clears the flag) if the pointer moved or buttons changed since
   the last call - lets the desktop avoid redrawing when nothing happened. */
bool    mouse_poll_changed(void);

#endif /* ZERT_MOUSE_H */
