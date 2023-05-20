	.file	"lol4.c"
	.text
	.p2align 4
	.globl	move
	.type	move, @function
move:
.LFB11:
	.cfi_startproc
	subl	$1, %esi
	movq	%rdi, %rcx
	leal	-1(%rdx), %r10d
	movl	%edx, %r8d
	movl	%esi, %r9d
	je	.L2
	movl	%esi, %edx
	xorl	%eax, %eax
	addq	%rdi, %rdx
	.p2align 4,,10
	.p2align 3
.L3:
	movzbl	(%rcx,%rax), %esi
	movzbl	(%rdx), %edi
	subq	$1, %rdx
	movb	%dil, (%rcx,%rax)
	addq	$1, %rax
	movb	%sil, 1(%rdx)
	movl	%r9d, %esi
	subl	%eax, %esi
	cmpl	%esi, %eax
	jb	.L3
	testl	%r10d, %r10d
	je	.L9
.L5:
	movl	%r10d, %edx
	xorl	%eax, %eax
	addq	%rcx, %rdx
	.p2align 4,,10
	.p2align 3
.L6:
	movzbl	(%rcx,%rax), %esi
	movzbl	(%rdx), %edi
	subq	$1, %rdx
	movb	%dil, (%rcx,%rax)
	addq	$1, %rax
	movb	%sil, 1(%rdx)
	movl	%r10d, %esi
	subl	%eax, %esi
	cmpl	%esi, %eax
	jb	.L6
.L9:
	cmpl	%r9d, %r8d
	jnb	.L17
	movl	%r8d, %eax
	movl	%r9d, %edx
	addq	%rcx, %rax
	addq	%rcx, %rdx
	leal	(%r9,%rax), %edi
	subl	%eax, %r8d
	.p2align 4,,10
	.p2align 3
.L10:
	movzbl	(%rax), %ecx
	movzbl	(%rdx), %esi
	addq	$1, %rax
	subq	$1, %rdx
	movb	%sil, -1(%rax)
	leal	(%r8,%rax), %esi
	movb	%cl, 1(%rdx)
	movl	%edi, %ecx
	subl	%eax, %ecx
	cmpl	%ecx, %esi
	jb	.L10
	ret
	.p2align 4,,10
	.p2align 3
.L2:
	cmpl	$1, %edx
	jne	.L5
	ret
	.p2align 4,,10
	.p2align 3
.L17:
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
	jne	.L21
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
.L21:
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
