# RUN: llvm-mc -arch=etca -mcpu=generic -show-inst < %s | FileCheck %s
# RUN: llvm-mc -arch=etca -mcpu=generic -filetype=obj -o /dev/null %s

# Test that labels parse correctly and instructions are recognized.
# Full branch resolution with -filetype=obj exercises the applyFixup
# path for PC-relative branch/call fixups.

# CHECK: start:
start:
# CHECK: add %r0, 1
  add %r0, 1
# CHECK: br done
  br done
# CHECK: beq label1
  beq label1
# CHECK: nop
  nop

label1:
  sub %r0, 1
  bne start
  call sub_func
  jmpr %r7
# CHECK: label1:
# CHECK: sub %r0, 1
# CHECK: bne start
# CHECK: call sub_func
# CHECK: jmpr %r7

done:
  nop
# CHECK: done:
# CHECK: nop

sub_func:
  push %r5
  add %r5, 1
  pop %r5
  jmpr %r7
# CHECK: sub_func:
# CHECK: push %r5
# CHECK: add %r5, 1
# CHECK: pop %r5
# CHECK: jmpr %r7
