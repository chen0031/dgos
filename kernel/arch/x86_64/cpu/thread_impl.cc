#include "cpu/thread_impl.h"
#include "debug.h"
#include "process.h"
#include "types.h"
#include "interrupts.h"
#include "string.h"
#include "assert.h"
#include "likely.h"

#include "thread.h"
#include "main.h"
#include "halt.h"
#include "cpu.h"
#include "gdt.h"
#include "mm.h"
#include "time.h"
#include "export.h"
#include "printk.h"
#include "apic.h"
#include "callout.h"
#include "vector.h"
#include "mutex.h"
#include "except.h"
#include "work_queue.h"
#include "cxxexcept.h"
#include "basic_set.h"

// Implements platform independent thread.h

#define DEBUG_THREAD    1
#if DEBUG_THREAD
#define THREAD_TRACE(...) printdbg("thread: " __VA_ARGS__)
#else
#define THREAD_TRACE(...) ((void)0)
#endif

#define DEBUG_THREAD_STACK    1
#if DEBUG_THREAD_STACK
#define THREAD_STK_TRACE(...) printdbg("thread_stk: " __VA_ARGS__)
#else
#define THREAD_STK_TRACE(...) ((void)0)
#endif

enum thread_state_t : uint32_t {
    THREAD_IS_UNINITIALIZED = 0,
    THREAD_IS_INITIALIZING,
    THREAD_IS_READY,
    THREAD_IS_RUNNING,
    THREAD_IS_SLEEPING,
    THREAD_IS_DESTRUCTING,
    THREAD_IS_FINISHED,
    THREAD_IS_EXITING,

    // Flag keeps other cpus from taking thread
    // until after stack switch
    THREAD_BUSY = 0x00000010U,
    THREAD_IS_READY_BUSY = THREAD_IS_READY | THREAD_BUSY,
    THREAD_IS_FINISHED_BUSY = THREAD_IS_FINISHED | THREAD_BUSY,
    THREAD_IS_SLEEPING_BUSY = THREAD_IS_SLEEPING | THREAD_BUSY,
    THREAD_IS_DESTRUCTING_BUSY = THREAD_IS_DESTRUCTING | THREAD_BUSY,
    THREAD_IS_EXITING_BUSY = THREAD_IS_EXITING | THREAD_BUSY
};

// When state is equal to one of these:
//  THREAD_IS_READY
// any CPU can transition it to THREAD_IS_RUNNING.
//
// When state is equal to one of these:
//  THREAD_IS_SLEEPING
// any CPU can transition it to THREAD_IS_READY
//
// When state is equal to one of these:
//  THREAD_IS_UNINITIALIZED
// any CPU can transition it to THREAD_IS_INITIALIZING
//
// Put another way, another CPU can only change
// the state if it is currently one of these:
//  THREAD_IS_READY
//  THREAD_IS_SLEEPING
//  THREAD_IS_UNINITIALIZED
//

struct link_t {
    link_t *next;
    link_t *prev;

    template<typename T, size_t offset = offsetof(T, link)>
    T *container(link_t *link)
    {
        return (T*)((char*)link - offset);
    }
};

using timestamp_t = uint64_t;
using ready_map_t = std::map<timestamp_t, thread_info_t*>;

struct alignas(256) thread_info_t {
    // Exclusive line, frequently modified items, rarely shared

    isr_context_t * volatile ctx;

    // Modified every context switch and/or syscall
    // TODO: need place to put 32 bit flags during syscall entry
    void * volatile fsbase;
    void * volatile gsbase;
    uint32_t syscall_mxcsr, syscall_fcw87;
    char * volatile xsave_ptr;

    // Modified every context switch
    uint64_t used_time;

    uint64_t reserved[2];

    // --- cache line --- shared line

    // 2 64-bit pointers
    link_t link;

    // Owning process
    process_t *process;

    char *xsave_stack;

    // Points to place in stack to use when an interrupt occurs at lower priv
    char *priv_chg_stack;

    thread_t thread_id;

    // 1 until closed, then 0
    int ref_count;

    thread_state_t volatile state;

    // Higher numbers are higher priority
    thread_priority_t volatile priority;
    thread_priority_t volatile priority_boost;

    uint64_t volatile wake_time;

    // --- cache line ---

    // 4 64-bit values
    std::mutex lock;   // 32 bytes

    // 3 64-bit values
    std::condition_variable done_cond;

    // Current CPU exception longjmp container
    void *exception_chain;

    // --- cache line ---

    // Thread current kernel errno
    errno_t errno;
    // 3 bytes...

    // Doesn't include guard pages
    uint32_t stack_size;

    uint32_t thread_flags;

    int wake_count;

    // Process exit code
    int exit_code;

    int reserved2;

    // 2 64-bit values
    __cxa_eh_globals cxx_exception_info;

    // Timestamp at moment thread was resumed
    uint64_t sched_timestamp;

    // Threads that got their timeslice sooner are ones that have slept,
    // so they are implicitly higher priority
    // When their timeslice is used up, they get a new one timestamped now
    // losing their privileged status allowing other threads to have a turn
    uint64_t timeslice_timestamp;

    // Each time a thread context switches, time is removed from this value
    // When it reaches zero, it is replenished, and timeslice_timestamp
    // is set to now. Preemption is set up so the timer will fire when this
    // time elapses.
    uint64_t timeslice_remaining;

    // --- cache line ---

    char *stack;

    thread_cpu_mask_t cpu_affinity;

    std::vector<__exception_jmp_buf_t> exit_jmpbuf;

    char *name;

    bool closed;

    // Iterator points to the node we inserted into the
    // ready list, otherwise default constructed
    ready_map_t::iterator schedule_node;
};

C_ASSERT_ISPO2(sizeof(thread_info_t));

// Verify asm_constants.h values
C_ASSERT(offsetof(thread_info_t, fsbase) == THREAD_FSBASE_OFS);
C_ASSERT(offsetof(thread_info_t, gsbase) == THREAD_GSBASE_OFS);
C_ASSERT(offsetof(thread_info_t, process) == THREAD_PROCESS_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, xsave_ptr) == THREAD_XSAVE_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, priv_chg_stack) == THREAD_PRIV_CHG_STACK_OFS);
C_ASSERT(offsetof(thread_info_t, thread_id) == THREAD_THREAD_ID_OFS);
C_ASSERT(offsetof(thread_info_t, syscall_mxcsr) == THREAD_SC_MXCSR_OFS);
C_ASSERT(offsetof(thread_info_t, syscall_fcw87) == THREAD_SC_FCW87_OFS);

// Save FPU context when interrupted in user mode
// (always on for threads that execute user programs)
#define THREAD_FLAGS_USER_FPU   (1U<<0)

// Save FPU context when interrupted in kernel mode
#define THREAD_FLAGS_KERNEL_FPU (1U<<1)

#define THREAD_FLAGS_ANY_FPU ( \
    THREAD_FLAGS_KERNEL_FPU | \
    THREAD_FLAGS_USER_FPU)

// Store in a big array, for now
#define MAX_THREADS 512
static thread_info_t threads[MAX_THREADS];

