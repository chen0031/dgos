#include "mmu.h"
#include "debug.h"
#include "assert.h"
#include "mm.h"
#include "control_regs.h"
#include "interrupts.h"
#include "irq.h"
#include "apic.h"
#include "printk.h"
#include "time.h"
#include "string.h"
#include "atomic.h"
#include "bios_data.h"
#include "callout.h"
#include "likely.h"
#include "cpu/cpuid.h"
#include "thread_impl.h"
#include "threadsync.h"
#include "idt.h"
#include "bsearch.h"
#include "process.h"
#include "cpu_broadcast.h"
#include "errno.h"
#include "vector.h"
#include "main.h"
#include "inttypes.h"
#include "except.h"
#include "contig_alloc.h"
#include "asan.h"
#include "unique_ptr.h"
#include "cxxstring.h"
#include "nofault.h"
#include "phys_alloc.h"

// Allow G bit set in PDPT and PD in recursive page table mapping
// This causes KVM to throw #PF(reserved_bit_set|present)
#define GLOBAL_RECURSIVE_MAPPING    0

#define DEBUG_CREATE_PT         0
#define DEBUG_PAGE_TABLES       0
#define DEBUG_PAGE_FAULT        0

#define PROFILE_PHYS_ALLOC      0
#if PROFILE_PHYS_ALLOC
#define PROFILE_PHYS_ALLOC_ONLY(p) p
#else
#define PROFILE_PHYS_ALLOC_ONLY(p)
#endif

#define PROFILE_LINEAR_ALLOC    0
#if PROFILE_LINEAR_ALLOC
#define PROFILE_LINEAR_ALLOC_ONLY(p) p
#else
#define PROFILE_LINEAR_ALLOC_ONLY(p)
#endif

#define PROFILE_MMAP            0
#if PROFILE_MMAP
#define PROFILE_MMAP_ONLY(p) p
#else
#define PROFILE_MMAP_ONLY(p)
#endif

#define PROFILE_SLOWPATH        0
#if PROFILE_SLOWPATH
#define PROFILE_SLOWPATH_ONLY(p) p
#else
#define PROFILE_SLOWPATH_ONLY(p)
#endif

// Intel manual, page 2786

// The entries of the 4 levels of page tables are named:
//  PML4E (maps 512 512GB regions)
//  PDPTE (maps 512 1GB regions)
//  PDE   (maps 512 2MB regions)
//  PTE   (maps 512 4KB regions)

// 4KB pages


extern char ___text_st[];
extern char ___text_en[];

// -512GB
static uint64_t min_kern_addr = UINT64_C(0xFFFFFF8000000000);

#define PT_KERNBASE     uintptr_t(___text_st)
#define PT_MAX_ADDR     CANONICALIZE((PT_BEGIN + (UINT64_C(512) << 30)))

// Virtual address space map
// Low half
// +-----------------+---------------------------------+-------+
// |                 |                                 |       |
// +-----------------+---------------------------------+-------+
// |  4M - +128T-4M  | User space                      | <128T |
// +-----------------+---------------------------------+-------+----+
// |    0  -   +4M   | NULL Guard page                 |    4M |    |
// +-----------------+---------------------------------+-------+    |
// High half                                                   | 8M |<- nullptr
// +-----------------+---------------------------------+-------+    |
// |   -4M -    0    | Reserved per elf64 spec (guard) |    4M |    |
// +-----------------+---------------------------------+-------+----+
// | -512G -   -4M   | Reserved for kernel and modules | <512G |
// +-----------------+---------------------------------+-------+
// | -513G - -512G   | Zeroing page                    |    1G |
// +-----------------+---------------------------------+-------+
// | -127.5T - -512G | Kernel heap                     | <128T |
// +-----------------+---------------------------------+-------+
// | -128T - -127.5T | Recursive map                   |  512G |
// |                 | FFFF8000...-FFFF8080...         |       |
// +-----------------+---------------------------------+-------+

// Page tables for entire address, worst case, consists of
//  68719476736 4KB pages  (134217728 PT pages,   0x8000000000 bytes, 512GB)
//    134217728 2MB pages  (   262144 PD pages,     0x40000000 bytes,   1GB)
//       262144 1GB pages  (      512 PDPT pages,     0x200000 bytes,   4MB)
//                         (        1 PML4 page,        0x1000 bytes,   4KB)


static format_flag_info_t pte_flags[] = {
    { "XD",     1,                  nullptr, PTE_NX_BIT       },
    { "PK",     PTE_PK_MASK,        nullptr, PTE_PK_BIT       },
    { "62:52",  PTE_AVAIL2_MASK,    nullptr, PTE_AVAIL2_BIT   },
    { "PADDR",  PTE_ADDR,           nullptr, 0                },
    { "11:9",   PTE_AVAIL1_MASK,    nullptr, PTE_AVAIL1_BIT   },
    { "G",      1,                  nullptr, PTE_GLOBAL_BIT   },
    { "PS",     1,                  nullptr, PTE_PAGESIZE_BIT },
    { "D",      1,                  nullptr, PTE_DIRTY_BIT    },
    { "A",      1,                  nullptr, PTE_ACCESSED_BIT },
    { "PCD",    1,                  nullptr, PTE_PCD_BIT      },
    { "PWT",    1,                  nullptr, PTE_PWT_BIT      },
    { "U",      1,                  nullptr, PTE_USER_BIT     },
    { "W",      1,                  nullptr, PTE_WRITABLE_BIT },
    { "P",      1,                  nullptr, PTE_PRESENT_BIT  },
    { nullptr,  0,                  nullptr, -1               }
};

static format_flag_info_t pf_err_flags[] = {
    { "Pr",         1,          nullptr, CTX_ERRCODE_PF_P_BIT   },
    { "Wr",         1,          nullptr, CTX_ERRCODE_PF_W_BIT   },
    { "Usr",        1,          nullptr, CTX_ERRCODE_PF_U_BIT   },
    { "RsvdBit",    1,          nullptr, CTX_ERRCODE_PF_R_BIT   },
    { "Insn",       1,          nullptr, CTX_ERRCODE_PF_I_BIT   },
    { "PK",         1,          nullptr, CTX_ERRCODE_PF_PK_BIT  },
    { "14:6",       0x1FF,      nullptr, 6                      },
    { "SGX",        1,          nullptr, CTX_ERRCODE_PF_SGX_BIT },
    { "31:16",      0xFFFF,     nullptr, 16                     },
    { nullptr,      1,          nullptr, -1                     }
};

static char const *pte_level_names[] = {
    "PML4",
    "PDPT",
    "PD",
    "PTE"
};

static size_t mmu_describe_pte(char *buf, size_t sz, pte_t pte)
{
    return format_flags_register(buf, sz, pte, pte_flags);
}

static size_t mmu_describe_pf(char *buf, size_t sz, int err_code)
{
    return format_flags_register(buf, sz, err_code, pf_err_flags);
}

static void mmu_dump_ptes(pte_t **ptes)
{
    char fmt_buf[64];

    bool present = true;

    for (size_t i = 0; i < 4; ++i) {
        if (present) {
            mmu_describe_pte(fmt_buf, sizeof(fmt_buf), *ptes[i]);
            printdbg("%5s: %016" PRIx64 " %s\n",
                     pte_level_names[i], *ptes[i], fmt_buf);
        } else {
            printdbg("%5s: <not present>\n", pte_level_names[i]);
        }

        if (!(*ptes[i] & PTE_PRESENT))
            present = false;
    }
}

static void mmu_dump_pf(uintptr_t pf)
{
    char fmt_buf[64];
    mmu_describe_pf(fmt_buf, sizeof(fmt_buf), pf);
    printdbg("#PF: %#04zx %s\n", pf, fmt_buf);
}

//
// Device mapping

// Device registration for memory mapped device
struct mmap_device_mapping_t {
    ext::unique_mmap<char> range;
    mm_dev_mapping_callback_t callback;
    void *context;
    std::mutex lock;
    std::condition_variable done_cond;
    int64_t active_read;
};

static int mm_dev_map_search(void const *v, void const *k, void *s);

static std::vector<mmap_device_mapping_t*> mm_dev_mappings;
using mm_dev_mapping_lock_type = ext::noirq_lock<ext::spinlock>;
using mm_dev_mapping_scoped_lock = std::unique_lock<mm_dev_mapping_lock_type>;
static mm_dev_mapping_lock_type mm_dev_mapping_lock;

/// Physical page allocation map
/// Consists of array of entries, one per physical page
/// Each slot contains the index of the next slot,
/// which forms a singly-linked list of pages.
/// Free pages are linked into a chain, allowing for O(1)
/// performance allocation and freeing of physical pages.
///
/// 2^32 pages is 2^(32+12-40) bytes = 16TB
///
/// This results in a 0.098% overhead for the physical page
/// allocation map at 4 bytes per 4KB.

// Original memory map
physmem_range_t phys_mem_map[128];
size_t phys_mem_map_count;

// Memory map
static physmem_range_t early_mem_ranges[128];
static size_t usable_early_mem_ranges;

physaddr_t root_physaddr;
static pte_t const * master_pagedir;
static pte_t *current_pagedir;

extern char ___init_brk[];

// Used as a simple bump allocator to get address space early on
static linaddr_t linear_base = PT_MAX_ADDR;

static thread_cpu_mask_t volatile shootdown_pending;

static contiguous_allocator_t linear_allocator;
//static contiguous_allocator_t near_allocator;
static contiguous_allocator_t contig_phys_allocator;
static contiguous_allocator_t hole_allocator;

//
// Contiguous physical memory allocator

EXPORT uintptr_t mm_alloc_contiguous(size_t size)
{
    return contig_phys_allocator.alloc_linear(round_up(size));
}

EXPORT void mm_free_contiguous(uintptr_t addr, size_t size)
{
    assert(!(addr & PAGE_MASK));
    contig_phys_allocator.release_linear(addr, round_up(size));
}

//
// Physical page memory allocator

static void mmu_free_phys(physaddr_t addr)
{
    phys_allocator.release_one(addr);
}

static physaddr_t mmu_alloc_phys()
{
    physaddr_t page;

    page = phys_allocator.alloc_one();

    return page;
}

//
// Path to PTE

static _always_inline void path_from_addr(unsigned *path, linaddr_t addr)
{
    path[3] = (addr >>= 12) & 0x1FF;
    path[2] = (addr >>= 9) & 0x1FF;
    path[1] = (addr >>= 9) & 0x1FF;
    path[0] = (addr >>= 9) & 0x1FF;
}

static int ptes_present(pte_t **ptes)
{
    int present_mask;

    present_mask = (*ptes[0] & PTE_PRESENT);
    present_mask |= (likely(present_mask == 1) &&
                     (*ptes[1] & PTE_PRESENT)) << 1;
    if (likely(present_mask == 3 && !(*ptes[1] & PTE_PAGESIZE))) {
        present_mask |= (*ptes[2] & PTE_PRESENT) << 2;
        if (likely(present_mask == 7 && !(*ptes[2] & PTE_PAGESIZE))) {
            present_mask |= (*ptes[3] & PTE_PRESENT) << 3;
        }
    }

    return present_mask;
}

// Returns the linear addresses of the page tables for the given path
static _always_inline void ptes_from_path(pte_t **pte, unsigned *path)
{
    uintptr_t page_index =
            (((uintptr_t)path[0]) << (9 * 3)) +
            (((uintptr_t)path[1]) << (9 * 2)) +
            (((uintptr_t)path[2]) << (9 * 1)) +
            (((uintptr_t)path[3]) << (9 * 0));

    uintptr_t indices[4];

    indices[0] = (page_index >> (9 * 3)) & (PT0_ENTRIES-1);
    indices[1] = (page_index >> (9 * 2)) & (PT1_ENTRIES-1);
    indices[2] = (page_index >> (9 * 1)) & (PT2_ENTRIES-1);
    indices[3] = (page_index >> (9 * 0)) & (PT3_ENTRIES-1);

    pte[0] = PT0_PTR + indices[0];
    pte[1] = PT1_PTR + indices[1];
    pte[2] = PT2_PTR + indices[2];
    pte[3] = PT3_PTR + indices[3];
}

static int path_present(unsigned *path, pte_t **ptes)
{
    ptes_from_path(ptes, path);
    return ptes_present(ptes);
}

static int addr_present(uintptr_t addr,
                        unsigned *path, pte_t **ptes)
{
    path_from_addr(path, addr);
    return path_present(path, ptes);
}

// Returns linear page index (addr>>12) represented by the specified path
static _always_inline uintptr_t path_inc(unsigned *path)
{
    // Branchless algorithm

    uintptr_t n =
            (linaddr_t)(path[3] << (9 * 0)) |
            ((linaddr_t)path[2] << (9 * 1)) |
            ((linaddr_t)path[1] << (9 * 2)) |
            ((linaddr_t)path[0] << (9 * 3));

    ++n;

    path[3] = (n >> (9 * 0)) & 0x1FF;
    path[2] = (n >> (9 * 1)) & 0x1FF;
    path[1] = (n >> (9 * 2)) & 0x1FF;
    path[0] = (n >> (9 * 3)) & 0x1FF;

    return n;
}

