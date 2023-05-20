	.file	"lol.c"
	.text
	.p2align 4
	.globl	move
	.type	move, @function
move:
.LFB11:
	.cfi_startproc
	cmpl	%esi, %edx
	jnb	.L15
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	movq	%rdi, %r9
	.p2align 4,,10
	.p2align 3
.L2:
	movl	%esi, %eax
	leal	(%rdx,%rdx), %ecx
	subl	%edx, %eax
	cmpl	%ecx, %esi
	jb	.L8
	movl	%eax, %esi
.L3:
	movl	%edx, %ebx
	movl	%esi, %r10d
	movq	%r9, %rax
	leaq	(%r9,%rbx), %r11
	subl	%r9d, %r10d
	testl	%edx, %edx
	je	.L7
	.p2align 4,,10
	.p2align 3
.L6:
	leal	(%r10,%rax), %ecx
	movzbl	(%rax), %edi
	addq	$1, %rax
	addq	%r9, %rcx
	movzbl	(%rcx), %r8d
	movb	%r8b, -1(%rax)
	movb	%dil, (%rcx)
	cmpq	%rax, %r11
	jne	.L6
.L7:
	addq	%rbx, %r9
	cmpl	%esi, %edx
	jb	.L2
	xorl	%eax, %eax
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L8:
	.cfi_restore_state
	movl	%edx, %esi
	movl	%eax, %edx
	jmp	.L3
.L15:
	.cfi_def_cfa_offset 8
	.cfi_restore 3
	xorl	%eax, %eax
	ret
	.cfi_endproc
.LFE11:
	.size	move, .-move
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"Before: %s\n"
.LC1:
	.string	"After:  %s\n"
	.section	.text.startup,"ax",@progbits
	.p2align 4
	.globl	main
	.type	main, @function
main:
.LFB12:
	.cfi_startproc
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	leaq	.LC0(%rip), %r12
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	movq	%r12, %rdi
	leaq	.LC1(%rip), %rbp
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	subq	$48, %rsp
	.cfi_def_cfa_offset 80
	movdqa	.LC2(%rip), %xmm0
	movq	%fs:40, %rax
	movq	%rax, 40(%rsp)
	xorl	%eax, %eax
	movq	%rsp, %rbx
	movabsq	$17850709461592429, %rax
	movaps	%xmm0, (%rsp)
	movdqa	.LC3(%rip), %xmm0
	movq	%rbx, %rsi
	movq	%rax, 32(%rsp)
	xorl	%eax, %eax
	movaps	%xmm0, 16(%rsp)
	call	printf@PLT
	movq	%rbx, %rdi
	call	strlen@PLT
	movl	$3, %edx
	leaq	2(%rsp), %rdi
	leal	-6(%rax), %esi
	call	move
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	%rbx, %rsi
	movq	%r12, %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	%rbx, %rdi
	call	strlen@PLT
	movl	$4, %edx
	leaq	6(%rsp), %rdi
	leal	-6(%rax), %esi
	call	move
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	%rbx, %rsi
	movq	%r12, %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	%rbx, %rdi
	call	strlen@PLT
	movq	%rbx, %rdi
	movl	$2, %edx
	leal	-10(%rax), %esi
	call	move
	xorl	%eax, %eax
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	call	printf@PLT
	movq	40(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L22
	addq	$48, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 32
	xorl	%eax, %eax
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%rbp
	.cfi_def_cfa_offset 16
	popq	%r12
	.cfi_def_cfa_offset 8
	ret
.L22:
	.cfi_restore_state
	call	__stack_chk_fail@PLT
	.cfi_endproc
.LFE12:
	.size	main, .-main
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC2:
	.quad	8031924123371070792
	.quad	2340021466346843250
	.align 16
.LC3:
	.quad	2338328219396956526
	.quad	8809357996311470657
	.ident	"GCC: (GNU) 13.1.1 20230429"
	.section	.note.GNU-stack,"",@progbits
