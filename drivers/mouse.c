/* PS/2 mouse driver.
 *
 * The mouse hangs off the second PS/2 port, served by IRQ12 on the slave PIC.
 * Packets are three bytes: flags, dx, dy. We accumulate them with a small
 * state machine, update the pointer position (clamped to the screen) and
 * expose the current button state. */

#include "mouse.h"
#include "isr.h"
#include "ports.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static int     pos_x, pos_y;
static int     max_x, max_y;
static uint8_t buttons;
static volatile bool changed;

static uint8_t packet[3];
static int     packet_index;

/* Wait until the controller is ready for a write (input buffer empty). */
static void ps2_wait_write(void)
{
    int timeout = 100000;
    while (timeout-- > 0) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
}

/* Wait until data is available to read (output buffer full). */
static void ps2_wait_read(void)
{
    int timeout = 100000;
    while (timeout-- > 0) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
}

/* Send a command byte to the mouse (not the controller). */
static void mouse_command(uint8_t cmd)
{
    ps2_wait_write();
    outb(PS2_CMD, 0xD4);        /* "next byte goes to the second PS/2 port" */
    ps2_wait_write();
    outb(PS2_DATA, cmd);
    ps2_wait_read();
    (void)inb(PS2_DATA);        /* read and discard the ACK (0xFA) */
}

static void mouse_callback(registers_t *regs)
{
    UNUSED(regs);
    uint8_t data = inb(PS2_DATA);

    switch (packet_index) {
    case 0:
        /* Bit 3 of the first byte is always 1; use it to resynchronise. */
        if (!(data & 0x08)) {
            return;
        }
        packet[0] = data;
        packet_index = 1;
        break;
    case 1:
        packet[1] = data;
        packet_index = 2;
        break;
    case 2: {
        packet[2] = data;
        packet_index = 0;

        uint8_t flags = packet[0];
        /* Discard packets with overflow set. */
        if (flags & 0xC0) {
            break;
        }

        /* dx/dy are 9-bit signed values; bit 4/5 of flags are the signs. */
        int dx = (int)packet[1] - ((flags & 0x10) ? 256 : 0);
        int dy = (int)packet[2] - ((flags & 0x20) ? 256 : 0);

        pos_x += dx;
        pos_y -= dy;            /* screen Y grows downward */

        if (pos_x < 0)      pos_x = 0;
        if (pos_y < 0)      pos_y = 0;
        if (pos_x > max_x)  pos_x = max_x;
        if (pos_y > max_y)  pos_y = max_y;

        buttons = flags & 0x07;
        changed = true;
        break;
    }
    default:
        packet_index = 0;
        break;
    }
}

void mouse_init(uint32_t screen_w, uint32_t screen_h)
{
    max_x = (int)screen_w - 1;
    max_y = (int)screen_h - 1;
    pos_x = (int)screen_w / 2;
    pos_y = (int)screen_h / 2;
    buttons = 0;
    packet_index = 0;

    /* Enable the auxiliary (mouse) PS/2 port. */
    ps2_wait_write();
    outb(PS2_CMD, 0xA8);

    /* Enable IRQ12: set bit 1 of the controller "compaq status" byte. */
    ps2_wait_write();
    outb(PS2_CMD, 0x20);
    ps2_wait_read();
    uint8_t status = inb(PS2_DATA);
    status |= 0x02;             /* enable second-port interrupt */
    status &= ~0x20;           /* enable the second clock       */
    ps2_wait_write();
    outb(PS2_CMD, 0x60);
    ps2_wait_write();
    outb(PS2_DATA, status);

    mouse_command(0xF6);        /* load defaults              */
    mouse_command(0xF4);        /* enable packet streaming    */

    register_interrupt_handler(IRQ12, mouse_callback);

    /* Unmask IRQ12 (bit 4) on the slave PIC. IRQ2 (cascade) is already
       unmasked on the master from idt_init(). */
    uint8_t slave_mask = inb(0xA1);
    slave_mask &= ~(1u << 4);
    outb(0xA1, slave_mask);
}

int     mouse_x(void)       { return pos_x; }
int     mouse_y(void)       { return pos_y; }
uint8_t mouse_buttons(void) { return buttons; }

bool mouse_poll_changed(void)
{
    if (changed) {
        changed = false;
        return true;
    }
    return false;
}
