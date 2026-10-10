	.text
	.file	"assembly_probe.cpp"
	.globl	veriqueue_probe_push            // -- Begin function veriqueue_probe_push
	.p2align	2
	.type	veriqueue_probe_push,@function
veriqueue_probe_push:                   // @veriqueue_probe_push
	.cfi_startproc
// %bb.0:
	ldp	x8, x9, [x0]
	sub	x9, x8, x9
	cmp	x9, #1024
	b.ne	.LBB0_3
// %bb.1:
	add	x9, x0, #80
	ldar	x9, [x9]
	sub	x10, x8, x9
	str	x9, [x0, #8]
	cmp	x10, #1024
	b.ne	.LBB0_3
// %bb.2:
	mov	w0, wzr
	ret
.LBB0_3:
	and	x9, x8, #0x3ff
	add	x8, x8, #1
	add	x9, x0, x9, lsl #3
	str	x8, [x0], #16
	str	x1, [x9, #128]
	stlr	x8, [x0]
	mov	w0, #1                          // =0x1
	ret
.Lfunc_end0:
	.size	veriqueue_probe_push, .Lfunc_end0-veriqueue_probe_push
	.cfi_endproc
                                        // -- End function
	.globl	veriqueue_probe_pop             // -- Begin function veriqueue_probe_pop
	.p2align	2
	.type	veriqueue_probe_pop,@function
veriqueue_probe_pop:                    // @veriqueue_probe_pop
	.cfi_startproc
// %bb.0:
	ldp	x8, x9, [x0, #64]
	cmp	x8, x9
	b.ne	.LBB1_3
// %bb.1:
	add	x9, x0, #16
	ldar	x9, [x9]
	cmp	x8, x9
	str	x9, [x0, #72]
	b.ne	.LBB1_3
// %bb.2:
	mov	w0, wzr
	ret
.LBB1_3:
	and	x9, x8, #0x3ff
	add	x8, x8, #1
	add	x9, x0, x9, lsl #3
	ldr	x9, [x9, #128]
	str	x9, [x1]
	add	x9, x0, #80
	str	x8, [x0, #64]
	mov	w0, #1                          // =0x1
	stlr	x8, [x9]
	ret
.Lfunc_end1:
	.size	veriqueue_probe_pop, .Lfunc_end1-veriqueue_probe_pop
	.cfi_endproc
                                        // -- End function
	.section	".linker-options","e",@llvm_linker_options
	.ident	"Ubuntu clang version 18.1.3 (1ubuntu1)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __gxx_personality_v0
