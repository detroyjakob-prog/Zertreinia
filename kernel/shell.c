/* Text-mode shell - the fallback UI when no graphical framebuffer is
 * available. It is a thin front-end over the shared command backend. */

#include "shell.h"
#include "vga.h"
#include "printf.h"
#include "string.h"
#include "keyboard.h"
#include "ports.h"
#include "commands.h"
#include "vfs.h"
#include "kernel.h"

#define LINE_MAX 256

static void shell_out(void *ctx, const char *s)
{
    UNUSED(ctx);
    vga_puts(s);
    /* mirror to serial for debugging */
}

static void shell_readline(char *buf, size_t max)
{
    size_t len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putchar('\n');
            buf[len] = '\0';
            return;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                vga_putchar('\b');
            }
        } else if (len < max - 1) {
            buf[len++] = c;
            vga_putchar(c);
        }
    }
}

static void do_reboot(void)
{
    uint8_t status = 0x02;
    while (status & 0x02) {
        status = inb(0x64);
    }
    outb(0x64, 0xFE);
    halt();
}

void shell_run(void)
{
    char line[LINE_MAX];
    vfs_node_t *cwd = vfs_root();
    cmd_out_t out = { shell_out, NULL };

    kprintf("\n");
    for (;;) {
        char path[128];
        vfs_abspath(cwd, path, sizeof(path));
        vga_set_color(vga_make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("zertreinia:%s$ ", path);
        vga_set_color(vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

        shell_readline(line, sizeof(line));

        cmd_result_t r = cmd_execute(line, &out, &cwd);
        switch (r) {
        case CMD_CLEAR:  vga_clear(); break;
        case CMD_REBOOT: kprintf("Rebooting...\n"); do_reboot(); break;
        case CMD_HALT:   kprintf("Halting.\n"); halt(); break;
        default: break;
        }
    }
}
