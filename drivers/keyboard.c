/* PS/2 keyboard driver: scancode set 1, US QWERTY layout.
 *
 * The IRQ1 handler reads raw scancodes from port 0x60, tracks the Shift and
 * Caps-Lock modifiers, translates to ASCII and pushes the result into a small
 * ring buffer that keyboard_getchar() drains. */

#include "keyboard.h"
#include "isr.h"
#include "ports.h"

#define KBD_DATA_PORT 0x60

/* Base (unshifted) scancode -> ASCII table for set 1. */
static const char keymap[128] = {
      0,  27, '1', '2', '3', '4', '5', '6', '7', '8',   /* 0x00 - 0x09 */
    '9', '0', '-', '=','\b','\t', 'q', 'w', 'e', 'r',    /* 0x0A - 0x13 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']','\n',   0,    /* 0x14 - 0x1D */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x1E - 0x27 */
   '\'', '`',   0,'\\', 'z', 'x', 'c', 'v', 'b', 'n',    /* 0x28 - 0x31 */
    'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,    /* 0x32 - 0x3B */
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
};

/* Shifted scancode -> ASCII table. */
static const char keymap_shift[128] = {
      0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+','\b','\t', 'Q', 'W', 'E', 'R',
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}','\n',   0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~',   0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
};

#define KBD_BUFFER_SIZE 256
static volatile char     kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head = 0;   /* write index (producer / IRQ) */
static volatile uint32_t kbd_tail = 0;   /* read index  (consumer)       */

static bool shift_down = false;
static bool caps_lock  = false;

static void buffer_push(char c)
{
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_tail) {          /* drop the key if the buffer is full */
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

static void keyboard_callback(registers_t *regs)
{
    UNUSED(regs);
    uint8_t scancode = inb(KBD_DATA_PORT);

    /* High bit set => key release. */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {  /* Left/Right Shift */
            shift_down = false;
        }
        return;
    }

    switch (scancode) {
    case 0x2A: case 0x36:           /* Shift pressed     */
        shift_down = true;
        return;
    case 0x3A:                      /* Caps Lock toggled */
        caps_lock = !caps_lock;
        return;
    default:
        break;
    }

    char base    = keymap[scancode];
    char shifted = keymap_shift[scancode];
    char c;

    if (base >= 'a' && base <= 'z') {
        /* Letters: Shift XOR Caps-Lock selects upper case. */
        c = (shift_down ^ caps_lock) ? shifted : base;
    } else {
        c = shift_down ? shifted : base;
    }

    if (c != 0) {
        buffer_push(c);
    }
}

void keyboard_init(void)
{
    register_interrupt_handler(IRQ1, keyboard_callback);

    /* Drain any byte the controller may have left in its output buffer
       so the first real key press generates a fresh IRQ. */
    while (inb(0x64) & 1) {
        (void)inb(KBD_DATA_PORT);
    }
}

char keyboard_trygetchar(void)
{
    if (kbd_head == kbd_tail) {
        return 0;
    }
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

char keyboard_getchar(void)
{
    char c;
    while ((c = keyboard_trygetchar()) == 0) {
        __asm__ volatile("hlt");   /* sleep until the next interrupt */
    }
    return c;
}
