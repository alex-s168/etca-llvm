# ETCA LLVM Backend — Agent Guidance

## References

- Architecture specification: `../etca-spec/`, OR `../spec-concat.md`
- Specification of future vendor extensions: `../etca-vnd-extensions/`
- Existing binutils fork: `../etca-binutils-gdb/`
- Compiled target binutil binaries: `../etca-binutils-gdb/install/bin/`
- WIP capstone disassembler fork: `../etca-capstone/`

## Requirements

The LLVM backend needs to:
- use the same ELF relocs (or more) as binutils.
- use Global ISel (GISel)!! No SDAG fallback!
- support all sensical combinations of word & address size: 16b+16b, 32b+32b, 32b+64b, 64b+32b, 64b+64b
- lots of documentation, so that beginners can add support for their vendor extensions.
  this includes: intrinsics, clang intrinsics, isel changes, specific optimization passes, etc
- **have a lot of tests using the LLVM test suite**

It also needs to support:
- clang, and custom clang intrinsics
- llvm-mc assembler
- scheduling models (for future OOO processors)

NEVER drop any requirement. If something is not easily possible, ask the user. They might tell you to rather modify llvm itself than not doing things at all.

## Extension Support Status

| Extension | Status | Notes |
|-----------|--------|-------|
| base + saf + all word/address combos | ✅ DONE (2026-05-14) | 16/32/64-bit word & pointer, all 5 CPU models |
| byte (SS=00, 8-bit ops) | ✅ DONE (2026-05-13) | All computation + LOAD8/STORE8, sign-extension semantics |
| multiply / divide (libcall) | ✅ DONE (2026-05-14) | All 15 arithmetic libcall operations (mul/sdiv/udiv/srem/urem × 16/32/64-bit) work on all 5 CPU models |
| extended registers (REX prefix) | ⬜ TODO | |
| fi: full immediates (VWI prefix) | ⬜ TODO | |
| mo1/mo2: complex memory operands | ⬜ TODO | |
| expanded opcodes (EXOP prefix) | ⬜ TODO | |
| conditional prefix | ⬜ TODO | |
| bm1 (bit manipulation 1) | ⬜ TODO | |

Minimum support target: **base + saf**. Do not assume other extensions.

## Active Design Decisions & Gotchas

### Sign-extension semantics
ETCa requires that narrow operations sign-extend to full register width **unless** the operation is `movz` (zero-extends).
LLVM's default behaviour (sub-register writes preserve high bits) does NOT match this.
**Fix**: `G_ZEXT`/`G_SEXT` use the **source** (narrow) width for selecting `MOVZ`/`MOVS` opcodes. A `COPY` bridges register classes.
Applies to all SS values: SS=00 (byte), SS=01 (word/16-bit), SS=10 (dword/32-bit), SS=11 (qword/64-bit).

### Assembly syntax (binutils-compatible)
- Register names: `%rN` (16b), `%rNd` (32b), `%rNq` (64b), `%rNh` (8b)
- AsmParser accepts both `%rN` and `rN` forms, plus ABI names (`a0`, `a1`, `a2`, `s0`, `s1`, `bp`, `sp`, `ln`)
- Backward-compat `dN`/`qN` also accepted

### GISel-only pipeline
- **No SDAG fallback!** The target machine pipeline is `IRTranslator → Legalizer → RegBankSelect → InstructionSelect`.
- `ETCATargetMachine` uses `addGIselPasses()` with no SDAG pass.
- The TableGen `CMakeLists.txt` does **not** use `-gen-dag-isel`; it does use `-gen-global-isel`.
- All SDAG lowering hooks (LowerOperation, LowerFormalArguments, LowerReturn, LowerCall, ReplaceNodeResults) have been **removed** from `ETCATargetLowering`. The class now only retains the constructor (register classes + operation action setup) and `isLegalAddressingMode`.
- `ETCADAGToDAGISel.cpp` has been **deleted** entirely. The `ETCAISD::NodeType` and `ETCAISD::CondCode` enums from `ETCAISelLowering.h` have also been removed — they contained incorrect condition code values that could mislead future contributors.

### AsmBackend location
The real `AsmBackend` is in `MCTargetDesc/ETCAMCTargetDesc.cpp`. `MCTargetDesc/ETCAAsmBackend.cpp` is an unused duplicate — do not modify.

### CMP-RI disassembler conflict
TableGen `-gen-disassembler` fails because CMP-RI and TEST-RI encodings overlap with other RR instructions. The disassembler is **manually implemented** in `Disassembler/ETCADisassembler.cpp`.

