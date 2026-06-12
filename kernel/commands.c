/* Shared command interpreter. */

#include "commands.h"
#include "printf.h"
#include "string.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "paging.h"

#define MAX_ARGS 24
#define LINE_MAX 256

static void (*editor_hook)(vfs_node_t *file);

void cmd_set_editor_hook(void (*hook)(vfs_node_t *file))
{
    editor_hook = hook;
}

void cmd_outf(cmd_out_t *out, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    out->write(out->ctx, buf);
}

/* ---- individual commands -------------------------------------------------- */

static void cmd_help(cmd_out_t *out)
{
    /* Split across several calls: cmd_outf has a fixed-size internal buffer. */
    cmd_outf(out, "Commands:\n");
    cmd_outf(out, "  help              this help\n");
    cmd_outf(out, "  clear             clear the screen\n");
    cmd_outf(out, "  echo <text>       print text\n");
    cmd_outf(out, "  about             about this OS\n");
    cmd_outf(out, "  meminfo           memory + heap statistics\n");
    cmd_outf(out, "  memtest           exercise the heap\n");
    cmd_outf(out, "  uptime / ticks    time since boot\n");
    cmd_outf(out, "  ls [path]         list a directory\n");
    cmd_outf(out, "  cd <path>         change directory\n");
    cmd_outf(out, "  pwd               print working directory\n");
    cmd_outf(out, "  cat <file>        show a file\n");
    cmd_outf(out, "  mkdir <name>      make a directory\n");
    cmd_outf(out, "  touch <name>      create an empty file\n");
    cmd_outf(out, "  write <f> <text>  write text to a file\n");
    cmd_outf(out, "  edit <file>       open file in the editor\n");
    cmd_outf(out, "  rm <name>         remove a file/dir\n");
    cmd_outf(out, "  tree              show the filesystem tree\n");
    cmd_outf(out, "  reboot / halt     stop the machine\n");
}

static void cmd_about(cmd_out_t *out)
{
    cmd_outf(out, "%s v%s\n", KERNEL_NAME, KERNEL_VERSION);
    cmd_outf(out, "A 32-bit x86 OS written from scratch in C and Assembly,\n");
    cmd_outf(out, "with a graphical desktop, windows, a mouse, an in-memory\n");
    cmd_outf(out, "filesystem and several built-in applications.\n");
}

static void cmd_meminfo(cmd_out_t *out)
{
    uint32_t total = pmm_total_frames();
    uint32_t used  = pmm_used_frames();
    uint32_t freef = pmm_free_frames();

    cmd_outf(out, "Physical memory:\n");
    cmd_outf(out, "  total : %u KiB (%u frames)\n", pmm_total_memory_kb(), total);
    cmd_outf(out, "  used  : %u KiB (%u frames)\n", used * 4, used);
    cmd_outf(out, "  free  : %u KiB (%u frames)\n", freef * 4, freef);
    cmd_outf(out, "  paging: %s\n", paging_enabled() ? "enabled" : "disabled");

    size_t ht, hu, hf;
    heap_stats(&ht, &hu, &hf);
    cmd_outf(out, "Kernel heap: %u used / %u total bytes\n",
             (uint32_t)hu, (uint32_t)ht);
}

static void cmd_memtest(cmd_out_t *out)
{
    char *a = (char *)kmalloc(64);
    char *b = (char *)kmalloc(128);
    char *c = (char *)kmalloc(256);
    cmd_outf(out, "alloc a=%p b=%p c=%p\n", a, b, c);
    if (a) { strcpy(a, "heap works"); cmd_outf(out, "a -> \"%s\"\n", a); }
    kfree(b);
    char *d = (char *)kmalloc(100);
    cmd_outf(out, "freed b, realloc d=%p %s\n", d,
             (d == b) ? "(reused)" : "");
    kfree(a); kfree(c); kfree(d);
    cmd_outf(out, "all freed, heap coalesced\n");
}

static const char *type_str(vfs_node_t *n)
{
    return n->type == VFS_DIR ? "<DIR>" : "file ";
}

static void cmd_ls(cmd_out_t *out, vfs_node_t *cwd, int argc, char **argv)
{
    vfs_node_t *dir = (argc >= 2) ? vfs_resolve(cwd, argv[1]) : cwd;
    if (!dir) {
        cmd_outf(out, "ls: no such path: %s\n", argv[1]);
        return;
    }
    if (dir->type != VFS_DIR) {
        cmd_outf(out, "%s %8u  %s\n", type_str(dir), dir->size, dir->name);
        return;
    }
    int n = 0;
    for (vfs_node_t *c = dir->children; c; c = c->next) {
        cmd_outf(out, "%s %8u  %s\n", type_str(c), c->size, c->name);
        n++;
    }
    if (n == 0) {
        cmd_outf(out, "(empty)\n");
    }
}

static void tree_recurse(cmd_out_t *out, vfs_node_t *dir, int depth)
{
    for (vfs_node_t *c = dir->children; c; c = c->next) {
        for (int i = 0; i < depth; i++) {
            cmd_outf(out, "  ");
        }
        cmd_outf(out, "%s%s\n", c->name, c->type == VFS_DIR ? "/" : "");
        if (c->type == VFS_DIR) {
            tree_recurse(out, c, depth + 1);
        }
    }
}

