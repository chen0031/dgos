#include "types.h"

void paging_init();
uint32_t paging_root_addr();
void paging_map_range(
        uint64_t linear_base,
        uint64_t length,
        uint64_t phys_addr,
        uint64_t pte_flags);