// Returns present mask for new page
static _always_inline int path_inc(unsigned *path, pte_t **ptes)
{
    uintptr_t n = path_inc(path);

    ptes[3] = PT3_PTR + (n >> (9 * 0));
    ptes[2] = PT2_PTR + (n >> (9 * 1));
    ptes[1] = PT1_PTR + (n >> (9 * 2));
    ptes[0] = PT0_PTR + (n >> (9 * 3));

    return ptes_present(ptes);
}

//static void mmu_mem_map_swap(physmem_range_t *a, physmem_range_t *b)
//{
//    physmem_range_t temp = *a;
//    *a = *b;
//    *b = temp;
//}

_used
static int ptes_leaf_level(pte_t const **ptes)
{
    pte_t constexpr present_large = PTE_PRESENT | PTE_PAGESIZE;

    if (unlikely((*ptes[0] & PTE_PRESENT) == 0))
        return -1;

    for (int level = 1; level < 3; ++level) {
        pte_t const pte = *ptes[level];

        if (unlikely((pte & PTE_PRESENT) == 0))
            return -1;

        if (unlikely((pte & present_large) == present_large))
            return level;
    }

    return 3;
}

//
// Page table garbage collection (frees entirely unused page table pages)
//

static void mmu_gc_pt_2M(
        mmu_phys_allocator_t::free_batch_t free_batch,
        uint64_t addr_st, uint64_t addr_en, int limit)
{
    pte_t *ptes_st[4];
    pte_t *ptes_en[4];

    ptes_from_addr(ptes_st, addr_st);
    ptes_from_addr(ptes_en, addr_en);

    size_t count = (ptes_en[2] + 512) - ptes_st[2];

    for (size_t i = 0; i < count; ++i) {
        if (ptes_st[2][i] & PTE_PRESENT) {
            free_batch.free(ptes_st[2][i] & PTE_ADDR);
            ptes_st[2][i] = 0;
        }
    }
}

static void mmu_gc_pt_1G(
        mmu_phys_allocator_t::free_batch_t free_batch,
        uint64_t addr_st, uint64_t addr_en, int limit)
{

}

static void mmu_gc_pt_512G(
        mmu_phys_allocator_t::free_batch_t free_batch,
        uint64_t addr_st, uint64_t addr_en, int limit)
{

}

void mmu_gc_pt(int limit)
{
    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    linear_allocator.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        uint64_t end = range.base + range.size - 1;

        uint64_t mask = UINT64_C(1) << 39;

        // Round start up, round end down.

        // Gets inclusive range of entirely free 512GB PDPTs
        uint64_t st_512G = (range.base + mask - 1) & -mask;
        uint64_t en_512G = end & -mask;

        mask >>= 9;

        // Gets inclusive range of entirely free 1GB PDs
        uint64_t st_1G = (range.base + mask - 1) & -mask;
        uint64_t en_1G = end & -mask;

        mask >>= 9;

        // Gets inclusive range of entirely free 2MB PTs
        uint64_t st_2M = (range.base + mask - 1) & -mask;
        uint64_t en_2M = end & -mask;

        if (en_512G >= st_512G) {
            // Free 512GB aligned 512GB ranges (PDPT pages)
            mmu_gc_pt_512G(free_batch, st_512G, en_512G, limit);
        } else if (en_1G >= st_1G) {
            // Free 1GB aligned 1GB ranges (PD pages)
            mmu_gc_pt_1G(free_batch, st_1G, en_1G, limit);
        } else if (en_2M >= st_2M) {
            // Free 2MB aligned 1MB ranges (PT pages)
            mmu_gc_pt_2M(free_batch, st_2M, en_2M, limit);
        }

        return limit-- > 0;
    });
}

//
// Physical memory allocation map processing

// Align bases to page boundaries
// Align sizes to multiples of page size
// Sort by base address
// Fix overlaps
static size_t mmu_fixup_mem_map(physmem_range_t *list)
{
    int did_something;
    size_t usable_count = 0;

    // Might need multiple passes to fully fixup the list
    do {
        did_something = 0;

//        // Make invalid and zero size entries get removed
//        for (size_t i = 1; i < phys_mem_map_count; ++i) {
//            if (list[i].size == 0 || !list[i].valid) {
//                list[i].base = ~0UL;
//                list[i].size = ~0UL;
//            }
//        }

//        // Simple sort algorithm for small list.
//        // Bubble sort is actually fast for nearly-sorted
//        // or already-sorted list. The list is probably
//        // already sorted
//        for (size_t i = 1; i < phys_mem_map_count; ++i) {
//            // Slide it back to where it should be
//            for (size_t j = i; j > 0; --j) {
//                if (list[j - 1].base < list[j].base)
//                    break;

//                mmu_mem_map_swap(list + j - 1, list + j);
//                did_something = 1;
//            }
//        }

//        if (did_something) {
//            printk("Memory map order fixed up\n");
//            continue;
//        }

//        // Discard entries marked for removal from the end
//        while (phys_mem_map_count > 0 &&
//               list[phys_mem_map_count-1].base == ~0UL &&
//               list[phys_mem_map_count-1].size == ~0UL) {
//            --phys_mem_map_count;
//            did_something = 1;
//        }

//        if (did_something) {
//            printk("Memory map entries were eliminated\n");
//            continue;
//        }

//        // Fixup overlaps
//        for (size_t i = 1; i < phys_mem_map_count; ++i) {
//            if (list[i-1].base + list[i-1].size > list[i].base) {
//                // Overlap

//                did_something = 1;

//                // Compute bounds of prev
//                physaddr_t prev_st = list[i-1].base;
//                physaddr_t prev_en = prev_st + list[i-1].size;

//                // Compute bounds of this
//                physaddr_t this_st = list[i].base;
//                physaddr_t this_en = this_st + list[i].size;

//                // Start at lowest bound, end at highest bound
//                physaddr_t st = prev_st < this_st ? prev_st : this_st;
//                physaddr_t en = prev_en > this_en ? prev_en : this_en;

//                if (list[i-1].type == list[i].type) {
//                    // Both same type,
//                    // make one cover combined range

//                    printk("Combining overlapping ranges of same type\n");

//                    // Remove previous entry
//                    list[i-1].base = ~0UL;
//                    list[i-1].size = ~0UL;

//                    list[i].base = st;
//                    list[i].size = en - st;

//                    break;
//                } else if (list[i-1].type == PHYSMEM_TYPE_NORMAL) {
//                    // This entry takes precedence over prev entry

//                    if (st < this_st && en > this_en) {
//                        // Punching a hole in the prev entry

//                        printk("Punching hole in memory range\n");

//                        // Reduce size of prev one to not overlap
//                        list[i-1].size = this_st - prev_st;

//                        // Make new entry with normal memory after this one
//                        // Sort will put it in the right position later
//                        list[phys_mem_map_count].base = this_en;
//                        list[phys_mem_map_count].size = en - this_en;
//                        list[phys_mem_map_count].type = PHYSMEM_TYPE_NORMAL;
//                        list[phys_mem_map_count].valid = 1;
//                        ++phys_mem_map_count;

//                        break;
//                    } else if (st < this_st && en >= this_en) {
//                        // Prev entry partially overlaps this entry

//                        printk("Correcting overlap\n");

//                        list[i-1].size = this_st - prev_st;
//                    }
//                } else {
//                    // Prev entry takes precedence over this entry

//                    if (st < this_st && en > this_en) {
//                        // Prev entry eliminates this entry
//                        list[i].base = ~0UL;
//                        list[i].size = ~0UL;

//                        printk("Removing completely overlapped range\n");
//                    } else if (st < this_st && en >= this_en) {
//                        // Prev entry partially overlaps this entry

//                        printk("Correcting overlap\n");

//                        list[i].base = prev_en;
//                        list[i].size = this_en - prev_en;
//                    }
//                }
//            } else if (list[i-1].type == list[i].type &&
//                       list[i-1].base + list[i-1].size == list[i].base) {
//                // Merge adjacent ranges of the same type
//                list[i].size += list[i-1].size;
//                list[i].base -= list[i-1].size;
//                list[i-1].base = ~0;
//                list[i-1].size = ~0;

//                did_something = 1;

//                printk("Merging adjacent range of same type\n");
//            }
//        }

//        if (did_something)
//            continue;

        usable_count = 0;

//        // Fixup page alignment
        for (size_t i = 0; i < phys_mem_map_count; ++i) {
            if (list[i].type == PHYSMEM_TYPE_NORMAL) {
                ++usable_count;

//                physaddr_t st = list[i].base;
//                physaddr_t en = list[i].base + list[i].size;

//                // Align start to a page boundary
//                st += PAGE_MASK;
//                st -= st & PAGE_MASK;

//                // Align end to a page boundary
//                en -= en & PAGE_MASK;

//                if (st == en || st != list[i].base ||
//                        en != list[i].base + list[i].size) {
//                    if (en > st) {
//                        list[i].base = st;
//                        list[i].size = en - st;
//                    } else {
//                        list[i].base = ~0UL;
//                        list[i].size = ~0UL;
//                    }
//                    did_something = 1;
//                }
            }
        }

        if (did_something) {
            printk("Memory map entry was not page aligned\n");
            continue;
        }
    } while (did_something);

    printk("Memory map fixup complete\n");

    return usable_count;
}

//
// Initialization page allocator

static physaddr_t init_take_page()
{
    if (likely(usable_early_mem_ranges == 0))
        return mmu_alloc_phys();

    assert(usable_early_mem_ranges > 0);

    physmem_range_t *range = nullptr;
    for (size_t i = usable_early_mem_ranges; i > 0; --i) {
        auto& chk = early_mem_ranges[i - 1];
        if (chk.size >= PAGE_SIZE) {
            range = &chk;
            break;
        }
    }

    assert(range);
    if (unlikely(!range))
        return 0;

    physaddr_t addr = range->base + range->size - PAGE_SIZE;

    // Take a page off the end of the range
    range->size -= PAGE_SIZE;

#if DEBUG_PHYS_ALLOC
    printdbg("Took early page @ %" PRIx64 "\n", addr);
#endif

    return addr;
}

// Return a pointer to the initial physical mapping
template<typename T>
static _always_inline constexpr T *init_phys(uint64_t addr)
{
//    if (phys_mapping)
//        return phys_mapping + addr;

    //assert(addr <= 0xFFFFFFFFU);

    // Physical mappings at highest 512GB boundary
    // that is >= 512GB before .text
    assert(addr < kernel_params->phys_mapping_sz);

    uint64_t physmap = kernel_params->phys_mapping;

    return (T*)(physmap + addr);
}

//
// Zero initialization

struct clear_phys_state_t {
    using lock_type = ext::noirq_lock<ext::spinlock>;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type locks[64];

    pte_t *pte;
    int log2_window_sz;
    int level;

    size_t cls_tlb_ver;

    // 3TB before end of address space
    static constexpr linaddr_t addr = 0xFFFFFD0000000000;

    void clear(physaddr_t addr);
    void reserve_addr();
};

static clear_phys_state_t clear_phys_state;

void mm_phys_clear_init()
{
    printdbg("Initializing physical memory clearing\n");

    if (cpuid_has_1gpage()) {
        clear_phys_state.log2_window_sz = 30;
        clear_phys_state.level = 1;
        printdbg("Using %s page(s) to clear page frames\n", "1GB");
    } else if (cpuid_has_2mpage()) {
        clear_phys_state.log2_window_sz = 21;
        clear_phys_state.level = 2;
        printdbg("Using %s page(s) to clear page frames\n", "2MB");
    } else {
        clear_phys_state.log2_window_sz = 12;
        clear_phys_state.level = 3;
        printdbg("Using %s page(s) to clear page frames\n", "4KB");
    }

    pte_t *ptes[4];
    ptes_from_addr(ptes, clear_phys_state_t::addr);

    pte_t global_mask = ~PTE_GLOBAL;

    // Allocate a CPU-local storage slot for per-cpu TLB invalidation version
    clear_phys_state.cls_tlb_ver = thread_cls_alloc();
    thread_cls_init_each_cpu(clear_phys_state.cls_tlb_ver, false, [&] {
        return nullptr;
    });

    pte_t *page_base = nullptr;
    for (int i = 0; i < clear_phys_state.level; ) {
        assert(*ptes[i] == 0);
        physaddr_t page = init_take_page();
        printdbg("Assigning phys_clear_init table"
                 " pte=%p page=%#.16" PRIx64 "\n",
                 (void*)ptes[i], page);
        *ptes[i] = (page | PTE_PRESENT | PTE_WRITABLE |
                PTE_ACCESSED | PTE_DIRTY | PTE_GLOBAL) & global_mask;

        // Clear new page table page
        cpu_page_invalidate(uintptr_t(ptes[++i]));
        clear_phys_state.pte = ptes[i];
        page_base = (pte_t*)(uintptr_t(ptes[i]) & -PAGE_SIZE);
        printdbg("page_base=%p\n", (void*)page_base);
        memset(page_base, 0, PAGE_SIZE);

        global_mask = -1;
    }
}

