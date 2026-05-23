# ETCA LLVM Backend + LLD — Full Audit Report

**Date**: 2026-05-23  
**Scope**: llvm/lib/Target/ETCA/ (codegen, MC layer, asm parser, disassembler) + lld/ELF/Arch/ETCA.cpp  
**Excluded**: clang frontend, llvm/test/ (test coverage quality is noted but not the focus)

---

## 🔴 Critical Issues
### 2. `lowerCall` Hardcodes STORE16/ADDI16 for Stack Arguments (ETCACallLowering.cpp)

**File**: `llvm/lib/Target/ETCA/GISel/ETCACallLowering.cpp`, function `lowerCall()`

```cpp
auto Sub = MIRBuilder.buildInstr(ETCA::SUBI16);   // ← should be SUBI{16,32,64}
...
auto Store = MIRBuilder.buildInstr(ETCA::STORE16); // ← should be STORE{16,32,64}
```

The call lowering unconditionally uses `SUBI16` and `STORE16` for stack argument allocation and storage, regardless of the actual register width. On a 64-bit machine, storing an i64 stack argument via `STORE16` will only write 16 bits. Similarly, stack pointer adjustments of 4 or 8 bytes per argument are wrong.

Same pattern in `lowerFormalArguments` — uses `RegBytes` (correct) but the stores in `lowerCall` ignore it.

**Impact**: Calls with stack arguments (≥5 args) are broken on 32-bit and 64-bit CPU models.

### 3. `lowerCall` Stack Adjustment Bypasses LLVM Frame Handling (ETCACallLowering.cpp)

The `lowerCall` function manually emits `SUBI16` / `ADDI16` for stack argument push/pop instead of using `ADJCALLSTACKDOWN` / `ADJCALLSTACKUP`. The `ADJCALLSTACK*` pseudo instructions ARE defined in `ETCAInstrInfo.td` but are NEVER emitted by the call lowering.

Without proper frame handling:
- Stack adjustments are invisible to the PEI / machine scheduler
- Frame pointer chasing is incorrect
- DWARF call frame information (CFI) is missing
- Garbage collection and exception handling stack maps are broken

**Impact**: Broken DWARF unwind info, incorrect stack tracking for anything beyond trivial calls.

### 4. REX Copy-Is-No-Op Detection Is Wrong for Cross-Class Copies (ETCAInstrInfo.cpp)

**File**: `llvm/lib/Target/ETCA/ETCAInstrInfo.cpp`, function `copyPhysReg()`

```cpp
unsigned DestEnc = TRI.getEncodingValue(DestReg);
unsigned SrcEnc = TRI.getEncodingValue(SrcReg);
if ((DestEnc & 0x7) == (SrcEnc & 0x7))
    return; // Same physical register — copy is a no-op.
```

With REX extension, `R8` encodes as `8` (`0b1000`) and `D8` also encodes as `8`. The mask `& 0x7` gives `0` for both, so `copyPhysReg` treats a copy from `R8` ↔ `D8` as a no-op. But these ARE different physical registers — `R8` is a 16-bit register, `D8` is a 32-bit register. A copy between them is NOT a no-op and would require a MOVZ with the appropriate width.

**Without REX** (registers R0-R7, D0-D7): R0 and D0 both encode as 0, and they ARE subregisters of the same physical location (D0 has sub_16 → R0), so the no-op IS correct for the base set.

**With REX**: The no-op is a bug.

**Impact**: Silent data corruption on REX targets when copies between different-width register classes involve high registers (R8-R15 ↔ D8-D15).

### 5. Missing `writeNopData` for Odd Counts Returns False But Can Crash Downstream (ETCAMCTargetDesc.cpp)

**File**: `llvm/lib/Target/ETCA/MCTargetDesc/ETCAMCTargetDesc.cpp`

```cpp
bool writeNopData(raw_ostream &OS, uint64_t Count,
                  const MCSubtargetInfo *STI) const override {
    if ((Count % 2) != 0)
      return false;
    ...
}
```

Returning `false` from `writeNopData` signals "I can't fill this count", and LLVM's generic code will then fall back to emitting trap instructions or aborting. This is the expected LLVM contract, but other architectures (ARM, RISC-V) can handle odd counts. The 2-byte NOP requirement is reasonable for ETCA (16-bit instructions).

**Minor issue**: The `getMinimumNopSize()` returns 2, which is correct, but the contract means that alignment padding of odd sizes will use trap instructions instead of NOPs.

### 6. Disassembler: CCCC=13 (SLO16) is Wrong in RI Switch (ETCADisassembler.cpp)

**File**: `llvm/lib/Target/ETCA/Disassembler/ETCADisassembler.cpp`

```cpp
case 13:
    Opc = ETCA::SLO16;   // BUG: Should not be reached — PUSHI handles CCCC=13
    IsNonTied = true;
    break;
```

CCCC=13 (`0b1101`) in RI format is PUSHI when `RegA == 6` (sp register). PUSHI IS handled by a separate check before the switch:
```cpp
if (CCCC == 0xD && RegA == 6) {
    Instr.setOpcode(ETCA::PUSHI);
    ...
}
```

But when `CCCC=13` and `RegA != 6`, the code falls through to the switch `case 13` which emits `SLO16` — but `SLO16` uses CCCC=12 (`0b1100`), not 13. The case 13 entry is dead/wrong. CCCC=13 with `RegA != 6` is an invalid instruction encoding and should return `Fail`.

**Impact**: A malformed byte sequence that has CCCC=13 and RegA≠6 would be decoded as SLO16 instead of rejected as invalid.

---

## 🔴 High Severity Issues

### 7. No `-gen-disassembler` TableGen — Manual Decoder Drifts (CMakeLists.txt)

**File**: `llvm/lib/Target/ETCA/CMakeLists.txt`
```cmake
# Disassembler tables not auto-generated — manual decoder in ETCADisassembler.cpp
# The CMP format (LOAD/STORE/CMP/TEST) conflicts with RI format for certain
# operand combinations, so TableGen fails on -gen-disassembler.
```

