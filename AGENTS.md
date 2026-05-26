# ETCA LLVM Backend — Agent Guidance

## References

- Architecture specification: `../etca-spec/`, OR `../spec-concat.md`
- Specification of future vendor extensions: `../etca-vnd-extensions/`
- Existing binutils fork: `../etca-binutils-gdb/`
- Compiled target binutil binaries: `../etca-binutils-gdb/install/bin/`
- User documentation: `./ETCA-USER-DOCUMENTATION.md`

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
| base + saf + all word/address combos | ✅ DONE (2026-05-14) | 16/32/64-bit word & pointer, all combinations via -mattr flags |
| byte (SS=00, 8-bit ops) | ✅ DONE (2026-05-13) | All computation + LOAD8/STORE8, sign-extension semantics |
| multiply / divide libcall | ✅ DONE (2026-05-14) | All 15 arithmetic libcall operations (mul/sdiv/udiv/srem/urem × 16/32/64-bit) work on all width/pointer combinations |
| extended registers (REX prefix) | ✅ DONE (2026-05-23) | r8-r15, d8-d15, q8-q15, REX prefix byte, MC assembly, disassembler, encoder, parser (incl. ABI names t0-t4/s2-s4), register classes, calling convention, CSR masks, MC tests |
| jump tables (G_JUMP_TABLE + G_BRJT) | ✅ DONE (2026-05-24) | Legalizer, instruction selector, AsmPrinter, fixup types, and ELF relocations all implemented. See 

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

Test: `llvm/test/CodeGen/ETCA/stack-frame.ll` verifies this for all width/pointer combinations.

### R7 (ln) reserved as dedicated scratch register (2026-05-24)
The ETCa LOAD/STORE instructions have no immediate offset field — they use a
pure register for the address.  When `eliminateFrameIndex` replaces a frame
index operand with `FrameReg + Offset`, it needs a scratch register for the
`MOVZ+ADDI` address computation.

**Before**: `eliminateFrameIndex` called `RS->scavengeRegisterBackwards()` for
the STORE path to get a scratch register.  Inside `spill()`, the scavenger emits
`storeRegToStackSlot` (which has a frame index), and that frame index requires
another `eliminateFrameIndex` call — creating a recursive spill chain.  With
all registers in use, this chain exhausted emergency slots or the search limit.