static void clear_phys(physaddr_t addr)
{
    size_t index = 0;
    pte_t& pte = clear_phys_state.pte[index << 3];

    pte_t page_flags;
    size_t offset;
    physaddr_t base;
    if (likely(clear_phys_state.log2_window_sz == 30)) {
        base = addr & -(1 << 30);
        page_flags = PTE_PAGESIZE;
    } else if (clear_phys_state.log2_window_sz == 21) {
        base = addr & -(1 << 21);
        page_flags = PTE_PAGESIZE;
    } else if (clear_phys_state.log2_window_sz == 12) {
        base = addr & -(1 << 12);
        page_flags = 0;
    } else {
        cpu_debug_break();
        __builtin_unreachable();
    }

    offset = addr - base;

    clear_phys_state_t::scoped_lock lock(clear_phys_state.locks[index]);

    linaddr_t window = clear_phys_state_t::addr +
            (index << (3 + clear_phys_state.log2_window_sz));

    pte_t expect = base | page_flags | PTE_WRITABLE | PTE_GLOBAL |
            PTE_ACCESSED | PTE_DIRTY | PTE_PRESENT;

    if (pte != expect)
        pte = expect;

    // Shootdowns for this are never done so just flush that TLB entry
    cpu_page_invalidate(window);

    clear64((char*)window + offset, PAGE_SIZE >> 3);
}

//
// Page table creation

static void mmu_map_page(linaddr_t addr, physaddr_t physaddr, pte_t flags)
{
    pte_t *ptes[4];
    pte_t pte;

    ptes_from_addr(ptes, addr);
    int present_mask = ptes_present(ptes);

    // G is MBZ in PML4e on AMD
    pte_t global_mask = ~PTE_GLOBAL;

    if (unlikely((present_mask & 0x07) != 0x07)) {
        pte_t path_flags = PTE_PRESENT | PTE_WRITABLE |
                (addr <= 0x7FFFFFFFFFFF ? PTE_USER : 0);

        for (int i = 0; i < 3; ++i) {
            pte = *ptes[i];

            if (!pte) {
                physaddr_t ptaddr = init_take_page();
                clear_phys(ptaddr);
                *ptes[i] = (ptaddr | path_flags) & global_mask;
                if (i < 3)
                    cpu_page_invalidate(uintptr_t(ptes[i + 1]));
            }
        }

#if GLOBAL_RECURSIVE_MAPPING
        global_mask = -1;
#endif
    }

    *ptes[3] = (physaddr & PTE_ADDR) | flags;
}

static void mmu_tlb_perform_shootdown(void)
{
    cpu_tlb_flush();
    thread_shootdown_notify();
}

// TLB shootdown IPI
static isr_context_t *mmu_tlb_shootdown_handler(int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_IPI_TLB_SHTDN);

    uint32_t cpu_nr = thread_cpu_number();

    // Clear pending
    shootdown_pending.atom_clr(cpu_nr);

    mmu_tlb_perform_shootdown();

    return ctx;
}

static void mmu_send_tlb_shootdown(bool synchronous = false)
{
    uint32_t cpu_count = thread_cpu_count();

    // Skip if too early or uniprocessor
    if (unlikely(cpu_count <= 1))
        return;

    cpu_scoped_irq_disable irq_was_enabled;
    uint32_t cur_cpu_nr = thread_cpu_number();
    thread_cpu_mask_t all_cpu_mask(-1);
    thread_cpu_mask_t cur_cpu_mask(cur_cpu_nr);
    thread_cpu_mask_t other_cpu_mask = all_cpu_mask - cur_cpu_mask;
    thread_cpu_mask_t new_pending = shootdown_pending.atom_or(other_cpu_mask);
    thread_cpu_mask_t need_ipi_mask = new_pending & other_cpu_mask;

    std::vector<uint64_t> shootdown_counts;
    if (synchronous) {
        shootdown_counts.reserve(thread_cpu_count());
        for (uint32_t i = 0; i < cpu_count; ++i) {
            if (unlikely(!shootdown_counts.push_back(
                             (i != cur_cpu_nr)
                             ? thread_shootdown_count(i)
                             : -1)))
                panic_oom();
        }
    }

    if (!!other_cpu_mask) {
        if (need_ipi_mask == other_cpu_mask) {
            // Send to all other CPUs
            apic_send_ipi(-1, INTR_IPI_TLB_SHTDN);
        } else {
            int cpu_mask = 1;
            for (uint32_t cpu = 0; cpu < cpu_count; ++cpu, cpu_mask <<= 1) {
                if (!(new_pending & thread_cpu_mask_t(cpu_mask)) &&
                        !!(need_ipi_mask & thread_cpu_mask_t(cpu_mask)))
                    thread_send_ipi(cpu, INTR_IPI_TLB_SHTDN);
            }
        }
    }

    if (unlikely(synchronous)) {
        uint64_t wait_st = nano_time();
        uint64_t loops = 0;
        for (int wait_count = cpu_count - 1; wait_count > 0; pause()) {
            for (uint32_t i = 0; i < cpu_count; ++i) {
                uint64_t &count = shootdown_counts[i];
                if (count != uint64_t(-1) &&
                        thread_shootdown_count(i) > count) {
                    count = -1;
                    --wait_count;
                }
            }
            ++loops;
        }
        uint64_t wait_en = nano_time();

        printdbg("TLB shootdown waited for "
                 "%" PRIu64 "u loops, %" PRIu64 " cycles\n", loops,
                 wait_en - wait_st);
    }
}

static intptr_t mmu_device_from_addr(linaddr_t rounded_addr)
{
    mm_dev_mapping_scoped_lock lock(mm_dev_mapping_lock);

    intptr_t device = binary_search(
                mm_dev_mappings.data(), mm_dev_mappings.size(),
                sizeof(*mm_dev_mappings.data()),
                (void*)rounded_addr,
                mm_dev_map_search, nullptr, 1);

    return device;
}

// Page fault
isr_context_t *mmu_page_fault_handler(int intr _unused, isr_context_t *ctx)
{
    if (likely(thread_cpu_count()))
        atomic_inc((cpu_gs_ptr<uint64_t, CPU_INFO_PF_COUNT_OFS>()));

    //printdbg("page fault\n");

    uintptr_t fault_addr = cpu_fault_address_get();

    pte_t *ptes[4];

    ptes_from_addr(ptes, fault_addr);
    int present_mask = ptes_present(ptes);

    uintptr_t const err_code = ISR_CTX_ERRCODE(ctx);

    // Examine the error code
    if (unlikely(err_code & CTX_ERRCODE_PF_R)) {
        // Reserved bit violation?!
        mmu_dump_pf(err_code);
        mmu_dump_ptes(ptes);
        cpu_debug_break();
        return nullptr;
    }

#if DEBUG_PAGE_FAULT
    printdbg("Page fault at %p\n", (void*)fault_addr);
#endif

    pte_t pte = (present_mask >= 0x07) ? *ptes[3] : 0;

    // Check for SMAP violation
    // (kernel accessing user mode memory with EFLAGS.AC==0)
    if (unlikely(ISR_CTX_REG_CS(ctx) == GDT_SEL_KERNEL_CODE64 &&
                 (ISR_CTX_REG_RFLAGS(ctx) & CPU_EFLAGS_AC) == 0 &&
                 cpuid_has_smap() &&
                 (pte & PTE_USER))) {
        printdbg("Supervisor mode access prevention violation\n");
        dump_context(ctx, true);
        cpu_debug_break();
        return nullptr;
    }

    // Check for lazy TLB shootdown
    if (present_mask == 0xF) {
        // It is a not a lazy shootdown, if
        //  - there was a reserved bit violation, or,
        //  - there was a protection key violation, or,
        //  - there was an SGX violation, or,
        //  - the pte is not present, or,
        //  - the access was a write and the pte is not writable, or,
        //  - the access was an insn fetch and the pte is not executable
        if (likely(!((err_code & CTX_ERRCODE_PF_R) ||
                     (err_code & CTX_ERRCODE_PF_PK) ||
                     (err_code & CTX_ERRCODE_PF_SGX) ||
                     ((err_code & CTX_ERRCODE_PF_W) &&
                      !(pte & PTE_WRITABLE)) ||
                     ((err_code & CTX_ERRCODE_PF_I) &&
                      (pte & PTE_NX)))))
            return ctx;
    }

    bool should_panic = false;

    // If the page table exists
    if (present_mask == 0x07) {
        // If it is lazy allocated
        if ((pte & (PTE_ADDR | PTE_EX_DEVICE)) == PTE_ADDR) {
            // Allocate a page
            physaddr_t page = mmu_alloc_phys();

            assert(page != 0);

#if DEBUG_PAGE_FAULT
            printdbg("Assigning %#zx with page %#zx\n",
                     fault_addr, page);
#endif

            pte_t page_flags;

            if (err_code & CTX_ERRCODE_PF_W) {
                // It was a write fault, mark it dirty. If not done here,
                // the CPU will beforced to do a locked RMW to set it
                // when it restarts the instruction
                page_flags = PTE_PRESENT | PTE_ACCESSED | PTE_DIRTY;
            } else {
                page_flags = PTE_PRESENT | PTE_ACCESSED;
            }

            // Update PTE and restart instruction
            if (unlikely(atomic_cmpxchg(
                             ptes[3], pte, (pte & ~PTE_ADDR) |
                             (page & PTE_ADDR) | page_flags) != pte)) {
                // Another thread beat us to it
                mmu_free_phys(page);
                page = 0;

                // Invalidate to make sure we pick up their change
                cpu_page_invalidate(fault_addr);

                // If not perfect, it'll fault again and we'll start over
                // and probably not race that time
            }

            return ctx;
        } else if (pte & PTE_EX_DEVICE) {
            //
            // Device mapping

            linaddr_t rounded_addr = fault_addr & -(intptr_t)PAGE_SIZE;


            // Lookup the device mapping
            intptr_t device = mmu_device_from_addr(rounded_addr);
            if (unlikely(device < 0))
                return nullptr;

            assert(device < (intptr_t)mm_dev_mappings.size());

            mmap_device_mapping_t *mapping = mm_dev_mappings[device];

            uint64_t mapping_offset = (char*)rounded_addr -
                    (char*)mapping->range;

            // Round down to nearest 64KB boundary
            mapping_offset &= -0x10000;

            rounded_addr = linaddr_t(mapping->range.get()) + mapping_offset;

            pte_t volatile *vpte = ptes[3];

            // Attempt to be the first CPU to start reading a block
            std::unique_lock<std::mutex> lock(mapping->lock);
            while (mapping->active_read >= 0 &&
                   !(*vpte & PTE_PRESENT))
                mapping->done_cond.wait(lock);

            // If the page became present while waiting, then done
            if (*vpte & PTE_PRESENT)
                return ctx;

            // Become the reader for this mapping
            mapping->active_read = mapping_offset;
            lock.unlock();

            int io_result = mapping->callback(
                        mapping->context, (void*)rounded_addr,
                        mapping_offset, 0x10000, true, false);

            if (likely(io_result >= 0)) {
                // Mark the range present from end to start
                ptes_from_addr(ptes, rounded_addr);
                for (size_t i = (0x10000 >> PAGE_SIZE_BIT); i > 0; --i)
                    atomic_or(ptes[3] + (i - 1), PTE_PRESENT | PTE_ACCESSED);
            }

            lock.lock();
            mapping->active_read = -1;
            lock.unlock();
            mapping->done_cond.notify_all();

            // Restart the instruction, or unhandled exception on I/O error
            return likely(io_result >= 0) ? ctx : nullptr;
        } else if (pte & PTE_EX_WAIT) {
            // Must wait for another CPU to finish doing something with PTE
            cpu_wait_bit_clear(ptes[3], PTE_EX_WAIT_BIT);
            return ctx;
        } else if (nofault_ip_in_range(uintptr_t(ISR_CTX_REG_RIP(ctx)))) {
            //
            // Fault occurred in nofault code region

            // Figure out which landing pad to use
            uintptr_t landing_pad = nofault_find_landing_pad(
                        uintptr_t(ISR_CTX_REG_RIP(ctx)));

            if (likely(landing_pad)) {
                // Put CPU on landing pad with rax == -1
                ISR_CTX_REG_RAX(ctx) = -1;
                ISR_CTX_REG_RIP(ctx) = (int(*)(void*))landing_pad;
                return ctx;
            }

            goto no_landing_pad;
        } else {
no_landing_pad:
            thread_t tid = thread_get_id();
            unsigned cpu = thread_current_cpu(-1);
            printdbg("Invalid page fault at %#zx, RIP=%p (tid=%d, cpu=%d)\n",
                     fault_addr, (void*)ISR_CTX_REG_RIP(ctx),
                     tid, cpu);
            if (thread_get_exception_top())
                return nullptr;

            dump_context(ctx, 1);

            linear_allocator.dump("page fault");
            linear_allocator.validate();

            should_panic = true;
        }
    } else if (present_mask != 0x0F) {
        if (thread_get_exception_top())
            return nullptr;

        dump_context(ctx, 1);

        assert(!"Invalid page fault path");
    } else if (present_mask == 0x0F &&
               (pte & PTE_NX) &&
               (err_code & CTX_ERRCODE_PF_I)) {
        printdbg("#PF: Instruction fetch from no-execute page\n");
    }

    printdbg("#PF: CR2=%#zx\n", fault_addr);

    printdbg("#PF: present=%d\n"
             "     write=%d\n"
             "     user=%d\n"
             "     reserved bit violation=%d\n"
             "     instruction fetch=%d\n"
             "     protection key violation=%d\n"
             "     SGX violation=%d\n",
             !!(err_code & CTX_ERRCODE_PF_P),
             !!(err_code & CTX_ERRCODE_PF_W),
             !!(err_code & CTX_ERRCODE_PF_U),
             !!(err_code & CTX_ERRCODE_PF_R),
             !!(err_code & CTX_ERRCODE_PF_I),
             !!(err_code & CTX_ERRCODE_PF_PK),
             !!(err_code & CTX_ERRCODE_PF_SGX));

    printdbg("     present_mask=%#x\n",
             present_mask);

    mmu_dump_pf(err_code);
    mmu_dump_ptes(ptes);

    if (unlikely(should_panic))
        panic("Invalid page fault");

    return nullptr;
}