### Frame layout fix (2026-05-15)
`getFrameIndexReference` in `ETCAFrameLowering.cpp` was returning `Offset = ObjectOffset - StackSize` for non-fixed objects, which placed all spill slots and local variables **below sp** (outside the allocated frame memory).  The fix changes the formula to `Offset = ObjectOffset` — PEI already assigns `ObjectOffset` values (-2, -4, -6, ...) that directly map to the desired bp-relative offsets, and the `sub r6, StackSize` in the prologue allocates the exact amount needed.

**Before** (spills go below sp, corrupting memory):
```
  sub r6, 8          ; sp = bp - 8
  store r3, r5-14    ; r3 at bp-14 — 6 bytes below sp!
  store r4, r5-16    ; r4 at bp-16 — 8 bytes below sp!
```

**After** (all spills within the allocated frame):
```
  sub r6, 8          ; sp = bp - 8
  store r3, r5-6     ; r3 at bp-6 — within [bp-8, bp-0] ✓
  store r4, r5-8     ; r4 at bp-8 (= sp) ✓
```

Test: `llvm/test/CodeGen/ETCA/stack-frame.ll` verifies this on all 5 CPU models.

### NOP encoding: 0x008F
All NOP emission paths use `0x008F` ([0x8F, 0x00] LE) — the canonical 2-byte base-ISA NOP per binutils `etca_build_nop`:
- `ETCAInstrInfo.td: NOP Inst{15-0}` = `0x008F`
- `ETCAMCTargetDesc.cpp: encodeInstruction` case ETCA::NOP → `0x008F`
- `ETCAMCTargetDesc.cpp: writeNopData` → writes `"\x8F\x00"`
- `ETCADisassembler.cpp` → checks for `0x008F`

### LLVM 21+ API notes
- `copyPhysReg` uses 8-param signature (`RenamableDest`, `RenamableSrc`)
- `getPointerRegClass` uses 1-param signature (`Kind` only)
- `isConstantPhysReg` is no longer overridable (generated base marks it `final`)
- `isMoveInstr` is no longer virtual
- `applyFixup` is non-const (pure virtual in base)
- `FKF_IsPCRel` flag deleted from `MCFixupKindInfo` entries

### Encoder is auto-generated (was manual)
The `ETCAGenMCCodeEmitter.inc` was originally removed because the `Inst` field bit
assignments in `ETCAInstrFormats.td` used a logical MSB-to-LSB order that didn't
match the actual 16-bit little-endian encoding. The bit assignments were fixed
(2026-05-15) to match the real byte layout. The `-gen-emitter` TableGen command
is now active and the generated file is included via `#include "ETCAGenMCCodeEmitter.inc"`
in `MCTargetDesc/ETCAMCTargetDesc.cpp`. Helper methods (`getRegisterOpValue`,
`getImmOpValue`, `encodeBranchTarget`, `encodeCallTarget`) are kept for the
generated code to call.

### Mul/Div libcalls require clean cmake cache
`ETCALegalizerInfo` uses `.libcallFor()` which relies on LLVM's libcall infrastructure. An early cmake cache build can leave broken RTLIB::RuntimeLibcallsInfo state that prevents libcall resolution (especially for 16-bit types like `__mulhi3`).
**Fix**: Delete the build directory and re-run cmake. The libcall routing then correctly emits calls to `__mulhi3`, `__mulsi3`, `__muldi3` (and all div/rem variants) for all bit widths.

## Implementation Status (by Subsystem)

### ✅ TableGen Definitions
- Register file: 16, 32, 64-bit with sub-register indices and classes (GPR, GPR32, GPR64; CCR)
- Instruction formats: RR/RI with SS bits (16/32/64-bit), branch (9-bit disp), SAF (call 12-bit disp, jmpr/callr, push/pop reg, push imm)
- All base ISA + SAF + pseudo ops, with predicated patterns per extension
- Calling convention: multi-width CC_ETCA / RetCC_ETCA (i16→r0-r3, i32→d0-d3, i64→q0-q3)
- Processor models: generic (16b+16b), etca32 (32b+32b), etca32p64 (32b+64b), etca64p32 (64b+32b), etca64 (64b+64b)
- Schedule model: basic in-order with ALU/LdSt resources

### ✅ C++ Core Infrastructure
- ETCASubtarget: dynamic DataLayout from WordSize/PtrSize; feature flags (HasSAF, HasDW, HasQW, HasDWAS, HasQWAS)
- ETCATargetMachine: GISel-only pass pipeline
- ETCAFrameLowering: variable stack alignment, SAF-aware frame register (r5=bp, r6=sp)
- ETCARegisterInfo: variable pointer reg class, SAF callee-saved regs (r3-r6)
- ETCAInstrInfo: multi-width copyPhysReg, store/loadRegFromStackSlot, isMoveInstr
- ETCAISelLowering: multi-width legalization, custom lowering for BR_CC, BRCOND, GlobalAddress, ConstantPool, JumpTable