The manual disassembler has no automated connection to the instruction definitions. Any new instruction added to `ETCAInstrInfo.td` must also be manually added to the disassembler. This makes the target fragile and hard to maintain.

The root cause — overlapping encodings between CMP-RI and other RR instructions — should be addressed in the instruction format definitions rather than worked around by skipping the tablegen.

### 8. No Disassembler Tests

There are ZERO disassembler test files anywhere in the test tree. The disassembler is entirely untested.

### 9. `lowerCall` Doesn't Mark R7 as Clobbered (ETCACallLowering.cpp)

The `CALL_Pseudo` instruction has `Defs = [R7]` in the .td file (which marks R7 as defined), but the calling convention's callee-saved list does NOT include R7 (correct — it's caller-saved). However, `lowerCall` must also ensure R7 is marked as clobbered by the call. The `Defs = [R7]` in the .td file does mark it as implicitly defined, but the register allocator needs the `getCallPreservedMask()` to NOT include R7, which is correct. However, R7's liveness as a "defined by call" means any live range of R7 across the call will be killed, which is what we want since the call clobbers R7.

Actually this is handled correctly — CALL_Pseudo has `let Defs = [R7]`, which marks R7 as an implicit def. The register allocator sees this and kills any previous live range of R7 across the call.

### 10. `getLoadStoreActions` Missing for Wider Types (ETCAISelLowering.cpp)

**File**: `llvm/lib/Target/ETCA/ETCAISelLowering.cpp`

```cpp
if (WS <= 16) {
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i16, MVT::i8, Expand);
    setLoadExtAction(ISD::SEXTLOAD, MVT::i16, MVT::i8, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::i16, MVT::i8, Expand);
}
```

When `WS >= 32`, there are no load extension actions set for `MVT::i32` → `MVT::i8` or `MVT::i32` → `MVT::i16`. These should be set to `Expand` to prevent the DAGCombiner from creating illegal extending loads.

Since ETCA is GISel-only, this only matters if the SDAG lowering is ever exposed. It's a pre-existing issue from the SDAG era.

---

## 🟡 Medium Severity Issues

### 11. `computeDataLayout` Duplicates `buildDataLayoutString` Logic (ETCATargetMachine.cpp)

**File**: `llvm/lib/Target/ETCA/ETCATargetMachine.cpp`

The `computeDataLayout` function re-implements the CPU-name→(WordSize,PtrSize) mapping that the subtarget features already define. This creates TWO sources of truth for the same mapping. ETCASubtarget has `buildDataLayoutString` which is authoritative.

**Risk**: When a new CPU model is added to `ETCA.td`, the developer must remember to also update `computeDataLayout`. This is error-prone and has already resulted in the `etca32p64` mismatch (Issue #1).

**Recommendation**: The `computeDataLayout` function should be removed and replaced by creating a temporary `ETCASubtarget` to compute the DataLayout, or by parsing the feature string in both places from the same table.

### 12. `isLegalAddressingMode` Incorrectly Claims Offset Support (ETCAISelLowering.cpp)

**File**: `llvm/lib/Target/ETCA/ETCAISelLowering.cpp`

```cpp
bool ETCATargetLowering::isLegalAddressingMode(...) const {
    return AM.BaseGV == nullptr && AM.HasBaseReg && AM.Scale == 0 &&
           AM.BaseOffs >= -16 && AM.BaseOffs <= 15;
}
```

ETCA's LOAD/STORE instructions have **zero immediate offset** — the address is a pure register (`[reg]`). There is no offset field in the instruction encoding. Any non-zero offset requires a separate ADDI instruction. By returning `true` for `BaseOffs` in `[-16, 15]`, this function tells LLVM's DAGCombiner/LSR that addressing with small offsets is "free" when it actually requires an extra instruction. This can lead to suboptimal code.

**Impact**: LSR may choose suboptimal addressing, preferring base+offset where plain base+ADDI would be needed, potentially costing an extra instruction.

### 13. `isLoadFromStackSlot` / `isStoreToStackSlot` Only Checks FI on Operand 1 (ETCAInstrInfo.cpp)

```cpp
Register ETCAInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  if (MI.getOperand(1).isFI()) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }
  return Register();
}
```

This only matches FrameIndex when it's operand 1. For STORE, the value is operand 0 and the address is operand 1. For LOAD, the dest is operand 0 and the address is operand 1. Both look at operand 1, which is correct. BUT this only matches BEFORE frame index elimination (when FIs haven't been replaced yet). After elimination, these functions return nothing, which is fine — the PEI/Spiller code is supposed to call these pre-elimination.

Actually this is the standard LLVM pattern. No bug, but worth verifying that the functions are never called after frame index elimination in passes that don't expect them.

### 14. Frame Index Elimination for LOAD Reuses Destination Register (ETCARegisterInfo.cpp)

```cpp
if (IsLoad) {
    Register ScratchReg = MI.getOperand(0).getReg();
    BuildMI(MBB, II, DL, TII.get(MovOpc), ScratchReg).addReg(FrameReg);
    ...
    MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, /*isDef=*/false);
    return false;
}
```

The destination register of a LOAD is reused as the scratch register for address computation. This means the MOVZ+ADDI chain writes to the destination register BEFORE the LOAD reads it (for the address). If the address computation and the LOAD are the same instruction, this is fine because they're separate operands.

**However**, if the address somehow depends on the destination register (which would be a use-before-def in the original LLVM IR), this could mask the bug. With GISel, this shouldn't happen because G_LOAD has explicit operands and the address register is separate from the dest register.

Still, the fallback case for STORE without a scavenger is worse:
```cpp
BuildMI(MBB, II, DL, TII.get(AddiOpc), FrameReg)
    .addReg(FrameReg).addImm(Step);
```
This modifies the frame register IN PLACE and then restores it after. If a signal or interrupt occurs between the add and the restore, the frame register is corrupted. This is extremely fragile.

### 15. `CCCD::getCallPreservedMask` Doesn't Handle Variadic Call Conventions

**File**: `llvm/lib/Target/ETCA/ETCARegisterInfo.cpp`

