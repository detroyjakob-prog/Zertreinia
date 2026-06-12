/* Kernel heap: a first-fit free-list allocator with block splitting and
 * forward/backward coalescing.
 *
 * The heap lives in a fixed, statically-reserved region inside the kernel's
 * BSS. This is simple and robust: it needs no page allocation and is always
 * covered by the identity map. Every block carries a small header. */

#include "heap.h"
#include "string.h"

#define HEAP_SIZE (4u * 1024u * 1024u)   /* 4 MiB */
#define ALIGN_TO  8u

/* Block header. `size` is the number of usable bytes that follow the header. */
typedef struct block {
    size_t        size;
    int           free;
    struct block *next;
    struct block *prev;
} block_t;

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));
static block_t *head;

static inline size_t align_up(size_t n)
{
    return (n + (ALIGN_TO - 1)) & ~(ALIGN_TO - 1);
}

void heap_init(void)
{
    head        = (block_t *)heap_area;
    head->size  = HEAP_SIZE - sizeof(block_t);
    head->free  = 1;
    head->next  = NULL;
    head->prev  = NULL;
}

/* Merge a free block with any free neighbours to fight fragmentation. */
static void coalesce(block_t *b)
{
    if (b->next && b->next->free) {
        b->size += sizeof(block_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(block_t) + b->size;
        b->prev->next = b->next;
        if (b->next) {
            b->next->prev = b->prev;
        }
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    size = align_up(size);

    for (block_t *cur = head; cur != NULL; cur = cur->next) {
        if (!cur->free || cur->size < size) {
            continue;
        }

        /* Split the block if there is room for another header + payload. */
        if (cur->size >= size + sizeof(block_t) + ALIGN_TO) {
            block_t *split =
                (block_t *)((uint8_t *)cur + sizeof(block_t) + size);
            split->size = cur->size - size - sizeof(block_t);
            split->free = 1;
            split->next = cur->next;
            split->prev = cur;
            if (cur->next) {
                cur->next->prev = split;
            }
            cur->next = split;
            cur->size = size;
        }

        cur->free = 0;
        return (void *)((uint8_t *)cur + sizeof(block_t));
    }

    return NULL;   /* heap exhausted */
}

void *kcalloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *p = kmalloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    block_t *b = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    b->free = 1;
    coalesce(b);
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr) {
        return kmalloc(size);
    }
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    block_t *b = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    if (b->size >= size) {
        return ptr;   /* current block is already big enough */
    }

    void *new_ptr = kmalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, b->size);
        kfree(ptr);
    }
    return new_ptr;
}

void heap_stats(size_t *out_total, size_t *out_used, size_t *out_free)
{
    size_t used = 0;
    size_t freeb = 0;
    for (block_t *cur = head; cur != NULL; cur = cur->next) {
        if (cur->free) {
            freeb += cur->size;
        } else {
            used += cur->size;
        }
    }
    if (out_total) *out_total = HEAP_SIZE;
    if (out_used)  *out_used  = used;
    if (out_free)  *out_free  = freeb;
}
