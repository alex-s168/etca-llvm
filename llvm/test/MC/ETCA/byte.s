# RUN: llvm-mc -arch=etca -mattr=+byte -show-encoding < %s | FileCheck %s

# Test BYTE extension (8-bit, SS=00) for all RR/RI/LOAD/STORE variants.
# The InstPrinter adds the 'h' suffix for byte-width register operands,
# enabling full assembly round-trip.

# === RR byte (SS=00) ===
# add %r0h, %r1h → SS=00 → 0x0400 → [0x00,0x04]
add %r0h, %r1h
# CHECK: add %r0h, %r1h            ; encoding: [0x00,0x04]

# sub %r2h, %r3h → 0x4C01 → [0x01,0x4c]
sub %r2h, %r3h
# CHECK: sub %r2h, %r3h            ; encoding: [0x01,0x4c]

# rsub %r4h, %r5h → 0x9402 → [0x02,0x94]
rsub %r4h, %r5h
# CHECK: rsub %r4h, %r5h           ; encoding: [0x02,0x94]

# or %r6h, %r7h → 0xDC04 → [0x04,0xdc]
or %r6h, %r7h
# CHECK: or %r6h, %r7h             ; encoding: [0x04,0xdc]

# xor %r0h, %r1h → 0x0405 → [0x05,0x04]
xor %r0h, %r1h
# CHECK: xor %r0h, %r1h            ; encoding: [0x05,0x04]

# and %r2h, %r3h → 0x4C06 → [0x06,0x4c]
and %r2h, %r3h
# CHECK: and %r2h, %r3h            ; encoding: [0x06,0x4c]

# movz %r4h, %r5h → 0x9408 → [0x08,0x94]
movz %r4h, %r5h
# CHECK: movz %r4h, %r5h           ; encoding: [0x08,0x94]

# movs %r6h, %r7h → 0xDC09 → [0x09,0xdc]
movs %r6h, %r7h
# CHECK: movs %r6h, %r7h           ; encoding: [0x09,0xdc]

# === CMP/TEST RR byte (SS=00) ===
# cmp %r0h, %r1h → 0x0403 → [0x03,0x04]
cmp %r0h, %r1h
# CHECK: cmp %r0h, %r1h            ; encoding: [0x03,0x04]

# cmp %r3h, %r0h → 0x6003 → [0x03,0x60]
cmp %r3h, %r0h
# CHECK: cmp %r3h, %r0h            ; encoding: [0x03,0x60]

# test %r6h, %r3h → 0xCC07 → [0x07,0xcc]
test %r6h, %r3h
# CHECK: test %r6h, %r3h           ; encoding: [0x07,0xcc]

# === RI byte (SS=00) ===
# add %r0h, 31 → 0x1F40 → [0x40,0x1f]
add %r0h, 31
# CHECK: add %r0h, 31              ; encoding: [0x40,0x1f]

# sub %r1h, -16 → 0x3041 → [0x41,0x30]
sub %r1h, -16
# CHECK: sub %r1h, -16             ; encoding: [0x41,0x30]

# rsub %r2h, 15 → 0x4F42 → [0x42,0x4f]
rsub %r2h, 15
# CHECK: rsub %r2h, 15             ; encoding: [0x42,0x4f]

# cmp %r3h, 0 → 0x6043 → [0x43,0x60]
cmp %r3h, 0
# CHECK: cmp %r3h, 0               ; encoding: [0x43,0x60]

# or %r4h, 7 → 0x8744 → [0x44,0x87]
or %r4h, 7
# CHECK: or %r4h, 7                ; encoding: [0x44,0x87]

# xor %r5h, 3 → 0xA345 → [0x45,0xa3]
xor %r5h, 3
# CHECK: xor %r5h, 3               ; encoding: [0x45,0xa3]

# and %r6h, 31 → 0xDF46 → [0x46,0xdf]
and %r6h, 31
# CHECK: and %r6h, 31              ; encoding: [0x46,0xdf]

# test %r7h, 0 → 0xE047 → [0x47,0xe0]
test %r7h, 0
# CHECK: test %r7h, 0              ; encoding: [0x47,0xe0]

# movz %r0h, 10 → 0x0A48 → [0x48,0x0a]
movz %r0h, 10
# CHECK: movz %r0h, 10             ; encoding: [0x48,0x0a]

# movs %r1h, -8 → 0x3849 → [0x49,0x38]
movs %r1h, -8
# CHECK: movs %r1h, -8             ; encoding: [0x49,0x38]

# === LOAD8/STORE8 (SS=00) ===
# load %r0h, %r1h → 0x040A → [0x0a,0x04]
load %r0h, %r1h
# CHECK: load %r0h, %r1h           ; encoding: [0x0a,0x04]

# store %r2h, %r3h → 0x4C0B → [0x0b,0x4c]
store %r2h, %r3h
# CHECK: store %r2h, %r3h          ; encoding: [0x0b,0x4c]

# === Non-byte widths for comparison (no 'h' suffix) ===
add %r0, %r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]

add %r0d, %r1d
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]

add %r0q, %r1q
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]

load %r0, %r1
# CHECK: load %r0, %r1             ; encoding: [0x1a,0x04]

store %r0, %r1
# CHECK: store %r0, %r1            ; encoding: [0x1b,0x04]

# CMP/TEST word for comparison (no 'h' suffix)
cmp %r6, %r3
# CHECK: cmp %r6, %r3              ; encoding: [0x13,0xcc]

test %r7, %r0
# CHECK: test %r7, %r0             ; encoding: [0x17,0xe0]