### ✅ GISel Pipeline
- ETCALegalizerInfo: dynamic legalizer based on word size (i16 always, i32 if WS≥32, i64 if WS≥64)
- ETCAInstructionSelector: handles all G_* opcodes with width-aware selection
- ETCACallLowering: multi-width lowerReturn, lowerFormalArguments, lowerCall
- ETCARegisterBankInfo: single GPR bank, per-operand size mappings
- ETCASelectExpand pass for G_SELECT → branch lowering

### ✅ MC Layer
- MCInstPrinter + auto-generated asm writer with `getRegisterName` and `%` prefix
- AsmParser: full manual parser for all base+SAF instructions
- Disassembler: full manual 16-bit decoder
- MCCodeEmitter: full manual encoder (16-bit LE)
- AsmBackend: NOP emission (0x0010), proper applyFixup
- ELFObjectWriter: relocation types matching binutils (R_ETCA_NONE through R_ETCA_IPREL_64)
- EM_ETCA = 0xE7Ca added to LLVM BinaryFormat

## Test Results

**All source files compile successfully on their target CPUs**:

### CodeGen Tests (15 source files)
| Test | Runs on | Description |
|------|---------|-------------|
| `arithmetic.ll` | generic, etca32, etca64 | All arithmetic ops |
| `byte-ops.ll` | generic +byte | BYTE extension (8-bit) |
| `calling-conv.ll` | generic, etca32, etca64 | Calling conventions |
| `constants.ll` | generic, etca32, etca64 | Constant materialization |
| `control-flow.ll` | generic, etca32, etca64 | CMP + conditional branches |
| `fibonacci.ll` | generic, etca32, etca64 | Recursive fib compiles (Greedy RA works!) |
| `memory.ll` | generic, etca32, etca64 | LOAD/STORE |
| `mul-div-16.ll` | generic, etca32, etca64 | 16-bit MIR libcall legalizer test |
| `mul-div-32.ll` | etca32, etca64 | 32-bit MIR libcall legalizer test |
| `mul-div-64.ll` | etca64 | 64-bit MIR libcall legalizer test |
| `mul-div-asm-16.ll` | generic, etca32, etca64 | 16-bit assembly libcall test |
| `multi-width-16.ll` | **all 5 CPUs** | 16-bit ops across all word/ptr combos |
| `multi-width-32.ll` | etca32, etca64 | 32-bit ops (DW extension) |
| `multi-width-64.ll` | etca64 | 64-bit ops (QW extension) |
| `saf.ll` | generic, etca32, etca64 | SAF calls |
| `signext.ll` | generic, etca32, etca64 | Sign-extension semantics |

### MC Tests (17)
| Test | Description |
|------|-------------|
| `addsub.s` | ADD/SUB RR+RI encodings for all widths |
| `byte.s` | BYTE extension (8-bit) instruction encodings |
| `cross-binutils.s` | Assembly syntax cross-check stub |
| `cross-dw-qw.s` | Dword/qword cross-check stub |
| `dword-qword.s` | All 32/64-bit RR + RI instruction encodings |
| `edge-cases.s` | Boundary values: imm 0/±16/15/31, max branch ±512, etc. |
| `logical.s` | AND/OR/XOR/TEST encodings |
| `mov.s` | MOVZ/MOVS all widths |
| `parser-errors.s` | Error message tests |
| `roundtrip-all.s` + `.dis` | Comprehensive decode(encode(inst)) round-trip |
| `saf-instructions.s` | SAF encoding tests |
| `simple.s` | Basic instruction encodings |
| `word-instructions.s` | Word-level instruction tests |

## Remaining Issues

### 1. Legalizer should be subtarget-aware ✅ FIXED
The legalizer now gates s32/s64 legality on HasDW/HasQW for computation ops (G_ADD, G_SUB, G_AND, G_OR, G_XOR, G_SHL, G_ICMP, G_SELECT). Data-flow ops (G_ZEXT, G_SEXT, G_TRUNC, G_MERGE_VALUES, G_UNMERGE_VALUES, G_CONSTANT, G_LOAD, G_STORE, G_PHI, G_IMPLICIT_DEF) remain s32/s64-legal because the instruction selector handles them manually. Helper narrowing ops (G_UADDE, G_UADDO, G_UMULH, etc.) are also always-legal since the legalizer creates them during narrowing.

