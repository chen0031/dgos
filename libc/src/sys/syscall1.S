.section .text, "ax", @progbits

//scp_t syscall1(scp_t p0, scn_t num);
.balign 16
.type syscall1,@function
.global syscall1
syscall1:
	.cfi_startproc

#ifdef __x86_64__

	mov %esi,%eax

#else

	movl 4+8*1(%esp),%eax

	movq 4+8*0(%esp),%xmm0

#endif

	syscall
	ret

	.cfi_endproc
