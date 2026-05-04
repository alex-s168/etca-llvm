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
blt 6
; CHECK: blt 6                     ; encoding: [0x8a,0x03]
bge 8
; CHECK: bge 8                     ; encoding: [0x8b,0x04]
bltu 10
; CHECK: bltu 10                   ; encoding: [0x84,0x05]
bgeu 12
; CHECK: bgeu 12                   ; encoding: [0x85,0x06]

; --- Backward branches (negative displacement) ---
br -2
; CHECK: br -2                     ; encoding: [0x9e,0xff]
beq -4
; CHECK: beq -4                    ; encoding: [0x90,0xfe]

; --- Maximum displacement ---
br 510
; CHECK: br 510                    ; encoding: [0x8e,0xff]
br -512
; CHECK: br -512                   ; encoding: [0x9e,0x00]
beq 256
; CHECK: beq 256                   ; encoding: [0x80,0x80]