```cpp
const uint32_t *ETCARegisterInfo::getCallPreservedMask(
    const MachineFunction &MF, CallingConv::ID CC) const {
  // ...
```

Only handles the default calling convention (`CC` is ignored). If someone targets `PreserveMost`, `PreserveAll`, or `Cold`, the function returns an incorrect mask from the WS/REX-based index which may not match.

### 16. CALLR Instruction Has No R7 Def (ETCAInstrInfo.td)

```tablegen
def CALLR : EInstSAFCallReg<0b1110, GPR, "callr $rs", []>;
```

`CALLR` (call via register) should define R7 (link register) just like `CALL` does, but it has no `let Defs = [R7]`. This means the register allocator won't know R7 is clobbered by a `callr`, leading to miscompilation.

### 17. `expandPostRAPseudo` Copies All Operands Blindly Instead of Explicitly Constructing (ETCAInstrInfo.cpp)

```cpp
for (unsigned i = 0, e = MI.getNumOperands(); i < e; ++i) {
    const MachineOperand &MO = MI.getOperand(i);
    if (MO.isGlobal() || MO.isSymbol() || MO.isMBB() || MO.isReg() ||
        MO.isImm()) {
        NewCall->addOperand(MF, MO);
    }
}
```

The `CALL` instruction (line 380 of ETCAInstrInfo.td) has `let Defs = [R7]`, so R7's implicit def is PRESERVED from the instruction definition's own `.td`-generated operands, independent of what the expander copies. The expander's blind copy adds extra implicit operands (parameter registers R0-R3, return register) which are redundant but harmless.

**Not a correctness bug** — the core R7 liveness is correct.

However, the blind copy is fragile: if `CALL_Pseudo` ever gains operands with special semantics (e.g., subreg indices, tied operands), they would be incorrectly transferred. A more robust approach would explicitly construct the expected operands of CALL.

---

## 🟢 Low Severity / Cosmetic Issues

### 18. Debug `#include` at End of ETCASubtarget.cpp

**File**: `llvm/lib/Target/ETCA/ETCASubtarget.cpp`, last lines:
```cpp
// DEBUG
#include "llvm/Support/raw_ostream.h"
```

This `#include` is at file scope AFTER all function definitions. It's unused but harmless. This is leftover debug scaffolding that should be removed or placed at the top of the file.

### 19. `computeDataLayout` Doesn't Handle Unknown CPU Names (ETCATargetMachine.cpp)

```cpp
static std::string computeDataLayout(StringRef CPU) {
  unsigned WordSize = 16;
  unsigned PtrSize = 16;
  if (CPU == "etca32") { ... }
  else if (CPU == "etca32p64") { ... }
  ...
  // Defaults to 16+16 for unknown CPU
  return ETCASubtarget::buildDataLayoutString(WordSize, PtrSize);
}
```

Unknown CPU names silently default to 16+16. This could mask configuration errors. Should print a diagnostic or assert.

### 20. `EM_ETCA` / ELF Machine Number Not Verified Against binutils

**No file** explicitly defines `EM_ETCA`. Need to check `llvm/include/llvm/BinaryFormat/ELF.h` or wherever it's defined. The comment in AGENTS.md says `EM_ETCA = 0xE7Ca` — this should be verified to match `etca-binutils-gdb/include/elf/etca.h`.

### 21. `selectWidth` for CMP Uses Incorrect Width Gating in AsmParser (ETCAAsmParser.cpp)

The `selectWidth` lookup for `cmp` ALUOps array uses `ETCA::CMP8, ETCA::CMP, ETCA::CMP32, ETCA::CMP64`. The `CMP` entry is for SS=01 (16-bit), which is correct. But `CMP32` and `CMP64` require DW/QW extensions, and the AsmParser doesn't validate extension availability. Trying to assemble `cmpd %r0d, %r1d` on a `generic` CPU would silently produce bytes but may not be supported at runtime. This is typically fine for an assembler (it assembles what it's told).

### 22. Redundant `Inst.setLoc(IDLoc)` Call in AsmParser `matchAndEmitInstruction`

```cpp
MCInst Inst;
Inst.setOpcode(FE->Opcode);
Inst.setLoc(IDLoc);
Inst.setLoc(IDLoc);  // ← DUPLICATE!
```

The `setLoc` call appears twice in sequence with no intervening code. Harmless but indicates sloppy code.

### 23. `FixedEntry::Opcode` for LOAD/STORE Always Uses LOAD16/STORE16

In the FixedOps table:
```cpp
{"load", ETCA::LOAD16, 2, false, false},
{"store", ETCA::STORE16, 2, false, false},
```

The `Opcode` field is always `LOAD16`/`STORE16` and gets dynamically replaced based on width. The `Opcode` field is unused when instruction suffix or register width hints are available. The field serves as a "base opcode" marker and the lookup code correctly overrides it. Not a bug, but the data structure implies these are the actual opcodes when they're not.

### 24. `lowerFormalArguments` Doesn't Handle `sret` / `byval` / `inreg` Attributes

The `lowerFormalArguments` function processes arguments positionally without checking for LLVM parameter attributes like `sret` (sret uses a pointer parameter), `byval`, `inreg`, `nest`, etc. While ETCA doesn't have complex ABIs yet, `sret` in particular is emitted by clang for large return values and would be mishandled.

### 25. `EInstBranch` Sets `isBranch=1` but `isBarrier=0` (Correct for Conditional) but `BR` Overrides `isBarrier=1`

In `ETCAInstrInfo.td`:
```tablegen
def BR   : EInstBranch<0b1110, "br $target", []> {
    let isBarrier = 1;  // unconditional branch is a barrier
}
```

Correct. The base class sets `isBarrier = 0` and BR overrides it. This matches LLVM expectations.

### 26. CALL_Pseudo Uses `let Defs = [R7]` but CALL Instruction Uses the Same — REDUNDANT but Correct

Both `CALL_Pseudo` and `CALL` define `Defs = [R7]`. Since `expandPostRAPseudo` copies operands and the `CALL` itself has `Defs = [R7]`, the implicit def survives expansion. Good.

