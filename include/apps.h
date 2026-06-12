/* Built-in applications. */
#ifndef ZERT_APPS_H
#define ZERT_APPS_H

#include "desktop.h"

/* Fill in a freshly-allocated window for the given app id (title, size,
   callbacks and private state). `arg` is interpreted per app (e.g. the editor
   takes a vfs_node_t* file to open). Returns false on failure. */
bool        app_create(window_t *win, int app_id, void *arg);

/* Short display name for taskbar buttons. */
const char *app_name(int app_id);

#endif /* ZERT_APPS_H */