//
// Initialization

static int mmu_have_pat(void)
{
    // 0 == unknown, 1 == supported, -1 == not supported
    static int supported;
    if (likely(supported != 0))
        return supported > 0;

    supported = (cpuid_edx_bit(16, 1, 0) << 1) - 1;

    return supported > 0;
}

static void mmu_configure_pat(void)
{
    if (likely(mmu_have_pat()))
        cpu_msr_set(CPU_MSR_IA32_PAT, PAT_CFG);
}

#define INIT_DEBUG 1
#if INIT_DEBUG
#define TRACE_INIT(...) printdbg("mmu init: " __VA_ARGS__)
#else
#define TRACE_INIT(...) ((void)0)
#endif

_constructor(ctor_mmu_init) static void mmu_init_bsp()
{
    mmu_init();
}

static bool mmu_init_pagedir(size_t idx, uint8_t log2_pagesz, physaddr_t paddr)
{
    clear_phys(paddr);
    PT0_PTR[idx + 256] = paddr | PTE_WRITABLE | PTE_PRESENT;
    return true;
}

void mmu_init()
{
    // Hook IPI for TLB shootdown
    TRACE_INIT("Hooking TLB shootdown\n");
    intr_hook(INTR_IPI_TLB_SHTDN, mmu_tlb_shootdown_handler,
              "sw_tlbshoot", eoi_lapic);

    if (unlikely(sizeof(*phys_mem_map) * kernel_params->phys_mem_table_size >
                 sizeof(phys_mem_map))) {
        printk("Warning: Memory map is too large to fit\n");
    }

    memcpy(phys_mem_map, kernel_params->phys_mem_table,
           std::min(sizeof(phys_mem_map), sizeof(*phys_mem_map) *
                           kernel_params->phys_mem_table_size));
    phys_mem_map_count = kernel_params->phys_mem_table_size;

    usable_early_mem_ranges = mmu_fixup_mem_map(phys_mem_map);

    if (usable_early_mem_ranges > countof(early_mem_ranges)) {
        printdbg("Physical memory is incredibly fragmented!\n");
        usable_early_mem_ranges = countof(early_mem_ranges);
    }

    for (physmem_range_t *ranges_in = phys_mem_map,
         *ranges_out = early_mem_ranges;
         ranges_in < phys_mem_map + phys_mem_map_count; ++ranges_in) {
        if (ranges_in->type == PHYSMEM_TYPE_NORMAL)
            *ranges_out++ = *ranges_in;

        // Cap
        if (ranges_out >= early_mem_ranges + usable_early_mem_ranges)
            break;
    }

    size_t usable_pages = 0;
    physaddr_t highest_usable = 0;
    for (physmem_range_t *mem = early_mem_ranges;
         mem < early_mem_ranges + usable_early_mem_ranges; ++mem) {
        printdbg("Memory: addr=%#" PRIx64 " size=%#" PRIx64 " type=%#x\n",
               mem->base, mem->size, mem->type);

        if (mem->base >= 0x100000) {
            physaddr_t end = mem->base + mem->size;

            if (mem->type == PHYSMEM_TYPE_NORMAL) {
                usable_pages += mem->size >> PAGE_SIZE_BIT;

                highest_usable = std::max(highest_usable, end);
            }
        }
    }

    // Exclude pages below 1MB line from physical page allocation map
    highest_usable -= 0x100000;

    // Compute number of slots needed
    highest_usable >>= PAGE_SIZE_BIT;
    printdbg("Usable pages = %" PRIu64 " (%" PRIu64 "MB)"
             " range_pages=%" PRId64 "\n",
             usable_pages, usable_pages >> (20 - PAGE_SIZE_BIT),
             highest_usable);

    //
    // Move all page tables into upper addresses and recursive map

    physaddr_t root_phys_addr;
    pte_t *pt;

    TRACE_INIT("Configuring page attribute table MSR\n");
    mmu_configure_pat();

    TRACE_INIT("Creating new PML4\n");

    // Get a page
    root_phys_addr = init_take_page();
    assert(root_phys_addr != 0);

    physaddr_t old_root = cpu_page_directory_get() & PTE_ADDR;

    pt = init_phys<pte_t>(root_phys_addr);
    cpu_page_invalidate(uintptr_t(pt));
    memcpy(pt, init_phys<pte_t>(old_root), PAGESIZE);

    pte_t ptflags = PTE_PRESENT | PTE_WRITABLE;

    //pt = init_phys<pte_t>(root_phys_addr);
    assert(pt[PT_RECURSE] == 0);
    pt[PT_RECURSE] = root_phys_addr | ptflags;

    root_physaddr = root_phys_addr;

    cpu_page_directory_set(root_phys_addr);

    // Make zero page not present to catch null pointers
    PT3_PTR[0] = 0;
    cpu_page_invalidate(0);
    cpu_tlb_flush();

    mm_phys_clear_init();

    // Reserve 4MB low memory for contiguous
    // physical allocator
    physaddr_t contiguous_start = -1;
    size_t contig_pool_size = 4 << 20;

    // Take space off the top of the highest address range which is large
    // enough to have the required amount of contiguous free space,
    // and use the taken space for the contiguous allocator pool
    for (int i = usable_early_mem_ranges; i > 0; --i) {
        auto& range = early_mem_ranges[i - 1];

        if (range.size >= contig_pool_size && range.base < 0x100000000) {
            contiguous_start = range.base + range.size - contig_pool_size;
            range.size -= contig_pool_size;

            if (range.size == 0) {
                memmove(early_mem_ranges + i - 1, early_mem_ranges + i,
                        sizeof(*early_mem_ranges) * --usable_early_mem_ranges - i);
            }

            break;
        }
    }

    linear_allocator.set_early_base(&linear_base);

    contig_phys_allocator.init(contiguous_start, contig_pool_size,
                               "contiguous_physical");

    printdbg("Highest usable offset from 1MB: %#zx\n",
             highest_usable << PAGE_SIZE_BIT);

    size_t physalloc_size = mmu_phys_allocator_t::size_from_highest_page(
                highest_usable);

    printdbg("Allocating %zuKB for physical memory allocator\n",
             physalloc_size >> 10);

    void *phys_alloc = mmap(nullptr, physalloc_size,
                            PROT_READ | PROT_WRITE,
                            MAP_POPULATE | MAP_UNINITIALIZED);

    printdbg("Building physical memory free list\n");

    phys_allocator.init(phys_alloc, 0x100000, highest_usable);

    // Put all of the remaining physical memory into the free lists
    uintptr_t free_count = 0;

    while (usable_early_mem_ranges) {
        physmem_range_t range = early_mem_ranges[usable_early_mem_ranges-1];

        if (range.base >= 0x100000 && range.size > 0) {
            printdbg("...Adding base=%#zx, size=%#zx\n",
                     range.base, range.size);
            phys_allocator.add_free_space(range.base, range.size);
            free_count += range.size >> PAGE_SCALE;
        }

        --usable_early_mem_ranges;
    }

    // Start using physical memory allocator
    usable_early_mem_ranges = 0;

    printdbg("%" PRIu64 " pages free (%" PRIu64 "MB)\n",
           free_count, free_count >> (20 - PAGE_SIZE_BIT));

    // This isn't actually used. #PF is fast-pathed in ISR handling
    intr_hook(INTR_EX_PAGE, mmu_page_fault_handler, "sw_page", eoi_none);

    malloc_startup(nullptr);

    assert(malloc_validate(false));

    uintptr_t kmembase = linear_allocator.early_init(
                min_kern_addr - linear_base, "linear_allocator");

    printdbg("Kernel vaddr allocator base: %#zx\n", kmembase);

    clear_phys_state.reserve_addr();

    // Allocate guard page
    linear_allocator.alloc_linear(PAGE_SIZE);

    // Preallocate the second level kernel PTPD pages so we don't
    // need to worry about process-specific page directory synchronization
//    bool success;
//    success = phys_allocator.alloc_multiple(PAGE_SIZE * 256, mmu_init_pagedir);
//    assert(success);

    for (size_t i = 256; i < 512; ++i) {
        if (PT0_PTR[i] == 0) {
            physaddr_t page = mmu_alloc_phys();
            clear_phys(page);
            // Global bit MBZ in PML4e on AMD
            PT0_PTR[i] = page | PTE_WRITABLE | PTE_PRESENT;
        }
    }

    // Alias master page directory to be accessible
    // from all process contexts
    master_pagedir = (pte_t*)mmap((void*)root_physaddr, PAGE_SIZE,
                                  PROT_READ, MAP_PHYSICAL);

    if (unlikely(master_pagedir == MAP_FAILED))
        panic("Insufficient memory to initialize memory allocator!");

    current_pagedir = PT0_PTR;

    // Find holes in address space suitable for mapping hardware
    bool first_hole = true;

    physaddr_t last_hole_end = 0x100000;
    for (size_t i = 0; i < phys_mem_map_count; ++i) {
        physmem_range_t const& range = phys_mem_map[i];

        // Ignore holes in 1st MB
        if (range.base < 0x100000)
            continue;

        if (range.base > last_hole_end) {
            if (!first_hole) {
                hole_allocator.release_linear(last_hole_end,
                                              range.base - last_hole_end);
            } else {
                first_hole = false;
                hole_allocator.init(last_hole_end, range.base - last_hole_end,
                                    "hole_allocator");
            }
        }

        last_hole_end = range.base + range.size;
    }

    physaddr_t max_physaddr = physaddr_t(1) << cpuid_paddr_bits();

    if (max_physaddr > last_hole_end) {
        if (!first_hole) {
            hole_allocator.release_linear(last_hole_end,
                                          max_physaddr - last_hole_end);
        } else {
            first_hole = false;
            hole_allocator.init(last_hole_end,
                                max_physaddr - last_hole_end,
                                "hole_allocator");
        }
    }

    // Unmap physical mapping of first 4GB
    munmap(init_phys<void>(0), kernel_params->phys_mapping_sz);
    cpu_tlb_flush();

    callout_call(callout_type_t::vmm_ready);

    assert(malloc_validate(false));

#if 0 // Hack
    printdbg("Allocating and filling all memory with garbage\n");

    size_t blocks_cap = 5<<(30-12);
    size_t blocks_sz = sizeof(void*) * blocks_cap;
    void **blocks = (void**)mmap(nullptr, blocks_sz, PROT_READ | PROT_WRITE,
                                 MAP_POPULATE | MAP_UNINITIALIZED);
    size_t blocks_cnt = 0;
    memset(blocks, 0xFA, blocks_sz);

    for (;;) {
        void *block = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                           MAP_POPULATE | MAP_UNINITIALIZED);
        if (block == MAP_FAILED)
            break;

        memset(block, 0xF0, 4096);


        blocks[blocks_cnt++] = block;

        if (blocks_cnt && (blocks_cnt & 0xFFFF) == 0)
            printk("...done %10zu KB\r", blocks_cnt << (12 - 10));
    }

    printk("...done %10zu KB\n", blocks_cnt << (12 - 10));

    printk("Freeing all memory\n");

    for (size_t i = 0; i < blocks_cnt; ++i) {
        if (i && (i & 0xFFFF) == 0)
            printk("...done %10zu KB\r", i << (12 - 10));

        munmap(blocks[i], 4096);
    }
    printk("...done %10zu KB\n", blocks_cnt << (12 - 10));

    munmap(blocks, blocks_sz);

    printdbg("Memory fill stress done\n");
#endif
}

// Returns the present mask for the new page
static _always_inline int ptes_step(pte_t **ptes)
{
    ++ptes[3];
    if (unlikely(!(uintptr_t(ptes[3]) & 0xFF8))) {
        // Carry
        ++ptes[2];
        if (unlikely(!(uintptr_t(ptes[2]) & 0xFF8))) {
            // Carry
            ++ptes[1];
            if (unlikely(!(uintptr_t(ptes[1]) & 0xFF8))) {
                // Carry
                ++ptes[0];
            }
        }
    }
    return ptes_present(ptes);
}