Key design: Three tiers of type legality:
- **Tier 1 (data-flow)**: Always legal for s8/s16/s32/s64 (with HasByte gate for s8)
- **Tier 2 (computation)**: Conditioned on HasDW (s32) / HasQW (s64)
- **Tier 3 (narrow helpers)**: Always legal, types match the narrow computation types plus s1 carry

### 2. Redundant bridging COPYs after extension ✅ FIXED
`G_ZEXT`/`G_SEXT` emit a two-step sequence (narrow MOVZ + COPY). When the source and destination share the same register number (e.g., `MOVZ16 %r0, %r0d` + `COPY %r0d, %r0`), `copyPhysReg()` now detects that both registers map to the same underlying ETCa register number and skips the copy as a no-op.
**Impact**: Eliminated — only the correct narrow MOVZ/MOVS instruction remains.

### 3. MC tests: byte roundtrip ✅ FIXED
The InstPrinter now prints `%rNh` suffix for byte-width register operands. Full roundtrip is verified:
  - `disassembler.s`: byte instructions disassembled and reassembled (encode→decode→check)
  - `roundtrip-all.s` + `.dis`: byte instructions in comprehensive roundtrip
  - `byte.s`: encoding verification (show-encoding)

## TODO — Extension Improvements

### Tests
- [x] Legalizer subtarget-aware (HasDW/HasQW gate computation ops)
- [x] Multi-width tests split by CPU capability (16/32/64 bit)
- [x] Explicit FileCheck RUN lines for all 5 CPU models in CodeGen tests
- [ ] ELF object verification (EM_ETCA, section headers, relocations)
- [ ] Integration tests (Fibonacci, memcpy, recursive factorial)
- [ ] LLVM test suite integration

### Clang & Tools
- [x] clang driver support (`etca-unknown-elf` target triple, `-mcpu=` for 5 CPU models, proper DataLayout, preprocessor defines `__etca__`/`__ETCA__`, C++ name mangling, all word/address width combos supported)
- [ ] clang intrinsics for ETCa-specific operations (READCR, WRITECR, etc.)
- [ ] compiler-rt builtins (soft-float, div/mod, etc.)
- [ ] lld linker support (ETCA ELF linking)
- [ ] Assembly syntax tests cross-checked vs etca binutils output

### Driver Implementation Details
- `clang/lib/Basic/Targets/ETCA.{h,cpp}` — TargetInfo: dynamic type sizes via `setCPU()`, 5 CPU models (generic/etca32/etca32p64/etca64p32/etca64), LP-like C type model, GCC register names and aliases for inline asm, preprocessor defines (`__etca__`, `__ETCA__`, `__ETCA_GENERIC__`, `__ETCA32__`, etc., `__ETCA_WORD_SIZE__`, `__ETCA_PTR_SIZE__`, extension detection macros)
- `clang/lib/Driver/ToolChains/ETCA.{h,cpp}` — ToolChain: `Generic_ELF`-based, GCC installation discovery, ELF linker (cta-elf-ld), bare-metal defaults
- Registered in `Driver.cpp`, `Targets.cpp`, `Clang.cpp` (isSignedCharDefault), `CommonArgs.cpp` (getCPUName with `-mcpu=` mapping)

### Spec Conformance Audit
- [ ] Re-check instruction encodings against `etca-spec/base-isa.md` (SS bits, CCCC opcodes, condition codes)
- [ ] Re-check flag semantics (Z, N, C, V) against spec
- [ ] Re-check memory semantics (unaligned access, pointer width, memory-mapped IO)
- [ ] Re-check register file (Dwarf numbering, ABI names)
- [ ] Verify relocation types match `etca-binutils-gdb/` exactly

### Future Extensions
- [ ] extended registers (REX prefix)
- [ ] fi: full immediates (VWI prefix)
- [ ] mo1/mo2: complex memory operands
- [ ] expanded opcodes (EXOP prefix)
- [ ] conditional prefix
- [ ] bm1 (bit manipulation 1)

## Build Notes

```sh
# Release build
cmake -S llvm -B build-etca -G Ninja \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=ETCA \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=c++

# Build everything. NEVER build only individual targets!
ninja -C build-etca

# Quick smoke test
echo 'define i16 @add(i16 %a, i16 %b) {
  %r = add i16 %a, %b
  ret i16 %r
}' | build-etca/bin/llc -march=etca -mcpu=generic -filetype=asm

# Run all ETCA tests
llvm-lit build-etca/test/MC/ETCA/ build-etca/test/CodeGen/ETCA/
```

**Compiler note**: Clang 22.1.4 + libc++ has `abi_tag` incompatibility with `libDebugInfoGSYM`. GCC works but is slower.