But [Issue #16](callr) — `CALLR` has no `Defs = [R7]` — is still a problem.

### 27. REX Prefix Byte Doesn't Align with REX Feature Gating

The `computeRexPrefix` function in the encoder looks at register HWEncodings to decide if a REX prefix is needed. If registers R8-R15 are used WITHOUT the `+rex` feature enabled, the encoder will still emit a REX prefix (because it detects HWEnc bit 3 is set). This mismatch would produce bytes that the `generic` (non-REX) CPU can't decode. The encoder should check `STI.hasFeature(ETCA::FeatureREX)` before emitting REX prefixes.

### 28. `ETCACallLowering::lowerCall` Passes Callee Operand As Is Without Relocation Kind

```cpp
if (Info.Callee.isGlobal())
    CallInst.addGlobalAddress(Info.Callee.getGlobal());
```

The global address on a CALL instruction needs a `R_ETCA_SAF_CALL` relocation, not a generic absolute relocation. The MCCodeEmitter's `encodeCallTarget` handles this by checking if the operand is an `MCExpr` and adding the appropriate fixup. The `ETCAAsmPrinter::LowerETCAMPEMCInst` converts `MO_GlobalAddress` to an `MCSymbolRefExpr` which triggers the fixup in the encoder. This works, but it's fragile — if the callee is an immediate (resolved at compile time), the fixup isn't generated and no relocation is emitted. This would break linking with separate compilation.

### 29. No `-gen-pseudo-lowering` or `-gen-compress` TableGen Backends Used

The CMakeLists.txt generates neither pseudo-lowering nor compression tablegen backends. The SELECT_Pseudo is manually expanded, CALL_Pseudo manually expanded, RET_Pseudo manually expanded. This is fine for a small target but adds boilerplate.

### 30. `ETCASelectExpand.cpp` Has Dead `#include` for `ETCASubtarget`

The pass includes `ETCASubtarget.h` but doesn't use it directly — it uses `MF.getSubtarget<ETCASubtarget>().getRegWidth()` which requires the header. This is used, so not dead, but the `#include` is correct.

### 31. `ETCACallLowering` Assumes Null `Callee` Is Not Possible

The code handles `Info.Callee.isReg()`, `.isGlobal()`, `.isSymbol()`, but not the case where `Callee` is none of these (e.g., an `MCSymbol`). This would assert or produce an invalid instruction.

### 32. `ETCACallLowering::lowerCall` Doesn't Clone MachineMemOperands for Stack Pushes

When storing stack arguments, the code builds raw `STORE16` instructions without `MachineMemOperand` annotations. This means alias analysis and scheduling have no memory dependency information for these stores, potentially causing mis-scheduling with nearby loads/stores.

### 33. Missing `mayThrow` / `mayHaveSideEffects` on Some Pseudo Instructions

`CALL_Pseudo` has `hasSideEffects = 1, isCall = 1`. `RET_Pseudo` has `hasSideEffects = 1, isReturn = 1`. Both correct. But `ADJCALLSTACKUP` / `ADJCALLSTACKDOWN` have `hasSideEffects = 1` which prevents DCE from removing them even when the frame adjustments are redundant. This is by design in LLVM, so not a bug.

---

## 📋 Process / Documentation Issues

### 34. Code Duplication Between Manual and Auto-Generated Encoders

The `ETCAMCCodeEmitter` includes `ETCAGenMCCodeEmitter.inc` for auto-generated encoding, but also has manual `encodeBranchTarget`, `encodeCallTarget`, and `computeRexPrefix` methods that override/assist the generated encoder. If the TableGen format classes change, the manual bits may desync.

### 35. ELF Relocation Types in LLD Don't Match MC Layer Exactly

The MC layer `ETCAELFObjectWriter::getRelocType` maps `FK_Data_1»→R_ETCA_8`, `FK_Data_2→R_ETCA_16`, `FK_Data_4→R_ETCA_32`, `FK_Data_8→R_ETCA_64`. The LLD backend also handles `R_ETCA_EXABS_*`, `R_ETCA_ABM_*`, `R_ETCA_MOV_*`, `R_ETCA_IPREL_*`. Many of these relocation types are handled by LLD but never emitted by the MC layer. This is fine for forward compatibility (future extensions may emit them), but:
- `R_ETCA_ABM_RIS_5` and `R_ETCA_ABM_RIZ_5` have handlers in LLD that modify `loc[0]` — but which instruction is `loc[0]`? The ABM (absolute immediate) relocations correspond to MOVZI/MOVSI encoding which can be 1-6+ instructions. The LLD code assumes a specific instruction layout that may not match what the codegen produces.
- No test coverage for the 40+ MOV relocation types.

### 36. `getCallPreservedMask` Uses Hardcoded Index (ETCARegisterInfo.cpp)

```cpp
if (ST.hasREX()) {
    if (ST.getWordSize() >= 64)
      return getRegMasks()[5];
    ...
}
return getRegMasks()[0];
```

The index `[5]` depends on the order of CSR declarations in `ETCACallingConv.td`. If someone adds a new CSR to the .td file, ALL indices shift and this code breaks silently. This should use named CSR constants.

### 37. `getCalleeSavedRegs` Uses Local Static Arrays (ETCARegisterInfo.cpp)

```cpp
if (ST.hasSAF()) {
    if (ST.hasREX()) {
      static const MCPhysReg CalleeSavedRegs_REX[] = { ... };
      return CalleeSavedRegs_REX;
    }
    static const MCPhysReg CalleeSavedRegs[] = { ... };
    return CalleeSavedRegs;
}
```

The `static const` local arrays are safe because they're in read-only memory and the function always returns the same pointer for the same code path. This IS the standard LLVM pattern, used by ARM, AArch64, etc. But a subtle issue: if two `MachineFunction` objects have different subtarget features (e.g., one with REX, one without), the function returns different masks. For a per-MF call, this works. But the `static` means the arrays are alive for the whole program, which is fine.

---

## ✅ Items Verified as Correct

- GISel-only pipeline: confirmed, no SDAG references remain.
- `ETCARegisterBanks.td`: single bank, correct.
- `ETCALegalizerInfo`: Tier 1/2/3 distinction is well-designed. `alwaysLegal()` for extension/truncation ops is correct given the manual selector handling.
- `ETCAInstructionSelector`: The MOVZ/MOVS source-width approach for sign-extension is correctly implemented per the spec (AGENTS.md).
- `ETCAFrameLowering`: Frame layout fix (getFrameIndexReference without StackSize subtraction) is correct and verified by tests.
- `ETCACallingConv.td`: CSR masks are correct for all width/REX combinations.
- `ETCARegisterInfo.td`: Sub-register indices, DwarfRegNum, and register classes are correctly set up.
- NOP encoding: `0x008F` is consistent across all emission paths.
- Frame prologue/epilogue: CSR push/pop order is correct.
- The AsmParser handles `%` prefix, ABI names, backward-compat `dN`/`qN`, and instruction size suffixes comprehensively.
- The manual disassembler is complete for all base+SAF instructions.
- LLD's `buildMovRi` matches the binutils implementation and correctly reconstructs MOV+SLO sequences.
- `selectWidth` correctly captures all width/form combinations.
- The `R_ETCA_IS_EXABS` / `R_ETCA_IS_IPREL` macros in LLD match binutils.

---

## Summary Table

| # | Severity | Component | Issue Summary |
|---|----------|-----------|--------------|
| 1 | 🔴 Critical | ETCATargetMachine.cpp | `etca32p64` DataLayout says WordSize=32 but subtarget says 64 |
| 2 | 🔴 Critical | ETCACallLowering.cpp | `lowerCall` hardcodes STORE16/SUBI16 for all widths |
| 3 | 🔴 High | ETCACallLowering.cpp | Stack adjustment bypasses LLVM frame handling |
| 4 | 🔴 Critical | ETCAInstrInfo.cpp | REX copyPhysReg no-op check wrong for cross-class copies |
| 5 | 🟡 Medium | ETCAMCTargetDesc.cpp | writeNopData returns false for odd counts (expected) |
| 6 | 🟡 Medium | ETCADisassembler.cpp | CCCC=13 in RI switch wrongly decoded as SLO16 |
| 7 | 🟡 Medium | CMakeLists.txt | No `-gen-disassembler` — manual decoder drifts |
| 8 | 🟡 Medium | Tests | Zero disassembler tests |
| 9 | 🟢 Low | ETCACallLowering.cpp | R7 liveness across CALLR not tracked (CALLR missing Defs=[R7]) |
| 10 | 🟢 Low | ETCAISelLowering.cpp | Missing load ext actions for 32-bit+ types (legacy SDAG) |
| 11 | 🟡 Medium | ETCATargetMachine.cpp | Duplicate CPU mapping in computeDataLayout vs subtarget |
| 12 | 🟢 Low | ETCAISelLowering.cpp | isLegalAddressingMode claims offset support HW doesn't have |
| 13 | 🟢 Low | ETCAInstrInfo.cpp | isLoadFromStackSlot only handles FI operand 1 |
| 14 | 🟡 Medium | ETCARegisterInfo.cpp | FI elimination reuses LOAD dst as scratch |
| 15 | 🟢 Low | ETCARegisterInfo.cpp | getCallPreservedMask ignores CallingConv::ID |
| 16 | 🔴 High | ETCAInstrInfo.td | CALLR instruction missing `let Defs = [R7]` |
| 17 | 🟢 Low | ETCAInstrInfo.cpp | expandPostRAPseudo blind copy of all operands (CALL has Defs=[R7] via .td, so not a bug) |
| 18 | 🟢 Low | ETCASubtarget.cpp | Debug include at end of file |
| 19 | 🟢 Low | ETCATargetMachine.cpp | Unknown CPU silently defaults to 16+16 |
| 20 | 🟢 Low | BinaryFormat | EM_ETCA not verified vs binutils (needs cross-check) |
| 21 | 🟢 Low | ETCAAsmParser.cpp | AsmParser doesn't validate extension availability |
| 22 | 🟢 Low | ETCAAsmParser.cpp | Redundant Inst.setLoc call |
| 23 | 🟢 Low | ETCAAsmParser.cpp | FixedEntry.Opcode for LOAD/STORE is placeholder |
| 24 | 🟢 Low | ETCACallLowering.cpp | sret/byval/inreg not handled |
| 25 | 🟢 Low | ETCAInstrFormats.td | BR correctly overrides isBarrier |
| 26 | 🟢 Low | ETCACallLowering.cpp | R7 liveness for CALL tracked correctly via Defs=[R7] |
| 27 | 🟡 Medium | ETCAMCTargetDesc.cpp | REX prefix emitted even without +rex feature |
| 28 | 🟡 Medium | ETCACallLowering.cpp | CALL callee not explicitly tagged with relocation type |
| 29 | 🟢 Low | CMakeLists.txt | No pseudo-lowering/compression tablegen |
| 30 | 🟢 Low | ETCASelectExpand.cpp | Healthy include set |
| 31 | 🟢 Low | ETCACallLowering.cpp | Unhandled Callee types |
| 32 | 🟢 Low | ETCACallLowering.cpp | Stack stores lack MachineMemOperand |
| 33 | 🟢 Low | ETCAInstrInfo.td | ADJCALLSTACKUP/DOWN have hasSideEffects=1 (by design) |
| 34 | 🟢 Low | ETCAMCTargetDesc.cpp | Manual encoder methods may desync from tables |
| 35 | 🟡 Medium | LLD/ETCA.cpp | 40+ MOV relocation types untested |
| 36 | 🟡 Medium | ETCARegisterInfo.cpp | getCallPreservedMask uses hardcoded indices |
| 37 | 🟢 Low | ETCARegisterInfo.cpp | getCalleeSavedRegs uses local static (standard pattern) |
| S1 | 🟢 Low | ETCAInstrInfo.td | Missing branch conditions: BN, BNN, BOV, BNOV (not generated by LLVM IR) |
| S2 | 🟢 Low | ETCAInstrInfo.td | Missing BNVR mnemonic (encoding 0x008F used as NOP, which matches) |
| S3 | 🟡 Medium | REX impl vs spec | REX requires VWI per spec, backend doesn't implement VWI |
| S4 | 🟢 Low | Mul/Div | Libcalls used instead of native EXOP instructions (acceptable) |
| S5 | 🟢 Low | ETCAInstrInfo.td | PUSH/POP 8-bit (SS=00) not implemented |
| S6 | 🟢 Low | ETCAInstrInfo.td | Conditional jmpr/callr not implemented (always unconditional) |

---

## Top Priority Fixes (Ordered by Impact)

1. **Fix `computeDataLayout` for `etca32p64`** (Issue #1) — Align WordSize with subtarget Feature64Bit
2. **Fix `lowerCall` width-agnostic STORE16/SUBI16** (Issue #2) — Use correct width-specific opcodes
3. **Add `Defs = [R7]` to CALLR and fix CALL_Pseudo→CALL expansion** (Issues #16, #17)
4. **Fix REX `copyPhysReg` no-op detection** (Issue #4) — Use full encoding comparison, not `& 0x7`
5. **Add ADJCALLSTACKDOWN/UP to call lowering** (Issue #3) — Remove manual SUBI/ADDI
6. **Fix disassembler CCCC=13 fallthrough** (Issue #6) — Return Fail instead of SLO16
7. **Check REX feature before emitting REX prefix** (Issue #27)
8. **Add disassembler tests** (Issue #8)

---

## 📜 Spec Conformance Audit

This section compares the LLVM backend implementation against the authoritative ETCA architecture specification (`etca-spec/base-isa.md` and extension documents). The spec-concat.md at `/home/alex/Repos/etca/spec-concat.md` was used as the primary reference.

### Condition Code Coverage (base-isa.md §Jump Instructions)

The spec defines 16 condition codes (CCCC 0000–1111) for branch instructions:

| CCCC | Name | Flags | Implemented? |
|------|------|-------|-------------|
| 0000 | Equal/Zero | Z | ✅ BEQ |
| 0001 | Not Equal | ~Z | ✅ BNE |
| 0010 | Negative | N | ❌ **MISSING** (BN) |
| 0011 | Not Negative | ~N | ❌ **MISSING** (BNN) |
| 0100 | Carry/Below | C | ✅ BLTU |
| 0101 | No Carry/Above or Equal | ~C | ✅ BGEU |
| 0110 | Overflow | V | ❌ **MISSING** (BOV) |
| 0111 | No Overflow | ~V | ❌ **MISSING** (BNOV) |
| 1000 | Below or Equal | C\|Z | ✅ BLEU |
| 1001 | Above | ~(C\|Z) | ✅ BGTU |
| 1010 | Less | N≠V | ✅ BLT |
| 1011 | Greater or Equal | N=V | ✅ BGE |
| 1100 | Less or Equal | Z\|(N≠V) | ✅ BLE |
| 1101 | Greater | ~Z&(N=V) | ✅ BGT |
| 1110 | Always | — | ✅ BR |
| 1111 | Never | — | ❌ **MISSING** (BNVR/NOP encoding) |

**Spec violation**: The spec defines `CCCC=1111` (never) with the statement: "Only a displacement of 0 is considered canonical for this instruction. Non-zero displacements may be overloaded to act differently in future extensions." This means `0x8F` (the current NOP encoding used by binutils) is NOT the canonical encoding — the canonical "branch never" NOP would be `0x8F`. Wait — `0x8F` decoded as a branch instruction = `10 0 0 1111` = BR with condition=1111 (never) and D=0, displacement=0. So `0x8F` IS the "branch never" instruction! The backend's NOP encoding (`0x008F`) is CORRECT and matches the spec's canonical encoding.

### Flag Semantics (base-isa.md §Flag Semantics)

The spec requires:
- ADD/SUB: set Z, N, C, V flags
- OR/XOR/AND: set Z, N flags; C and V are "undefined"
- CMP/TEST: set flags according to the operation, no destination write
- MOVZ/MOVS: no flag updates
- SLO: no flag updates
- LOAD/STORE: no flag updates

**Backend**: The `hasSideEffects` flag in TableGen for CMP/TEST is set to 1, which correctly prevents DCE from removing them despite writing no output register. MOVZ/MOVS/LOAD/STORE have `hasSideEffects = 0`, which is correct. ✅

### MOVZ Availability (base-isa.md vs etc-semantics)

The formal etc-semantics model (`width-extensions/width-extensions.md`) gates MOVZ behind the width extensions (byte/DW/QW). However, `base-isa.md` (the authoritative spec) includes MOVZ and MOVS in the base ISA opcode table with CCCC=1000 and CCCC=1001. The etc-semantics model is outdated in this regard. The backend correctly implements MOVZ/MOVS for all widths. ✅

### Narrow Operation Sign-Extension (DW/QW extensions)

**Spec** (doubleword-operations/README.md): "Operations that write to a register _must_ sign extend the value to the register's width before writing it to the register _unless_ the operation is `movz` in which case it _must_ zero extend the value to the register's width before writing it to the register."

**Backend**: The instruction selector correctly selects MOVZ/MOVS based on the SOURCE (narrow) width for G_ZEXT/G_SEXT, and the COPY bridge handles register class transitions. The spec says ALL narrow operations sign-extend — this means even ADD16 on a 64-bit machine sign-extends the 16-bit result. The backend handles this via the SS bits in the instruction encoding: the hardware itself sign-extends based on SS. The TableGen patterns don't need to do anything special — the hardware does it. ✅

### RSUB Operand Order (base-isa.md §Opcodes)

**Spec**: "RSUB: The B operand is the left source register and the A operand is the right source register." So RSUB computes B - A.

**Backend**: Tied-def constraint means `dst = src1 = A`, and `src2 = B`. The operation `dst = src2 - src1 = B - A`. This matches the spec. ✅

### STORE Operand Order (base-isa.md §Opcodes)

**Spec**: "STORE: This instruction uses the B operand to specify which memory address is written to." So `MEM[B] ← A`.

**Backend**: In `EInstStoreRRBase`, AAA=val, BBB=addr. The encoding is correct. The asm string is `store $val, $addr`. ✅

### PUSH/POP Semantics (stack-and-functions/README.md)

**Spec**:
- PUSH: `SP ← SP - 2; mem[SP - 2] ← B` (both SP-2 refer to the same value)
- POP: `SP ← SP + 2; A ← mem[SP]`
- "All register reads for these instructions occur before any writes."

**Backend**: The PUSH/POP are implemented as real instructions in the .td file — the encoding goes directly to the hardware. The backend doesn't emulate the stack operations in software; it emits the encoded instruction bytes. The hardware handles the register read/write ordering. ✅

**Note**: `pop %sp` has special semantics per spec: "pop stores the incremented sp before writing data to the destination, so pop %sp will not increment the popped data." This is an edge case — the backend generates POP for CSR restoration and `pop r5` (frame pointer), which are fine. It never generates `pop sp`. ✅

### Size Extension on PUSH/POP (stack-and-functions/README.md)

**Spec**: "If the SS bits are 00, then instead of +/- 2, you'll do +/- 1. Similarly, if set to 10 or 11 it will be 4 or 8 respectively."

**Backend**: PUSH/POP32 have SS=10 (4-byte), PUSH/POP64 have SS=11 (8-byte). PUSH/POP (base) have SS=01 (2-byte). PUSH/POP 8-bit variants are not implemented (SS=00). ✅

### SAF CALL Encoding (stack-and-functions/README.md)

**Spec**: `10 1 1 DDDD DDDDDDDD` — Byte 0 bits[7:4]=1011, bits[3:0]=D[11:8]; Byte 1=D[7:0]. Displacement is 12-bit signed, added to PC.

**Backend**: `EInstSAFCall` encodes `Inst{7-4}=0b1011, Inst{3-0}=target{11-8}, Inst{15-8}=target{7-0}`. ✅

### SAF CALL Register (stack-and-functions/README.md)

**Spec**: `10 1 0 1111 RRR X CCCC` — RRR=register, X=1 for call. "The conditional absolute register function call will conditionally store the next instruction's address in the link register."

**Backend**: `EInstSAFCallReg` encodes `Inst{15-13}=rs, Inst{12}=1, Inst{11-8}=cond_val, Inst{7-0}=0xAF`. Condition is always 0b1110 (always). ✅

### SAF JMP Register (stack-and-functions/README.md)

**Spec**: `10 1 0 1111 RRR 0 CCCC` — same as CALL but X=0.

**Backend**: `EInstSAFJmp` encodes `Inst{12}=0`. ✅

### Byte Extension Register Naming (half-word-operations/README.md)

**Spec**: "8 bit register references/8 bit operations are marked by the infix/prefix `h` (i.e. `%rh0`)."

**Backend**: The AsmParser supports both `%r0h` (postfix) and `%rh0` (infix) forms. The register width hint from the name correctly identifies 8-bit operations. The `printOperand` method appends `h` suffix for 8-bit register printing. ✅

### DW Extension Register Naming (doubleword-operations/README.md)

**Spec**: "32 bit register references/32 bit operations are marked by the infix/prefix `d` (i.e. `%rd0`)."

**Backend**: Supports `%r0d`, `%rd0`, and `%d0` forms. ✅

### QW Extension Register Naming (quadword-operations/README.md)

**Spec**: "64 bit register references/64 bit operations are marked by the infix/prefix `q` (i.e. `%rq0`)."

**Backend**: Supports `%r0q`, `%rq0`, and `%q0` forms. ✅

### CMP and TEST — No Destination (base-isa.md §Opcodes, notes 2)

**Spec**: "Placed here to ease decoding; xx11 => do not store result." CMP (CCCC=3=0011) and TEST (CCCC=7=0111) don't write the destination register.

**Backend**: `EInstCmp` (and CMP16/CMP32/CMP64 etc.) has `(outs), (ins ...)` — no output operands. ✅

### RSUB Enables NEG/NOT (base-isa.md §Opcodes, note 1)

**Spec**: "Enables NEG and NOT to be encoded as RSUB r, imm. NEG can be implemented as RSUB r, 0. NOT can be implemented as RSUB r, -1."

**Backend**: RSUB is defined in both RR and RI forms for all widths. The RI form with imm=0 or imm=-1 produces NEG/NOT respectively. The codegen can use these via constant folding in the instruction selector. ✅

### Memory Alignment (base-isa.md §Memory Semantics)

**Spec**: "Unaligned memory accesses are _unspecified_ behavior. A memory access is unaligned if the address is not a multiple of 2."

**Backend**: No unaligned access support is implemented. The default alignment for i16 is 2 bytes, matching the spec. ✅

### NOP Canonical Encoding

**Spec**: No explicit NOP instruction. The `CCCC=1111` (never) branch condition with displacement 0 provides a "do nothing" instruction: encoding `0x008F` (`10 0 0 1111 | 0000 0000`).

**Backend**: Uses `0x008F` consistently as NOP encoding. This matches the `CCCC=1111` with zero displacement. ✅

### CPUID / Control Registers (base-isa.md §Control Register Read and Write Instructions)

**Spec**: Defines CR0 (CPUID1), CR1 (CPUID2), CR2 (FEAT) as readable, writes are NOPs. Reading undefined CRs is unspecified.

**Backend**: READCR/WRITECR instructions are defined but no special handling for CR numbers is implemented. This is correct — the instructions encode the CR number in the immediate field, which is passed directly to the hardware. ✅

### Flag State for Logical Ops (base-isa.md §Opcodes, note 5)

**Spec**: "The C and V flags are in an _unspecified_ state after execution of these instructions. Extensions _may_ mandate a particular behavior, with good enough reason, but must **NOT** mandate that the value of these flags after the operation depends on their value before the operation."

**Backend**: No code depends on C/V after logical ops. All branch conditions used for comparisons (BLT/BGE/BLE/BGT) check N and V flags, which are set correctly by CMP/TEST, not by logical ops. ✅

### Carry Flag Semantics for Subtraction (base-isa.md §Flag Semantics, item 3)

**Spec**: "C is set to 1 by subtraction operations in exactly the opposite case [of addition]; when the 17th bit would be a zero." This means `C = NOT(carry_out)` for subtraction — a borrow does NOT set C, it clears C.

**Backend**: The branch conditions for unsigned comparisons use: BLTU when C=1, BGEU when C=0, BLEU when C\|Z, BGTU when ~(C\|Z). After SUB/CMP: C=1 means borrow DID happen, so A < B (unsigned). This matches `C = NOT(carry_out)` semantics. ✅

### Overflow Flag Semantics (base-isa.md §Flag Semantics, item 4)

**Spec**: "V is set to 1 if the operands to that addition are both positive but the result is negative, or if both operands are negative but the result is positive."

**Backend**: Signed comparisons BLT/BGE check N≠V and N=V respectively. This is the standard signed overflow correction: when overflow occurs (V=1), the sign flag N is inverted from the mathematically correct result. ✅

### Recommended ABI Register Roles (stack-and-functions/README.md §Suggested ABI)

**Spec**:
- r0 (a0/v0) — argument/return value 0
- r1 (a1/v1) — argument/return value 1
- r2 (a2) — argument 2
- r3 (s0) — callee-saved
- r4 (s1) — callee-saved
- r5 (bp) — callee-saved
- r6 (sp) — callee-saved
- r7 (ln) — caller-saved

REX extension:
- r8-r12 (t0-t4) — caller-saved temps
- r13-r15 (s2-s4) — callee-saved

**Backend**: `ETCACallingConv.td` and `ETCARegisterInfo.cpp` implement exactly this mapping. ✅

### REX Prefix Byte (expanded-registers/README.md)

**Spec**: `1100 Q A B X` — A provides bit 3 for AAA field, B provides bit 3 for BBB field. Unused bits SHOULD be set to 0.

**Backend**: `computeRexPrefix` computes `0xC0 | (RexA << 2) | (RexB << 1)`. REX.Q and REX.X are always 0, which is correct per "unused bits should be 0." ✅

**Spec** (REX): "Requires Base, VWI". The VWI (Variable Width Instruction) extension is NOT currently implemented in the backend. This means the REX extension as implemented in the backend is technically a spec violation — it should require VWI. However, since VWI only adds variable-length instruction support (which REX uses to extend the instruction size from 2 to 3 bytes), and the backend manually implements the 3-byte encoding without needing the formal VWI infrastructure, this is a pragmatic deviation. **Document as known deviation.**

### Multiply/Divide (multiply-divide/README.md)

**Spec**: Defines native UDIV, SDIV, UREM, SREM, UMUL, SMUL, UHMUL, SHMUL instructions using the EXOP prefix (expanded opcodes, CP2.0). Requires Base + VWI + CP2.0.

**Backend**: Since EXOP/VWI are not implemented, the backend uses libcalls (compiler-rt `__mulhi3`, `__divhi3`, etc.) via `ETCALegalizerInfo::libcallFor()`. This is correct behavior for targets without the native multiply/divide extension. ✅

### R_ETCA_ABM Relocation Types in LLD vs Spec

The LLD backend implements `R_ETCA_ABM_RIS_*` and `R_ETCA_ABM_RIZ_*` relocation types (absolute immediate, sign-extended/zero-extended, for various widths). These correspond to the FI (full immediates) extension which is NOT yet implemented. The relocation handlers modify `loc[0]` assuming an ABM byte format, but since the codegen doesn't emit FI relocations yet, this code is dead/unreachable from LLVM-generated code. It's there for binutils compatibility.

**Risk**: When FI is implemented, the relocation handler assumes the first byte of the instruction has a specific format (`[reg|imm5]`) which matches the RI instruction format. If FI uses a different encoding, the relocation logic will need updating. ⚠️

### R_ETCA_MOV Relocation Types (MOV_5 through MOV_64, MOV_8 through MOV_32, and REX variants)

The LLD's `buildMovRi` function reconstructs complete MOVZ+SLO chains from placeholder bytes. This is a complex 1:1 port of binutils `etca_build_mov_ri()`. The code assumes:
- Instruction at `loc[0]` (or `loc[1]` with REX) is a MOVZ/MOVS opcode byte
- Subsequent instructions (at fixed offsets from `loc`) are SLO opcodes
- NOP padding fills unused instruction slots

**Risk**: The codegen currently emits MOV+SLO sequences manually in the instruction selector (see `G_CONSTANT` handling). If the codegen's sequence length or structure changes, the relocation may write past the buffer or corrupt adjacent instructions. The buffer size is determined by `movRelocToInsnCount`, which counts instructions from the relocation type. If the actual codegen sequence has fewer instructions than the relocation type implies, the NOP padding fills the gap correctly. If it has MORE, the relocation will overwrite following code. This is fragile.

### Summary of Spec Non-Conformances

| # | Severity | Issue |
|---|----------|-------|
| S2 | 🟢 Low | **Missing BNVR instruction**: CCCC=1111 (never) not implemented as a branch mnemonic. The encoding 0x008F IS used as NOP, which matches the canonical encoding for "branch never with displacement 0." |
| S3 | 🟡 Medium | **REX requires VWI per spec**: The expanded-registers spec says "Requires: Base, VWI." The backend doesn't implement VWI. The REX extension works without it because the backend manually handles the 3-byte encoding. This is a pragmatic deviation but should be documented. |
| S4 | 🟢 Low | **Multiply/divide uses libcalls instead of native EXOP**: Acceptable since EXOP is not implemented. The spec defines native instructions, but libcalls are ABI-compatible. |
| S6 | 🟢 Low | **SAF conditional jmpr/callr not implemented**: The spec defines CCCC field in JMPR/CALLR encoding for conditional jumps. The backend hardcodes CCCC=0b1110 (always). Conditional register jumps can't be assembled. |