// Returns number of last level PTEs to skip forward
// to get to next entry at the lowest level that is not present
// Makes sense when we are at a 4KB or 2MB or 1GB or 512GB boundary
static _always_inline ptrdiff_t ptes_mask_skip(int present_mask)
{
    // See if we need to advance 512GB
    if (unlikely(!(present_mask & 1)))
        return ptrdiff_t(512) * 512 * 512;

    // See if we need to advance 1GB
    if (unlikely(!(present_mask & 2)))
        return ptrdiff_t(512) * 512;

    // See if we need to advance 2MB
    if (unlikely(!(present_mask & 4)))
        return 512;

    // No skip
    return 0;
}

// Returns true at 2MB boundaries,
// which indicates that the present mask needs to be checked
static _always_inline bool ptes_advance(pte_t **ptes, ptrdiff_t distance)
{
    size_t page_index = (ptes[3] - PT3_PTR) + distance;
    ptes[3] += distance;
    ptes[2] = PT2_PTR + (page_index >> (9 * 1));
    ptes[1] = PT1_PTR + (page_index >> (9 * 2));
    ptes[0] = PT0_PTR + (page_index >> (9 * 3));
    return (page_index & 0x1FF) == 0;
}

//
// Public API

bool mpresent(uintptr_t addr, size_t size)
{
    pte_t *ptes[4];

    unsigned misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;
    size = round_up(size);

    ptes_from_addr(ptes, addr);
    pte_t *end = ptes[3] + (size >> PAGE_SCALE);

    while (ptes[3] < end) {
        if (ptes_present(ptes) != 0x0F)
            return false;
        ptes_step(ptes);
    }

    return true;
}

bool mwritable(uintptr_t addr, size_t size)
{

    unsigned misalignment = addr & PAGE_MASK;
    addr -= misalignment;
    size += misalignment;
    size = round_up(size);

    pte_t *ptes[4];
    ptes_from_addr(ptes, addr);
    pte_t *end = ptes[3] + (size >> PAGE_SCALE);

    while (ptes[3] < end) {
        if (ptes_present(ptes) != 0x0F)
            return false;

        if (!(*ptes[3] & PTE_WRITABLE))
            return false;

        ptes_step(ptes);
    }

    return true;
}

static void mm_destroy_pagetables_aligned(uintptr_t start, size_t size)
{
    // TODO: implement
}

static pte_t *mm_create_pagetables_aligned(
        uintptr_t start, size_t size, size_t levels,
        mmu_phys_allocator_t::free_batch_t& free_batch)
{
    pte_t *pte_st[4];
    pte_t *pte_en[4];

    uintptr_t const end = start + size - 1;

    ptes_from_addr(pte_st, start);
    ptes_from_addr(pte_en, end);

    // Fastpath small allocations fully within a 2MB region,
    // where there is nothing to do
    if (likely(levels == 3 &&
               pte_st[2] == pte_en[2] &&
               (*pte_st[0] & PTE_PRESENT) &&
               (*pte_st[1] & PTE_PRESENT) &&
               (*pte_st[2] & PTE_PRESENT)))
        return pte_st[3];

    if (levels == 2 &&
            pte_st[1] == pte_en[1] &&
            (*pte_st[0] & PTE_PRESENT) &&
            (*pte_st[1] & PTE_PRESENT))
        return pte_st[2];

    if (levels == 1 &&
            pte_st[0] == pte_en[0] &&
            (*pte_st[0] & PTE_PRESENT))
        return pte_st[1];

    bool const low = (start & 0xFFFFFFFFFFFFU) < 0x800000000000U;

    // Mark high pages PTE_GLOBAL, low pages PTE_USER
    // First 3 levels PTE_ACCESSED and PTE_DIRTY
    pte_t const page_flags = (low ? 0 : PTE_GLOBAL) | PTE_USER |
            PTE_ACCESSED | PTE_DIRTY | PTE_PRESENT | PTE_WRITABLE;

    pte_t global_mask = ~PTE_GLOBAL;

    for (unsigned level = 0; level < levels; ++level) {
        pte_t * const base = pte_st[level];
        // Plus one because it is an inclusive end, not half open range
        size_t const range_count = (pte_en[level] - base) + 1;
        for (size_t i = 0; i < range_count; ++i) {
            pte_t old = base[i];
            if (!(old & PTE_PRESENT)) {
                physaddr_t const page = mmu_alloc_phys();

                assert(page);
                if (unlikely(!page))
                    return nullptr;

                clear_phys(page);

                pte_t new_pte = (page | page_flags) & global_mask;

                pte_t previous = atomic_cmpxchg(base + i, old, new_pte);
                if (previous != old) {
                    assert(pte_is_sysmem(previous));
                    free_batch.free(page);
                }
            }
        }

#if GLOBAL_RECURSIVE_MAPPING
        global_mask = -1;
#endif
    }

    return pte_st[levels];
}

template<typename T>
static _always_inline constexpr T zero_if_false(bool cond, T bits)
{
    return bits & -cond;
}

template<typename T>
static _always_inline T select_mask(bool cond, T true_val, T false_val)
{
    T mask = -cond;
    return (true_val & mask) | (false_val & ~mask);
}

EXPORT void *mm_alloc_space(size_t size)
{
    return (void*)linear_allocator.alloc_linear(round_up(size));
}

EXPORT void *mmap(void *addr, size_t len, int prot,
                  int flags, int fd, off_t offset)
{
    (void)offset;
    assert(offset == 0);

    // Fail on invalid protection mask
    if (unlikely(prot != (prot & (PROT_READ | PROT_WRITE | PROT_EXEC))))
        return MAP_FAILED;

    // Any invalid flag set returns failure
    if (unlikely(flags & MAP_INVALID_MASK))
        return MAP_FAILED;

    static constexpr int user_prohibited =
            MAP_NEAR | MAP_DEVICE | MAP_GLOBAL |
            MAP_UNINITIALIZED;

    if (unlikely((flags & MAP_USER) && (flags & user_prohibited)))
        return MAP_FAILED;

    if (unlikely(len == 0))
        return nullptr;

    PROFILE_MMAP_ONLY( uint64_t profile_st = cpu_rdtsc() );

#if DEBUG_PAGE_TABLES
    printdbg("Mapping len=%#zx prot=%#x flags=%#x addr=%#" PRIx64 "\n",
             len, prot, flags, (uintptr_t)addr);
#endif

    pte_t page_flags = 0;

    // Populate 32 bit memory, always
    flags |= zero_if_false(flags & MAP_32BIT, MAP_POPULATE);

    // Populate kernel stacks
    flags |= zero_if_false((flags & (MAP_STACK | MAP_USER)) == MAP_STACK,
                           MAP_POPULATE);

    // Set physical and present flag on physical memory mapping
    page_flags |= zero_if_false(flags & MAP_PHYSICAL,
                                PTE_EX_PHYSICAL | PTE_PRESENT);

    page_flags |= zero_if_false(fd >= 0 && !(flags & MAP_DEVICE),
                                PTE_EX_FILEMAP | PTE_EX_DEVICE);

    // Force nocommit if filemapping
    flags |= zero_if_false(page_flags & PTE_EX_FILEMAP, MAP_NOCOMMIT);

    if (likely(!(flags & MAP_WEAKORDER))) {
        page_flags |= zero_if_false(flags & MAP_NOCACHE, PTE_PCD);
        page_flags |= zero_if_false(flags & MAP_WRITETHRU, PTE_PWT);
    } else {
        // Weakly ordered memory, set write combining in PTE if capable
        page_flags |= select_mask(mmu_have_pat(),
                                  PTE_PTEPAT_n(PAT_IDX_WC),
                                  PTE_PCD | PTE_PWT);
    }

    page_flags |= zero_if_false(flags & MAP_POPULATE, PTE_PRESENT);
    page_flags |= zero_if_false(flags & MAP_DEVICE, PTE_EX_DEVICE);
    page_flags |= zero_if_false(flags & MAP_USER, PTE_USER);
    page_flags |= zero_if_false(!(flags & MAP_USER), PTE_GLOBAL);
    page_flags |= zero_if_false(prot & PROT_WRITE, PTE_WRITABLE);
    page_flags |= zero_if_false(!(prot & PROT_EXEC), PTE_NX & cpuid_nx_mask);

    uintptr_t misalignment = uintptr_t(addr) & PAGE_MASK;
    addr = (void*)(uintptr_t(addr) - misalignment);
    len += misalignment;
    len = round_up(len);

    contiguous_allocator_t *allocator =
            (flags & MAP_USER) ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator()
            //: (flags & MAP_NEAR) ? &near_allocator
            : &linear_allocator;

    PROFILE_LINEAR_ALLOC_ONLY( uint64_t profile_linear_st = cpu_rdtsc() );

    linaddr_t linear_addr;

    uintptr_t const oneGB = 1 << (12+9+9);
    uintptr_t const twoMB = 1 << (12+9);
    uintptr_t const fourK = 1 << (12);

    if (likely(!addr || (flags & MAP_PHYSICAL))) {
        if (likely(!(flags & MAP_HUGETLB))) {
            linear_addr = allocator->alloc_linear(len);
        } else {
            // Make suitably aligned linear allocation for hugetlb

            uintptr_t paddr = uintptr_t(addr);

            uintptr_t alignment;

            if (paddr == (paddr & -oneGB)) {
                alignment = oneGB;
                misalignment = paddr & ~-oneGB;
            } else if (paddr == (paddr & -twoMB)) {
                alignment = twoMB;
                misalignment = paddr & ~-twoMB;
            } else {
                alignment = fourK;
                misalignment = paddr & ~-fourK;
            }

            // Allocate 1GB extra in case we need to round up the start
            // to a 1GB boundary
            linear_addr = allocator->alloc_linear(len + alignment);

            if (alignment) {
                uintptr_t aligned_start;
                aligned_start = (linear_addr + (alignment - 1)) & -alignment;

                uintptr_t used_end = aligned_start + len;

                uintptr_t unused_at_start = aligned_start - linear_addr;
                uintptr_t unused_at_end = len + alignment - used_end;

                if (unused_at_start)
                    allocator->release_linear(linear_addr, unused_at_start);

                if (unused_at_end)
                    allocator->release_linear(used_end, unused_at_end);

                linear_addr = aligned_start;
            }
        }
    } else {
        linear_addr = (linaddr_t)addr;

        if (unlikely((flags & MAP_USER) &&
                     (linear_addr >= 0x7FFFFFFFF000 ||
                      linear_addr + len > 0x7FFFFFFFF000)))
            return MAP_FAILED;

        if (unlikely(!allocator->take_linear(
                         linear_addr, len, flags & MAP_EXCLUSIVE)))
            return MAP_FAILED;
    }

    PROFILE_LINEAR_ALLOC_ONLY(
                printdbg("Allocation of %" PRIu64 " bytes of"
                         " address space took %" PRIu64 " cycles\n",
                         len, cpu_rdtsc() - profile_linear_st) );

    assert(linear_addr > 0x100000);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    if (likely(!usable_early_mem_ranges)) {
        // Normal operation

        pte_t *base_pte = nullptr;

        if (likely(!(flags & MAP_PHYSICAL))) {
            base_pte = mm_create_pagetables_aligned(
                        linear_addr, len, 3, free_batch);

            if (unlikely(!base_pte)) {
                mm_destroy_pagetables_aligned(linear_addr, len);
                munmap((void*)linear_addr, len);
                return MAP_FAILED;
            }
        }

        if ((flags & (MAP_POPULATE | MAP_PHYSICAL)) == MAP_POPULATE) {
            // POPULATE, not PHYSICAL

            bool success;
            success = phys_allocator.alloc_multiple(
                        len, [&](size_t idx, uint8_t log2_pagesz,
                            physaddr_t paddr) {
                if (likely(!(flags & MAP_UNINITIALIZED)))
                    clear_phys(paddr);

                pte_t old = atomic_xchg(base_pte + idx, paddr | page_flags);

                if (old && (old & (PTE_PRESENT | PTE_EX_PHYSICAL)) ==
                    PTE_PRESENT)
                    free_batch.free(old & PTE_ADDR);

                return true;
            });

            if (unlikely(!success))
                return MAP_FAILED;
        } else if (!(flags & MAP_PHYSICAL)) {
            // Demand paged

            size_t ofs = 0;
            pte_t pte;

            physaddr_t paddr = 0;

            size_t end = len >> PAGE_SCALE;

            if ((prot & (PROT_READ | PROT_WRITE)) &&
                    !(flags & (MAP_NOCOMMIT | MAP_DEVICE))) {
                paddr = mmu_alloc_phys();

                if (paddr && !(flags & MAP_UNINITIALIZED))
                    clear_phys(paddr);
            }

            pte = 0;

            if (paddr)
                pte = paddr | page_flags;

            if ((prot & (PROT_READ | PROT_WRITE)) &&
                    !(flags & MAP_NOCOMMIT)) {
                if (paddr && (paddr & PTE_ADDR) != PTE_ADDR)
                    pte |= PTE_PRESENT;

                if (paddr && !(flags & (MAP_STACK | MAP_NOCOMMIT))) {
                    // Commit first page
                    pte = atomic_xchg(base_pte, pte);

                    ++ofs;
                } else if (paddr && !(flags & MAP_NOCOMMIT)) {
                    // Commit last page

                    pte = atomic_xchg(base_pte + --end, pte);
                }

                if (unlikely(pte_is_sysmem(pte)))
                    free_batch.free(pte & PTE_ADDR);
            }

            pte_t demand_fill = page_flags |
                    ((prot & (PROT_READ | PROT_WRITE))
                    ? PTE_ADDR
                    : ((PTE_ADDR >> 1) & PTE_ADDR));
            // Demand page fill
            for ( ; ofs < end; ++ofs) {
                pte = demand_fill;

                pte = atomic_xchg(base_pte + ofs, pte);

                if (unlikely(pte_is_sysmem(pte)))
                    free_batch.free(pte & PTE_ADDR);
            }
        } else if (flags & MAP_PHYSICAL) {

            uintptr_t paddr = uintptr_t(addr);
            uintptr_t pend = paddr + len;
            uintptr_t vaddr = linear_addr;

            bool huge = (flags & MAP_HUGETLB);

            do {
                size_t level;
                size_t huge_shift;
                pte_t huge_flags;
                size_t huge_count;

                uintptr_t next_2MB_boundary;
                next_2MB_boundary = (paddr + twoMB) & -twoMB;

                // 1GB aligned, and length is even multiple of 1GB
                if (huge && (paddr == (paddr & -oneGB) &&
                            len >= oneGB)) {
                    // run of 1GB pages
                    level = 1;

                    huge_shift = 12 + 9 + 9;

                    huge_count = len >> huge_shift;

                    huge_flags = PTE_PAGESIZE |
                            ((page_flags & PTE_PTEPAT)
                             ? (page_flags & ~PTE_PTEPAT) | PTE_PDEPAT
                             : page_flags);
                } else if (huge && (paddr == (paddr & -twoMB) &&
                                    len >= twoMB)) {
                    // run of 2MB pages

                    level = 2;

                    huge_shift = 12 + 9;

                    huge_count = len >> huge_shift;

                    huge_flags = PTE_PAGESIZE |
                            ((page_flags & PTE_PTEPAT)
                             ? (page_flags & ~PTE_PTEPAT) | PTE_PDEPAT
                             : page_flags);
                } else if (huge && next_2MB_boundary < pend) {
                    // 4KB pages until upcoming 2MB boundary
                    level = 3;

                    huge_shift = 12;

                    huge_count = (next_2MB_boundary - paddr) >> huge_shift;

                    huge_flags = page_flags;
                } else {
                    level = 3;

                    huge_shift = 12;

                    huge_flags = page_flags;

                    huge_count = len >> huge_shift;
                }

                huge_count = len >> huge_shift;
                size_t huge_size = 1 << huge_shift;
                size_t huge_total = huge_count << huge_shift;

                pte_t *p = mm_create_pagetables_aligned(
                            vaddr, huge_total, level, free_batch);

                for (size_t i = 0; i < huge_count; ++i) {
                    pte_t new_pte = paddr | huge_flags;
                    pte_t old_pte = atomic_xchg(p + i, new_pte);

                    if (unlikely(pte_is_sysmem(old_pte)))
                        free_batch.free(old_pte & PTE_ADDR);

                    vaddr += huge_size;
                    paddr += huge_size;
                    len -= huge_size;
                }
            } while (len);


//            for (size_t ofs = 0, end = (len >> PAGE_SCALE); ofs < end;
//                 ++ofs, paddr += PAGE_SIZE) {
//                pte = paddr | page_flags;
//                pte = atomic_xchg(base_pte + ofs, pte);

//                if (unlikely(pte_is_sysmem(pte)))
//                    free_batch.free(pte & PTE_ADDR);
//            }
        } else {
            assert(!"Unhandled condition");
        }
    } else {
        // Early
        for (size_t ofs = len; ofs > 0; ofs -= PAGE_SIZE)
        {
            if (likely(!(flags & MAP_PHYSICAL))) {
                // Allocate normal memory

                // Not present pages with max physaddr are demand committed
                physaddr_t page = PTE_ADDR;

                // If populating, assign physical memory immediately
                // Always commit first page immediately
                if (ofs == PAGE_SIZE || unlikely(flags & MAP_POPULATE)) {
                    page = init_take_page();
                    assert(page != 0);
                    if (likely(!(flags & MAP_UNINITIALIZED)))
                        clear_phys(page);
                }

                mmu_map_page(linear_addr + ofs - PAGE_SIZE, page, page_flags |
                             ((ofs == 0) & PTE_PRESENT));
            } else {
                // addr is a physical address, caller uses
                // returned linear address to access it
                mmu_map_page(linear_addr + ofs - PAGE_SIZE,
                             (((physaddr_t)addr) + ofs) & PTE_ADDR,
                             page_flags);
            }
        }
    }

    assert(linear_addr > 0x100000);

    PROFILE_MMAP_ONLY( printdbg("mmap of %zd bytes took %" PRIu64 " cycles\n",
                                len, cpu_rdtsc() - profile_st); )

    return (void*)(linear_addr + misalignment);
}

