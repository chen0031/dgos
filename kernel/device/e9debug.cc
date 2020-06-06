#include "e9debug.h"
#include "debug.h"
#include "cpu/ioport.h"
#include "printk.h"
#include "serial-uart.h"
#include "callout.h"
#include "string.h"
#include "cpu/spinlock.h"
#include "bootinfo.h"

#include "cpu/cpuid.h"

static spinlock_t e9debug_lock;

static uart_dev_t *uart;
static bool uart_ready;

static char vt102_reset[] = "\x1B" "c";

_constructor(ctor_ctors_ran) static void e9debug_serial_ready(void*)
{
    // HACK! Enable bochs debugger hardware
    outw(0x8A00, 0x8A00);

    if (bootinfo_parameter(bootparam_t::boot_serial_log)) {
        printk("Enabling serial debug output\n");
        uart = uart_dev_t::open(0, true, 8, 'N', 1);
        uart_ready = true;

        // Send some VT102 initialization sequences
        uart->write(vt102_reset, sizeof(vt102_reset)-1);
    } else {
        uart_ready = false;
    }
}

static int e9debug_write_debug_str(char const *str, intptr_t len)
{
    int n = 0;

    if (!len)
        len = strlen(str);

    if (bootinfo_parameter(bootparam_t::port_e9_debug)) {
        if (len > 1 && str) {
            outsb(0xE9, str, len);
            n = len;
        } else if (len == 1 && str) {
            outb(0xE9, str[0]);
            n = 1;
        } else if (str) {
            while (*str) {
                outb(0xE9, *str++);
                ++n;
            }
        }
    }

    if (uart_ready)
        uart->write(str, len);

    return n;
}

void e9debug_init(void)
{
    write_debug_str_set_handler(e9debug_write_debug_str);
    printdbg("----------------------------------------------------\n"
             "DebugLog Started...\n"
             "----------------------------------------------------\n");

    printdbg("Booting on %s\n", cpuid_brand());
}

//REGISTER_CALLOUT(e9debug_serial_ready, nullptr,
//                 callout_type_t::constructors_ran, "000");
