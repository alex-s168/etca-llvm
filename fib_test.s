	.file	"fib_test.ll"
	.text
	.globl	fib                             ; -- Begin function fib
	.p2align	1
	.type	fib,@function
fib:                                    ; @fib
; %bb.0:                                ; %entry
	push r5
	movz r5, r6
	sub r6, 2
	store r3, r5                            ; 2-byte Folded Spill
	movz r1, 2
	cmp r0, r1
	bltu .LBB0_2
	br .LBB0_1
.LBB0_1:                                ; %recurse
	movz r2, 1
	sub r2, r0
	movz r3, r0
	movz r0, r2
	call fib
	movz r2, r0
	sub r1, r3
	movz r0, r1
	call fib
	add r0, r2
.LBB0_2:                                ; %return
	jmpr r7
.Lfunc_end0:
	.size	fib, .Lfunc_end0-fib
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