EXPORT void *mremap(
        void *old_address,
        size_t old_size,
        size_t new_size,
        int flags,
        void *new_address,
        errno_t *ret_errno)
{
    old_size = round_up(old_size);
    new_size = round_up(new_size);

    // Convert pointer to address
    linaddr_t old_st = linaddr_t(old_address);
    linaddr_t new_st = linaddr_t(new_address);

    // Fail with EINVAL if:
    //  - old_address and new_address must be page aligned
    //  - Flags must be valid
    //  - New size must be nonzero
    //  - Old address must be sane
    //  - new_address is zero and MREMAP_FIXED was set in flags
    //  - new_address is nonzero and MREMAP_FIXED was not set in flags
    if (unlikely(old_st & PAGE_MASK) ||
            unlikely(old_st < 0x400000) ||
            unlikely(new_st & PAGE_MASK) ||
            unlikely(new_st && new_st < 0x400000) ||
            unlikely(old_size == 0) ||
            unlikely(new_size == 0) ||
            unlikely(flags & MREMAP_INVALID_MASK) ||
            unlikely((!(flags & MREMAP_FIXED)) != (new_st == 0))) {
        if (ret_errno)
            *ret_errno = errno_t::EINVAL;
        return MAP_FAILED;
    }

    // Only support simplified API:
    //  - Must pass MREMAP_MAYMOVE
    //  - Must not pass MREMAP_FIXED
    if (flags != MREMAP_MAYMOVE) {
        if (ret_errno)
            *ret_errno = errno_t::ENOSYS;
        return MAP_FAILED;
    }

    if (new_size < old_size) {
        //
        // Got smaller

        size_t freed_size = old_size - new_size;

        // Release space at the end
        munmap((void*)(old_st + new_size), freed_size);

        return (void*)old_st;
    } else if (unlikely(new_size == old_size)) {
        return (void*)new_st;
    }

    bool const low = (old_st < 0x800000000000U);
    contiguous_allocator_t *allocator = low ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator() :
                &linear_allocator;

    allocator->dump("mremap allocator dump\n");

    pte_t *new_base;

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    if (allocator->take_linear(old_st + old_size, new_size - old_size, true)) {
        // Expand in place
        new_st = old_st + old_size;
        new_size -= old_size;
        new_base = mm_create_pagetables_aligned(
                    new_st, new_size, 3, free_batch);
        pte_t new_pte = (low ? PTE_USER : PTE_GLOBAL) |
                PTE_WRITABLE | PTE_ADDR;
        for (size_t i = 0, e = new_size >> PAGE_SCALE; i < e; ++i) {
            pte_t pte = atomic_xchg(new_base + i, new_pte);
            if (unlikely(pte_is_sysmem(pte)))
                free_batch.free(pte & PTE_ADDR);
        }
        return (void*)old_st;
    }

    pte_t *old_pte[4];
    ptes_from_addr(old_pte, old_st);

    new_st = allocator->alloc_linear(new_size);
    new_base = mm_create_pagetables_aligned(new_st, new_size, 3, free_batch);

    size_t i, e;
    for (i = 0, e = old_size >> PAGE_SCALE; i < e; ++i) {
        pte_t pte = atomic_xchg(old_pte[3] + i, 0);
        pte = atomic_xchg(new_base + i, pte);
        if (unlikely(pte_is_sysmem(pte)))
            free_batch.free(pte & PTE_ADDR);
    }

    for (e = new_size >> PAGE_SCALE; i < e; ++i) {
        pte_t pte = atomic_xchg(new_base + i, PTE_ADDR | PTE_WRITABLE);
        if (unlikely(pte_is_sysmem(pte)))
            free_batch.free(pte & PTE_ADDR);
    }

    return (void*)new_st;
}

EXPORT int munmap(void *addr, size_t size)
{
    __asan_freeN_noabort(addr, size);

    linaddr_t a = (linaddr_t)addr;

    uintptr_t misalignment = a & PAGE_MASK;
    a -= misalignment;
    size += misalignment;
    size = round_up(size);

    pte_t *ptes[4];
    ptes_from_addr(ptes, a);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    size_t freed = 0;
    int present_mask = ptes_present(ptes);
    for (size_t ofs = 0; ofs < size; ) {
        size_t distance = 0;
        pte_t pte = 0;

        if ((present_mask & 0x07) == 0x07) {
            if ((*ptes[2] & PTE_PAGESIZE) == 0) {
                // PT page level is present, 4KB mapping
                pte = atomic_xchg(ptes[3], 0);

                if (pte_is_sysmem(pte)) {
                    physaddr_t physaddr = pte & PTE_ADDR;

                    free_batch.free(physaddr);
                    ++freed;
                }

                distance = PAGE_SIZE;
            } else {
                // 2MB mapping
                pte = atomic_xchg(ptes[2], 0);

                if ((pte & (PTE_EX_PHYSICAL | PTE_PRESENT)) == PTE_PRESENT) {
                    physaddr_t physaddr = pte & (PTE_ADDR & -(1 << 21));

                    for (physaddr_t i = 0; i < (1 << 21); i += PAGE_SIZE)
                        free_batch.free(physaddr + i);
                }

                if (pte & PTE_PRESENT)
                    freed += 512;

                distance = (1 << 21);
            }
        } else if ((present_mask & 0x03) == 0x03) {
            if ((*ptes[1] & PTE_PAGESIZE) == 0) {

            } else {
                // 1GB mapping
                pte = atomic_xchg(ptes[1], 0);

                if ((pte & (PTE_EX_PHYSICAL | PTE_PRESENT)) == PTE_PRESENT) {
                    physaddr_t physaddr = pte & (PTE_ADDR & -(1 << 30));

                    for (physaddr_t i = 0; i < (1 << 30); i += PAGE_SIZE)
                        free_batch.free(physaddr + i);
                }
            }
            distance = (1 << 30);
        } else {
            distance = PAGE_SIZE;
        }

        if (pte & PTE_PRESENT)
            cpu_page_invalidate(a);

        assert(distance != 0);

        ofs += distance;
        a += distance;

        if (ptes_advance(ptes, distance >> PAGE_SCALE))
            present_mask = ptes_present(ptes);
    }

    free_batch.flush();

    if (freed)
        mmu_send_tlb_shootdown();

    contiguous_allocator_t *allocator =
            (a < 0x800000000000U) ?
                (contiguous_allocator_t*)
                thread_current_process()->get_allocator()
            : &linear_allocator;

    allocator->release_linear((linaddr_t)addr - misalignment, size);

    return 0;
}