/* ---- dispatch ------------------------------------------------------------- */

static int tokenize(char *line, char **argv)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return argc;
}

cmd_result_t cmd_execute(const char *line, cmd_out_t *out, vfs_node_t **cwd)
{
    char buf[LINE_MAX];
    char *argv[MAX_ARGS];

    strncpy(buf, line, LINE_MAX - 1);
    buf[LINE_MAX - 1] = '\0';

    int argc = tokenize(buf, argv);
    if (argc == 0) {
        return CMD_OK;
    }
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0) {
        cmd_help(out);
    } else if (strcmp(cmd, "clear") == 0) {
        return CMD_CLEAR;
    } else if (strcmp(cmd, "reboot") == 0) {
        return CMD_REBOOT;
    } else if (strcmp(cmd, "halt") == 0) {
        return CMD_HALT;
    } else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            cmd_outf(out, "%s%s", argv[i], (i + 1 < argc) ? " " : "");
        }
        cmd_outf(out, "\n");
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about(out);
    } else if (strcmp(cmd, "meminfo") == 0) {
        cmd_meminfo(out);
    } else if (strcmp(cmd, "memtest") == 0) {
        cmd_memtest(out);
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_outf(out, "up %u s (%u ticks @ %u Hz)\n",
                 timer_seconds(), timer_ticks(), timer_frequency());
    } else if (strcmp(cmd, "ticks") == 0) {
        cmd_outf(out, "%u\n", timer_ticks());
    } else if (strcmp(cmd, "pwd") == 0) {
        char path[256];
        vfs_abspath(*cwd, path, sizeof(path));
        cmd_outf(out, "%s\n", path);
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(out, *cwd, argc, argv);
    } else if (strcmp(cmd, "cd") == 0) {
        if (argc < 2) { *cwd = vfs_root(); }
        else {
            vfs_node_t *d = vfs_resolve(*cwd, argv[1]);
            if (!d) cmd_outf(out, "cd: no such path: %s\n", argv[1]);
            else if (d->type != VFS_DIR) cmd_outf(out, "cd: not a directory: %s\n", argv[1]);
            else *cwd = d;
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (argc < 2) { cmd_outf(out, "usage: cat <file>\n"); }
        else {
            vfs_node_t *f = vfs_resolve(*cwd, argv[1]);
            if (!f || f->type != VFS_FILE) cmd_outf(out, "cat: no such file: %s\n", argv[1]);
            else {
                if (f->data) out->write(out->ctx, (const char *)f->data);
                if (f->size == 0 || (f->data && f->data[f->size - 1] != '\n'))
                    cmd_outf(out, "\n");
            }
        }
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (argc < 2) cmd_outf(out, "usage: mkdir <name>\n");
        else if (!vfs_create(*cwd, argv[1], VFS_DIR)) cmd_outf(out, "mkdir: failed (exists?)\n");
    } else if (strcmp(cmd, "touch") == 0) {
        if (argc < 2) cmd_outf(out, "usage: touch <name>\n");
        else if (!vfs_find_child(*cwd, argv[1]) && !vfs_create(*cwd, argv[1], VFS_FILE))
            cmd_outf(out, "touch: failed\n");
    } else if (strcmp(cmd, "write") == 0) {
        if (argc < 3) { cmd_outf(out, "usage: write <file> <text...>\n"); }
        else {
            vfs_node_t *f = vfs_find_child(*cwd, argv[1]);
            if (!f) f = vfs_create(*cwd, argv[1], VFS_FILE);
            if (!f || f->type != VFS_FILE) { cmd_outf(out, "write: cannot write %s\n", argv[1]); }
            else {
                char text[LINE_MAX];
                int pos = 0;
                for (int i = 2; i < argc; i++) {
                    pos += snprintf(text + pos, sizeof(text) - pos, "%s%s",
                                    argv[i], (i + 1 < argc) ? " " : "\n");
                }
                vfs_write(f, text, (uint32_t)strlen(text));
                cmd_outf(out, "wrote %u bytes to %s\n", f->size, argv[1]);
            }
        }
    } else if (strcmp(cmd, "edit") == 0) {
        if (argc < 2) { cmd_outf(out, "usage: edit <file>\n"); }
        else {
            vfs_node_t *f = vfs_find_child(*cwd, argv[1]);
            if (!f) f = vfs_create(*cwd, argv[1], VFS_FILE);
            if (f && editor_hook) editor_hook(f);
            else cmd_outf(out, "edit: editor not available\n");
        }
    } else if (strcmp(cmd, "rm") == 0) {
        if (argc < 2) { cmd_outf(out, "usage: rm <name>\n"); }
        else {
            vfs_node_t *n = vfs_resolve(*cwd, argv[1]);
            if (!n) cmd_outf(out, "rm: no such file: %s\n", argv[1]);
            else if (vfs_delete(n) != 0) cmd_outf(out, "rm: cannot remove %s\n", argv[1]);
        }
    } else if (strcmp(cmd, "tree") == 0) {
        cmd_outf(out, "/\n");
        tree_recurse(out, vfs_root(), 1);
    } else {
        cmd_outf(out, "unknown command: %s (try 'help')\n", cmd);
    }

    return CMD_OK;
}
