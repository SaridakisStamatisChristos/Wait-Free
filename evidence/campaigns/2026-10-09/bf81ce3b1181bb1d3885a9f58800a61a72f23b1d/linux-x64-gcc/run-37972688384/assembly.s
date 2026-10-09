	.file	"assembly_probe.cpp"
	.text
	.p2align 4
	.globl	veriqueue_probe_push
	.type	veriqueue_probe_push, @function
veriqueue_probe_push:
.LFB5607:
	.cfi_startproc
	endbr64
	movq	(%rdi), %rax
	movq	%rax, %rdx
	subq	8(%rdi), %rdx
	cmpq	$1024, %rdx
	je	.L2
.L4:
	movq	%rax, %rdx
	addq	$1, %rax
	andl	$1023, %edx
	movq	%rsi, 128(%rdi,%rdx,8)
	movq	%rax, (%rdi)
	movq	%rax, 16(%rdi)
	movl	$1, %eax
	ret
	.p2align 4,,10
	.p2align 3
.L2:
	movq	80(%rdi), %rdx
	movq	%rax, %rcx
	subq	%rdx, %rcx
	movq	%rdx, 8(%rdi)
	cmpq	$1024, %rcx
	jne	.L4
	xorl	%eax, %eax
	ret
	.cfi_endproc
.LFE5607:
	.size	veriqueue_probe_push, .-veriqueue_probe_push
	.p2align 4
	.globl	veriqueue_probe_pop
	.type	veriqueue_probe_pop, @function
veriqueue_probe_pop:
.LFB5609:
	.cfi_startproc
	endbr64
	movq	64(%rdi), %rax
	cmpq	72(%rdi), %rax
	je	.L7
.L9:
	movq	%rax, %rdx
	addq	$1, %rax
	andl	$1023, %edx
	movq	128(%rdi,%rdx,8), %rdx
	movq	%rdx, (%rsi)
	movq	%rax, 64(%rdi)
	movq	%rax, 80(%rdi)
	movl	$1, %eax
	ret
	.p2align 4,,10
	.p2align 3
.L7:
	movq	16(%rdi), %rdx
	movq	%rdx, 72(%rdi)
	cmpq	%rdx, %rax
	jne	.L9
	xorl	%eax, %eax
	ret
	.cfi_endproc
.LFE5609:
	.size	veriqueue_probe_pop, .-veriqueue_probe_pop
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