// Separate to avoid constantly sharing dirty lines
static uint8_t run_cpu[MAX_THREADS];

static size_t volatile thread_count;

uint32_t volatile thread_smp_running;
int thread_idle_ready;
int spincount_mask;
size_t storage_next_slot;
bool thread_cls_ready;

static size_t constexpr xsave_stack_size = (size_t(64) << 10);

enum fpu_state_t : uint8_t {
    // FPU is discarded at entry to syscall
    discarded,
    saved,
    restored
};

struct alignas(128) cpu_info_t {
    cpu_info_t *self = nullptr;

    thread_info_t * volatile cur_thread = nullptr;

    // Accessed every syscall
    tss_t *tss_ptr = nullptr;
    uintptr_t syscall_flags;

    bool online = false;
    fpu_state_t fpu_state = discarded;
    uint8_t reserved2[2] = {};

    // Which thread's context is in the FPU, or -1 if discarded
    thread_t fpu_owner = -1;

    thread_info_t *goto_thread = nullptr;

    uint64_t pf_count = 0;

    // Context switch is prevented when this is nonzero
    uint32_t locks_held = 0;
    // When locks_held transitions to zero, a context switch is forced
    // when this is true. Deferring a context switch because locks_held
    // is nonzero sets this to true
    uint32_t csw_deferred = 0;

    uint64_t volatile tlb_shootdown_count = 0;

    // --- cache line ---

    uint32_t apic_id = ~uint32_t(0);
    uint32_t time_ratio = 0;

    uint32_t busy_ratio = 0;
    uint32_t busy_percent_x1k = 0;

    uint32_t cr0_shadow = 0;
    unsigned cpu_nr = 0;

    uint64_t irq_count = 0;

    uint64_t irq_time = 0;

    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    lock_type queue_lock;

    //unsigned reserved3 = 0;

    // Cleanup to be run after switching stacks on a context switch
    void (*after_csw_fn)(void*) = nullptr;
    void *after_csw_vp = nullptr;

    void *storage[7] = {};

    // Tree sorted in order of when timestamp was issued.
    // Only threads that are ready to run appear in this tree.
    // Threads that block on I/O and keep an old timeslice issued further
    // in the past get drastically more priority than threads that used up
    // their timeslice, and recently got a newer timeslice.
    ready_map_t ready_list;

    thread_t slih_return;
};
// Make sure pointer arithmetic would never do a divide
C_ASSERT_ISPO2(sizeof(cpu_info_t));

// Verify asm_constants.h values
C_ASSERT(offsetof(cpu_info_t, self) == CPU_INFO_SELF_OFS);
C_ASSERT(offsetof(cpu_info_t, cur_thread) == CPU_INFO_CURTHREAD_OFS);
C_ASSERT(offsetof(cpu_info_t, tss_ptr) == CPU_INFO_TSS_PTR_OFS);
C_ASSERT(offsetof(cpu_info_t, syscall_flags) == CPU_INFO_SYSCALL_FLAGS_OFS);
C_ASSERT(offsetof(cpu_info_t, pf_count) == CPU_INFO_PF_COUNT_OFS);
C_ASSERT(offsetof(cpu_info_t, tlb_shootdown_count) ==
         CPU_INFO_TLB_SHOOTDOWN_COUNT_OFS);
C_ASSERT(offsetof(cpu_info_t, locks_held) == CPU_INFO_LOCKS_HELD_OFS);
C_ASSERT(offsetof(cpu_info_t, csw_deferred) == CPU_INFO_CSW_DEFERRED_OFS);
C_ASSERT(offsetof(cpu_info_t, after_csw_fn) == CPU_INFO_AFTER_CSW_FN_OFS);
C_ASSERT(offsetof(cpu_info_t, after_csw_vp) == CPU_INFO_AFTER_CSW_VP_OFS);
cpu_info_t cpus[MAX_CPUS];
cpu_info_t *cpus_end = cpus + MAX_CPUS;

_constructor(ctor_cpu_init_cpus)
static void cpus_init_t()
{
    // Do some basic initializations on the first CPU
    // without having a big initializer list of unreadable
    // nonsense above
    cpus[0].self = cpus;
    cpus[0].cur_thread = threads;
    cpus[0].tss_ptr = tss_list;
}

// Ticks up as each AP starts up
uint32_t cpu_count;

// Holds full count
uint32_t total_cpus;

void thread_destruct(thread_info_t *thread);
static void thread_signal_completion(thread_info_t *thread);

///

// Per-CPU scheduling queue
class cpu_queue_t {
public:
    cpu_queue_t();
    ~cpu_queue_t();

    thread_info_t *choose_next();

private:
    thread_info_t *current;

    // 32 vectors, one per priority level
    std::vector<thread_t> priorities[32];

    // Bitmask caches emptiness and allows O(1) selection of highest priority
    uint32_t empty_flags;

    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    lock_type lock;
};

