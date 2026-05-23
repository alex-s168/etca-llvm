; RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

; Test branch instructions.
; Branch encoding: byte0=0x80|Cond|(D8<<4), byte1=D[7:0], 9-bit signed disp

; --- Forward branches (positive displacement) ---
br 0
; CHECK: br 0                      ; encoding: [0x8e,0x00]

beq 2
; CHECK: beq 2                     ; encoding: [0x80,0x01]
bne 4
; CHECK: bne 4                     ; encoding: [0x81,0x02]

; Direct flag tests (BN, BNN, BOV, BNOV)
bn 6
; CHECK: bn 6                      ; encoding: [0x82,0x03]
bnn 8
; CHECK: bnn 8                     ; encoding: [0x83,0x04]
bov 10
; CHECK: bov 10                    ; encoding: [0x86,0x05]
bnov 12
; CHECK: bnov 12                   ; encoding: [0x87,0x06]

blt 14
; CHECK: blt 14                    ; encoding: [0x8a,0x07]
bge 16
; CHECK: bge 16                    ; encoding: [0x8b,0x08]
bltu 18
; CHECK: bltu 18                   ; encoding: [0x84,0x09]
bgeu 20
; CHECK: bgeu 20                   ; encoding: [0x85,0x0a]

; --- Backward branches (negative displacement) ---
br -2
; CHECK: br -2                     ; encoding: [0x9e,0xff]
beq -4
; CHECK: beq -4                    ; encoding: [0x90,0xfe]

; Direct flag tests, backward
bn -6
; CHECK: bn -6                     ; encoding: [0x92,0xfd]
bnn -8
; CHECK: bnn -8                    ; encoding: [0x93,0xfc]

; --- Maximum displacement ---
br 510
; CHECK: br 510                    ; encoding: [0x8e,0xff]
br -512
; CHECK: br -512                   ; encoding: [0x9e,0x00]
beq 256
; CHECK: beq 256                   ; encoding: [0x80,0x80]
bn 254
; CHECK: bn 254                    ; encoding: [0x82,0x7f]
