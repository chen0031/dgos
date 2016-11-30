#include "main.h"
#include "cpu.h"
#include "mm.h"
#include "printk.h"
#include "cpu/halt.h"
#include "thread.h"
#include "device/pci.h"
#include "device/keyb8042.h"
#include "callout.h"
#include "rbtree.h"

int life_and_stuff = 42;

size_t const kernel_stack_size = 8192;
char kernel_stack[262144];

char buf[12];

extern void (*device_constructor_list[])(void);

__thread int tls_thing;
//__thread int tls_thing_2;
__thread int tls_initialized_thing = 42;
__thread int tls_initialized_thing_2 = 43;
//__thread char tls_buf[10] = {24};

void volatile *trick;

static void smp_main(void *arg)
{
    (void)arg;
    cpu_init(1);
}

REGISTER_CALLOUT(smp_main, 0, 'S', "999");

static __attribute__((constructor)) void start_me_up()
{
    tls_thing = 1;
    //tls_thing_2 = 23;
    tls_initialized_thing = 2;
    tls_initialized_thing_2 = 3;
    trick = &tls_initialized_thing;
}

static __attribute__((destructor)) void goin_down()
{
    //tls_thing = -22;
}

// Pull in the device constructors
// to cause them to be initialized
void (** volatile device_list)(void) = device_constructor_list;

#define TEST_FORMAT(f, t, v) \
    printk("Test %8s -> '" f \
    "' 99=%d\t\t", f, (t)v, 99)

char shell_stack[4096];

static int shell_thread(void *p)
{
    printk("From shell thread!! %016lx", (uint64_t)p);
    while (1) {
        ++*(char*)0xb8002;
        thread_sleep_for(1000);
    }
    return 0;
}

//char test_stacks[4][4096];

typedef struct test_thread_param_t {
    char *p;
    int sleep;
} test_thread_param_t;

static int other_thread(void *p)
{
    test_thread_param_t *tp = p;
    while (1) {
        ++(*tp->p);
        thread_sleep_for(tp->sleep);
    }
    return 0;
}

int main(void)
{
    pci_init();
    keyb8042_init();

    //rbtree_test();

    // Crash
    //*(uint64_t*)0xfeedbeefbaadf00d = 0;

    thread_create(shell_thread, (void*)0xfeedbeeffacef00d,
                  shell_stack, sizeof(shell_stack));

    test_thread_param_t ttp[4] = {
        { (char*)0xb8004, 953 },
        { (char*)0xb8006, 701 },
        { (char*)0xb8008, 556 },
        { (char*)0xb800A, 299 }
    };

    char *test_stacks = mmap(
                0, 4096 * 4,
                PROT_READ | PROT_WRITE,
                MAP_STACK, -1, 0);

    thread_create(other_thread, ttp + 0, test_stacks + (0 << 12), 4096);
    thread_create(other_thread, ttp + 1, test_stacks + (1 << 12), 4096);
    thread_create(other_thread, ttp + 2, test_stacks + (2 << 12), 4096);
    thread_create(other_thread, ttp + 3, test_stacks + (3 << 12), 4096);

    while (1)
        halt();

    return 0;
}
