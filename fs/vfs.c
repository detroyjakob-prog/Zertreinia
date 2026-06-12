/* In-memory Virtual File System. */

#include "vfs.h"
#include "heap.h"
#include "string.h"

static vfs_node_t *root;

static vfs_node_t *node_alloc(const char *name, vfs_type_t type)
{
    vfs_node_t *n = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!n) {
        return NULL;
    }
    memset(n, 0, sizeof(*n));
    strncpy(n->name, name, VFS_NAME_MAX - 1);
    n->name[VFS_NAME_MAX - 1] = '\0';
    n->type = type;
    return n;
}

vfs_node_t *vfs_find_child(vfs_node_t *dir, const char *name)
{
    if (!dir || dir->type != VFS_DIR) {
        return NULL;
    }
    for (vfs_node_t *c = dir->children; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

vfs_node_t *vfs_create(vfs_node_t *dir, const char *name, vfs_type_t type)
{
    if (!dir || dir->type != VFS_DIR || name[0] == '\0') {
        return NULL;
    }
    if (vfs_find_child(dir, name)) {
        return NULL;   /* already exists */
    }

    vfs_node_t *n = node_alloc(name, type);
    if (!n) {
        return NULL;
    }
    n->parent = dir;

    /* Append to the end of the child list so directory order is stable. */
    if (!dir->children) {
        dir->children = n;
    } else {
        vfs_node_t *c = dir->children;
        while (c->next) {
            c = c->next;
        }
        c->next = n;
    }
    return n;
}

vfs_node_t *vfs_resolve(vfs_node_t *cwd, const char *path)
{
    vfs_node_t *cur = (path[0] == '/' || !cwd) ? root : cwd;

    char token[VFS_NAME_MAX];
    const char *p = path;

    while (*p) {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        /* Copy the next path component into `token`. */
        size_t len = 0;
        while (*p && *p != '/' && len < VFS_NAME_MAX - 1) {
            token[len++] = *p++;
        }
        token[len] = '\0';
        while (*p && *p != '/') {
            p++;   /* skip an over-long component tail */
        }

        if (strcmp(token, ".") == 0) {
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (cur->parent) {
                cur = cur->parent;
            }
            continue;
        }

        vfs_node_t *child = vfs_find_child(cur, token);
        if (!child) {
            return NULL;
        }
        cur = child;
    }
    return cur;
}

uint32_t vfs_read(vfs_node_t *file, void *buf, uint32_t max)
{
    if (!file || file->type != VFS_FILE || !file->data) {
        return 0;
    }
    uint32_t n = file->size < max ? file->size : max;
    memcpy(buf, file->data, n);
    return n;
}

int vfs_write(vfs_node_t *file, const void *buf, uint32_t len)
{
    if (!file || file->type != VFS_FILE) {
        return -1;
    }
    if (len > file->capacity) {
        uint8_t *nd = (uint8_t *)kmalloc(len + 1);
        if (!nd) {
            return -1;
        }
        if (file->data) {
            kfree(file->data);
        }
        file->data = nd;
        file->capacity = len + 1;
    }
    if (len > 0) {
        memcpy(file->data, buf, len);
    }
    file->size = len;
    if (file->data) {
        file->data[len] = '\0';   /* keep text NUL-terminated for convenience */
    }
    return (int)len;
}

int vfs_delete(vfs_node_t *node)
{
    if (!node || node == root) {
        return -1;
    }

    /* Recursively delete children first. */
    if (node->type == VFS_DIR) {
        vfs_node_t *c = node->children;
        while (c) {
            vfs_node_t *next = c->next;
            vfs_delete(c);
            c = next;
        }
    }

    /* Unlink from the parent's child list. */
    vfs_node_t *parent = node->parent;
    if (parent) {
        if (parent->children == node) {
            parent->children = node->next;
        } else {
            for (vfs_node_t *c = parent->children; c; c = c->next) {
                if (c->next == node) {
                    c->next = node->next;
                    break;
                }
            }
        }
    }

    if (node->data) {
        kfree(node->data);
    }
    kfree(node);
    return 0;
}

void vfs_abspath(vfs_node_t *node, char *buf, size_t size)
{
    if (!node || size == 0) {
        if (size) buf[0] = '\0';
        return;
    }
    if (node == root) {
        strncpy(buf, "/", size);
        buf[size - 1] = '\0';
        return;
    }

    /* Walk up to the root collecting components, then assemble. */
    const char *parts[32];
    int count = 0;
    for (vfs_node_t *n = node; n && n != root && count < 32; n = n->parent) {
        parts[count++] = n->name;
    }

    size_t pos = 0;
    for (int i = count - 1; i >= 0; i--) {
        if (pos + 1 < size) buf[pos++] = '/';
        for (const char *c = parts[i]; *c && pos + 1 < size; c++) {
            buf[pos++] = *c;
        }
    }
    buf[pos < size ? pos : size - 1] = '\0';
}

/* Helper for population: create a file with initial text content. */
static void make_file(vfs_node_t *dir, const char *name, const char *content)
{
    vfs_node_t *f = vfs_create(dir, name, VFS_FILE);
    if (f) {
        vfs_write(f, content, (uint32_t)strlen(content));
    }
}

void vfs_init(void)
{
    root = node_alloc("", VFS_DIR);

    vfs_node_t *bin  = vfs_create(root, "bin", VFS_DIR);
    vfs_node_t *etc  = vfs_create(root, "etc", VFS_DIR);
    vfs_node_t *home = vfs_create(root, "home", VFS_DIR);
    vfs_create(root, "tmp", VFS_DIR);
    vfs_node_t *user = vfs_create(home, "user", VFS_DIR);

    make_file(etc, "motd",
              "Welcome to Zertreinia OS!\n"
              "A hand-written 32-bit operating system with a graphical desktop.\n");
    make_file(etc, "version", KERNEL_NAME " " KERNEL_VERSION "\n");
    make_file(etc, "hostname", "zertreinia\n");

    make_file(bin, "apps",
              "terminal\nfiles\neditor\nmonitor\nabout\n");

    make_file(user, "readme.txt",
              "This is your home directory.\n\n"
              "Try these in the Terminal:\n"
              "  ls            list files\n"
              "  cat <file>    show a file\n"
              "  mkdir <dir>   make a directory\n"
              "  touch <file>  create an empty file\n"
              "  write f text  write text into a file\n"
              "  edit <file>   open a file in the editor\n\n"
              "Double-click files in the File Manager to edit them.\n");
    make_file(user, "notes.txt",
              "Scratch notes - edit me in the Text Editor and press Save.\n");
    make_file(user, "todo.txt",
              "[ ] explore the desktop\n[ ] open the system monitor\n[ ] write some files\n");
}

vfs_node_t *vfs_root(void) { return root; }