EXPORT int mprotect(void *addr, size_t len, int prot)
{
    // Fail on invalid protection mask
    if (unlikely(prot != (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)))) {
        assert(!"Invalid parameter!");
        return -1;
    }

    // Fail on invalid address
    if (unlikely(!addr))
        return -1;

    unsigned misalignment = (uintptr_t)addr & PAGE_MASK;
    addr = (char*)addr - misalignment;
    len += misalignment;

    len = round_up(len);

    if (unlikely(len == 0))
        return 0;

    /// Demand paged PTE, readable
    ///  present=0, addr=PTE_ADDR
    /// Demand paged PTE, not readable
    ///  present=0, addr=(PTE_ADDR>>1)&PTE_ADDR

    // Unreadable demand paged has MSB of physical address cleared
    pte_t const demand_no_read = (PTE_ADDR >> 1) & PTE_ADDR;

    pte_t no_exec = cpuid_nx_mask & PTE_NX;

    // Bits to set in PTE
    pte_t set_bits =
            (-(!(prot & PROT_EXEC)) & no_exec) |
            (-(!!(prot & PROT_READ)) & PTE_PRESENT) |
            (-(!!(prot & PROT_WRITE)) & PTE_WRITABLE);

    // Bits to clear in PTE
    // If the protection is no-read,
    // we clear the highest bit in the physical address.
    // This invalidates the demand paging value without
    // invalidating the
    pte_t clr_bits = set_bits ^ (no_exec | PTE_PRESENT | PTE_WRITABLE);

    pte_t *pt[4];
    ptes_from_addr(pt, linaddr_t(addr));
    pte_t *end = pt[3] + (len >> PAGE_SCALE);

    while (pt[3] < end)
    {
        assert((*pt[0] & PTE_PRESENT) &&
                (*pt[1] & PTE_PRESENT) &&
                (*pt[2] & PTE_PRESENT));

        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            bool demand_paged = ((expect & demand_no_read) == demand_no_read);

            if (expect == 0)
                return -1;
            else if (demand_paged && (prot & PROT_READ))
                // We are enabling read on demand paged entry
                replace = (expect & ~clr_bits) |
                        (set_bits & ~PTE_PRESENT) | PTE_ADDR;
            else if (demand_paged && !(prot & (PROT_READ | PROT_WRITE)))
                // We are disabling read on a demand paged entry
                replace = (expect & demand_no_read & ~clr_bits) |
                        (set_bits & ~PTE_PRESENT);
            else
                // Just change permission bits
                replace = (expect & ~clr_bits) | set_bits;

            // Try to update PTE
            if (likely(atomic_cmpxchg_upd(pt[3], &expect, replace)))
                break;
        }

        cpu_page_invalidate((uintptr_t)addr);
        addr = (char*)addr + PAGE_SIZE;

        ptes_step(pt);
    }

    mmu_send_tlb_shootdown();

    return 0;
}

// Tell whether an array of 4 PTEs are present, accounting for large page flags
bool pte_list_present(pte_t **ptes)
{
    if (unlikely(!(*ptes[0] & PTE_PRESENT)))
        return false;

    if (unlikely(!(*ptes[1] & PTE_PRESENT)))
        return false;

    if (unlikely((*ptes[1] & PTE_PAGESIZE)))
        return true;

    if (unlikely(!(*ptes[2] & PTE_PRESENT)))
        return false;

    if (unlikely(*ptes[2] & PTE_PAGESIZE))
        return true;

    if (unlikely(!(*ptes[2] & PTE_PRESENT)))
        return false;

    return true;
}

// Returns the return value of the callback
// Ends the loop when the callback returns a negative number
// Calls the callback with each present sub-range
// Callback signature: int(linaddr_t base, size_t len)
template<typename F>
static int present_ranges(F callback, linaddr_t rounded_addr, size_t len,
                          bool is_present = true)
{
    assert(rounded_addr != 0);
    assert(((rounded_addr) & PAGE_MASK) == 0);
    assert(((len) & PAGE_MASK) == 0);

    pte_t *ptes[4];

    linaddr_t end = rounded_addr + len;

    ptes_from_addr(ptes, rounded_addr);
    int present_mask = ptes_present(ptes);

    int result = 0;
    linaddr_t range_start = 0;

    for (linaddr_t addr = rounded_addr;
         result >= 0 && addr < end; addr += PAGE_SIZE) {
        if ((present_mask == 0xF) == is_present) {
            if (!range_start)
                range_start = addr;
        } else {
            if (range_start)
                result = callback(range_start, addr - range_start);

            range_start = 0;
        }

        ptes_step(ptes);
        present_mask = ptes_present(ptes);
    }

    if (range_start && result >= 0)
        result = callback(range_start, end - range_start);

    return result;
}

// Support discarding pages and reverting to demand
// paged state with MADV_DONTNEED.
// Support enabling/disabling write combining
// with MADV_WEAKORDER/MADV_STRONGORDER
EXPORT int madvise(void *addr, size_t len, int advice)
{
    if (unlikely(len == 0))
        return 0;

    pte_t order_bits = 0;

    bool uninitialized = advice & MADV_UNINITIALIZED;
    advice &= ~MADV_UNINITIALIZED;

    switch (advice) {
    case MADV_WEAKORDER:
        order_bits = PTE_PTEPAT_n(PAT_IDX_WC);
        break;

    case MADV_STRONGORDER:
        order_bits = PTE_PTEPAT_n(PAT_IDX_WB);
        break;

    case MADV_DONTNEED:
        order_bits = -1;
        break;

    case MADV_WILLNEED:
        order_bits = 0;
        break;

    default:
        return 0;
    }

    size_t misalignment = uintptr_t(addr) & PAGE_MASK;
    addr = (char*)addr - misalignment;
    len += misalignment;
    len = round_up(len);

    pte_t *pt[4];
    ptes_from_addr(pt, linaddr_t(addr));
    pte_t *end = pt[3] + (len >> PAGE_SCALE);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    while (pt[3] < end && pte_list_present(pt)) {
        pte_t replace;
        for (pte_t expect = *pt[3]; ; pause()) {
            if (order_bits == pte_t(-1)) {
                // Discarding
                physaddr_t page = 0;
                if (pte_is_sysmem(expect)) {
                    page = expect & PTE_ADDR;
                    // Replace with demand paged entry
                    replace = (expect | PTE_ADDR) & ~PTE_PRESENT;

                    if (unlikely(!atomic_cmpxchg_upd(pt[3], &expect, replace)))
                        continue;

                    free_batch.free(page);
                }
                break;
            } else if (order_bits == pte_t(0)) {
                // Committing
                bool success;

                // Scan for at least one not-present pte
                size_t i, e;
                for (i = 0, e = len >> PAGE_SCALE; i < e; ++i) {
                    if (!(pt[3][i] & PTE_PRESENT))
                        break;
                }
                if (unlikely(i == e))
                    return 0;

                intptr_t device_index = mmu_device_from_addr(linaddr_t(addr));
                if (unlikely(device_index >= 0)) {
                    // Page in device pages explicitly
                    mmap_device_mapping_t *mapping =
                            mm_dev_mappings[device_index];

                    int io_result = present_ranges([&](linaddr_t base,
                                                   size_t range_len) -> int  {
                        uintptr_t mapping_offset = base -
                            linaddr_t(mapping->range.get());

                        int io_result = mapping->callback(
                                    mapping->context, (void*)base,
                                    mapping_offset, range_len,
                                    true, false);

                        if (likely(io_result >= 0)) {
                            pte_t *ptes[4];
                            ptes_from_addr(ptes, base);
                            for (size_t i = range_len >> PAGE_SIZE_BIT;
                                    i > 0; --i) {
                                atomic_or(ptes[3] + (i - 1), PTE_PRESENT);
                            }
                        }

                        return io_result;
                    }, linaddr_t(addr), len, false);

                    if (unlikely(io_result < 0))
                        return io_result;

                    return 0;
                }

                success = phys_allocator.alloc_multiple(
                            len, [&](size_t idx, uint8_t, physaddr_t paddr) {
                    pte_t pte = pt[3][idx];
                    while (pte_is_demand(pte) || pte_is_guard(pte)) {
                        assert(!(pte & PTE_PRESENT));

                        if (!uninitialized)
                            clear_phys(paddr);

                        pte_t replacement = (pte & ~PTE_ADDR) |
                            paddr | PTE_PRESENT;

                        // If updating PTE succeeded, we want the page
                        if (likely(atomic_cmpxchg_upd(&pt[3][idx],
                                   &pte, replacement)))
                            return true;

                        // Unlikely racing change, retry...
                        continue;
                    }

                    // Do not want the page
                    return false;
                });
                pt[3] += len >> PAGE_SCALE;
                if (!success)
                    return -1;
                break;
            } else {
                // Weak/Strong order
                replace = (expect & ~(PTE_PTEPAT | PTE_PCD | PTE_PWT)) |
                        order_bits;

                if (likely(atomic_cmpxchg_upd(pt[3], &expect, replace)))
                    break;
            }
        }

        cpu_page_invalidate((uintptr_t)addr);
        addr = (char*)addr + PAGE_SIZE;

        ptes_step(pt);
    }

    mmu_send_tlb_shootdown();

    return 0;
}

EXPORT int msync(void const *addr, size_t len, int flags)
{
    // Check for validity, particularly accidentally using O_SYNC
    assert((flags & (MS_SYNC | MS_INVALIDATE)) == flags);

    size_t misalignment = linaddr_t(addr) & (PAGE_SIZE - 1);
    linaddr_t rounded_addr = linaddr_t(addr) & -PAGE_SIZE;
    len += misalignment;
    len = round_up(len);

    if (unlikely(len == 0))
        return 0;

    intptr_t device = mmu_device_from_addr(rounded_addr);

    if (unlikely(device < 0))
        return -int(errno_t::EFAULT);

    mmap_device_mapping_t *mapping = mm_dev_mappings[device];

    std::unique_lock<std::mutex> lock(mapping->lock);

    while (mapping->active_read >= 0)
        mapping->done_cond.wait(lock);

    bool need_flush = (flags & MS_SYNC) != 0;

    int result = present_ranges([&](linaddr_t base, size_t range_len) -> int {
        uintptr_t offset = base - uintptr_t(mapping->range.get());

        return mapping->callback(mapping->context, (void*)base,
                                 offset, range_len, false, need_flush);
    }, rounded_addr, len);

    return result;
}

EXPORT uintptr_t mphysaddr(void volatile const *addr)
{
    linaddr_t linaddr = linaddr_t(addr);

    unsigned misalignment = linaddr & PAGE_MASK;
    linaddr -= misalignment;

    pte_t *ptes[4];
    ptes_from_addr(ptes, linaddr);
    int present_mask = ptes_present(ptes);

    if ((present_mask & 0x07) != 0x07)
        return 0;

    pte_t pte = *ptes[3];
    physaddr_t page = pte & PTE_ADDR;

    // If page is being demand paged
    if (pte_is_demand(pte)) {
        // Commit a page
        page = mmu_alloc_phys();

        clear_phys(page);

        pte_t new_pte = (pte & ~PTE_ADDR) | page;

        if (atomic_cmpxchg_upd(ptes[3], &pte, new_pte))
            pte = new_pte;
        else
            mmu_free_phys(page);
    } else if (!(pte & PTE_PRESENT)) {
        // Assert that it is not a PROT_NONE page
        assert(pte != ((PTE_ADDR >> 1) & PTE_ADDR));
        return 0;
    }

    return (pte & PTE_ADDR) + misalignment;
}

static _always_inline int mphysranges_enum(
        void const *addr, size_t size,
        int (*callback)(mmphysrange_t, void*),
        void *context)
{
    linaddr_t linaddr = (linaddr_t)addr;
    size_t remaining_size = size;
    physaddr_t page_end;
    mmphysrange_t range;
    int result;

    do {
        // Get physical address for this linear address
        range.physaddr = mphysaddr((void*)linaddr);

        // Calculate physical address of the end of the page
        page_end = round_down(range.physaddr + PAGE_SIZE);

        // Calculate size of data until end of page
        range.size = page_end - range.physaddr;

        // Cap at requested maximum
        if (unlikely(range.size > remaining_size))
            range.size = remaining_size;

        // Advance the current linear address
        // Pass this range to the callback
        // Remember the callback return value
        // Loop again if the callback did not return 0 and
        // the remaining size is not equal to zero
    } while (linaddr += range.size,
             ((result = callback(range, context)) &&
             (remaining_size -= range.size)));

    return result;
}

struct mphysranges_state_t {
    mmphysrange_t *range;
    size_t ranges_count;
    size_t count;
    size_t max_size;
    physaddr_t last_end;
    mmphysrange_t cur_range;
};

static _always_inline int mphysranges_callback(
        mmphysrange_t range, void *context)
{
    mphysranges_state_t *state = (mphysranges_state_t *)context;

    int contiguous = state->count > 0 &&
            (state->range->physaddr + state->range->size == range.physaddr);

    if (state->count == 0) {
        // Initial range
        *state->range = range;
        ++state->count;
    } else if (state->range->size < state->max_size && contiguous) {
        // Extend the range
        state->range->size += range.size;
    } else if (state->count < state->ranges_count && range.size > 0) {
        *++state->range = range;
        ++state->count;
    } else {
        return 0;
    }

    return 1;
}

EXPORT size_t mphysranges(
        mmphysrange_t *ranges, size_t ranges_count,
        void const *addr, size_t size, size_t max_size)
{
    if (unlikely(size == 0))
        return 0;

    mphysranges_state_t state;

    // Request data
    state.ranges_count = ranges_count;
    state.max_size = max_size;

    // Result data
    state.range = ranges;
    state.count = 0;
    state.last_end = 0;
    state.cur_range.physaddr = 0;
    state.cur_range.size = 0;

    mphysranges_enum(addr, size, mphysranges_callback, &state);

    if (state.count < ranges_count) {
        // Flush last region
        state.cur_range.physaddr = ~0UL;
        state.cur_range.size = 0;
        mphysranges_callback(state.cur_range, &state);
    }

    return state.count;
}

