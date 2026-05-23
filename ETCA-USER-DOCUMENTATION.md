# ETCA LLVM Backend User Documentation



## Table of Contents

1. [Building LLVM with the ETCA Backend](#1-building-llvm-with-the-etca-backend)
2. [Using the Clang Driver](#2-using-the-clang-driver)
   - [Target Triple](#21-target-triple)
   - [CPU Models (-mcpu)](#22-cpu-models--mcpu)
   - [Extension Flags (-mattr)](#23-extension-flags--mattr)
   - [Preprocessor Defines](#24-preprocessor-defines)
   - [Inline Assembly](#25-inline-assembly)
3. [Assembly Syntax](#3-assembly-syntax)
4. [Linker (LD / LLD)](#4-linker-ld--lld)
   - [LLD (Recommended)](#41-lld-recommended)
   - [Using the binutils linker](#42-using-the-binutils-linker--fuse-ldbfd-)
   - [Binutils / LLD Cross-Compatibility](#43-binutils--lld-cross-compatibility)
   - [Producing Flat Binaries / ROM Images](#44-producing-flat-binaries--rom-images)
   - [Linker Scripts](#45-linker-scripts)
5. [Appendix: Complete Instruction Reference](#5-appendix-complete-instruction-reference)

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
| `ld.lld` | `build-etca/bin/ld.lld` | LLD ELF linker (ETCA support built-in) |
| `llvm-lit` | `build-etca/bin/llvm-lit` | Test runner |



### Running Tests

```sh
# Run all ETCA tests (MC assembly, CodeGen, Clang driver, preprocessor, LLD)
build-etca/bin/llvm-lit llvm/test/*/ETCA/ clang/test/*/ETCA/ clang/test/*/etca-* lld/test/ELF/etca-*
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

ETCA supports GCC-style inline assembly.

**Constraints:** `r` (any GPR), `i` (immediate), `m` (memory), `0`–`7` (specific rN).

**Register names in inline asm:** `r0`–`r15`, `d0`–`d15` (32-bit), `q0`–`q15` (64-bit).
r8+ and their d/q variants require `-mattr=+rex`.

```c
int result;
asm("add %0, %1" : "=r"(result) : "r"(a), "r"(b));
asm("movz %0, %1" : "=r"(result) : "0"(a));
asm("" : : : "r0", "r1", "r5", "r6");   // clobber list
```

**Important:** ETCA does not currently support Clang builtins for ETCa-specific instructions (READCR, WRITECR, etc.). Use inline assembly to access these.

---

## 3. Assembly Syntax

The complete ETCA assembly syntax reference (register naming, instruction set,
instruction syntax, size suffixes, labels, directives, and full instruction
tables) has been moved to a separate document:

➡️ **[`ETCA-ASM-SYNTAX.md`](./ETCA-ASM-SYNTAX.md)**

Key points:
- Registers use `%rN` (16-bit), `%rNd`/`%dN` (32-bit), `%rNq`/`%qN` (64-bit), `%rNh` (8-bit).
- Instructions follow RR (register-register) and RI (register-immediate) formats.
- Size suffixes: `h` (8-bit), `d` (32-bit), `q` (64-bit), or inferred from register.
- SAF extension adds `push`/`pop`/`call`/`jmpr`/`callr`.
- Branch instructions use 9-bit signed displacements (±512 bytes).
- Standard ELF directives (`.text`, `.data`, `.globl`, `.byte`, `.hword`, etc.).

---







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


## 4. Linker (LD / LLD)

ETCA ELF executables can be linked with either **LLD** (built as part of the LLVM
project, **default**) or **GNU ld** (from etca-binutils-gdb, opt-in). Both produce
compatible ELF output with `EM_ETCA` machine type.

### 4.1 LLD (Recommended, Default)

LLD has built-in ETCA support and does not require any external tools.
It is invoked via `ld.lld` or `lld` with the `-m elf32etca` or `-m elf64etca`
emulation flag.

**Basic usage:**

```sh
# Link a single object file into an executable (32-bit)
ld.lld -m elf32etca -o program.elf program.o

# Link multiple object files
ld.lld -m elf32etca -o program.elf main.o lib.o

# Link with libraries
ld.lld -m elf32etca -L/path/to/lib -o program.elf main.o -lmylib

# 64-bit ELF output
ld.lld -m elf64etca -o program.elf program.o

# Relocatable (partial) link
ld.lld -m elf32etca -r -o partial.o a.o b.o

# Produce a relocatable object (no link)
ld.lld -m elf32etca -r -o merged.o a.o b.o
```

**Via Clang (LLD is the default linker):**

```sh
# Single-step compilation + linking with LLD (no -fuse-ld= needed!)
clang --target=etca-unknown-elf -mcpu=generic -nostdlib -o program.elf program.c

# With startup code and library (cr0.o + libc)
clang --target=etca-unknown-elf -mcpu=generic program.c cr0.o -o program.elf

# 64-bit
clang --target=etca-unknown-elf -mcpu=etca64 -nostdlib -o program.elf program.c
```
```

**Emulation flags by CPU model:**

| CPU Model | LLD Flag | ELF Class |
|-----------|----------|-----------|
| generic (16-bit ptr) | `-m elf32etca` | ELF32 |
| etca32 (32-bit ptr)  | `-m elf32etca` | ELF32 |
| etca32p64 (64-bit ptr) | `-m elf64etca` | ELF64 |
| etca64p32 (32-bit ptr) | `-m elf32etca` | ELF32 |
| etca64 (64-bit ptr)  | `-m elf64etca` | ELF64 |

**Important notes:**
- ETCA is a **bare-metal target**. There is no operating system, so you need
to provide your own startup code (`_start` entry point) and linker script if
you need custom memory layout.
- By default, LLD places `.text` at address `0xB4` (after ELF headers and
program headers). Use `-Ttext=` or a linker script to override.
- `--gc-sections` is automatically enabled for final links (not for `-r` links).
- When invoked through clang with `-fuse-ld=lld`, LLD is found via the
  standard driver search path (`ld.lld` in the build `bin/` directory).

### 4.2 Using the binutils linker (`-fuse-ld=bfd`)

If you have the etca-binutils-gdb toolchain installed, tell clang to use it
via `-fuse-ld=bfd`:

```sh
# Use binutils ld.bfd instead of the default LLD
clang --target=etca-unknown-elf -mcpu=generic -fuse-ld=bfd -nostdlib -o program.elf program.c
#   → invokes: ld.bfd -melf16_etca -o program.elf ...
```

This works for all CPU models — clang passes the correct binutils emulation flag:

| CPU Model | Binutils flag (`-fuse-ld=bfd`) |
|-----------|-------------------------------|
| generic (16-bit ptr) | `-melf16_etca` |
| etca32 (32-bit ptr)  | `-melf32_etca` |
| etca32p64 (64-bit ptr) | `-melf64_etca` |
| etca64p32 (32-bit ptr) | `-melf32_etca` |
| etca64 (64-bit ptr)  | `-melf64_etca` |

If `ld.bfd` is not in your `$PATH`, the driver will report an error. Install
the binutils toolchain or add its `bin/` directory to `$PATH`.

**Note:** When using `-fuse-ld=bfd`, clang searches for `ld.bfd` (not
`etca-elf-ld`). The `-fuse-ld=` mechanism always looks for `ld.<flavor>`.

### 4.3 Binutils / LLD Cross-Compatibility

Object files produced by LLVM's assembler (via `llvm-mc` or `clang -c`) use
**RELA** format relocations, which are compatible with both LLD and GNU ld.
This means you can assemble with LLVM and link with binutils (via
`-fuse-ld=bfd`), or assemble with binutils and link with LLD (the default):

### 4.4 Producing Flat Binaries / ROM Images

ETCA bare-metal systems typically run directly from ROM. You can produce
flat binary images (raw machine code without ELF headers) in two ways:

#### Method A: `--oformat binary` (direct, no intermediate ELF)

LLD can output a flat binary directly with the `--oformat binary` flag:

```sh
# Write only the .text section contents as raw binary
ld.lld -m elf32etca --oformat binary -o program.bin program.o

# With a custom base address (sets VMA, binary starts at offset 0)
ld.lld -m elf32etca --oformat binary -Ttext=0x1000 -o program.bin program.o
```

Only `SHF_ALLOC` sections (`.text`, `.data`, `.rodata`) are included in the
binary output. `SHT_NOBITS` sections (`.bss`) are skipped.

#### Method B: `llvm-objcopy -O binary` (from ELF)

Alternatively, link to ELF first, then extract the binary image:

```sh
# Step 1: Link to ELF
ld.lld -m elf32etca -o program.elf program.o

# Step 2: Extract binary image
llvm-objcopy -O binary program.elf program.bin
```

Both methods produce identical output:

```sh
ld.lld -m elf32etca --oformat binary -o a.bin a.o
llvm-objcopy -O binary a.elf a.bin   # same result as above
diff a.bin b.bin && echo "identical"
```

#### Method C: Via Clang + LLD + objcopy (full pipeline)

For a complete C source → ROM image pipeline:

```sh
clang --target=etca-unknown-elf -mcpu=generic -fuse-ld=lld -nostdlib \
  -Wl,-T,rom.ld -o program.elf program.c
llvm-objcopy -O binary program.elf program.bin
```

#### Complete ROM Image Example

Create a linker script `etca_rom.ld` that places code in ROM and data in RAM:

```ld
MEMORY
{
  ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
  RAM (rwx) : ORIGIN = 0x10000000, LENGTH = 1M
}

SECTIONS
{
  .text : { *(.text*) } > ROM
  .rodata : { *(.rodata*) } > ROM
  .data : { *(.data*) } > RAM AT > ROM
  .bss : { *(.bss*) } > RAM

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
    *(.debug*)
  }
}
```

Then build the ROM image:

```sh
# Compile
clang --target=etca-unknown-elf -mcpu=generic -c -o program.o program.c

# Link and produce ELF
ld.lld -m elf32etca -T etca_rom.ld -o program.elf program.o

# Extract ROM contents (flat binary)
llvm-objcopy -O binary program.elf program.bin

# Check ROM image size and contents
llvm-objdump -d program.elf          # verify instructions
llvm-readelf -S program.elf          # verify section layout
xxd program.bin | head -20           # inspect raw bytes
```

**Notes on ROM images:**
- `.bss` (uninitialized data) is NOT included in the binary — your startup
  code must zero-initialize the BSS section at runtime using the VMA/length
  stored in the ELF symbols.
- `.data` initializers are stored in ROM (after `.text`) and must be copied
  to RAM by startup code before `main()` is called.
- The linker script `AT > ROM` directive places the data section's contents
  in the ROM load address even though the runtime address is in RAM.
- Use `-Ttext=0x...` for simple cases, or a full linker script for complex
  memory maps with multiple regions.

### 4.5 Linker Scripts

ETCA supports standard GNU linker scripts for custom memory layouts:

```ld
/* etca.ld - Example linker script for ETCA */
MEMORY
{
  ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
  RAM (rwx) : ORIGIN = 0x00010000, LENGTH = 32K
}

SECTIONS
{
  .text : { *(.text*) } > ROM
  .data : { *(.data*) } > RAM AT > ROM
  .bss  : { *(.bss*)  } > RAM
}
```

Use with:
```sh
ld.lld -m elf32etca -T etca.ld -o program.elf program.o
```


## FAQ / Troubleshooting

### Can't link — no linker script

ETCA is a bare-metal target. You'll need to provide your own linker script and startup code.

### Linker not found (when using `-fuse-ld=bfd`)

This error only occurs when you explicitly request the binutils linker and
it's not installed. By default, LLD is used and is always available in the
build directory (`bin/ld.lld`). If you do pass `-fuse-ld=bfd` without having
the binutils toolchain installed, you'll see:

```
clang: error: etca-elf-ld command failed with exit code 1
```

**Solutions:**
- Remove `-fuse-ld=bfd` to use LLD (the default, always works).
- Or install the etca-binutils-gdb toolchain and add its `bin/` directory
  to `$PATH`.

### LLD says "internal linker error: cannot read addend"

This means LLD encountered a relocation type it doesn't understand.
Make sure you're using a version of llvm-mc that has the correct relocation
mapping (ELF fixup → relocation type). Rebuild with the latest ETCA backend.

### LLD in clang link step fails (`-lc` / `-lgcc` not found)

ETCA is bare-metal and doesn't have libc by default. Add `-nostdlib` to
the clang invocation to skip standard library linking:

```sh
clang --target=etca-unknown-elf -mcpu=generic -nostdlib -o out.elf file.c
```

---


