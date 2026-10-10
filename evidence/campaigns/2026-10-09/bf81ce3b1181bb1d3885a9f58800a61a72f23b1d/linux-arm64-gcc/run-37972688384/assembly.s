	.arch armv8-a
	.file	"assembly_probe.cpp"
	.text
	.align	2
	.p2align 4,,11
	.global	veriqueue_probe_push
	.type	veriqueue_probe_push, %function
veriqueue_probe_push:
.LFB5608:
	.cfi_startproc
	ldp	x2, x3, [x0]
	sub	x3, x2, x3
	cmp	x3, 1024
	beq	.L2
.L4:
	ubfiz	x4, x2, 3, 10
	mov	x3, x0
	add	x0, x0, x4
	add	x2, x2, 1
	str	x1, [x0, 128]
	str	x2, [x3], 16
	stlr	x2, [x3]
	mov	w0, 1
	ret
	.p2align 2,,3
.L2:
	add	x3, x0, 80
	ldar	x3, [x3]
	sub	x4, x2, x3
	str	x3, [x0, 8]
	cmp	x4, 1024
	bne	.L4
	mov	w0, 0
	ret
	.cfi_endproc
.LFE5608:
	.size	veriqueue_probe_push, .-veriqueue_probe_push
	.align	2
	.p2align 4,,11
	.global	veriqueue_probe_pop
	.type	veriqueue_probe_pop, %function
veriqueue_probe_pop:
.LFB5610:
	.cfi_startproc
	ldp	x2, x3, [x0, 64]
	cmp	x2, x3
	beq	.L7
.L9:
	ubfiz	x3, x2, 3, 10
	add	x2, x2, 1
	add	x3, x3, 128
	ldr	x3, [x0, x3]
	str	x3, [x1]
	str	x2, [x0, 64]
	add	x0, x0, 80
	stlr	x2, [x0]
	mov	w0, 1
	ret
	.p2align 2,,3
.L7:
	add	x3, x0, 16
	ldar	x3, [x3]
	str	x3, [x0, 72]
	cmp	x2, x3
	bne	.L9
	mov	w0, 0
	ret
	.cfi_endproc
.LFE5610:
	.size	veriqueue_probe_pop, .-veriqueue_probe_pop
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
