/* Virtual File System with an in-memory (RAM) backing store.
 *
 * A simple hierarchical filesystem: every node is either a directory (with a
 * linked list of children) or a file (with a heap-allocated data buffer).
 * Paths may be absolute ("/etc/motd") or relative, and support "." / "..". */
#ifndef ZERT_VFS_H
#define ZERT_VFS_H

#include "types.h"

#define VFS_NAME_MAX 64

typedef enum {
    VFS_FILE = 1,
    VFS_DIR  = 2,
} vfs_type_t;

typedef struct vfs_node {
    char              name[VFS_NAME_MAX];
    vfs_type_t        type;
    struct vfs_node  *parent;
    struct vfs_node  *children;   /* first child (directories only) */
    struct vfs_node  *next;       /* next sibling                   */
    uint8_t          *data;       /* file contents (files only)     */
    uint32_t          size;       /* used bytes                     */
    uint32_t          capacity;   /* allocated bytes                */
} vfs_node_t;

void        vfs_init(void);
vfs_node_t *vfs_root(void);

/* Create a child node; returns NULL on failure or name clash. */
vfs_node_t *vfs_create(vfs_node_t *dir, const char *name, vfs_type_t type);

/* Look up a single child by name. */
vfs_node_t *vfs_find_child(vfs_node_t *dir, const char *name);

/* Resolve a path relative to `cwd` (or from root if it starts with '/'). */
vfs_node_t *vfs_resolve(vfs_node_t *cwd, const char *path);

/* File I/O. vfs_write replaces the entire content. Both return bytes moved. */
uint32_t    vfs_read(vfs_node_t *file, void *buf, uint32_t max);
int         vfs_write(vfs_node_t *file, const void *buf, uint32_t len);

/* Remove a node (recursively for directories) and free its memory. */
int         vfs_delete(vfs_node_t *node);

/* Write the absolute path of `node` into buf. */
void        vfs_abspath(vfs_node_t *node, char *buf, size_t size);

#endif /* ZERT_VFS_H */
