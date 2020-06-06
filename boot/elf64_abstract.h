#pragma once

#include "types.h"
#include "elf64decl.h"
#include "screen.h"

#define ELF64_DEBUG    0
#if ELF64_DEBUG
#define ELF64_TRACE(...) PRINT(TSTR "elf64: " __VA_ARGS__)
#else
#define ELF64_TRACE(...) ((void)0)
#endif

struct elf64_context_t {
    int64_t start_time;
    int64_t last_time;
    uint64_t last_file_bytes;
    uint64_t done_file_bytes;
    uint64_t total_file_bytes;
    uint64_t done_mem_bytes;
    uint64_t total_mem_bytes;
    uintptr_t address_window;
    uint64_t page_flags;
};

elf64_context_t *load_kernel_begin();
bool load_kernel_chunk(Elf64_Phdr *blk, int file, elf64_context_t *ctx);
void load_kernel_end(elf64_context_t *ctx);
