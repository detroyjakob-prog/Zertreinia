/* Shared command interpreter backend.
 *
 * Commands write through an abstract output sink, so the same implementation
 * powers both the text-mode shell and the GUI terminal. Side effects that the
 * caller must perform (clear screen, reboot, halt) are returned as a result. */
#ifndef ZERT_COMMANDS_H
#define ZERT_COMMANDS_H

#include "types.h"
#include "vfs.h"

typedef struct cmd_out {
    void (*write)(void *ctx, const char *s);
    void *ctx;
} cmd_out_t;

typedef enum {
    CMD_OK = 0,
    CMD_CLEAR,
    CMD_REBOOT,
    CMD_HALT,
} cmd_result_t;

/* Execute one command line. `cwd` is read and may be updated (e.g. by cd). */
cmd_result_t cmd_execute(const char *line, cmd_out_t *out, vfs_node_t **cwd);

/* printf-style helper that routes through an output sink. */
void cmd_outf(cmd_out_t *out, const char *fmt, ...);

/* Lets the desktop provide an "open file in the editor" action for `edit`. */
void cmd_set_editor_hook(void (*hook)(vfs_node_t *file));

#endif /* ZERT_COMMANDS_H */