EXPORT bool mphysranges_split(mmphysrange_t *ranges, size_t &ranges_count,
                              size_t count_limit, uint8_t log2_boundary)
{
    if (unlikely(ranges_count == 0))
        return true;

    size_t boundary = size_t(1) << log2_boundary;

    for (size_t i = ranges_count; i > 0 && ranges_count < count_limit; --i) {
        mmphysrange_t *range = ranges + i - 1;

        uintptr_t end = range->physaddr + range->size;

        uintptr_t chk1 = range->physaddr >> log2_boundary;
        uintptr_t chk2 = (end - 1) >> log2_boundary;

        if (chk1 != chk2) {
            // Needs split

            if (unlikely(ranges_count >= count_limit))
                return false;

            uintptr_t new_end = end & -intptr_t(boundary);
            memmove(range + 1, range, (++ranges_count - i) * sizeof(*range));
            range[0].size = new_end - range[0].physaddr;
            range[1].physaddr = range[0].physaddr + range[0].size;
            range[1].size = end - range[1].physaddr;
        }
    }

    return true;
}

EXPORT void *mmap_register_device(void *context,
                           uint64_t block_size,
                           uint64_t block_count,
                           int prot,
                           mm_dev_mapping_callback_t callback,
                           void *addr)
{
    mm_dev_mapping_scoped_lock lock(mm_dev_mapping_lock);

    auto ins = find(mm_dev_mappings.begin(), mm_dev_mappings.end(), nullptr);

    size_t sz = block_size * block_count;

    mmap_device_mapping_t *mapping = new (ext::nothrow) mmap_device_mapping_t();

    if (!mapping)
        return nullptr;

    if (!mapping->range.mmap(addr, sz, prot, MAP_DEVICE,
                             int(ins - mm_dev_mappings.begin())))
        return nullptr;

    mapping->context = context;
    mapping->callback = callback;

    mapping->active_read = -1;

    if (ins == mm_dev_mappings.end()) {
        if (unlikely(!mm_dev_mappings.push_back(mapping))) {
            munmap(mapping->range, sz);
            delete mapping;
            return nullptr;
        }
    } else {
        *ins = mapping;
    }

    return likely(mapping) ? mapping->range.get() : nullptr;
}

static int mm_dev_map_search(void const *v, void const *k, void *s)
{
    (void)s;
    mmap_device_mapping_t const *mapping = *(mmap_device_mapping_t const **)v;

    if (k < mapping->range)
        return -1;

    void const *mapping_end = (char const*)mapping->range +
            mapping->range.size();
    if (k >= mapping_end)
        return 1;

    return 0;
}


EXPORT uintptr_t mm_alloc_hole(size_t size)
{
    size = (size + 63) & -64;
    return hole_allocator.alloc_linear(size);
}

EXPORT void mm_free_hole(uintptr_t addr, size_t size)
{
    hole_allocator.release_linear(addr, size);
}

EXPORT void *mmap_window(size_t size)
{
    size = round_up(size);

    linaddr_t addr = linear_allocator.alloc_linear(size);

    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        mmu_map_page(addr + i, PTE_ADDR,
                     PTE_ACCESSED | PTE_DIRTY | PTE_WRITABLE);
    }

    return (void*)addr;
}

void munmap_window(void *addr, size_t size)
{
    size = round_up(size);
    linear_allocator.release_linear((uintptr_t)addr, size);
}

EXPORT int alias_window(void *addr, size_t size,
                        mmphysrange_t const *ranges,
                        size_t range_count)
{
    linaddr_t base = linaddr_t(addr);
    unsigned misalignment = base & PAGE_MASK;
    base -= misalignment;
    size += misalignment;
    size = round_up(size);

    mmphysrange_t const *range = ranges;
    mmphysrange_t const *ranges_end = ranges + range_count;

    size_t range_offset = 0;

    pte_t *ptes[4];
    ptes_from_addr(ptes, base);
    int present_mask = ptes_present(ptes);
    size_t pte_index = 0;

    assert((present_mask & 0x7) == 0x7);
    if ((present_mask & 0x7) != 0x7)
        return 0;

    if (ranges) {
        for (size_t offset = 0; offset < size;
             offset += PAGE_SIZE, range_offset += PAGE_SIZE) {
            // If we have reached the end of the range, advance to next range
            if (range_offset >= range->size) {
                if (++range == ranges_end)
                    return (offset + PAGE_SIZE) == size;
                range_offset = 0;
            }

            ptes[3][pte_index++] =
                    ((range->physaddr + range_offset) & PTE_ADDR) |
                    PTE_ACCESSED | PTE_DIRTY | PTE_WRITABLE | PTE_PRESENT;
        }
    } else {
        for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
            ptes[3][pte_index++] = PTE_ADDR | PTE_ACCESSED |
                    PTE_DIRTY | PTE_WRITABLE;
        }
    }

    for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
        cpu_page_invalidate(base + offset);

    return 1;
}

int mlock(void const *addr, size_t len)
{
//    return 0;

    if (unlikely(len == 0))
        return 0;

    linaddr_t staddr = linaddr_t(addr);
    linaddr_t enaddr = staddr + len;

    bool st_kernel = intptr_t(staddr) < 0;
    bool en_kernel = intptr_t(enaddr-1) < 0;

    // Spans across privilege boundary
    if (unlikely(st_kernel != en_kernel))
        return -int(errno_t::EINVAL);

    len = round_up(len);

    phys_allocator.adjref_virtual_range(linaddr_t(addr), len, 1);

    return 0;
}

int munlock(void const *addr, size_t len)
{
//    return 0;

    if (unlikely(len == 0))
        return 0;

    linaddr_t staddr = linaddr_t(addr);
    linaddr_t enaddr = staddr + len;

    bool kernel = staddr >= 0x800000000000;

    if (unlikely(!kernel && enaddr >= 0x800000000000))
        return -int(errno_t::EINVAL);

    phys_allocator.adjref_virtual_range(linaddr_t(addr), len, -1);

    return 0;
}

uintptr_t mm_new_process(process_t *process, bool use64)
{
    // Allocate a page directory
    pte_t *dir = (pte_t*)mmap(nullptr, PAGE_SIZE,
                              PROT_READ | PROT_WRITE, MAP_POPULATE);

    // Copy upper memory mappings into new page directory
    std::copy(master_pagedir + 256, master_pagedir + 512, dir + 256);

    // Get the physical address for the new process page directory
    physaddr_t dir_physaddr = mphysaddr(dir);

    // Initialize recursive mapping
    dir[PT_RECURSE] = dir_physaddr | PTE_PRESENT | PTE_WRITABLE |
            PTE_ACCESSED | PTE_DIRTY;

    // Switch to new page directory
    cpu_page_directory_set(dir_physaddr);

    //cpu_tlb_flush();

    mm_init_process(process, use64);

    return dir_physaddr;
}

void mm_destroy_process()
{
    physaddr_t dir = cpu_page_directory_get() & PTE_ADDR;

    assert(dir != root_physaddr);

    unsigned path[4];
    pte_t *ptes[4];

    std::vector<physaddr_t> pending_frees;
    pending_frees.reserve(4);

    mmu_phys_allocator_t::free_batch_t free_batch(phys_allocator);

    for (linaddr_t addr = 0; addr <= 0x800000000000; ) {
        for (physaddr_t physaddr : pending_frees)
            free_batch.free(physaddr);
        pending_frees.clear();
        free_batch.flush();

        if (addr == 0x800000000000)
            break;

        int present_mask = addr_present(addr, path, ptes);

        if ((present_mask & 0xF) == 0xF &&
                !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
            if (unlikely(!pending_frees.push_back(*ptes[3] & PTE_ADDR)))
                panic_oom();
        if (unlikely(path[3] == 511)) {
            if ((present_mask & 0x7) == 0x7 &&
                    !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                if (unlikely(!pending_frees.push_back(*ptes[2] & PTE_ADDR)))
                    panic_oom();
            if (path[2] == 511) {
                if ((present_mask & 0x3) == 0x3 &&
                        !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                    if (unlikely(!pending_frees.push_back(*ptes[1] & PTE_ADDR)))
                        panic_oom();
                if (path[1] == 511) {
                    if ((present_mask & 0x1) == 0x1 &&
                            !(*ptes[3] & (PTE_EX_PHYSICAL | PTE_EX_DEVICE)))
                        if (unlikely(!pending_frees.push_back(
                                         *ptes[0] & PTE_ADDR)))
                            panic_oom();
                }
            }
        }

        // If level 0 is not present, advance 512GB (2^39)
        if (unlikely(!(present_mask & 1))) {
            addr += 1L << (12 + (9*3));
            continue;
        }

        // If level 1 is not present, advance 1GB   (2^30)
        if (unlikely(!(present_mask & 2))) {
            addr += 1L << (12 + (9*2));
            continue;
        }

        // If level 2 is not present, advance 2MB   (2^21)
        if (unlikely(!(present_mask & 4))) {
            addr += 1L << (12 + (9*1));
            continue;
        }

        addr += 1L << (12 + (9*0)); // 2^12
    }

    cpu_page_directory_set(root_physaddr);

    free_batch.free(dir);
}

void mm_init_process(process_t *process, bool use64)
{
    contiguous_allocator_t *allocator =
            new (ext::nothrow) contiguous_allocator_t{};

    uintptr_t user_mem_st;
    uintptr_t user_mem_en;
    uintptr_t userTop = UINT64_C(1) << 47;
    constexpr uintptr_t fourMB = 4 << 20;

    // 4MB unavailable margin at start and end of memory
    // (top-4MB) to (top) is reserved (4MB)
    userTop = use64 ? UINT64_C(1) << 47 : (UINT32_C(1) << 31);

    user_mem_st = fourMB;
    user_mem_en = userTop - fourMB;
    allocator->init(user_mem_st, user_mem_en - user_mem_st, "process");

    process->set_allocator(allocator);
}

// Returns the physical address of the original page directory
uintptr_t mm_fork_kernel_text()
{
    uintptr_t st = uintptr_t(___text_st);
    uintptr_t en = uintptr_t(___text_en) - 1;

    // Make aliasing window big enough for two pages
    pte_t *window = (pte_t*)mmap_window(PAGE_SIZE << 1);
    uintptr_t src_linaddr = uintptr_t(window);
    uintptr_t dst_linaddr = uintptr_t(window + 512);

    constexpr pte_t flags = PTE_PRESENT | PTE_WRITABLE;

    // Clone root page directory
    physaddr_t page = mmu_alloc_phys();
    mmu_map_page(uintptr_t(window), page, flags);
    cpu_page_invalidate(uintptr_t(window));
    memcpy(window, master_pagedir, PAGE_SIZE);
    window[PT_RECURSE] = page | flags;

    uintptr_t orig_pagedir = cpu_page_directory_get();
    cpu_page_directory_set(page);
    cpu_tlb_flush();

    pte_t *st_ptes[4];
    pte_t *en_ptes[4];

    ptes_from_addr(st_ptes, st);
    ptes_from_addr(en_ptes, en);

    for (size_t level = 0; level < 4; ++level) {
        for (pte_t *pte = st_ptes[level]; pte <= en_ptes[level]; ++pte) {
            // Allocate clone
            page = mmu_alloc_phys();

            // Map original
            pte_t orig_pte = *pte;
            physaddr_t orig_physaddr = orig_pte & PTE_ADDR;
            mmu_map_page(src_linaddr, orig_physaddr, flags);

            // Map clone
            mmu_map_page(dst_linaddr, page, flags);

            cpu_page_invalidate(src_linaddr);
            cpu_page_invalidate(dst_linaddr);

            // Copy original to clone
            memcpy(window + 512, window, PAGE_SIZE);

            // Point PTE to clone
            *pte = page | (orig_pte & ~PTE_ADDR);

            cpu_tlb_flush();
        }
    }

    munmap_window(window, PAGE_SIZE << 1);
    cpu_tlb_flush();

    return orig_pagedir;
}

void clear_phys_state_t::reserve_addr()
{
    bool ok;
    size_t size = size_t(1) << log2_window_sz;
//    linear_allocator.dump("before reserve_addr"
//                          " take_linear(addr=%#zx, size=%#zx\n",
//                          addr, size);
    ok = linear_allocator.take_linear(addr, size, true);
//    linear_allocator.dump("after reserve_addr take_linear\n");
    assert(ok);
}

void mm_set_master_pagedir()
{
    cpu_page_directory_set(root_physaddr);
}

// Hierarchical bitmap allocator
//
// with 64 bit word size
//
// +-------+-------+-----------+--------+
// |levels | Items | Mem 2^(n) |  Mem   |
// +-------+-------+-----------+--------+
// |   1   |     8 |      ( 3) |   ~0   |
// |   2   |   512 |      ( 9) |   ~0   |
// |   3   |   32K |      (15) |  ~32KB |
// |   4   |   64G |      (21) |   ~2MB |
// |   5   |    4T |      (27) | ~128MB |
// +-------+-------+-----------+--------+

uint_least64_t mm_memory_remaining()
{
    return phys_allocator.get_free_page_count();
}

uint_least64_t mm_get_free_page_count()
{
    return phys_allocator.get_free_page_count();
}

uint_least64_t mm_get_phys_mem_size()
{
    return phys_allocator.get_phys_mem_size();
}
