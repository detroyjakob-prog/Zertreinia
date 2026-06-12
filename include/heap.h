/* Kernel heap - a first-fit linked-list allocator with coalescing. */
#ifndef ZERT_HEAP_H
#define ZERT_HEAP_H

#include "types.h"

void  heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);

/* Heap usage statistics, in bytes. */
void  heap_stats(size_t *out_total, size_t *out_used, size_t *out_free);

#endif /* ZERT_HEAP_H */
