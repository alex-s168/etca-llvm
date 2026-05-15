; RUN: llc -march=etca -mcpu=generic -O1 < %s | FileCheck %s --check-prefix=GEN
; RUN: llc -march=etca -mcpu=etca32 -O1 < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=etca64 -O1 < %s | FileCheck %s --check-prefix=QW
; RUN: llc -march=etca -mcpu=etca32p64 -O1 < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=etca64p32 -O1 < %s | FileCheck %s --check-prefix=QW

;; ===========================================================================
;; ETCA strcpy codegen at -O1
;;
;; Tests that strcpy-style byte-per-byte pointer copy loops compile correctly
;; under -O1 optimisation (basic optimisation passes run before GISel).
;;
;; The store_load_ptr function exercises basic pointer store+load (p0) with
;; minimal compilation.
;; The astrcpy function exercises the full byte-copy loop with alloca-based
;; pointer slots, byte loads/stores (s8), and pointer arithmetic (G_PTR_ADD).
;;
;; At -O1 the IR optimizer runs before instruction selection, so the
;; generated assembly may differ from -O0 but the overall loop structure
;; (load byte → compare → copy → increment both pointers) must be preserved.
;;
;; On generic (16-bit), s8 loads/stores are widened to s16; the instruction
;; selector selects LOAD8/STORE8 independently.
;; On DW/QW systems, the full address width is used for pointer arithmetic.
;; ===========================================================================

;; --- Test 1: store+load pointer values (p0), then return ---
define ptr @store_load_ptr(ptr %addr, ptr %val) {
; GEN-LABEL: store_load_ptr:
; GEN:       store %r1, %r0
; GEN-NEXT:  load %r0, %r0
; GEN:       jmpr %r7
;
; DW-LABEL: store_load_ptr:
; DW:       store %r1, %r0
; DW-NEXT:  load %r0, %r0
; DW:       jmpr %r7
;
; QW-LABEL: store_load_ptr:
; QW:       store %r1, %r0
; QW-NEXT:  load %r0, %r0
; QW:       jmpr %r7
  store ptr %val, ptr %addr
  %loaded = load ptr, ptr %addr
  ret ptr %loaded
}

;; --- Test 2: full strcpy loop (byte-per-byte copy) at -O1 ---
define ptr @astrcpy(ptr %dest, ptr %src) {
; GEN-LABEL: astrcpy:
; GEN:       push %r5
; GEN:       movz %r5, %r6
;; Prologue: allocate stack frame, spill callee-saves, set up stack slots
; GEN:       sub %r6,
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Loop header — load src pointer and byte
; GEN:       load %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN-NEXT:  load %r{{[0-9]+}}h
;; Compare byte with 0 and branch
; GEN:       cmp %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN-NEXT:  bne
; GEN:       br
;; Copy byte from src to dest
; GEN:       load %r{{[0-9]+}}h
; GEN:       store %r{{[0-9]+}}h
;; Increment both pointers
; GEN:       add %r{{[0-9]+}}, 1
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; GEN:       add %r{{[0-9]+}}, 1
; GEN:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Return original dest pointer
; GEN:       jmpr %r7
;
; DW-LABEL: astrcpy:
; DW:       push %r5
; DW:       movz %r5, %r6
;; Prologue: allocate stack frame, spill callee-saves
; DW:       sub %r6,
; DW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; DW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Setup zero constant (byte null terminator)
; DW:       movz %r{{[0-9]+}}h, 0
;; Loop header — load src pointer, load byte
; DW:       load %r{{[0-9]+}}, %r{{[0-9]+}}
; DW-NEXT:  load %r{{[0-9]+}}h
;; Compare byte with 0
; DW:       cmp %r{{[0-9]+}}, %r{{[0-9]+}}
; DW-NEXT:  bne
; DW:       br
;; Copy: load byte from src, store to dest
; DW:       load %r{{[0-9]+}}h
; DW:       store %r{{[0-9]+}}h
;; Increment src pointer
; DW:       add %r{{[0-9]+}}, 1
; DW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Increment dst pointer
; DW:       add %r{{[0-9]+}}, 1
; DW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Return
; DW:       jmpr %r7
;
; QW-LABEL: astrcpy:
; QW:       push %r5
; QW:       movz %r5, %r6
;; Prologue: allocate stack frame, spill callee-saves
; QW:       sub %r6,
; QW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
; QW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Setup zero constant (byte null terminator)
; QW:       movz %r{{[0-9]+}}h, 0
;; Loop header — load src pointer, load byte
; QW:       load %r{{[0-9]+}}, %r{{[0-9]+}}
; QW-NEXT:  load %r{{[0-9]+}}h
;; Compare byte with 0
; QW:       cmp %r{{[0-9]+}}, %r{{[0-9]+}}
; QW-NEXT:  bne
; QW:       br
;; Copy: load byte from src, store to dest
; QW:       load %r{{[0-9]+}}h
; QW:       store %r{{[0-9]+}}h
;; Increment src pointer
; QW:       add %r{{[0-9]+}}, 1
; QW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Increment dst pointer
; QW:       add %r{{[0-9]+}}, 1
; QW:       store %r{{[0-9]+}}, %r{{[0-9]+}}
;; Return
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