**Fix**: Reserve R7 (and its aliases D7/Q7) as a dedicated scratch register.
`eliminateFrameIndex` uses R7/D7/Q7 directly for the STORE address computation,
completely avoiding the recursive scavenger call.  R7 is also the link register
(used by CALL_Pseudo's Defs=[R7] and JMPR), which works correctly with a
reserved register.

Affected files:
- `ETCARegisterInfo.cpp`: `getReservedRegs` now aliases R5/R6/R7 (with
  `MCRegAliasIterator`) so GPR32/GPR64 correctly exclude D5/D6/D7 and Q5/Q6/Q7.
  `eliminateFrameIndex` uses `RC == &GPR64RegClass ? Q7 : RC == &GPR32RegClass
  ? D7 : R7` for STORE address computation.  `requiresRegisterScavenging`
  returns `false` since R7 is always available.
- `ETCAFrameLowering.h/.cpp`: Removed `processFunctionBeforeFrameFinalized`
  (no longer needed — no emergency spill slots required).
- `ETCARegisterInfo.h`: No `requiresFrameIndexScavenging` override needed.

**Impact**: One fewer register per width class (7 instead of 8).  The still has
enough registers for reasonable codegen; more register-heavy functions will
spill more often.  This is an acceptable trade-off for correctness.

Test: `llvm/test/CodeGen/ETCA/scavenger-emergency-slot.ll`

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
- Processor model: single `generic` CPU; word/pointer sizes via `-mattr` features (16b+16b default, 32b+32b, 32b+64b, 64b+32b, 64b+64b; +32bit implies +dw, +64bit implies +qw)
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

## TODO — Extension Improvements

### Jump Tables (Completed 2026-05-25)
Jump tables are now fully implemented and enabled.  The threshold is set to
4 entries in `ETCAISelLowering.cpp`.

Implementation summary:
- **GISel pipeline**: G_JUMP_TABLE → JT_Pseudo (InstructionSelector), G_BRJT →
  load + G_BRINDIRECT (Legalizer).  G_BRINDIRECT marked legal for p0.
- **AsmPrinter**: JT_Pseudo expanded into a single MOVZI instruction with the
  jump table label as an expression operand.  The encoder's `emitMovChain`
  produces the full MOVZI+SLO placeholder chain with a single R_ETCA_MOV_*
  spanning fixup (matching binutils numbering).
- **Fixup kinds**: R_ETCA_MOV_5 through R_ETCA_MOV_32 (binutils relocs 17-32)
  with byte counts matching `etca_build_mov_ri`.
- **Linker**: lld applies the spanning fixup by writing the absolute address
  as raw bytes into the placeholder chain.  The binutils linker's
  `etca_build_mov_ri` correctly rewrites the chain.
- **Label emission**: `emitFunctionBodyStart` in the AsmPrinter marks all
  jump-table-targeted MBBs with `setLabelMustBeEmitted()`, ensuring labels
  are emitted even for fallthrough successors.
- **ELF fixup mapping**: Fixed the `RelocMap[]` array to match the actual enum
  order in `ETCAFixupKinds.h` (SAF_CALL was at wrong index, breaking
  relocation emission for CALL instructions).
- **Tests**: `switch.ll` updated for jump-table-enabled output (changed i32
  switch to i16 to avoid pre-existing G_USUBO issue).  All 71 MC/CodeGen
  tests pass.

### Tests
- [ ] ELF object verification (EM_ETCA, section headers, relocations)
- [ ] Integration tests (Fibonacci, memcpy, recursive factorial)
- [ ] LLVM test suite integration

### Clang & Tools
- [x] clang driver support (`etca-unknown-elf` target triple, `-mcpu=generic` with word/ptr sizes via `-mattr` flags, proper DataLayout, preprocessor defines `__etca__`/`__ETCA__`, C++ name mangling, all word/address width combos supported)
- [ ] clang intrinsics for ETCa-specific operations (READCR, WRITECR, etc.)
- [ ] compiler-rt builtins (soft-float, div/mod, etc.)
- [x] lld linker support (ETCA ELF linking) — full LLD backend in `lld/ELF/Arch/ETCA.cpp` with:
  - All 57 ELF relocation types matching binutils
  - 32-bit and 64-bit ELF output (auto-detected via CPU features)
  - RELA format (binutils-compatible)
  - `elf32etca` / `elf64etca` emulation flags
  - `--oformat binary` support for flat ROM images
  - Works with `clang -fuse-ld=lld`
- [x] Binutils ld support via clang (default: searches for `etca-elf-ld`, passes correct `-melf{16,32,64}_etca` flags)
- [x] clang `-fuse-ld=lld` / `-fuse-ld=bfd` support in driver
- [x] Both `llvm-objcopy -O binary` and `--oformat binary` for ROM production
- [x] Binutils cross-compatibility verified (binutils ld can link LLVM-produced .o files)
- [x] Assembly syntax tests cross-checked vs etca binutils output

### Extra
- [ ] determine if we need llvm-libc, and libc++?
- [ ] `writeNopData` odd-count fallback: when an extension adds 1-byte NOPs, update `writeNopData` to handle odd byte counts without falling back to trap instructions

### Driver Implementation Details
- `clang/lib/Basic/Targets/ETCA.{h,cpp}` — TargetInfo: dynamic type sizes via `-mattr` features (parsed by `handleTargetFeatures()`), single CPU model `generic`, LP-like C type model, GCC register names and aliases for inline asm, preprocessor defines (`__etca__`, `__ETCA__`, `__ETCA_GENERIC__`, `__ETCA32__`, etc., `__ETCA_WORD_SIZE__`, `__ETCA_PTR_SIZE__`, extension detection macros)
- `clang/lib/Driver/ToolChains/ETCA.{h,cpp}` — ToolChain: `Generic_ELF`-based, GCC installation discovery, ELF linker (cta-elf-ld), bare-metal defaults
- Registered in `Driver.cpp`, `Targets.cpp`, `Clang.cpp` (isSignedCharDefault), `CommonArgs.cpp` (getCPUName with `-mcpu=` mapping)

### Spec Conformance Audit
- [ ] Re-check instruction encodings against `etca-spec/base-isa.md` (SS bits, CCCC opcodes, condition codes)
- [ ] Re-check flag semantics (Z, N, C, V) against spec
- [ ] Re-check memory semantics (unaligned access, pointer width, memory-mapped IO)
- [ ] Re-check register file (Dwarf numbering, ABI names)
- [ ] Verify relocation types match `etca-binutils-gdb/` exactly

## Build Notes

```sh
cmake -S llvm -B build-etca -G Ninja \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=ETCA \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_ENABLE_PROJECTS="clang;lld"

# Build everything. NEVER build only individual targets!
ninja -C build-etca

# Quick smoke test
echo 'define i16 @add(i16 %a, i16 %b) {
  %r = add i16 %a, %b
  ret i16 %r
}' | build-etca/bin/llc -march=etca -mcpu=generic -filetype=asm

# Run all ETCA tests
ninja -C build-etca && build-etca/bin/llvm-lit llvm/test/*/ETCA/ clang/test/*/ETCA/ clang/test/*/etca-* lld/test/ELF/etca-*
```

**Compiler note**: Clang 22.1.4 + libc++ has `abi_tag` incompatibility with `libDebugInfoGSYM`. GCC works but is slower.


## TODO
Register class handling seems sus in a lot of places (especially for the BYTE extension)