// Get executing APIC ID (the slow expensive way, for early initialization)
static uint32_t get_apic_id()
{
    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    uint32_t apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

// Get executing CPU by APIC ID (slow, only used in early init)
cpu_info_t *this_cpu_by_apic_id_slow()
{
    uint32_t apic_id = get_apic_id();
    for (size_t i = 0, e = total_cpus; i != e; ++i)
        if (cpus[i].apic_id == apic_id)
            return cpus + i;
    return nullptr;
}

_hot
static _always_inline cpu_info_t *this_cpu()
{
    return cpu_gs_read<cpu_info_t*, 0>();
}

_hot
static _always_inline thread_info_t *this_thread()
{
    return cpu_gs_read<thread_info_t*, offsetof(cpu_info_t, cur_thread)>();
}

static void thread_cleanup()
{
    thread_info_t *thread = this_thread();

    cpu_irq_disable();

    assert(thread->state == THREAD_IS_RUNNING);

    atomic_barrier();
    thread->priority = 0;
    thread->priority_boost = 0;
    if (likely(thread->state == THREAD_IS_RUNNING))
        thread->state = THREAD_IS_DESTRUCTING_BUSY;
    else
        assert(!"Unexpected state!");

    thread_yield();
}

void thread_startup(thread_fn_t fn, void *p, thread_t id)
{
    threads[id].exit_code = fn(p);
    thread_cleanup();
}

static constexpr size_t stack_guard_size = (64<<10);

// Allocate a stack with a large guard region at both ends and
// return a pointer to the end of the middle committed part
static char *thread_allocate_stack(
        thread_t tid, size_t stack_size, char const *noun, int fill)
{
    // Whole thing is guard pages at first
    char *stack;
    stack = (char*)mmap(nullptr, stack_guard_size +
                        stack_size + stack_guard_size, PROT_NONE,
                        MAP_UNINITIALIZED, -1, 0);

    if (unlikely(stack == MAP_FAILED))
        return nullptr;

    THREAD_STK_TRACE("Allocated %s stack"
                     ", size=%#zx"
                     ", tid=%d"
                     ", filled=%d\n",
                     noun, stack_size, tid, fill);

    //
    // Guard pages

    // Mark guard region address ranges
    char *guard0_st = stack;
    char *guard0_en = guard0_st + stack_guard_size;
    char *guard1_st = guard0_en + stack_size;
    mprotect(guard0_en, stack_size, PROT_READ | PROT_WRITE);
    madvise(guard0_en, stack_size, MADV_WILLNEED);

    THREAD_TRACE("Thread %d %s stack range=%p-%p,"
                 " stack=%p-%p\n", tid, noun,
                 (void*)stack,
                 (void*)(stack + stack_guard_size + stack_size +
                         stack_guard_size),
                 (void*)(guard0_en),
                 (void*)(guard1_st));

    memset(guard0_en, fill, stack_size);

    return guard1_st;
}

// Returns threads array index or 0 on error
// Minimum allowable stack space is 4KB
static thread_t thread_create_with_state(
        thread_fn_t fn, void *userdata, size_t stack_size,
        thread_state_t state, thread_cpu_mask_t const &affinity,
        thread_priority_t priority, bool user, bool is_float)
{
    if (stack_size == 0)
        stack_size = 65536;
    else if (stack_size < 16384)
        return -1;

    thread_info_t *thread = nullptr;
    size_t i;

    for (i = 0; ; ++i) {
        if (unlikely(i >= MAX_THREADS)) {
            printdbg("Out of threads, yielding\n");
            thread_yield();
            i = 0;
        }

        thread = threads + i;

        if (thread->state != THREAD_IS_UNINITIALIZED)
            continue;

        // Atomically grab the thread
        if (likely(atomic_cmpxchg(&thread->state,
                                  THREAD_IS_UNINITIALIZED,
                                  THREAD_IS_INITIALIZING) ==
                   THREAD_IS_UNINITIALIZED))
        {
            break;
        }

        pause();
    }

    atomic_barrier();

    thread->thread_flags = 0;

    // 0 = never
    thread->wake_time = 0;

    //mutex_init(&thread->lock);
    //condvar_init(&thread->done_cond);

    thread->ref_count = 1;

    char *stack = thread_allocate_stack(
                i, stack_size, "create_with_state", 0xF0);

    if (unlikely(stack == nullptr))
        panic_oom();

    thread->stack = stack;
    thread->stack_size = stack_size;

    char *xsave_stack = nullptr;
    if (user || is_float) {
        // XSave stack

        thread->thread_flags |= THREAD_FLAGS_KERNEL_FPU;

        xsave_stack = thread_allocate_stack(
                    i, xsave_stack_size, "xsave", 0);

        assert(sse_context_size >= 512);
        thread->xsave_ptr = xsave_stack - sse_context_size;
    } else {
        thread->xsave_ptr = nullptr;
    }
    //thread->syscall_stack = syscall_stack;
    thread->xsave_stack = xsave_stack;

//    thread_info_t *creator_thread = this_thread();

    thread->priority = priority;
    thread->priority_boost = 0;
    thread->cpu_affinity = affinity;
    thread->fsbase = nullptr;
    thread->gsbase = nullptr;

    // Created thread inherits creator's process
    thread->process = thread_current_process();

    uintptr_t stack_addr = uintptr_t(stack);
    uintptr_t stack_end = stack_addr;

    constexpr size_t ctx_size = sizeof(isr_context_t);

    // Adjust start of context to make room for context
    uintptr_t ctx_addr = stack_end - ctx_size;

    isr_context_t *ctx = (isr_context_t*)ctx_addr;
    memset(ctx, 0, ctx_size);
    ctx->fpr = (isr_fxsave_context_t*)thread->xsave_ptr;

    ISR_CTX_REG_RSP(ctx) = stack_end;

    ISR_CTX_REG_SS(ctx) = GDT_SEL_KERNEL_DATA;

    ISR_CTX_REG_RFLAGS(ctx) = CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS;

    ISR_CTX_REG_RIP(ctx) = thread_fn_t(uintptr_t(thread_entry));

    ISR_CTX_REG_CS(ctx) = GDT_SEL_KERNEL_CODE64;
    ISR_CTX_REG_DS(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_ES(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_FS(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_GS(ctx) = GDT_SEL_USER_DATA | 3;

    ISR_CTX_REG_RBP(ctx) = uintptr_t(&ISR_CTX_REG_RBP(ctx));

    ISR_CTX_REG_RDI(ctx) = uintptr_t(fn);
    ISR_CTX_REG_RSI(ctx) = uintptr_t(userdata);
    ISR_CTX_REG_RDX(ctx) = i;
    ISR_CTX_REG_CR3(ctx) = cpu_page_directory_get();

    if (thread->thread_flags & THREAD_FLAGS_ANY_FPU) {
        ISR_CTX_SSE_MXCSR(ctx) = (CPU_MXCSR_MASK_ALL |
                CPU_MXCSR_RC_n(CPU_MXCSR_RC_NEAREST)) &
                default_mxcsr_mask;
        ISR_CTX_SSE_MXCSR_MASK(ctx) = default_mxcsr_mask;

        // All FPU registers empty
        ISR_CTX_FPU_FSW(ctx) = CPU_FPUSW_TOP_n(7);

        // 53 bit FPU precision
        ISR_CTX_FPU_FCW(ctx) = CPU_FPUCW_PC_n(CPU_FPUCW_PC_53) | CPU_FPUCW_IM |
                CPU_FPUCW_DM | CPU_FPUCW_ZM | CPU_FPUCW_OM |
                CPU_FPUCW_UM | CPU_FPUCW_PM;
    }

    thread->ctx = ctx;

    atomic_barrier();
    atomic_st_rel(&thread->state, state);

    // Atomically make sure thread_count > i
    atomic_max(&thread_count, i + 1);

    return i;
}


EXPORT thread_t thread_create(thread_fn_t fn, void *userdata,
                              size_t stack_size, bool user, bool is_float)
{
    thread_create_info_t info{};
    info.fn = fn;
    info.userdata = userdata;
    info.stack_size = stack_size;
    info.user = user;
    info.is_float = is_float;

    return thread_create_with_info(&info);
}

EXPORT thread_t thread_create_with_info(thread_create_info_t const* info)
{
    return thread_create_with_state(
                info->fn, info->userdata, info->stack_size,
                info->suspended ? THREAD_IS_SLEEPING: THREAD_IS_READY,
                thread_cpu_mask_t(-1), 0, info->user, info->is_float);
}

#if 0
static void thread_monitor_mwait()
{
    uint64_t rax = 0;
    __asm__ __volatile__ (
        "lea timer_ms(%%rip),%%rax\n\t"
        "monitor\n\t"
        "mwait\n\t"
        : "+a" (rax)
        : "d" (0), "c" (0)
        : "memory"
    );
}
#endif

static int smp_idle_thread(void *)
{
    // Enable spinning
    spincount_mask = -1;

    apic_config_cpu();

    printdbg("SMP thread running\n");
    atomic_inc(&thread_smp_running);
    thread_check_stack();

    cpu_irq_enable();

    thread_idle();
}

_hot
void thread_idle()
{
    assert(cpu_irq_is_enabled());
    for(;;)
        halt();
}

// Hardware interrupt handler (an IPI) to provoke other CPUs to reschedule
static isr_context_t *thread_ipi_resched(int intr, isr_context_t *ctx)
{
    return thread_schedule(ctx);
}

_constructor(ctor_thread_init_bsp)
static void thread_init_bsp()
{
    thread_init(0);
}

void thread_set_cpu_count(size_t new_cpu_count)
{
    total_cpus = new_cpu_count;
}

void thread_init(int ap)
{
    if (!thread_cls_ready)
        thread_cls_ready = true;

    uint32_t cpu_nr = atomic_xadd(&cpu_count, 1);

    cpu_info_t *cpu = cpus + cpu_nr;

    cpu->cpu_nr = cpu_nr;

    if (cpu_nr > 0) {
        // Load hidden tssbase, point to cpu-local TSS
        gdt_load_tr(cpu_nr);
        cpu->tss_ptr = tss_list + cpu_nr;
    }

    assert(thread_count == cpu_nr);

    thread_info_t *thread = threads + cpu_nr;

    cpu->self = cpu;
    cpu->apic_id = get_apic_id();
    cpu->online = 1;

    // Initialize kernel gsbase and other gsbase
    cpu_gsbase_set(cpu);
    cpu_altgsbase_set(nullptr);

    // Remember CR0 so we can avoid changing it
    cpu->cr0_shadow = uint32_t(cpu_cr0_get());

    if (!ap) {
        //
        // Perform BSP-only initializations

        thread->process = process_t::init(cpu_page_directory_get());

        for (unsigned i = 0; i < countof(threads); ++i) {
            // Initialize every thread ID so pointer tricks aren't needed
            threads[i].thread_id = i;

            // First 2N threads, 1N for idle threads, 1N for per cpu workers
            if (i < total_cpus * 2)
                threads[i].process = threads[0].process;
        }

        // Hook handler that performs a reschedule requested by another CPU
        intr_hook(INTR_IPI_RESCHED, thread_ipi_resched,
                  "hw_ipi_resched", eoi_lapic);

        // The current thread for this CPU is this thread
        cpu->cur_thread = thread;

        mm_init_process(thread->process);

        thread->sched_timestamp = cpu_rdtsc();
        thread->used_time = 0;
        thread->ctx = nullptr;
        thread->priority = -256;

        // BSP stack (grows down)
        thread->stack = thread_allocate_stack(
                    0, kernel_stack_size, "idle", 0xFE);

        thread->xsave_stack = nullptr;
        thread->xsave_ptr = nullptr;
        thread->cpu_affinity = thread_cpu_mask_t(1);
        atomic_barrier();
        thread->state = THREAD_IS_RUNNING;
        thread_count = 1;
    } else {
        cpu_irq_disable();

        thread = threads + thread_create_with_state(
                    smp_idle_thread, nullptr, 0,
                    THREAD_IS_INITIALIZING,
                    thread_cpu_mask_t(cpu_nr),
                    -256, false, false);

        thread->process = threads[0].process;

        thread->used_time = 0;

        cpu->goto_thread = thread;

        atomic_barrier();
        thread_yield();
        __builtin_unreachable();
    }
}

///
/// struct sched_entry_t {
///     // Timestamp when this thread should run again, in nanoseconds
///     uint64_t run_ns;
///     size_t thread_id;
/// }
///
/// When a thread's timeslice expires, calculate
///  now + (timeslice_duration * running_threads)
/// and reinsert it into the scheduling set
///
/// When a thread voluntarily blocks, it is removed from the scheduling
/// set, but when it unblocks, it regains the run_ns value it had when
/// it blocked, making threads which have blocked longest get serviced first.
///
/// set<sched_entry_t>

static thread_info_t *thread_choose_next(
        cpu_info_t *cpu,
        thread_info_t * const outgoing)
{
    size_t cpu_nr = cpu->cpu_nr;
    size_t i = outgoing->thread_id;
    thread_info_t *incoming = nullptr;
    thread_info_t *candidate;
    uint64_t now = 0;

    assert(i < countof(threads));

    // If we have not created all of the idle threads yet, don't context switch
    if (unlikely(thread_count < cpu_count))
        return outgoing;

    thread_info_t &slih_thread = threads[cpu_count + cpu_nr];

    // If the SLIH thread is ready, instantly choose that thread
    if (unlikely(slih_thread.state == THREAD_IS_READY)) {
        return threads + (cpu_count + cpu_nr);
    }

    // If the current thread is SLIH, and it is not ready, return to the
    // interrupted thread
    if (outgoing->thread_id >= thread_t(cpu_count) &&
            outgoing->thread_id < thread_t(cpu_count * 2) &&
            slih_thread.state != THREAD_IS_RUNNING) {
        thread_info_t &return_thread = threads[cpu->slih_return];
        if (return_thread.state == THREAD_IS_READY)
            return &return_thread;
    }

    // Consider each thread, excluding idle threads
    size_t count = thread_count - cpu_count;

    for (size_t checked = 0; ++i, checked < count; ++checked) {
        // Skip idle threads, or wrap to the first non-idle, non-SLIH thread
        if (i < cpu_count || i >= thread_count)
            i = cpu_count * 2;

        // Ignore threads not currently running on this CPU
        if (run_cpu[i] != cpu_nr)
            continue;

        candidate = threads + i;

        // Quickly ignore running and suspended threads
        if (candidate->state == THREAD_IS_RUNNING ||
                candidate->state == THREAD_IS_UNINITIALIZED ||
                candidate->state == THREAD_IS_EXITING)
            continue;

        // If this thread is not allowed to run on this CPU
        // then skip it
        if (unlikely(!(candidate->cpu_affinity[cpu_nr])))
            continue;

        // Garbage collect destructing threads
        if (unlikely(candidate->state == THREAD_IS_DESTRUCTING)) {
            if (atomic_cmpxchg(&candidate->state, THREAD_IS_DESTRUCTING,
                               THREAD_IS_EXITING) == THREAD_IS_DESTRUCTING) {
                if (candidate->process->del_thread(candidate->thread_id))
                    thread_signal_completion(candidate);
            }
            continue;
        }

        //
        // Expect states to have busy bit set if it is the outgoing thread

        thread_state_t expected_sleep = (outgoing == candidate)
                ? THREAD_IS_SLEEPING_BUSY
                : THREAD_IS_SLEEPING;

        thread_state_t expected_ready = (outgoing == candidate)
                ? THREAD_IS_READY_BUSY
                : THREAD_IS_READY;

        if (likely(candidate->state == expected_sleep)) {
            // The thread is sleeping, see if it should wake up yet

            // Infinite timeout?
            if (likely(!candidate->wake_time))
                continue;

            // If we didn't get current time yet, get it
            if (unlikely(now == 0))
                now = time_ns();

            // Not yet?
            if (likely(now < candidate->wake_time))
                continue;

            // Race to transition it to ready
            if (unlikely(atomic_cmpxchg(&candidate->state, expected_sleep,
                                        expected_ready) != expected_sleep)) {
                // Another CPU beat us to it
                continue;
            }
        } else if (unlikely(candidate->state != expected_ready))
            continue;

        if (likely(incoming)) {
            // Must be better than best
            if (likely(candidate->priority + candidate->priority_boost >
                       incoming->priority + incoming->priority_boost))
                incoming = candidate;
        } else if (likely(outgoing->state == THREAD_IS_READY_BUSY)) {
            // Must be at least the same priority as outgoing
            if (likely(candidate->priority + candidate->priority_boost >=
                       outgoing->priority + outgoing->priority_boost))
                incoming = candidate;
        } else {
            // Outgoing thread is not ready, any thread is better
            incoming = candidate;
        }
    }

    // Did not find any ready thread, choose idle thread
    if (unlikely(!incoming))
        incoming = threads + cpu_nr;

    assert(incoming
           ? incoming >= threads && incoming <= threads + countof(threads)
           : outgoing >= threads && outgoing <= threads + countof(threads));

    return incoming;
}

static void thread_clear_busy(void *outgoing)
{
    thread_info_t *thread = (thread_info_t*)outgoing;

    // When the exiting thread has finished getting off the CPU, immediately
    // delete the thread from the owning process
    if (unlikely(atomic_and(&thread->state,
                            (thread_state_t)~THREAD_BUSY) ==
                 THREAD_IS_EXITING)) {
        thread_destruct(thread);
    }
}

static void thread_free_stacks(thread_info_t *thread)
{
    char *stk;
    size_t stk_sz;

    assert(thread->state == THREAD_IS_EXITING ||
           thread->state == THREAD_IS_FINISHED);

    // The thread stack
    if (thread->stack != nullptr && thread->stack_size > 0) {
        stk = thread->stack - thread->stack_size - stack_guard_size;
        stk_sz = stack_guard_size + thread->stack_size + stack_guard_size;
        thread->stack = nullptr;
        thread->stack_size = 0;

        THREAD_STK_TRACE("Freeing %s stack"
                         ", addr=%#zx"
                         ", size=%#zx"
                         ", tid=%d\n",
                         "thread", uintptr_t(stk),
                         stk_sz, thread->thread_id);

        assert(stk != nullptr);
        assert(stk_sz != 0);
        munmap(stk, stk_sz);
    }

    // The xsave stack
    if (thread->thread_flags & THREAD_FLAGS_ANY_FPU) {
        stk = thread->xsave_stack - xsave_stack_size - stack_guard_size;
        stk_sz = stack_guard_size + xsave_stack_size + stack_guard_size;
        thread->xsave_stack = nullptr;
        thread->thread_flags &= ~THREAD_FLAGS_ANY_FPU;

        THREAD_STK_TRACE("Freeing %s stack"
                         ", addr=%#zx"
                         ", size=%#zx"
                         ", tid=%d\n",
                         "xsave", uintptr_t(stk),
                         stk_sz, thread->thread_id);

        assert(stk != nullptr);
        assert(stk_sz != 0);
        munmap(stk, stk_sz);
    }
}

static void thread_signal_completion(thread_info_t *thread)
{
    // Wake up any threads waiting for this thread to exit ASAP
    std::unique_lock<std::mutex> mutex_lock(thread->lock);
    thread->state = THREAD_IS_FINISHED;
    mutex_lock.unlock();
    thread->done_cond.notify_all();
}

// Thread is not running anymore, destroy things only needed when it runs
void thread_destruct(thread_info_t *thread)
{\
    cpu_scoped_irq_disable irq_dis;

    thread_free_stacks(thread);

    // If everybody has closed their handle to this thread,
    // then mark it for recycling immediately
    if (thread->ref_count == 0) {
        // New mutex
        //mutex_destroy(&thread->lock);
        atomic_st_rel(&thread->state, THREAD_IS_UNINITIALIZED);
    }
}

EXPORT int thread_close(thread_t tid)
{
    thread_info_t& thread = threads[tid];
    if (thread.closed)
        return 0;

    if (thread.state == THREAD_IS_FINISHED) {
        //mutex_destroy(&thread.lock);
        atomic_st_rel(&thread.state, THREAD_IS_UNINITIALIZED);
        return 1;
    }
    return 0;
}

_hot isr_context_t *thread_schedule(isr_context_t *ctx)
{
    //need this if ISRs run with IRQ enabled:
    //cpu_scoped_irq_disable intr_dis;

    cpu_info_t *cpu = this_cpu();

    thread_info_t *thread = cpu->cur_thread;

    thread_info_t * const outgoing = thread;

    if (unlikely(cpu->goto_thread)) {
        thread = cpu->goto_thread;
        cpu->cur_thread = thread;
        cpu->goto_thread = nullptr;
        atomic_st_rel(&thread->state, THREAD_IS_RUNNING);
        ctx = thread->ctx;
        thread->ctx = nullptr;
        return ctx;
    }

    // Defer reschedule if locks are held
    if (unlikely(cpu->locks_held)) {
        cpu->csw_deferred = true;
        return ctx;
    }

    // Store context pointer for resume later
    assert(thread->ctx == nullptr);
    thread->ctx = ctx;

    uint64_t now = cpu_rdtsc();
    uint64_t elapsed = now - thread->sched_timestamp;

    thread->used_time += elapsed;

    //
    // Accumulate used and busy time on this CPU

    if (uint32_t(thread->thread_id) > cpu_count)
        cpu->busy_ratio += elapsed;
    cpu->time_ratio += elapsed;

    // Normalize ratio to < 32768
    uint8_t time_scale = bit_msb_set(cpu->time_ratio);
    if (time_scale >= 15) {
        time_scale -= 14;
        cpu->time_ratio >>= time_scale;
        cpu->busy_ratio >>= time_scale;
    }

    if (likely(cpu->time_ratio)) {
        // 1032 == 1.032%
        int busy_percent = 100000 * cpu->busy_ratio / cpu->time_ratio;
        atomic_st_rel(&cpu->busy_percent_x1k, busy_percent);
    } else {
        atomic_st_rel(&cpu->busy_percent_x1k, 0);
    }

    thread_state_t state = atomic_ld_acq(&thread->state);

    // Change to ready if running
    if (likely(state == THREAD_IS_RUNNING))
        atomic_st_rel(&thread->state, THREAD_IS_READY_BUSY);

    // Retry because another CPU might steal this
    // thread after it transitions from sleeping to
    // ready
    int retries = 0;
    for ( ; ; ++retries) {
        thread = thread_choose_next(cpu, outgoing);

//        if (thread != outgoing)
//            printdbg("Switching to thread %#x\n", thread->thread_id);

        assert((thread >= threads + cpu_count &&
                thread < threads + countof(threads)) ||
               thread == threads + cpu->cpu_nr);

        if (thread == outgoing && thread->state == THREAD_IS_READY_BUSY) {
            // This doesn't need to be cmpxchg because the
            // outgoing thread is still marked busy
            atomic_st_rel(&thread->state, THREAD_IS_RUNNING);
            break;
        } else if (thread->state == THREAD_IS_READY &&
                atomic_cmpxchg(&thread->state,
                               THREAD_IS_READY, THREAD_IS_RUNNING) ==
                   THREAD_IS_READY) {

            bool to_slih = thread->thread_id >= thread_t(cpu_count) &&
                    thread->thread_id < thread_t(cpu_count * 2);

            if (to_slih)
                cpu->slih_return = outgoing->thread_id;

            break;
        }

        pause();
    }

    fpu_state_t& fpu_state = cpu->fpu_state;

    if (thread != outgoing) {
        //
        // FPU context switch save and/or restore, lockout/unlock

        unsigned from_cs = ISR_CTX_REG_CS(ctx);
        unsigned to_cs = ISR_CTX_REG_CS(thread->ctx);

        bool from_kern = GDT_SEL_RPL_IS_KERNEL(from_cs);
        bool to_kern = GDT_SEL_RPL_IS_KERNEL(to_cs);

        bool from_cs64 = GDT_SEL_IS_C64(from_cs);
        bool to_cs64 = GDT_SEL_IS_C64(to_cs);

        bool from_fpu = outgoing->thread_flags & THREAD_FLAGS_ANY_FPU;
        bool to_fpu = thread->thread_flags & THREAD_FLAGS_ANY_FPU;

        bool from_kern_fpu = outgoing->thread_flags & THREAD_FLAGS_KERNEL_FPU;
        bool to_kern_fpu = thread->thread_flags & THREAD_FLAGS_KERNEL_FPU;

        enum action_t { ignore, ctrlwords, zeros, dataregs };

        action_t save = ignore;

        if (from_fpu) {
            // Only save control words when fpu is discarded
            if (fpu_state == discarded)
                save = ctrlwords;

            // Don't save FPU again on nested context save
            else if (fpu_state == saved)
                save = ignore;

            // Possibly save FPU registers from kernel mode
            else if (!from_kern || from_kern_fpu)
                save = dataregs;
        }

        action_t restore = ignore;

        if (to_fpu) {
            // In decreasing order of likelihood
            if (!to_kern)
                restore = dataregs;
            else if (!to_kern_fpu)
                restore = ctrlwords;
            else
                restore = dataregs;
        }

        switch (save) {
        case dataregs:
            //
            // The outgoing FPU context needs to be saved

            THREAD_TRACE("Saving fpu context of thread %#x\n",
                         outgoing->thread_id);

            // Save it correctly
            // Don't bother saving FPU for voluntary context switch
            if (ISR_CTX_INTR(ctx) != INTR_THREAD_YIELD) {
                ISR_CTX_FPU(ctx) = from_cs64
                        ? isr_save_fpu_ctx64(outgoing)
                        : isr_save_fpu_ctx32(outgoing);
            } else {
                // Only save FPU control words, not data registers
                outgoing->syscall_mxcsr = cpu_mxcsr_get();
                outgoing->syscall_fcw87 = cpu_fcw_get();
                ISR_CTX_FPU(ctx) = nullptr;
            }

            fpu_state = saved;
            break;

        case ctrlwords:
            outgoing->syscall_mxcsr = cpu_mxcsr_get();
            outgoing->syscall_fcw87 = cpu_fcw_get();
            ISR_CTX_FPU(ctx) = nullptr;
            fpu_state = saved;
            break;

        case zeros:
        case ignore:
            assert(ISR_CTX_FPU(ctx) == nullptr);
            break;
        }

        switch (restore) {
        case dataregs:
            //
            // The incoming FPU context needs to be restored

            // If FPU is blocked...
            if (cpu->cr0_shadow & CPU_CR0_TS) {
                // Clear TS flag to unblock access to FPU
                cpu->cr0_shadow &= ~CPU_CR0_TS;
                cpu_cr0_clts();
            }

            THREAD_TRACE("Restoring fpu context of thread %#x\n",
                         thread->thread_id);

            if (ISR_CTX_FPU(thread->ctx)) {
                // Use the correct one
                if (to_cs64)
                    isr_restore_fpu_ctx64(thread);
                else
                    isr_restore_fpu_ctx32(thread);

            } else if (ISR_CTX_INTR(thread->ctx) == INTR_THREAD_YIELD) {
                // Restore just control words
                cpu_mxcsr_set(thread->syscall_mxcsr);
                cpu_fcw_set(thread->syscall_fcw87);
            }

            fpu_state = restored;
            break;

        case ctrlwords:
        case zeros:
            if (!(cpu->cr0_shadow & CPU_CR0_TS))
                cpu_clear_fpu();
            fpu_state = saved;
            break;

        case ignore:
            break;
        }

        if (to_fpu) {
            if (cpu->cr0_shadow & CPU_CR0_TS) {
                // Allow use of FPU
                printdbg("FPU allowed\n");
                cpu->cr0_shadow &= ~CPU_CR0_TS;
                cpu_cr0_clts();
            }
        } else if ((cpu->cr0_shadow & CPU_CR0_TS) == 0) {
            // Lock out the FPU
            // Set TS flag to block access to FPU
            printdbg("FPU locked out\n");
            cpu->cr0_shadow |= CPU_CR0_TS;
            cpu_cr0_set(cpu->cr0_shadow);
        }
    }

    thread->sched_timestamp = now;

    // Thread loses its priority boost when it is scheduled
    thread->priority_boost = 0;

    assert(thread->state == THREAD_IS_RUNNING);

    ctx = thread->ctx;
    thread->ctx = nullptr;
    assert(ctx != nullptr);
    atomic_st_rel(&cpu->cur_thread, thread);

    assert(ctx->gpr.s.r[0] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[1] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[2] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[3] == (GDT_SEL_USER_DATA | 3));

    if (thread != outgoing) {
        cpu->after_csw_fn = thread_clear_busy;
        cpu->after_csw_vp = outgoing;
    } else {
        assert(thread->state == THREAD_IS_RUNNING);
    }

    assert(ctx);

    return ctx;
}

static void thread_early_sleep(uint64_t timeout_time)
{
    while (time_ns() < timeout_time)
        halt();
}

EXPORT void thread_sleep_until(uint64_t timeout_time)
{
    if (thread_idle_ready) {
        thread_info_t *thread = this_thread();

        thread->wake_time = timeout_time;
        atomic_st_rel(&thread->state, THREAD_IS_SLEEPING_BUSY);
        thread_yield();
    } else {
        thread_early_sleep(timeout_time);
    }
}

EXPORT void thread_sleep_for(uint64_t ms)
{
    thread_sleep_until(time_ns() + ms * 1000000);
}

EXPORT uint64_t thread_get_usage(int id)
{
    if (unlikely(unsigned(id) >= unsigned(countof(threads))))
        return -1;

    thread_info_t *thread = id < 0 ? this_thread() : (threads + id);
    return thread->used_time;
}

// specify 0 in timeout_time for infinite timeout
uintptr_t thread_sleep_release(spinlock_t *lock, thread_t *thread_id,
                               int64_t timeout_time)
{
    thread_info_t *thread = this_thread();

    // Idle threads should never try to block and context switch!
    assert(thread->thread_id >= thread_t(cpu_count));

    *thread_id = thread->thread_id;

    thread->wake_time = timeout_time;

    thread_state_t old_state;
    old_state = atomic_cmpxchg(&thread->state,
                               THREAD_IS_RUNNING, THREAD_IS_SLEEPING_BUSY);
    assert(old_state == THREAD_IS_RUNNING);

    spinlock_unlock(lock);

    assert(thread_locks_held() == 0);
    uintptr_t result = thread_yield();
    assert(thread->state == THREAD_IS_RUNNING);

    //!!!not locked on return anymore!!! spinlock_lock(lock);

    return result;
}

_hot
EXPORT void thread_resume(thread_t tid, intptr_t exit_code)
{
    thread_info_t *thread = threads + tid;

    for (;;) {
        //THREAD_TRACE("Resuming %d\n", tid);

        if (thread->state != THREAD_IS_SLEEPING) {
            uint64_t wait_sleeping_st = time_ns();
            cpu_wait_value(&thread->state, THREAD_IS_SLEEPING);
            uint64_t wait_sleeping_en = time_ns();
            uint64_t wait_sleeping = wait_sleeping_en - wait_sleeping_st;
            printdbg("Waited %" PRIu64 "ns to wake thread from sleep\n",
                     wait_sleeping);
        }

        // Transition it to sleeping+busy so another cpu won't touch it
        if (thread->state == THREAD_IS_SLEEPING &&
            atomic_cmpxchg(&thread->state, THREAD_IS_SLEEPING,
                           THREAD_IS_SLEEPING_BUSY) == THREAD_IS_SLEEPING) {
            // Should be a fast, voluntarily yielded context
            assert(ISR_CTX_CTX_FLAGS(thread->ctx) &
                   (1<<ISR_CTX_CTX_FLAGS_FAST_BIT));

            // Set return value
            ISR_CTX_ERRCODE(thread->ctx) = exit_code;

            // If its home is another CPU
            auto this_tid = this_thread()->thread_id;

            // Done manipulating it, mark it ready
            atomic_st_rel(&thread->state, THREAD_IS_READY);

            // Kick the home CPU
            if (run_cpu[this_tid] != run_cpu[tid])
                apic_send_ipi(cpus[run_cpu[tid]].apic_id, INTR_IPI_RESCHED);

            return;
        }

        if (thread->state == THREAD_IS_SLEEPING_BUSY &&
                atomic_cmpxchg(&thread->state, THREAD_IS_SLEEPING_BUSY,
                               THREAD_IS_READY_BUSY)) {
            return;
        }

        THREAD_TRACE("Did not resume %d! Retrying I guess\n", tid);
    }
}

EXPORT int thread_wait(thread_t thread_id)
{
    thread_info_t *thread = threads + thread_id;
    std::unique_lock<std::mutex> mutex_lock(thread->lock);
    while (thread->state != THREAD_IS_FINISHED)
        thread->done_cond.wait(mutex_lock);
    return thread->exit_code;
}

uint32_t thread_cpus_started()
{
    return thread_smp_running + 1;
}

EXPORT thread_t thread_get_id()
{
    if (thread_cls_ready) {
        thread_t thread_id;

        thread_info_t *cur_thread = this_thread();
        thread_id = cur_thread->thread_id;

        return thread_id;
    }

    // Too early to get a thread ID
    return 0;
}

EXPORT void thread_set_gsbase(thread_t tid, uintptr_t gsbase)
{
    cpu_info_t *cpu = this_cpu();
    if (tid < 0)
        tid = cpu->cur_thread->thread_id;
    if (uintptr_t(tid) < countof(threads))
        threads[tid].gsbase = (void*)gsbase;
}

EXPORT void thread_set_fsbase(thread_t tid, uintptr_t fsbase)
{
    cpu_info_t *cpu = this_cpu();
    int cur_tid = cpu->cur_thread->thread_id;
    if (tid < 0)
        tid = cpu->cur_thread->thread_id;
    if (uintptr_t(tid) < countof(threads))
        threads[tid].fsbase = (void*)fsbase;
    if (cur_tid == tid)
        cpu_fsbase_set((void*)fsbase);
}

EXPORT thread_cpu_mask_t const* thread_get_affinity(int id)
{
    return &threads[id].cpu_affinity;
}

EXPORT size_t thread_get_cpu_count()
{
    return cpu_count;
}

EXPORT void thread_set_affinity(int id, thread_cpu_mask_t const &affinity)
{
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    size_t cpu_nr = cpu->cpu_nr;

    threads[id].cpu_affinity = affinity;

    if ((affinity[run_cpu[id]]) == false) {
        // Home CPU is not in the affinity mask
        // Move home to a cpu in the affinity mask
        run_cpu[id] = affinity.lsb_set();
    }

    // Are we changing current thread affinity?
    if (cpu->cur_thread->thread_id == id) {
        while (!(affinity[cpu_nr])) {
            // Get off this CPU
            thread_yield();

            // Check again, a racing CPU may have picked
            // up this thread without seeing change
            cpu = this_cpu();
            cpu_nr = cpu->cpu_nr;
        }
    }
}

EXPORT thread_priority_t thread_get_priority(thread_t thread_id)
{
    return threads[thread_id].priority;
}

EXPORT void thread_set_priority(thread_t thread_id,
                                thread_priority_t priority)
{
    threads[thread_id].priority = priority;
}

void thread_check_stack()
{
    thread_info_t *thread = this_thread();

    char *sp = (char*)cpu_stack_ptr_get();

    if (sp > thread->stack ||
        sp < thread->stack - thread->stack_size)
    {
        cpu_debug_break();
        cpu_crash();
    }
}

void thread_idle_set_ready()
{
    thread_idle_ready = 1;
}

void *thread_get_exception_top()
{
    // Ensure that threading is initialized, in case of early exception
    if (likely(thread_get_cpu_count())) {
        thread_info_t *thread = this_thread();
        return thread->exception_chain;
    }
    return nullptr;
}

void *thread_set_exception_top(void *chain)
{
    thread_info_t *thread = this_thread();
    void *old = thread->exception_chain;
    thread->exception_chain = chain;
    return old;
}

size_t thread_cls_alloc()
{
    return atomic_xadd(&storage_next_slot, 1);
}

void *thread_cls_get(size_t slot)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->storage[slot];
}

void thread_cls_set(size_t slot, void *value)
{
    cpu_info_t *cpu = this_cpu();
    cpu->storage[slot] = value;
}

void thread_cls_init_each_cpu(
        size_t slot, thread_cls_init_handler_t handler, void *arg)
{
    for (size_t c = 0; c < cpu_count; ++c)
        cpus[c].storage[slot] = handler(arg);
}

void thread_cls_for_each_cpu(
        size_t slot, int other_only,
        thread_cls_each_handler_t handler, void *arg, size_t size)
{
    cpu_info_t *cpu = other_only ? this_cpu() : nullptr;
    for (size_t c = 0; c < cpu_count; ++c) {
        if (cpus + c != cpu)
            handler(c, cpus[c].storage[slot], arg, size);
    }
}

void thread_send_ipi(int cpu, int intr)
{
    assert(cpu >= 0 && (unsigned)cpu < cpu_count);
    apic_send_ipi(cpus[cpu].apic_id, intr);
}

EXPORT int thread_cpu_number()
{
    cpu_info_t *cpu = this_cpu();
    return cpu->cpu_nr;
}

_hot
EXPORT isr_context_t *thread_schedule_postirq(isr_context_t *ctx)
{
    cpu_info_t *cur_cpu = this_cpu();
    thread_info_t *cur_thread = cur_cpu->cur_thread;
    unsigned tid = cur_thread->thread_id;

    // If idle thread was interrupted,
    // or the SLIH thread is ready and the SLIH thread wasn't running already
    if ((thread_idle_ready && tid < cpu_count) ||
            ((threads[cpu_count + cur_cpu->cpu_nr].state == THREAD_IS_READY) &&
             tid != cpu_count + cur_cpu->cpu_nr))
        return thread_schedule(ctx);

    return ctx;
}

EXPORT process_t *thread_current_process()
{
    thread_info_t *thread = this_thread();
    return thread->process;
}

EXPORT unsigned thread_current_cpu(thread_t tid)
{
    cpu_info_t *cpu = tid < 0 ? this_cpu() : &cpus[run_cpu[tid]];
    return cpu->cpu_nr;
}

uint32_t thread_get_cpu_apic_id(int cpu)
{
    return cpus[cpu].apic_id;
}

uint64_t thread_shootdown_count(int cpu_nr)
{
    cpu_info_t const *cpu = cpus + cpu_nr;
    return cpu->tlb_shootdown_count;
}

void thread_shootdown_notify()
{
    cpu_info_t *cpu = this_cpu();
    atomic_inc(&cpu->tlb_shootdown_count);
}

void *thread_get_fsbase(int thread)
{
    if (cpu_count) {
        thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
        return info->fsbase;
    }
    return nullptr;
}

void *thread_get_gsbase(int thread)
{
    if (cpu_count) {
        thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
        return info->gsbase;
    }
    return nullptr;
}

void thread_set_process(int thread, process_t *process)
{
    thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
    info->process = process;
}

void thread_tss_ready(void*)
{
    cpus[0].tss_ptr = tss_list;
}

REGISTER_CALLOUT(thread_tss_ready, nullptr,
                 callout_type_t::tss_list_ready, "000");

void thread_terminate(thread_t tid)
{
}

void thread_exit(int exit_code)
{
    thread_info_t *info = this_thread();
    thread_t tid = info->thread_id;
    info->exit_code = exit_code;
    info->process->del_thread(tid);
    thread_cleanup();
}

cpu_info_t *thread_set_cpu_gsbase(int ap)
{
    cpu_info_t *cpu = ap ? this_cpu_by_apic_id_slow() : cpus;
    cpu_gsbase_set(cpu);
    return cpu;
}

void thread_init_cpu_count(int count)
{
    for (size_t i = 0, e = countof(run_cpu); i != e; ++i)
        run_cpu[i] = i % count;
}

void thread_init_cpu(size_t cpu_nr, uint32_t apic_id)
{
    cpu_info_t *cpu = cpus + cpu_nr;
    cpu->self = cpu;
    cpu->apic_id = apic_id;
    cpu->cpu_nr = cpu_nr;
    cpu->cur_thread = threads + cpu_nr;
}

// PCID address space is 4096 bits
//  0 bits are free pcids, 1 bits are taken pcids
//  4096 is 64*64, so represent it with a 2-level hierarchy
//  The underlying maps are in pcid_alloc_map 1 thru 64 inclusive
//  The top map is stored in pcid_alloc_map[0]
//  The top map will have 1 bits if all of the underlying 64 bits are 1
//  The top map will have 0 bits if any of the underlying 64 bits are 0
static uint64_t pcid_alloc_map[65];

int thread_pcid_alloc()
{
    // The top map will be all 1 bits when all pcids are taken
    if (unlikely(~pcid_alloc_map[0] == 0))
        return -1;

    // Find the first qword with a 0 bit
    size_t word = bit_lsb_set(~pcid_alloc_map[0]);

    // Find the first 0 bit in that qword
    uint8_t bit = bit_lsb_set(~pcid_alloc_map[word+1]);

    uint64_t upd = pcid_alloc_map[word+1] | (UINT64_C(1) << bit);

    pcid_alloc_map[word+1] = upd;

    // Build a mask that will set the top bit to 1
    // if all underlying bits are now 1
    uint64_t top_mask = (UINT64_C(1) << word) & -(~upd == 0);

    pcid_alloc_map[0] |= top_mask;

    return int(word << 6) + bit;
}

void thread_pcid_free(int pcid)
{
    assert(size_t(pcid) < 4096);

    size_t word = unsigned(pcid) >> 6;

    uint8_t bit = word & 63;

    // Clear that bit
    pcid_alloc_map[word+1] &= ~(UINT64_C(1) << bit);

    // Since we freed one, we know that the bit of level 0 must become 0
    pcid_alloc_map[0] &= ~(UINT64_C(1) << word);
}

EXPORT unsigned thread_cpu_usage_x1k(size_t cpu)
{
    return 100000 - atomic_ld_acq(&cpus[cpu].busy_percent_x1k);
}

void thread_add_cpu_irq_time(uint64_t tsc_ticks)
{
    atomic_add((cpu_gs_ptr<uint64_t, offsetof(cpu_info_t, irq_time)>()),
               tsc_ticks);
}

//void thread_info_t::awaken(cpu_info_t *cpu, uint64_t now)
//{
//    // If thread has nothing, then replenish timeslice
//    if (timeslice_remaining == 0) {
//        timeslice_timestamp = now;
//        timeslice_remaining = 16666666;
//    }

//    // If enough time has elapsed for the thread to have gotten another
//    // timeslice by now, and the thread doesn't have a full timeslice,
//    // then add a full timeslice on top of what they had
//    // This gives enough to catch up if they slept less than one timeslice
//    if (now >= timeslice_timestamp + 16666666 &&
//            timeslice_remaining < 16666666) {
//        timeslice_remaining += 16666666;
//    }

//    // Find insertion point
//    link_t *ins;
//    for (ins = cpu->ready_list.next; ins != &cpu->ready_list;
//         ins = ins->next) {
//        auto thread = ins->container<thread_info_t>(ins);
//        if (thread->timeslice_timestamp > timeslice_timestamp)
//            break;
//    }

//    link.prev = ins->prev;
//    link.next = ins;
//    link.prev->next = &link;
//    link.next->prev = &link;
//}

void thread_cls_init_early(int ap)
{
    cpu_info_t *cpu = thread_set_cpu_gsbase(ap);
    cpu->self = cpu;
    cpu->cpu_nr = cpu - cpus;
}

uint32_t thread_locks_held()
{
    cpu_info_t *cpu = this_cpu();
    return cpu->locks_held;
}

__cxa_eh_globals *thread_cxa_get_globals()
{
    return &this_thread()->cxx_exception_info;
}
