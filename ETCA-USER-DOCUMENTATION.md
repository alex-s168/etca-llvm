# ETCA LLVM Backend User Documentation



## Table of Contents

1. [Building LLVM with the ETCA Backend](#1-building-llvm-with-the-etca-backend)
2. [Using the Clang Driver](#2-using-the-clang-driver)
   - [Target Triple](#21-target-triple)
   - [CPU Models (-mcpu)](#22-cpu-models--mcpu)
   - [Extension Flags (-mattr)](#23-extension-flags--mattr)
   - [Preprocessor Defines](#24-preprocessor-defines)
   - [Inline Assembly](#25-inline-assembly)
3. [LLVM Assembler (llvm-mc)](#3-llvm-assembler-llvm-mc)
   - [Register Naming](#31-register-naming)
   - [Instruction Set](#32-instruction-set)
   - [Instruction Syntax](#33-instruction-syntax)
   - [Size Suffixes](#34-size-suffixes)
   - [Labels](#35-labels)
   - [Directives](#36-directives)
4. [Appendix: Complete Instruction Reference](#4-appendix-complete-instruction-reference)

---

## 1. Building LLVM with the ETCA Backend

### Prerequisites

- CMake >= 3.20
- Ninja build system (recommended)
- C++17 compiler (GCC or Clang)
- Python 3 (for lit tests)

### Recommended Build (Release with Assertions)

For day-to-day development and testing, use a **Release build with debug assertions**
enabled. This gives you the speed of an optimised compiler while catching bugs with
LLVM's internal consistency checks (e.g. in the register allocator, instruction
selector, and verifier passes):

```sh
cmake -S llvm -B build-etca -G Ninja \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=ETCA \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_ENABLE_PROJECTS="clang;lld"

ninja -C build-etca
```

**Explanation of flags:**
- `LLVM_EXPERIMENTAL_TARGETS_TO_BUILD=ETCA` — enables the ETCA target (it's still listed as experimental in the LLVM build system)
- `LLVM_TARGETS_TO_BUILD=""` — builds ONLY the ETCA target (faster). Omit to build all targets.
- `CMAKE_BUILD_TYPE=Release` — optimised build (use `Debug` for full debug info if you need to step through LLVM code)
- `LLVM_ENABLE_ASSERTIONS=ON` — enables assertion checks; catches many bugs early without the full performance cost of a Debug build
- `LLVM_ENABLE_PROJECTS="clang;lld"` — builds Clang and the LLD linker alongside LLVM. Without this, only core LLVM tools (llc, llvm-mc, etc.) are built.

### What gets built

| Tool | Path | Purpose |
|------|------|---------|
| `llvm-mc` | `build-etca/bin/llvm-mc` | Assembler / disassembler |
| `llc` | `build-etca/bin/llc` | LLVM IR → assembly / object code |
| `llvm-objdump` | `build-etca/bin/llvm-objdump` | Object file inspection |
| `llvm-readobj` | `build-etca/bin/llvm-readobj` | Object file reading |
| `clang` | `build-etca/bin/clang` | C/C++/... compiler |
| `lld` | `build-etca/bin/lld` | ELF linker (with `-m etcaelf`) |
| `llvm-lit` | `build-etca/bin/llvm-lit` | Test runner |



### Running Tests

```sh
# Run all ETCA tests (MC assembly, CodeGen, Clang driver, preprocessor)
build-etca/bin/llvm-lit llvm/test/*/ETCA/ clang/test/*/ETCA/ clang/test/*/etca-*
```

---

## 2. Using the Clang Driver

### 2.1 Target Triple

ETCA uses the target triple `etca-unknown-elf`.  Pick the CPU model with `-mcpu=`;
the triple stays the same regardless:

```sh
# 16-bit word, 16-bit pointer (generic)
clang --target=etca-unknown-elf -mcpu=generic -c file.c

# 32-bit word, 32-bit pointer
clang --target=etca-unknown-elf -mcpu=etca32 -c file.c

# 64-bit word, 64-bit pointer
clang --target=etca-unknown-elf -mcpu=etca64 -c file.c
```

The code generation data layout (pointer size, type sizes) is computed from the
`-mcpu=` setting, so you only need `-elf` without suffixes.

The triple variants `etca-unknown-elf32` and `etca-unknown-elf64` exist only for
the assembler (llvm-mc) to pick the correct pointer-sensitive defaults when invoked
without a `-mcpu=` flag.  You do not need them when using clang.

### 2.2 CPU Models (-mcpu)

Five CPU models are supported:

| `-mcpu=` | Word Size | Pointer Size | Enabled Extensions | Description |
|----------|-----------|--------------|---------------------|-------------|
| `generic` | 16-bit | 16-bit | SAF, BYTE | Base ISA + stack/functions |
| `etca32` | 32-bit | 32-bit | SAF, BYTE, DW, DWAS | 32-bit everything |
| `etca32p64` | 64-bit | 64-bit | SAF, BYTE, DW, QW, QWAS | 32-bit ops, 64-bit addr |
| `etca64p32` | 64-bit | 32-bit | SAF, BYTE, DW, QW, DWAS | 64-bit ops, 32-bit addr |
| `etca64` | 64-bit | 64-bit | SAF, BYTE, DW, QW, QWAS | Full 64-bit |

The default CPU is `generic` (16-bit). To use a different CPU:
```sh
clang --target=etca-unknown-elf -mcpu=etca32 -c file.c
clang --target=etca-unknown-elf -mcpu=etca64 -c file.c
```

### 2.3 Extension Flags (-mattr)

Individual extensions can be enabled/disabled via `-mattr`:

```sh
# Enable REX extension (expanded registers r8-r15)
clang --target=etca-unknown-elf -mcpu=generic -mattr=+rex -c file.c

# Disable SAF (no function calls)
clang --target=etca-unknown-elf -mcpu=generic -mattr=-saf -c file.c

# Combine: enable REX and disable BYTE
clang --target=etca-unknown-elf -mcpu=generic -mattr=+rex,-byte -c file.c
```

**Available features:**

| Feature | Flag | Description |
|---------|------|-------------|
| SAF | `+saf` / `-saf` | Stack and Functions (calls, push/pop). Enabled by default on all CPUs. |
| BYTE | `+byte` / `-byte` | 8-bit byte operations (SS=00). Enabled by default on all CPUs. |
| DW | `+dw` / `-dw` | 32-bit doubleword operations |
| QW | `+qw` / `-qw` | 64-bit quadword operations |
| REX | `+rex` / `-rex` | Expanded registers (r8-r15) |
| DWAS | `+dwas` / `-dwas` | 32-bit address space |
| QWAS | `+qwas` / `-qwas` | 64-bit address space |
| 16bit | `+16bit` / `-16bit` | 16-bit word mode |
| 32bit | `+32bit` / `-32bit` | 32-bit word mode |
| 64bit | `+64bit` / `-64bit` | 64-bit word mode |
| ptr16 | `+ptr16` / `-ptr16` | 16-bit pointer |
| ptr32 | `+ptr32` / `-ptr32` | 32-bit pointer |
| ptr64 | `+ptr64` / `-ptr64` | 64-bit pointer |

**Important notes:**
- `-mcpu=generic` enables SAF, BYTE by default. It does NOT enable REX (you need `-mattr=+rex`).
- DW and QW are automatically enabled by CPU models that need them.
- The pointer size from the target triple should match the CPU model (use `elf32` for 32-bit, `elf64` for 64-bit).

### 2.4 Preprocessor Defines

The Clang driver defines the following preprocessor macros:

**Always defined:**
```c
__etca__               // 1 — ETCA architecture
__ETCA__               // 1 — Uppercase variant
__ELF__                // 1 — ELF object format
__ETCA_WORD_SIZE__     // Word size in bits: 16, 32, or 64
__SIZEOF_POINTER__     // Pointer size in bytes: 2, 4, or 8
__ETCA_PTR_SIZE__      // Pointer size in bits: 16, 32, or 64
__ETCA_HAS_SAF__       // 1 — SAF extension (always on supported CPUs)
__ETCA_HAS_BYTE__      // 1 — BYTE extension (always on supported CPUs)
```

**CPU-specific macros:**
```c
// For -mcpu=generic:
__ETCA_GENERIC__

// For -mcpu=etca32:
__ETCA32__

// For -mcpu=etca32p64:
__ETCA32P64__

// For -mcpu=etca64p32:
__ETCA64P32__

// For -mcpu=etca64:
__ETCA64__
```

**Optional extension macros:**
```c
__ETCA_HAS_DW__        // 1 — 32-bit ops available (word size >= 32)
__ETCA_HAS_QW__        // 1 — 64-bit ops available (word size >= 64)
__ETCA_HAS_DWAS__      // 1 — 32-bit address space (ptr size >= 32)
__ETCA_HAS_QWAS__      // 1 — 64-bit address space (ptr size >= 64)
__ETCA_HAS_REX__       // 1 — REX extension enabled (-mattr=+rex)
```

**Type sizes** (follow the word/pointer size):

| CPU | `sizeof(int)` | `sizeof(long)` | `sizeof(long long)` | `sizeof(void*)` | `sizeof(size_t)` |
|-----|--------------|----------------|---------------------|-----------------|-------------------|
| generic | 2 | 4 | 8 | 2 | 4 |
| etca32 | 4 | 4 | 8 | 4 | 4 |
| etca64 | 4 | 8 | 8 | 8 | 8 |

`char` is **signed** by default on ETCA (matching binutils conventions).

### 2.5 Inline Assembly

ETCA supports GCC-style inline assembly:

```c
// General register constraint
int result;
asm("add %0, %1" : "=r"(result) : "r"(a), "r"(b));

// Specific register constraints (r0-r7)
asm("movz %0, %1" : "=r"(result) : "0"(a), "r"(b));

// Immediate constraint
asm("add %0, %1, %2" : "=r"(result) : "r"(a), "i"(5));
```

**Constraint letters:**

| Constraint | Meaning |
|------------|---------|
| `r` | Any general-purpose register |
| `i` | Immediate constant |
| `m` | Memory operand |
| `0`–`7` | Specific register (r0–r7) / matching constraint |

**Clobber list:**
```c
// Mark registers as clobbered
asm volatile("" : : : "r0", "r1", "r5", "r6", "r7");
```

**Register names** that can be used in inline asm:
```
r0  r1  r2  r3  r4  r5  r6  r7
r8  r9  r10 r11 r12 r13 r14 r15     (with +rex)
d0  d1  d2  d3  d4  d5  d6  d7
d8  d9  d10 d11 d12 d13 d14 d15     (with +rex)
q0  q1  q2  q3  q4  q5  q6  q7
q8  q9  q10 q11 q12 q13 q14 q15     (with +rex)
```

**Important:** ETCA does NOT currently support Clang builtins/intrinsics for ETCa-specific instructions (READCR, WRITECR, etc.). Use inline assembly to access these.

---

## 3. Assembler

ETCA assembly can be assembled either through the Clang driver (recommended for
normal use) or directly with `llvm-mc` (for testing and debugging).

### Recommended: via Clang

For everyday assembly work, use `clang -c` — it handles the target triple, CPU
model, and extension flags in one place:

```sh
# Assemble a .s file into an object file
clang --target=etca-unknown-elf -mcpu=generic -c file.s -o file.o

# With REX extension
clang --target=etca-unknown-elf -mcpu=generic -mattr=+rex -c file.s

# 32-bit CPU
clang --target=etca-unknown-elf -mcpu=etca32 -c file.s

# Generate assembly listing from C
clang --target=etca-unknown-elf -mcpu=generic -S file.c -o file.s
```

### Low-level: llvm-mc

Use `llvm-mc` when you need to inspect encodings, test round-tripping, or debug
the assembler itself:

```sh
# Assemble to object file
llvm-mc -arch=etca -mcpu=generic -filetype=obj file.s -o file.o

# Show instruction encodings (useful for tests)
llvm-mc -arch=etca -mcpu=generic --show-encoding file.s

# Specify extensions
llvm-mc -arch=etca -mcpu=generic -mattr=+rex file.s

# Output assembly listing (default output)
llvm-mc -arch=etca -mcpu=generic -filetype=asm file.s

# Disassemble object file
llvm-objdump -d --arch-name=etca file.o
```

Note that `llvm-mc` requires the explicit `-arch=etca` flag, while `clang -c`
derives the architecture from the target triple.

### 3.1 Register Naming

ETCA registers follow binutils conventions. The `%` prefix is optional but recommended for consistency with binutils.

**16-bit word registers (base ISA):**
```
%r0   %r1   %r2   %r3   %r4   %r5   %r6   %r7
```

**16-bit with REX extension (`-mattr=+rex`):**
```
%r8   %r9   %r10  %r11  %r12  %r13  %r14  %r15
```

**32-bit doubleword registers (SS=10):**
```
%r0d  %r1d  %r2d  %r3d  %r4d  %r5d  %r6d  %r7d   (or %d0..%d7)
%r8d  %r9d  %r10d %r11d %r12d %r13d %r14d %r15d   (with +rex)
```

**64-bit quadword registers (SS=11):**
```
%r0q  %r1q  %r2q  %r3q  %r4q  %r5q  %r6q  %r7q   (or %q0..%q7)
%r8q  %r9q  %r10q %r11q %r12q %r13q %r14q %r15q   (with +rex)
```

**8-bit byte registers (SS=00):**
```
%r0h  %r1h  %r2h  %r3h  %r4h  %r5h  %r6h  %r7h
```

**Alternative naming (all forms accepted):**
| Form | Example | Meaning |
|------|---------|---------|
| `%rN` | `%r0` | 16-bit word register |
| `%rNd` | `%r0d` | 32-bit doubleword |
| `%rNq` | `%r0q` | 64-bit quadword |
| `%rNh` | `%r0h` | 8-bit byte |
| `%rNx` | `%r0x` | 16-bit word (explicit) |
| `%rdN` | `%rd0` | 32-bit doubleword (infix) |
| `%rqN` | `%rq0` | 64-bit quadword (infix) |
| `%dN` | `%d0` | 32-bit (backward compat) |
| `%qN` | `%q0` | 64-bit (backward compat) |
| `rN` | `r0` | Any register (without % prefix) |

**ABI names (all point to the corresponding register):**

| ABI Name | Register | Description |
|----------|----------|-------------|
| `a0` / `a1` / `a2` | r0 / r1 / r2 | Argument registers |
| `s0` / `s1` | r3 / r4 | Callee-saved registers |
| `bp` | r5 | Base pointer (frame pointer) |
| `sp` | r6 | Stack pointer |
| `ln` | r7 | Link register (return address) |
| `t0` / `t1` / `t2` / `t3` / `t4` | r8 / r9 / r10 / r11 / r12 | Temp registers (REX, caller-saved) |
| `s2` / `s3` / `s4` | r13 / r14 / r15 | Callee-saved registers (REX) |

Size-qualified ABI names are also accepted: `t0d`, `s2q`, etc.

### 3.2 Instruction Set

#### Base ISA — Computation (RR format)

| Mnemonic | Opcode | Operation | Flags | Description |
|----------|--------|-----------|-------|-------------|
| `add dst, src2` | 0000 | `dst ← dst + src2` | ZNCV | Add |
| `sub dst, src2` | 0001 | `dst ← dst - src2` | ZNCV | Subtract |
| `rsub dst, src2` | 0010 | `dst ← src2 - dst` | ZNCV | Reverse subtract |
| `cmp src1, src2` | 0011 | `_ ← src1 - src2` | ZNCV | Compare (no dest) |
| `or dst, src2` | 0100 | `dst ← dst \| src2` | ZN | Bitwise OR |
| `xor dst, src2` | 0101 | `dst ← dst ^ src2` | ZN | Bitwise XOR |
| `and dst, src2` | 0110 | `dst ← dst & src2` | ZN | Bitwise AND |
| `test src1, src2` | 0111 | `_ ← src1 & src2` | ZN | Test bits (no dest) |
| `movz dst, src` | 1000 | `dst ← src` | None | Move, zero-extend |
| `movs dst, src` | 1001 | `dst ← src` | None | Move, sign-extend |
| `load dst, addr` | 1010 | `dst ← MEM[addr]` | None | Load from memory |
| `store val, addr` | 1011 | `MEM[addr] ← val` | None | Store to memory |

#### Base ISA — Computation (RI format)

Same operations, but second operand is a 5-bit immediate:
| Mnemonic | Example | Extension |
|----------|---------|-----------|
| `add dst, imm` | `add %r0, 5` | Sign-extended |
| `sub dst, imm` | `sub %r0, 5` | Sign-extended |
| `cmp dst, imm` | `cmp %r0, 5` | Sign-extended |
| `movz dst, imm` | `movz %r0, 31` | Zero-extended |
| `movs dst, imm` | `movs %r0, -1` | Sign-extended |
| `slo dst, imm` | `slo %r0, 7` | Shift-left-OR |

Immediate range: `-16` to `+15` (sign-extended) for most ops, `0` to `31` (zero-extended) for MOVZ.

#### BYTE Extension (SS=00)

All RR/RI operations work on 8-bit data when suffixed with `h` or when the source register has `h` suffix:

```
add %r0h, %r1h     # 8-bit addition
load %r0h, %r1     # 8-bit load (byte)
store %r0h, %r1    # 8-bit store (byte)
```

#### DW/QW Extensions (SS=10 / SS=11)

Operations work on 32-bit or 64-bit data with the `d`/`q` register suffix or size suffix:

```
add %r0d, %r1d     # 32-bit addition (requires DW)
add %r0q, %r1q     # 64-bit addition (requires QW)
```

#### Control Register Access

```
readcr %r0, 0      # Read CPUID1 into %r0
readcr %r0, 1      # Read CPUID2
readcr %r0, 2      # Read FEAT
writecr %r0, 0     # Write CPUID1 (NOP)
```

#### SAF Extension

```
# Stack operations
push %r0           # Push register onto stack
push 5             # Push immediate
pop %r0            # Pop register from stack

# Calls and jumps
call label         # PC-relative call (12-bit displacement)
jmpr %r0           # Jump to register (return)
callr %r0          # Call subroutine via register

# Return (pseudo-instruction)
ret                # Return from function (expands to jmpr %r7)
```

#### Branch Instructions

```
br label            # Unconditional branch
beq label           # Branch if equal (Z)
bne label           # Branch if not equal (!Z)
bltu label          # Branch if below/less unsigned (C)
bgeu label          # Branch if above/equal unsigned (!C)
bleu label          # Branch if below or equal (C|Z)
bgtu label          # Branch if above (!(C|Z))
blt label           # Branch if less signed (N!=V)
bge label           # Branch if greater/equal signed (N=V)
ble label           # Branch if less or equal (Z|(N!=V))
bgt label           # Branch if greater (Z&(N=V))
```

Branch displacement is 9-bit signed (±256 halfwords = ±512 bytes).

#### NOP

```
nop                 # No operation (encoded as 0x008F)
```

### 3.3 Instruction Syntax

**RR (Register-Register) format:**
```
mnemonic %dst, %src2
```
Example: `add %r0, %r1` — adds r1 to r0, stores result in r0.

Exception for `movz`/`movs`: `movz %dst, %src` — copies src to dst.

Exception for `cmp`/`test`: `cmp %src1, %src2` — no destination.

**RI (Register-Immediate) format:**
```
mnemonic %dst, imm5
```
Example: `add %r0, 5` — adds 5 to r0, stores in r0.

For `movz`/`movs` with immediate: `movz %dst, imm5`.

**LOAD/STORE:**
```
load %dst, %addr     # Load from address in addr reg
store %val, %addr    # Store val to address in addr reg
```

**Branch/CALL:**
```
br label
call label
```

**SAF Jump/Call via register:**
```
jmpr %reg
callr %reg
```

**Size-qualified mnemonics:**
```
addh %r0, %r1       # 8-bit addition (same as add with h regs)
addd %r0, %r1       # 32-bit addition (same as add with d regs)
addq %r0, %r1       # 64-bit addition (same as add with q regs)
```

### 3.4 Size Suffixes

Instructions accept an optional size suffix that determines the operation width:

| Suffix | Width | Example |
|--------|-------|---------|
| (none) | Inferred from register | `add %r0, %r1` → 16-bit |
| `h` | 8-bit | `addh %r0, %r1` → 8-bit |
| `d` | 32-bit | `addd %r0, %r1` → 32-bit |
| `q` | 64-bit | `addq %r0, %r1` → 64-bit |

If the register width differs from the suffix, the suffix takes precedence:

```
addd %r0, %r1       # 32-bit add (even though r0/r1 are 16-bit registers)
```

### 3.5 Labels

Labels follow standard asm syntax:

```asm
.text
    movz %r0, 10        # r0 = 10
    movz %r1, 20        # r1 = 20
loop:
    sub %r0, 1          # r0--
    cmp %r0, %r1        # compare
    bne loop            # if not equal, continue loop
    br done
done:
    jmpr %r7            # return
```

### 3.6 Directives

Standard ELF directives are supported:

```asm
.text                   # Code section
.data                   # Data section
.section .bss           # BSS section
.globl func_name        # Export symbol
.type func_name,@function
.size func_name, .-func_name
.p2align 1              # Align to 2 bytes
.byte 0x10, 0x04        # Emit raw bytes
.hword 0x1234           # Emit 16-bit value
.word 0x12345678        # Emit 32-bit value
.quad 0x1234            # Emit 64-bit value
.string "hello"         # Emit string
.zero 16                # Emit 16 zero bytes
```

---

## 4. Appendix: Complete Instruction Reference

### RR Computation Instructions (Base + BYTE + DW + QW)

| Mnemonic | 8-bit (SS=00) | 16-bit (SS=01) | 32-bit (SS=10) | 64-bit (SS=11) |
|----------|---------------|----------------|-----------------|-----------------|
| `add` | `add %rh, %rh` | `add %r, %r` | `add %rd, %rd` | `add %rq, %rq` |
| `sub` | `sub %rh, %rh` | `sub %r, %r` | `sub %rd, %rd` | `sub %rq, %rq` |
| `rsub` | `rsub %rh, %rh` | `rsub %r, %r` | `rsub %rd, %rd` | `rsub %rq, %rq` |
| `cmp` | `cmp %rh, %rh` | `cmp %r, %r` | `cmp %rd, %rd` | `cmp %rq, %rq` |
| `or` | `or %rh, %rh` | `or %r, %r` | `or %rd, %rd` | `or %rq, %rq` |
| `xor` | `xor %rh, %rh` | `xor %r, %r` | `xor %rd, %rd` | `xor %rq, %rq` |
| `and` | `and %rh, %rh` | `and %r, %r` | `and %rd, %rd` | `and %rq, %rq` |
| `test` | `test %rh, %rh` | `test %r, %r` | `test %rd, %rd` | `test %rq, %rq` |
| `movz` | `movz %rh, %rh` | `movz %r, %r` | `movz %rd, %rd` | `movz %rq, %rq` |
| `movs` | `movs %rh, %rh` | `movs %r, %r` | `movs %rd, %rd` | `movs %rq, %rq` |
| `load` | `load %rh, %r` | `load %r, %r` | `load %rd, %rd` | `load %rq, %rq` |
| `store` | `store %rh, %r` | `store %r, %r` | `store %rd, %rd` | `store %rq, %rq` |

**Note on load/store address register:** The address register width must match the pointer size of the CPU model. For 16-bit pointers (generic), use `%rN` as the address. For 32-bit pointers (etca32, etca64p32), use `%rNd`/`%dN`. For 64-bit pointers (etca32p64, etca64), use `%rNq`/`%qN`.

### RI Computation Instructions

| Mnemonic | 8-bit | 16-bit | 32-bit | 64-bit |
|----------|-------|--------|--------|--------|
| `add %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `sub %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `rsub %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `cmp %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `or %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `xor %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `and %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `test %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `movz %r, imm` | ✓ | ✓ | ✓ | ✓ |
| `movs %r, imm` | ✓ | ✓ | ✓ | ✓ |

### SAF Instructions

| Instruction | Description |
|-------------|-------------|
| `push %r` | Push register onto stack |
| `push imm` | Push 5-bit unsigned immediate |
| `pop %r` | Pop register from stack |
| `call label` | PC-relative call (12-bit displacement) |
| `jmpr %r` | Jump to register (return) |
| `callr %r` | Call via register |

### Branch Instructions

| Instruction | Condition | Encoding |
|-------------|-----------|----------|
| `br label` | Always (unconditional) | 0xEE |
| `beq label` | Equal (Z) | 0x00 |
| `bne label` | Not equal (!Z) | 0x01 |
| `bltu label` | Below/unsigned less (C) | 0x04 |
| `bgeu label` | Above or equal unsigned (!C) | 0x05 |
| `bleu label` | Below or equal (C\|Z) | 0x08 |
| `bgtu label` | Above unsigned (!(C\|Z)) | 0x09 |
| `blt label` | Less signed (N≠V) | 0x0A |
| `bge label` | Greater or equal signed (N=V) | 0x0B |
| `ble label` | Less or equal (Z\|(N≠V)) | 0x0C |
| `bgt label` | Greater signed (!Z & N=V) | 0x0D |

### Pseudo Instructions

| Pseudo | Expansion | Description |
|--------|-----------|-------------|
| `nop` | `0x008F` (hardware NOP) | No operation |
| `ret` | `jmpr %r7` | Return from function |
| `mov dst, src` | `movz` or `movs` depending on context | Generic move |

### REX Extension (r8-r15)

When `-mattr=+rex` is enabled, registers r8-r15 (and their d/q variants) become available. The assembler automatically emits a REX prefix byte before any instruction that uses a register ≥ 8:

```asm
add %r8, %r9         # Uses REX prefix (A=1, B=1)
add %r8d, %r9d       # 32-bit with REX
add %r8q, %r9q       # 64-bit with REX
add %r8, 5           # RI with REX
load %r8, %r9        # LOAD with REX
push %r8             # PUSH with REX
pop %r9              # POP with REX
```

The REX prefix is automatically inserted by the encoder; you never write it explicitly.

---

## FAQ / Troubleshooting

### "first operand must be a register" error

You're trying to use an instruction that expects a register operand with a non-register value. Check that the mnemonic and operand types match (e.g., `add` needs two registers, `addi` is not a valid mnemonic — use `add` with an immediate).

### "unknown instruction mnemonic" error

The instruction may require a specific extension. Enable it with `-mattr=+saf` (for push/pop/call), `-mattr=+rex` (for r8-r15), or use the correct CPU model.

### "instruction not available in this width/form"

The operation doesn't exist for the requested width. For example, `slo` only exists in 16-bit form.

### REX prefix not emitted

The REX prefix is emitted automatically when any register operand has a register number ≥ 8. Ensure `-mattr=+rex` is passed to the assembler.

### Can't link — no linker script

ETCA is a bare-metal target. You'll need to provide your own linker script and startup code. The default linker is `etca-elf-ld`.

---


