; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=GEN
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32,+dw < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s --check-prefix=QW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw < %s | FileCheck %s --check-prefix=QW

;; ===========================================================================
;; ETCA strcpy codegen tests
;;
;; Tests that strcpy-style byte-per-byte pointer copy loops compile correctly,
;; including pointer loads/stores (p0 type), byte loads/stores (s8 type), and
;; pointer arithmetic via G_PTR_ADD.
;;
;; These exercise the G_STORE/G_LOAD legalizer for pointer types (p0), which
;; must be accepted alongside scalar types (s8/s16/s32/s64) to handle alloca-
;; based code where pointer values are stored to memory.
;;
;; On generic (16-bit), s8 loads/stores are widened to s16; the instruction
;; selector selects LOAD8/STORE8 independently.  On DW/QW systems, the
;; s32/s64 offset type for G_PTR_ADD must also be legal for pointer
;; arithmetic to be legalizable.
;; ===========================================================================

;; --- Test 1: store+load pointer values (p0), then return ---
;; Use the loaded value to prevent dead-code elimination from removing
;; the load.
define ptr @store_load_ptr(ptr %addr, ptr %val) {
; GEN-LABEL: store_load_ptr:
; GEN:       store %r1, %r0
; GEN:       load %r0, %r0
; GEN:       jmpr %r7
;
; DW-LABEL: store_load_ptr:
; DW:       store %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; DW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; DW:       jmpr %r7
;
; QW-LABEL: store_load_ptr:
; QW:       store %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; QW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; QW:       jmpr %r7
  store ptr %val, ptr %addr
  %loaded = load ptr, ptr %addr
  ret ptr %loaded
}

;; --- Test 2: full strcpy loop (byte-per-byte copy) ---
define ptr @astrcpy(ptr %dest, ptr %src) {
; GEN-LABEL: astrcpy:
; GEN:       push %r5
; GEN:       movz %r5, %r6
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Loop header — load src pointer and byte
; GEN:       load %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN-NEXT:  load %r{{[0-9]+}}h
;; Compare byte with 0
; GEN:       cmp %r{{[0-9]+}}{{h?}}, %r{{[0-9]+}}{{h?}}
; GEN-NEXT:  bne
; GEN-NEXT:  br
;; Copy byte from src to dest
; GEN:       load %r{{[0-9]+}}h
; GEN:       store %r{{[0-9]+}}h
;; Increment both pointers
; GEN:       add %r{{[0-9]+}}, 1
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN:       add %r{{[0-9]+}}, 1
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Return
; GEN:       jmpr %r7
;
; DW-LABEL: astrcpy:
; DW:       push %r5
; DW:       movz %r5, %r6
; DW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; DW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
;; Loop: load byte, compare with 0, branch
; DW:       load %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; DW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; DW:       cmp %r{{[0-9]+[dq]?}}{{h?}}, %r{{[0-9]+[dq]?}}{{h?}}
; DW-NEXT:  bne
; DW-NEXT:  br
;; Copy byte, increment, store back
; DW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; DW:       store %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; DW:       add %r{{[0-9]+[dq]?}}, 1
; DW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; DW:       add %r{{[0-9]+[dq]?}}, 1
; DW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; DW:       jmpr %r7
;
; QW-LABEL: astrcpy:
; QW:       push %r5
; QW:       movz %r5, %r6
; QW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; QW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
;; Loop: load byte, compare with 0, branch
; QW:       load %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; QW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; QW:       cmp %r{{[0-9]+[dq]?}}{{h?}}, %r{{[0-9]+[dq]?}}{{h?}}
; QW-NEXT:  bne
; QW-NEXT:  br
;; Copy byte, increment, store back
; QW:       load %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; QW:       store %r{{[0-9]+}}{{[dqh]?}}, %r{{[0-9]+}}{{[dqh]?}}
; QW:       add %r{{[0-9]+[dq]?}}, 1
; QW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; QW:       add %r{{[0-9]+[dq]?}}, 1
; QW:       store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; QW:       jmpr %r7
  %ptr_dest = alloca ptr
  %ptr_src = alloca ptr
  store ptr %dest, ptr %ptr_dest
  store ptr %src, ptr %ptr_src
  br label %loop

loop:
  %cur_src = load ptr, ptr %ptr_src
  %byte = load i8, ptr %cur_src
  %is_zero = icmp ne i8 %byte, 0
  br i1 %is_zero, label %copy, label %done

copy:
  %cur_src2 = load ptr, ptr %ptr_src
  %byte2 = load i8, ptr %cur_src2
  %cur_dst = load ptr, ptr %ptr_dest
  store i8 %byte2, ptr %cur_dst
  br label %inc

inc:
  %next_src = load ptr, ptr %ptr_src
  %inc_src = getelementptr inbounds nuw i8, ptr %next_src, i32 1
  store ptr %inc_src, ptr %ptr_src
  %next_dst = load ptr, ptr %ptr_dest
  %inc_dst = getelementptr inbounds nuw i8, ptr %next_dst, i32 1
  store ptr %inc_dst, ptr %ptr_dest
  br label %loop

done:
  ret ptr %dest
}
