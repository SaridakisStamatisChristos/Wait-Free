	.text
	.file	"assembly_probe.cpp"
	.globl	veriqueue_probe_push            # -- Begin function veriqueue_probe_push
	.p2align	4, 0x90
	.type	veriqueue_probe_push,@function
veriqueue_probe_push:                   # @veriqueue_probe_push
	.cfi_startproc
# %bb.0:
	movq	(%rdi), %rax
	movq	%rax, %rcx
	subq	8(%rdi), %rcx
	cmpq	$1024, %rcx                     # imm = 0x400
	jne	.LBB0_3
# %bb.1:
	movq	80(%rdi), %rcx
	movq	%rcx, 8(%rdi)
	movq	%rax, %rdx
	subq	%rcx, %rdx
	cmpq	$1024, %rdx                     # imm = 0x400
	jne	.LBB0_3
# %bb.2:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB0_3:
	movl	%eax, %ecx
	andl	$1023, %ecx                     # imm = 0x3FF
	movq	%rsi, 128(%rdi,%rcx,8)
	incq	%rax
	movq	%rax, (%rdi)
	movq	%rax, 16(%rdi)
	movb	$1, %al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end0:
	.size	veriqueue_probe_push, .Lfunc_end0-veriqueue_probe_push
	.cfi_endproc
                                        # -- End function
	.globl	veriqueue_probe_pop             # -- Begin function veriqueue_probe_pop
	.p2align	4, 0x90
	.type	veriqueue_probe_pop,@function
veriqueue_probe_pop:                    # @veriqueue_probe_pop
	.cfi_startproc
# %bb.0:
	movq	64(%rdi), %rax
	cmpq	72(%rdi), %rax
	jne	.LBB1_3
# %bb.1:
	movq	16(%rdi), %rcx
	movq	%rcx, 72(%rdi)
	cmpq	%rcx, %rax
	jne	.LBB1_3
# %bb.2:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB1_3:
	movl	%eax, %ecx
	andl	$1023, %ecx                     # imm = 0x3FF
	movq	128(%rdi,%rcx,8), %rcx
	movq	%rcx, (%rsi)
	incq	%rax
	movq	%rax, 64(%rdi)
	movq	%rax, 80(%rdi)
	movb	$1, %al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end1:
	.size	veriqueue_probe_pop, .Lfunc_end1-veriqueue_probe_pop
	.cfi_endproc
                                        # -- End function
	.section	".linker-options","e",@llvm_linker_options
	.ident	"Ubuntu clang version 18.1.3 (1ubuntu1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __gxx_personality_v0
