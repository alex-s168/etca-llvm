	.file	"test_crash.ll"
	.text
	.globl	test_redefine_r0                ; -- Begin function test_redefine_r0
	.p2align	1
	.type	test_redefine_r0,@function
test_redefine_r0:                       ; @test_redefine_r0
; %bb.0:                                ; %entry
	push r5
	movz r5, r6
	sub r6, 4
	store r3, r5                            ; 2-byte Folded Spill
	store r4, r5                            ; 2-byte Folded Spill
	movz r1, r0
	movz r4, 1
	movz r3, r1
	add r3, r4
	movz r0, r3
	call helper
	movz r2, r0
	add r2, r1
	movz r0, r2
	jmpr r7
.Lfunc_end0:
	.size	test_redefine_r0, .Lfunc_end0-test_redefine_r0
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
