#pragma once
#include "types.h"

enum constructor_order_t {
    ctor_asan_init = 1000,
    ctor_ctors_ran,
    ctor_cpu_init_cpus, ctor_cpu_init_cpus_done,
    ctor_cpu_init_bsp, ctor_cpu_init_bsp_done,
    ctor_text_dev, ctor_text_dev_done,
    ctor_mmu_init, ctor_mmu_init_done,
    ctor_thread_init_bsp, ctor_thread_init_bsp_done,
    ctor_cpu_hw_init, ctor_cpu_hw_init_done
};

// Types:
//  'M': VMM initialized
//  'S': Initialize SMP CPU
//  'E': Early initialized device
//  'T': SMP online
//  'C': Constructors ran
// from init thread, in this order:
//  'D': Facilities needed by drivers
//  'L': Late initialized device
//  'F': Filesystem implementation
//  'H': Storage interface
//  'P': Partition scanner
//  'N': Network interface
//  'U': USB interface

enum struct callout_type_t : uint32_t {
    // bootstrap
    vmm_ready = 'M',
    heap_ready = 'R',
    txt_dev = 'V',
    acpi_ready = 'A',
    smp_online = 'T',
    //constructors_ran = 'C',
    tss_list_ready = 128,
    init_thread_done = 130,

    // from init_thread
    driver_base= 'D',
    late_dev = 'L',
    reg_filesys = 'F',
    storage_dev = 'H',
    partition_probe = 'P',
    nic = 'N',
    nics_ready = 129,
    usb = 'U',

    devfs_ready = 256,
    tmpfs_up
};

struct callout_t {
    void (*fn)(void*);
    void *userarg;
    callout_type_t type;
    int32_t reserved;
    int64_t reserved2;
};

#define REGISTER_CALLOUT3(a,n)   a##n
#define REGISTER_CALLOUT2(a,n)   REGISTER_CALLOUT3(a,n)

#define REGISTER_CALLOUT(fn, arg, type, order) \
    __attribute__((section(".callout_array." order), used, aligned(16))) \
    static constexpr callout_t const \
    REGISTER_CALLOUT2(callout_, __COUNTER__) = { \
        (fn), (arg), (type), 0, 0 \
    }

extern "C" size_t callout_call(callout_type_t type, bool as_thread = false);
