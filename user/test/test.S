#include <sys/syscall_num.h>

.code64
.section .text, "ax", @progbits

.balign 16
.global main
main:
    .cfi_startproc
    .cfi_undefined rip
    .cfi_undefined rbx
    .cfi_undefined rbp
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    xor %eax,%eax
    rdtscp
    shl $ 32,%rdx
    or %rdx,%rax
    mov %rax,%r13
    mov $ SYS_getpriority,%ebx
    mov $ 1000000,%r12d
.balign 16
0:	mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    mov %ebx,%eax
    syscall

    dec %r12d
    jnz 0b

    xor %eax,%eax
    rdtscp
    shl $ 32,%rdx
    or %rdx,%rax
    sub %r13,%rax
    mov %rax,%rdi
    mov $ SYS_exit,%eax
    syscall
    ud2
    .cfi_endproc

test_nanosleep:
    push $ 0
    push $ 0
0:	mov $ 35,%eax
    mov %rsp,%rdi
    xor %rsi,%rsi
    syscall
    jmp 0b

