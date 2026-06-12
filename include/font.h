/* 8x8 bitmap font (public domain, by Daniel Hepper / Marcel Sondaar). */
#ifndef ZERT_FONT_H
#define ZERT_FONT_H

#include "types.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 8

/* font8x8_basic[c] is 8 bytes; byte r is row r, and bit (1<<col) is the
   pixel at column `col` (bit 0 = leftmost column). */
extern const uint8_t font8x8_basic[128][8];

#endif /* ZERT_FONT_H */
