/* Paging - identity-maps low memory and enables the MMU. */
#ifndef ZERT_PAGING_H
#define ZERT_PAGING_H

#include "types.h"

/* Amount of physical memory we identity-map at boot (must be a multiple
   of 4 MiB). The kernel image, heap and all early structures live here. */
#define PAGING_IDENTITY_MB 64u

void paging_init(void);
bool paging_enabled(void);

/* Identity-map a physical region using 4 MiB pages (enables CR4.PSE).
   Used to make a high linear framebuffer reachable by the kernel. */
void paging_identity_map_4mb(uint32_t phys_start, uint32_t size);

#endif /* ZERT_PAGING_H */
