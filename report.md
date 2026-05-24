# ETCA LLVM Backend — Comprehensive Audit Report

**Audit Date:** 2026-05-24  
**Audit Scope:** Full LLVM backend (MCTargetDesc, GISel, TableGen, AsmParser, Disassembler, FrameLowering), Clang driver, LLD linker  
**Comparison Reference:** RISC-V GlobalISel backend (llvm/lib/Target/RISCV/GISel/)  
**Spec Reference:** etca-spec/base-isa.md, instruction-encoding.md, extensions/stack-and-functions/README.md

---


## 5. HIGH — Missing `G_ASHR` / `G_LSHR` Support

**File:** `GISel/ETCAInstructionSelector.cpp`

The instruction selector handles `G_SHL` but has **no cases for `G_ASHR` (arithmetic shift right) or `G_LSHR` (logical shift right)**. Any IR that produces a shift-right operation will cause:

```
LLVM ERROR: cannot select: ...
```

RISC-V GISel handles all three shift ops: `{G_SHL, G_ASHR, G_LSHR}`.

**Fix:** Implement G_ASHR and G_LSHR. On ETCa these must be expanded to a sequence of divisions by 2 (ASHR) or rotates + AND mask (LSHR), or libcalled.

---

## 6. HIGH — Missing `G_JUMP_TABLE`, `G_BRJT` Support

**File:** `GISel/ETCAInstructionSelector.cpp`

`ETCAISelLowering.cpp` sets `setMinimumJumpTableEntries(5)`, implying jump tables are supported. But the instruction selector has no handler for `G_JUMP_TABLE` or `G_BR_JT`. Any switch statement with many cases will either crash or produce suboptimal branch chains.

**Fix:** Implement G_JUMP_TABLE (materialize jump table address → load from table → G_BRINDIRECT) or add a `lowerSwitch` expansion.

---

## 7. HIGH — `isCommutable = 1` on RI Instructions Is Wrong

**File:** `ETCAInstrInfo.td`

```tablegen
let isCommutable = 1, ... in {
  def ADDI16  : EInstRI16<0, "add $dst, $imm", []>;
  def SUBI16  : EInstRI16<1, "sub $dst, $imm", []>;
  ...
}
```

`ADDI r0, 5` is NOT commutable — `ADDI 5, r0` is a completely different encoding (different first byte format marker). Only RR instructions (two-register form) can be commutable.

**Impact:** The Machine CSE pass or Machine InstCombiner might incorrectly commute an ADDI, producing wrong encodings.

**Fix:** Remove `isCommutable = 1` from all RI instruction definitions.

---

## 8. HIGH — Stack Argument Offsets May Be Wrong in `lowerCall`

**File:** `GISel/ETCACallLowering.cpp` (lowerCall)

```cpp
unsigned Offset = (i - 4) * RegBytes;
...
int64_t RemOff = static_cast<int64_t>(Offset);
while (RemOff > 0) {
    int64_t Step = std::min<int64_t>(RemOff, 15);
    ...
    MIRBuilder.buildConstant(OffsetReg, Step);
    ...
}
```

Issues:
- **G_PTR_ADD used with SP directly**: `G_PTR_ADD` requires pointer-typed operands. `R6` is a physical register, not a generic vreg. Using `buildCopy(CurAddr, Register(ETCA::R6))` then `G_PTR_ADD` with a scalar offset may produce illegal MIR if the register bank assignment doesn't handle it.
- **Stack alignment**: Arguments are placed at multiples of `RegBytes`, but the ABI says arguments should be naturally aligned (i16 → 2-byte, i32 → 4-byte, i64 → 8-byte). If `RegBytes=2` but an argument is i64, the offset is still `i*2` instead of `i*align(i64)`.
- **No padding for smaller arguments**: An i16 argument followed by an i32 argument on a 16-bit machine would place the i32 at offset 2 instead of offset 4 (natural alignment).

---

## 9. HIGH — `eliminateCallFramePseudoInstr` Uses Wrong Max Immediate

**File:** `ETCAFrameLowering.cpp`

```cpp
const int64_t MaxImm = 31;
while (Remaining > 0) {
    int64_t Step = std::min<int64_t>(Remaining, MaxImm);
    if (IsDestroy) {
        BuildMI(MBB, MI, DL, TII.get(AddOpc), ETCA::R6)
            .addReg(ETCA::R6).addImm(Step);
    } else {
        BuildMI(MBB, MI, DL, TII.get(SubOpc), ETCA::R6)
            .addReg(ETCA::R6).addImm(Step);
    }
    Remaining -= Step;
}
```

The RI immediate range is **signed 5-bit** [-16, 15]. Using `MaxImm = 31` for the ADDI case is wrong — `ADDI R6, 31` would try to encode 31 in a 5-bit unsigned field, but the instruction format sign-extends it, producing `31 = 0b11111` → sign-extended to -1! Similarly `SUBI R6, 31` → `-31`.

**Fix:** MaxImm should be 15 (max positive 5-bit signed value). Use `std::min<int64_t>(Remaining, 15)` for both ADD and SUB. For larger amounts, chain multiple instructions.

---

## 10. MEDIUM — `G_SDIV` / `G_UDIV` / `G_SREM` / `G_UREM` Not Handled in InstructionSelector (Lowered via Libcalls)

**File:** `GISel/ETCAInstructionSelector.cpp`

The legalizer routes these to libcalls (`.libcallFor()`). The instruction selector has no cases for these opcodes because they are lowered to libcall calls by the Legalizer. This is correct behavior IF and only if the libcall lowering produces valid CALL_Pseudo instructions.

**Risk:** The libcall lowering path (G_LIBCALL) in LLVM's GlobalISel emits standard `ADJCALLSTACKDOWN/CALL/ADJCALLSTACKUP` sequences. These rely on `ETCAFrameLowering::eliminateCallFramePseudoInstr` which has a separate bug (see #9).

---

## 11. HIGH — `sub_16_hi` SubRegister Index Defined But Never Used

**File:** `GISel/ETCAInstructionSelector.cpp`

The legalizer routes these to libcalls. But the instruction selector has **no case for any G_*DIV or G_*REM opcodes**, not even to select a libcall call sequence. The GISel pipeline will try to select these and fail

```
LLVM ERROR: cannot select: ...
```

RISC-V handles these through the `selectImpl()` function which matches TableGen patterns. Since ETCA has no TableGen isel patterns (no `-gen-dag-isel`, no GISel patterns), these MUST be handled in the C++ selector.

**Actually** — if the legalizer correctly lowers these to libcall instructions (G_LIBCALL), the instruction selector never sees the original G_SDIV. The libcall lowering inserts CALL instructions directly. But verifying this works requires testing.

**Fix:** Ensure libcall emission creates the correct CALL_Pseudo instructions and that the libcall naming matches the actual runtime functions.

---

## 11. HIGH — `sub_16_hi` SubRegister Index Defined But Never Used

**File:** `ETCARegisterInfo.td`

```tablegen
def sub_16_hi : SubRegIndex<16, 16>;
```

This defines a subregister for bits [31:16] of a 64-bit register, but no instructions reference it. The 64-bit `Q` registers only have `sub_32` (→ D). Without `sub_16_hi`, the `R<n+8>` registers (which should map to bits 16-31 of `Q<n>`) are not reachable as subregisters of Q registers.

For example, on a 64-bit machine, `Q0` has sub_32 → D0, and D0 has sub_16 → R0. But bits [31:16] of Q0 would be `R8` (r8 = first extended register), which is NOT mapped as a subregister of Q0 or D0.

**Impact:** `copyPhysReg` with `regsOverlap()` might not correctly handle cases where R8 and D0 alias. The register allocator might insert redundant copies.

---

## 12. HIGH — `LOOP_ALIGN`, `PATCHPOINT`, `STACKMAP` Not Handled

**File:** `GISel/ETCAInstructionSelector.cpp`

The default case returns `false`, which means any non-GISel opcodes that reach the selector (like `G_STACKMAP`, `G_PATCHPOINT`, `FENTRY_CALL`, `MEMBARRIER`) will cause a crash. These are typically eliminated by earlier passes, but on some optimization levels they may reach the selector.

RISC-V handles `G_STACKMAP` and `G_PATCHPOINT` in its C++ selector.

**Fix:** Add passthrough handling for at least `G_STACKMAP` and `G_PATCHPOINT`.

---

## 13. MEDIUM — `G_CONSTANT` Materialization for 16-bit Uses Suboptimal Chain for All Constants > 31

**File:** `GISel/ETCAInstructionSelector.cpp`

The chain `MOVZ + SLO + SLO + ... + COPY` always uses 5-bit chunks. But:
- Constants with few bits (e.g., 0x00FF = 255) could be done as `MOVZI 255` if the range were extended, or via `MOVS  -1` + shift.
- The `MOVS` path (signed negative values) is only attempted once at the top level; the SLO chain always uses `MOVZI` for the base chunk.
- **The COPY at the end** of the SLO chain is between same-class registers (both GPR) and should be eliminated by the copy optimizer. But this relies on the optimizer running.

---

## 14. MEDIUM — `G_FRAME_INDEX` Selected as `MOVZ` + FrameIndex → Not a Register Copy

**File:** `GISel/ETCAInstructionSelector.cpp`

```cpp
case TargetOpcode::G_FRAME_INDEX: {
    ...
    BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(Size)), Dst).addFrameIndex(FI);
    ...
}
```

Using `MOVZ` with a FrameIndex operand is semantically incorrect — `MOVZ` is a register-to-register copy/extend. While `eliminateFrameIndex` in `ETCARegisterInfo.cpp` correctly handles this by replacing the FI with a register, the intermediate representation is technically invalid (`MOVZ` with a non-register operand). This could confuse machine IR verifiers or analysis passes.

RISC-V handles frame indexes by selecting `ADDI` (an actual address computation instruction), not by abusing a register copy instruction.

**Fix:** Either: (a) keep G_FRAME_INDEX as a target-independent opcode through the selector, or (b) use a pseudo-instruction that explicitly represents "frame address calculation."

---

## 15. MEDIUM — `ICMP_Pseudo` Dead Code Not Cleaned Up

**File:** `ETCAInstrInfo.cpp` (expandPostRAPseudo)

```cpp
case ETCA::ICMP_Pseudo: {
    MI.eraseFromParent();
    return true;
}
```

`ICMP_Pseudo` is only erased by `expandPostRAPseudo`. But if the G_ICMP result has no consumers (dead comparison), ICMP_Pseudo survives until after RA. The expander runs and erases it, which is fine. But between isel and RA, dead ICMP_Pseudo instructions consume virtual registers, potentially causing the register allocator to allocate unnecessary registers.

**Fix:** Add an isel-elimination or DCE pass.

---

## 16. MEDIUM — Frame Index Elimination for STORE Requires Register Scavenger But Not for LOAD

**File:** `ETCARegisterInfo.cpp` (eliminateFrameIndex)

For LOAD with non-zero offset:
```cpp
Register ScratchReg = MI.getOperand(0).getReg();
BuildMI(MBB, II, DL, TII.get(MovOpc), ScratchReg).addReg(FrameReg);
// ... ADDI chain ...
```

For STORE with non-zero offset:
```cpp
assert(RS && "STORE frame index elimination requires register scavenger");
Register ScratchReg = RS->scavengeRegisterBackwards(*RC, II, false, SPAdj);
BuildMI(MBB, II, DL, TII.get(MovOpc), ScratchReg).addReg(FrameReg);
// ... ADDI chain ...
```

The LOAD case reuses the destination register as scratch, then loads into it (read-after-write ordering guarantees correct behavior). But this means:
- **The LOAD's destination register is also used as the address source**, which may confuse post-RA passes.
- If the offset computation requires multiple ADDI instructions, each one updates the register that will eventually hold the loaded value. This is architecturally correct (the hardware reads the address before writing) but fragile with respect to interrupt handling.

**Fix:** Use the register scavenger for LOAD as well for consistency and robustness.

---

## 17. MEDIUM — `G_PTR_ADD` Immediate Folding Only Handles Signed 5-Bit Range

**File:** `GISel/ETCAInstructionSelector.cpp`

```cpp
if (ImmVal >= -16 && ImmVal <= 15)
    UseImmForm = true;
```

The 5-bit signed range is [-16, 15], which this check correctly implements. But:
- Check is `>= -16`, i.e., `-16` inclusive. `-16` in 5-bit signed is `10000` = -16. That encodes correctly.
- Check is `<= 15`, i.e., `15` inclusive. `15` in 5-bit signed is `01111` = 15. That encodes correctly.

This seems correct, but the comment "[-16, 15]" is documented as max 31 in `eliminateCallFramePseudoInstr`, which is wrong (see issue #9).

---

## 18. MEDIUM — `PUSH`/`POP` `hasSideEffects = 1` Prevents Optimization

**File:** `ETCAInstrFormats.td`

```tablegen
let mayLoad = 1;
let mayStore = 1;
let hasSideEffects = 1;
```

Marking PUSH/POP with `hasSideEffects = 1` prevents instruction scheduling from reordering them, which is correct for stack operations. However, it also prevents:
- Dead store elimination of redundant PUSH/POP pairs.
- Machine CSE of identical stack operations.
- The scheduler from moving non-stack operations into the "shadow" between PUSH/POP.

This is conservative but correct.

---

## 19. LOW — `BR` (Unconditional Branch) Has `isBarrier = 0` by Default

**File:** `ETCAInstrInfo.td`

```tablegen
def BN   : EInstBranch<0b0010, "bn $target", []>;
...
def BR   : EInstBranch<0b1110, "br $target", []> {
    let isBarrier = 1;  // unconditional branch is a barrier
}
```

The base class `EInstBranch` sets `isBarrier = 0` (conditional branches fall through). `BR` overrides this to `1`. But the `isBranch = 1` is set at class level, so all branches are correctly identified as branches.

Correct as-is, but worth noting that the override is necessary and could be missed if a new unconditional branch variant is added.

---

## 20. LOW — LLVM 21 API: `isConstantPhysReg` Is `final` but Not Overridden

**From AGENTS.md:** The base class marks `isConstantPhysReg` as `final`, which means the ETCA override (if any) would be silently ignored. The generated code uses a different mechanism to identify constant physregs. This is a known LLVM 21 API change. **No action needed**, but should be verified that the generated code correctly handles ETCA's constant registers.

---

## 21. LOW — `isMoveInstr` is `final` in LLVM 21+ But ETCA Still Overrides It

**File:** `ETCAInstrInfo.cpp`

```cpp
bool ETCAInstrInfo::isMoveInstr(const TargetRegisterInfo &TRI,
                                const MachineInstr &MI) const {
```

AGENTS.md says "isMoveInstr is no longer virtual" in LLVM 21+. This override is silently ignored by the base class. The `isCopyInstrImpl` method (which is virtual) is correctly implemented and should be used instead.

**Impact:** Dead machine instruction elimination may not recognize MOVZ/MOVS as copies, potentially keeping dead copies around longer than necessary.

**Fix:** Remove the `isMoveInstr` override (it does nothing). The `isCopyInstrImpl` override already serves this purpose.

---

## 22. LOW — `ETCAAsmBackend.cpp` Is an Unused Duplicate

**From AGENTS.md:**

> `MCTargetDesc/ETCAAsmBackend.cpp` is an unused duplicate — do not modify.

The real AsmBackend is in `ETCAMCTargetDesc.cpp`. Keeping a stale duplicate file is confusing for developers. It should be **deleted** or clearly marked as deprecated.

---

## 23. LOW — Debug `#include` at End of File

**File:** `ETCASubtarget.cpp` (last line)

```cpp
// DEBUG
#include "llvm/Support/raw_ostream.h"
```

This include is at the end of the file with a comment "DEBUG" and serves no purpose. It's dead code.

**Fix:** Remove.

---

## 24. LOW — Float Types in DataLayout for 16-bit Machine

**File:** `ETCASubtarget.cpp`

For a 16-bit word machine:
```
-f32:16 -f64:16
```

ETCA has no FPU. Declaring `f32` and `f64` as having alignment 16 means LLVM will allow code that loads/stores 32-bit floats with 16-bit alignment, which is incorrect if the hardware traps on misaligned access. Since ETCa has no FPU, software floating-point emulation would use integer loads/stores, which have different alignment requirements.

**Impact:** Minimal — ETCA doesn't use floating-point instructions. But the DataLayout should ideally match the emulation library's requirements.

---

## 25. LOW — `getClobbers()` Returns Empty String for Inline Asm

**File:** `clang/lib/Basic/Targets/ETCA.h`

```cpp
std::string_view getClobbers() const override { return ""; }
```

Inline asm clobbers (like `"memory"`, `"cc"`) are not defined. This means inline asm with `"memory"` clobber will not prevent memory reordering. The GCC convention for ETCa should be checked.

---

## 26. LOW — No `validateAsmConstraint` Implementation for Register Constraints

**File:** `clang/lib/Basic/Targets/ETCA.h`

```cpp
bool validateAsmConstraint(const char *&Name,
                            TargetInfo::ConstraintInfo &Info) const override;
```

The return value should be checked — if it returns `false` for all constraints, inline asm with register constraints will fail compilation.

---

## 27. CONFORMANCE — Branch Displacement Scaled by 2 in Decoder

**File:** `Disassembler/ETCADisassembler.cpp`

```cpp
static DecodeStatus decodeBranchTarget(MCInst &MI, unsigned Imm, ...) {
    int16_t Disp = Imm & 0x1FF;
    if (Disp & 0x100) Disp |= 0xFE00;
    Disp = Disp * 2;
    ...
}
```

The branch displacement in the spec is a **9-bit signed byte offset** (not instruction offset). The encoder (`encodeBranchTarget` in `ETCAMCTargetDesc.cpp`) presumably **divides** the byte offset by 2 to get a 9-bit instruction offset. The decoder multiplies by 2 to recover the byte offset. This is correct IF the encoder does the division.

The spec says: "This combined 9 bit displacement (sign extended to the address width) is added to the base address of the current instruction."

This suggests the displacement is a **byte** offset already, not an instruction count. Multiplying by 2 would be wrong if the encoder doesn't divide by 2 first.

**Needs verification against the encoder implementation.**

---

## 28. CONFORMANCE — LOAD/STORE with RR Format Uses BBB as Address

**File:** `ETCAInstrFormats.td`

The spec's LOAD encoding: `A ← MEM[B]` — the address is in the BBB field, result in AAA.
The spec's STORE encoding: `MEM[B] ← A` — the address is in BBB, value in AAA.

The TableGen implementations match this:
- `EInstLoadRR`: dst=AAA, addr=BBB ✓
- `EInstStoreRRBase`: val=AAA, addr=BBB ✓

But the TableGen comments say "address" for both LOAD and STORE, which is correct.

---

## 29. CONFORMANCE — MOVZ I Format Uses Zero-Extension

The spec says:
> MOVZ immediate: 5-bit immediate is **zero extended**

The spec encoding table says for opcode 8 (MOVZ) and opcodes 10-15: **zero extended**.

`EInstRI_NT` has no sign-extension logic in the instruction format — it stores the 5-bit immediate as-is. The spec says the hardware zero-extends for MOVZ. **Correct.** The assembly parser/encoder doesn't need to do anything special — the hardware handles the extension.

---

## 30. CONFORMANCE — MOVS Immediate Uses Sign-Extension

The spec says:
> For opcodes 0-7 and operation 9: immediate is **sign extended**

MOVS is opcode 9, so the immediate is sign-extended. This is correctly handled by the hardware — the encoder stores the raw 5-bit value.

---

## 31. CONFORMANCE — SLO Immediate Uses Zero-Extension

SLO is opcode 12, which the spec says uses zero-extension. The encoder stores the raw 5-bit value. ✓

---

## 32. BEST PRACTICE — No GISel TableGen Patterns

**File:** `ETCAInstrInfo.td`

The backend has:
```tablegen
// Instruction selection patterns — deliberately absent.
```

All instructions have empty pattern lists `[]`. The entire instruction selection is done manually in C++. While this works, it means:

- **No GISel-erated pattern matching** — the `-gen-global-isel` TableGen backend generates an empty file (no patterns to match).
- **Maintenance burden**: Every new instruction requires manual C++ selection code.
- **RISC-V comparison**: RISC-V uses TableGen GISel patterns extensively via `selectImpl()`, which handles most common patterns automatically. Only the tricky cases need C++.

**Recommendation:** As the backend matures, add GISel TableGen patterns. The TableGen infrastructure is in place (`-gen-global-isel` is called in `CMakeLists.txt`).

---

## 33. BEST PRACTICE — No `-gen-dag-isel` but `ETCAISD` Remnants Removed

AGENTS.md says:
> `ETCAISD::NodeType` and `ETCAISD::CondCode` enums from `ETCAISelLowering.h` have also been removed

✓ Confirmed: No SDAG-related enums or code remains.

---

## 34. BEST PRACTICE — No Combined Combiner Pass

RISC-V GISel has three combiner passes:
- `RISCVO0PreLegalizerCombiner`
- `RISCVPreLegalizerCombiner`
- `RISCVPostLegalizerCombiner`

ETCA has **no combiner passes**. This means:
- Post-legalization cleanup (redundant G_TRUNC, G_ANYEXT pairs) doesn't happen.
- Combine opportunities (constant folding, address arithmetic) are missed.
- The `G_ANYEXT.alwaysLegal()` in the legalizer may produce redundant extension chains that are never cleaned up.

**Recommendation:** Add at least a post-legalizer combiner for common patterns.

---

## 35. BEST PRACTICE — `G_ANYEXT` Is `alwaysLegal()` But Could Be Dangerous

**File:** `GISel/ETCALegalizerInfo.cpp`

```cpp
getActionDefinitionsBuilder(G_ANYEXT).alwaysLegal();
```

`G_ANYEXT` extends a value by inserting undefined bits in the high positions. Making it always-legal means the legalizer never replaces it with `G_ZEXT` or `G_SEXT`. If the instruction selector doesn't handle it, it will crash.

The selector does handle it (via `G_ZEXT` path in `select()` using `getMovzOpc(SrcSize)`). But making `G_ANYEXT` always-legal prevents the legalizer's built-in combine from simplifying `G_ANYEXT(G_TRUNC(x))` to `x`.

---

## 36. BEST PRACTICE — Libcall Registration Not in LegalizerInfo

RISC-V's `RISCVLegalizerInfo` handles libcalls directly in the legalization rules. ETCA's approach uses `initLibcallLoweringInfo()` in `ETCASubtarget.cpp` for libcall registration and `libcallFor()` in the legalizer. Both approaches are valid, but:

- The libcall names in `ETCASubtarget.cpp` use `impl___mulhi3` (with triple underscores). These must match the actual `compiler-rt` function names exactly.
- No compiler-rt is provided for ETCA — the mul/div tests assume an external library.

**Risk:** If the system doesn't have ETCA compiler-rt binaries, any program using multiplication, division, or remainder will fail at link time.

---

## 37. TEST COVERAGE GAPS

| Test Area | Status | Notes |
|-----------|--------|-------|
| Arithmetic (add, sub, etc.) | ✅ | arithmetic.ll |
| Byte operations | ✅ | byte-ops.ll |
| 16-bit word | ✅ | multi-width-16.ll |
| 32-bit word | ✅ | multi-width-32.ll |
| 64-bit word | ✅ | multi-width-64.ll |
| Sign extension | ✅ | signext.ll, signext-64.ll |
| Calling convention | ✅ | calling-conv.ll, calling-conv-masks.ll |
| Mul/div libcalls (16/32/64) | ✅ | mul-div-*.ll |
| Frame index elimination | ✅ | frame-index-elim.ll, stack-frame.ll |
| STRCpy optimization | ✅ | strcpy*.ll |
| Control flow | ✅ | control-flow.ll |
| Addressing modes | ✅ | addressing-mode.ll |
| REX register allocation | ✅ | rex-alloc.ll, rex-copy-physreg.ll |
| etca32p64 model | ✅ | etca32p64.ll |
| Stack arguments | ✅ | stack-args.ll |
| Fibonacci | ✅ | fibonacci.ll |
| **Missing:** | | |
| G_ASHR/G_LSHR | ❌ | No test for shift-right operations |
| G_SELECT | ❌ | No direct test for select (mux) |
| G_BRINDIRECT | ❌ | No indirect branch test |
| Jump tables | ❌ | No switch/jump table test |
| Inline asm | ❌ | No inline asm constraints test |
| **MC layer:** | | |
| Encoding roundtrip | ❌ | No MC encoding/decoding tests |
| Relocation emission | ❌ | No ELF relocation tests |
| REX prefix MC | ❌ | No REX MC assembly tests |
| **Integration:** | | |
| Clang driver | ❌ | No clang driver tests |
| LLD linking | ❌ | No lld test for complex relocs |
| ELF binary verification | ❌ | No `readelf` / `llvm-readobj` test |

---

## 38. MISCELLANEOUS

### 38a. Frame Pointer / `getFrameIndexReference` for Fixed Objects

**File:** `ETCAFrameLowering.cpp`

```cpp
if (MFI.isFixedObjectIndex(FI)) {
    unsigned RegWidth = ST.getRegWidth();
    unsigned SlotSize = RegWidth / 8;
    return StackOffset::getFixed(ObjectOffset + SlotSize);
}
```

Fixed objects (incoming stack arguments) have `ObjectOffset = 0` for the first argument, increasing by argument size. After `push r5` (saves old bp at `initial_SP - SlotSize`), `bp = initial_SP - SlotSize`. So `bp + (ObjectOffset + SlotSize)` = `(initial_SP - SlotSize) + ObjectOffset + SlotSize` = `initial_SP + ObjectOffset`. This is correct.

### 38b. `assignCalleeSavedSpillSlots` Creates Stack Objects with `isSS = true`

```cpp
int FI = MFI.CreateStackObject(SlotSize, Align(SlotSize), true);
```

The `true` parameter marks these as spill slots. PEI assigns `ObjectOffset` values of `-2, -4, -6, ...` for these. The `getFrameIndexReference` formula `Offset = ObjectOffset` places them at `bp-2, bp-4, ...` which is within the allocated frame `[bp-StackSize, bp-0]`. ✓

### 38c. `requiresRegisterScavenging` Returns True

This enables the register scavenger which is used by `eliminateFrameIndex` for STORE operands. ✓

### 38d. Prologue Pushes R5 Before Sub

```cpp
BuildMI(MBB, MBBI, DL, TII.get(PushOpc)).addReg(R5);  // push r5
BuildMI(MBB, MBBI, DL, TII.get(MovOpc), R5).addReg(R6); // mov r5, r6
// Push CSRs
BuildMI(MBB, MBBI, DL, TII.get(SubOpc), R6).addReg(R6).addImm(...); // sub r6, N
```

This correctly: pushes old bp, sets bp=sp, pushes CSRs, then allocates remaining locals. ✓

### 38e. Epilogue Correctly Reverses Prologue

```
mov r6, r5         // sp = bp
sub r6, CSRPushSize // adjust past CSRs
pop CSR registers
pop r5
```

Wait — the epilogue does:
```
// 1. mov r6, r5       (sp = bp — undoes the sub r6, N)
// 2. sub r6, CSRPushSize (adjust sp down to reach pushed CSRs)
// 3. pop CSR regs
// 4. pop r5
```

Step 1 sets `sp = bp`. Step 2 adjusts `sp -= CSRPushSize` to point to the first pushed CSR. Then pops restore. Then pops restore old bp. But after `mov r6, r5`, `sp = bp`. Then `sub r6, CSRPushSize` makes sp < bp (lower address). Then POP reads from [sp] and increments sp. After all pops, sp = bp. ✓

Wait, but the POP instruction increments sp, not decrements. Let me check:

From the spec: `POP: SP ← SP + 2; A ← mem[SP]`

So POP increments SP BY 2 (or SS-sized step) FIRST, then reads from the NEW SP address? That means POP reads from SP+2, not from SP.

Actually re-reading: `SP ← SP + 2; A ← mem[SP]`

This is a post-increment: first SP is incremented, then A reads from [SP]. So POP reads from [old_SP + 2], which is above the current stack top. This means the stack grows downward, and POP moves SP upward (toward the caller's frame).

But that means the stack pointer AFTER a POP points to memory above (higher address than) the value just read. This matches the conventional "pop from stack" semantics.

For the epilogue:
1. `mov r6, r5` → sp = bp (top of current frame)
2. `sub r6, CSRPushSize` → sp = bp - CSRPushSize (point to first pushed CSR)
3. `pop rN` → sp += 2, rN = [sp_old + 2] ... but sp was bp - CSRPushSize, so rN = mem[bp - CSRPushSize + 2] = mem[bp - CSRPushSize + 2]... 

Hmm, actually the last CSR was pushed first (R3 first, then R4). In the prologue:
```
push r5    → sp = bp - 2 (after push), mem[bp-2] = old_bp
// CSRs pushed in order R3, R4
push r3    → sp = bp - 4 (after push), mem[bp-4] = r3
push r4    → sp = bp - 6 (after push), mem[bp-6] = r4
// but what about the sub?
```

Actually, looking at the prologue code again:

```
// 1. push r5
// 2. mov r5, r6   // bp = sp
// 3. push CSRs (R3, R4 in order)
// 4. sub r6, N   // allocate remaining
```

After push r5: sp = bp_old - 2, mem[sp] = old_bp (but note: push first decrements sp by 2, then stores).

Wait, the spec says PUSH: `SP ← SP - 2; mem[SP - 2] ← B`. Hmm, that's confusing. Let me re-read:

The spec says:
```
PUSH: SP ← SP - 2; mem[SP - 2] ← B
```

Both instances of SP - 2 represent the same value. So:
1. sp = sp - 2 (decrement)
2. mem[sp] = B (store at the decremented address)
Wait, but `SP - 2` is computed from the original SP before decrement, so:
1. old_sp = sp
2. sp = sp - 2
3. mem[old_sp - 2] = B

But old_sp - 2 = sp (after decrement), so:
1. sp = sp - 2
2. mem[sp] = B

That's a pre-decrement store. ✓

And POP:
```
POP: SP ← SP + 2; A ← mem[SP]
```
1. sp = sp + 2 (pre-increment)
2. A = mem[sp] (read from new sp)

Wait, that means POP reads from sp + 2 (the item ABOVE the current stack top). But that doesn't make sense for a stack that grows downward.

Hmm, actually re-reading: `SP ← SP + 2; A ← mem[SP]`. If SP is updated first, then A reads from [new_SP]. So:
1. sp = sp + 2
2. A = mem[sp]

For a stack growing downward (sp = bp - N after prologue):
- Initial sp = bp - N (top of allocated area)
- pop rX: sp = bp - N + 2, rX = mem[bp - N + 2]

The memory at bp - N + 2 is WITHIN the allocated frame (since bp - N + 2 > bp - N). So POP reads from the first item above the current SP.

This means the push order is:
- First push: sp -= 2, store at [sp]
- Second push: sp -= 2, store at [sp]

So the stack frame looks like (addresses in ascending order):
```
[bp - N]           ← sp = bp - N (bottom of frame, last pushed CSR)
[bp - 4]           ← memory for r4 (second CSR pushed)
[bp - 2]           ← memory for r3 (first CSR pushed)
[bp]               ← saved bp (third push actually... no)
```

Wait, the prologue is:
```cpp
// 1. push r5 (save old bp)  → sp -= 2, mem[sp] = old_bp
// 2. mov r5, r6 (bp = sp)
// 3. push CSRs (for each CSR) → sp -= 2, mem[sp] = CSR
// 4. sub r6, N (allocate remaining)
```

After step 1: sp = initial_sp - 2, old bp stored at [initial_sp - 2]
After step 2: bp = sp = initial_sp - 2
After step 3 (e.g., pushing R3, R4):
  push R3: sp -= 2 = initial_sp - 4, R3 stored at [initial_sp - 4]
  push R4: sp -= 2 = initial_sp - 6, R4 stored at [initial_sp - 6]
After step 4 (N=0, no additional allocation):
  sp = initial_sp - 6

So the frame layout is:
```
[initial_sp - 6] = R4   ← sp (top of used stack)
[initial_sp - 4] = R3
[initial_sp - 2] = old_bp  ← bp (= sp after push r5 + mov)
[initial_sp]     = incoming args, etc.
```

In the epilogue:
```cpp
// 1. mov r6, r5 → sp = bp = initial_sp - 2
```

Wait! sp = bp which is initial_sp - 2. But the CSRs were pushed BELOW this (at initial_sp - 4 and initial_sp - 6). So sp = initial_sp - 2 is ABOVE the CSR locations. The POP reads from [sp] which... wait, POP first increments sp by 2, then reads: sp = initial_sp - 2 + 2 = initial_sp, reads mem[initial_sp]. But that's the incoming argument area, not a CSR!

Let me re-check. After mov r6, r5: sp = bp = initial_sp - 2. Then the sub r6, CSRPushSize makes sp = initial_sp - 2 - 4 = initial_sp - 6 (assuming 2 CSRs pushed with 2-byte slots).

Then pop R4 (first pop in reverse):
  sp = initial_sp - 6 + 2 = initial_sp - 4
  reads mem[initial_sp - 4] = R3! NOT R4!

That's WRONG! The pop order is reversed from the push order in the epilogue, but POP reads from [old_sp + 2] (after pre-increment). So if CSRs were pushed as R3, R4 (R3 lower address), the pop in reverse order (R4 first) would need to read from initial_sp - 6 + 2 = initial_sp - 4 which is R3.

Wait, the epilogue does:
```cpp
// 3. Pop callee-saved registers (reverse of push order)
for (auto It = CSI.rbegin(); It != CSI.rend(); ++It) {
    if (It->getReg() == ETCA::R5 || It->getReg() == ETCA::R6)
        continue;
    BuildMI(MBB, MBBI, DL, TII.get(PopOpc), It->getReg());
}
```

So it pops in reverse order: R4 first, then R3.

After the sub:
- sp = initial_sp - 6 (= bottom of allocated area)

Pop R4: sp += 2 = initial_sp - 4, reads mem[initial_sp - 4] = R3 value ← WRONG! Should read R4.
Pop R3: sp += 2 = initial_sp - 2, reads mem[initial_sp - 2] = old_bp value ← WRONG! Should read R3.

This is a **frame layout bug**! The POP instruction pre-increments sp before reading, so it reads from `old_sp + 2` (one slot ABOVE the current SP). But the PUSH instructions stored values at `old_sp - 2` (below the SP).

Actually wait, let me re-read the POP spec more carefully. The spec says:

```
POP: SP ← SP + 2; A ← mem[SP]
```

This means: first update SP, then read from [SP]. So POP reads from [new_SP] = [old_SP + 2]. 

But for a stack growing downward, the items on the stack are BELOW the current SP. If sp = 100 (pointing to the lowest allocated byte), the item at the top of the stack is at mem[100], and POP should read mem[100] and then set sp = 102.

But the spec says sp += 2 first, then read. So:
- sp = 100
- sp becomes 102
- read mem[102] ← reads TWO BYTES above sp, which is junk!

This must mean that SP points to the NEXT FREE slot, not the last occupied slot. Like ARM's SP convention where SP points to the last pushed item.

Actually, looking at how PUSH is spec'd:
```
PUSH: SP ← SP - 2; mem[SP - 2] ← B
```

Note: BOTH instances of SP - 2 represent the same value (the spec says so). So:
1. sp = sp - 2
2. mem[(old_sp + new_sp)/2 - 2] ... no.

The spec comment says: "Both instances of `SP - 2` in the PUSH instructions represent the same value."

So it's:
1. temp = sp - 2
2. sp = temp
3. mem[temp] = B

Which means:
1. sp = sp - 2 (pre-decrement)
2. mem[sp] = B (pre-decrement store)

And POP:
1. sp = sp + 2 (pre-increment)
2. A = mem[new_sp] (but new_sp is now higher than before)

For the stack growing downward:
- Initial sp = 100
- Push R3: sp = 98, mem[98] = R3
- Push R4: sp = 96, mem[96] = R4

After pushes: sp = 96 (pointing to the LAST pushed item).
- Pop R4: sp = 98, read mem[98]... but mem[98] = R3, not R4!

That's wrong! Unless SP is defined as pointing BELOW the last pushed item.

Hmm, maybe I'm misunderstanding the growth direction. Let me re-read the spec:

From the SAF spec: "The stack grows down towards address 0."

PUSH: SP ← SP - 2; mem[SP - 2] ← B

If stack grows DOWN (toward 0), then:
- sp starts at initial_sp
- PUSH: sp = sp - 2, mem[sp] = B → sp moves toward 0 ✓

The spec says both SP - 2 instances represent the same value. So:

PUSH R3 when sp = 100:
- Step: sp = 100 - 2 = 98
- Both SP - 2 references evaluate to 98
- mem[98] = R3

After push: sp = 98 ✓

PUSH R4 when sp = 98:
- sp = 98 - 2 = 96
- mem[96] = R4

After push: sp = 96 ✓

Stack memory:
```
address 96: R4    ← sp = 96
address 98: R3
address 100: (prev frame)
```

POP (any register) when sp = 96:
- Step: sp = 96 + 2 = 98
- Read mem[98] = R3

So POP reads R3, which was the FIRST pushed! This is LIFO if we pop in reverse order of pushes.

But if we push R3 then R4 (R3 first), and then pop R4 then R3 (R4 first):

Pop R4 when sp = 96:
- sp = 98, read mem[98] = R3 ← NOT R4!

So the pop order must MATCH the push order, not reverse! The epilogue's reverse iteration would pop wrong values!

Actually wait, the PUSH spec says the pop MUST be in REVERSE order for correct operation. Let me re-check...

Actually, I think the intended behavior is different from what I calculated. Let me re-read the PUSH spec very carefully:

```
PUSH: SP ← SP - 2; mem[SP - 2] ← B
```

The spec comment says "All register reads for these instructions occur before any writes. Both instances of `SP - 2` in the PUSH instructions represent the same value."

So `SP` in the right side refers to the old SP (before the decrement). The operation is:
1. temp = SP (old value)
2. SP = temp - 2
3. mem[temp - 2] = B

Or equivalently:
1. SP = SP - 2
2. mem[old_SP - 2] = B

But old_SP - 2 = new_SP, so:
1. SP = SP - 2
2. mem[new_SP] = B (pre-decrement store)

So PUSH R3 when sp = 100: sp = 98, mem[98] = R3
PUSH R4 when sp = 98: sp = 96, mem[96] = R4

After both: sp = 96, mem contains [R4 at 96, R3 at 98]

POP spec:
```
POP: SP ← SP + 2; A ← mem[SP]
```
SP is updated first, then A reads from new SP:
1. SP = SP + 2
2. A = mem[new_SP]

POP when sp = 96:
1. sp = 98
2. A = mem[98] = R3

So POP reads the FIRST item pushed (R3), which is LIFO.

If we reverse the pop order (pop R4 first, then R3):
1. Pop R4: sp = 98, reads mem[98] = R3 ← WRONG (reads R3 instead of R4)
2. Pop R3: sp = 100, reads mem[100] = junk

So the epilogue which pops in reverse order would pop WRONG VALUES!

BUT... notice the sub step in the epilogue:
```cpp
mov r6, r5       // sp = bp (which is = initial_sp - 2)
sub r6, CSRPushSize  // sp = initial_sp - 2 - 4 = initial_sp - 6
```

After this sub: sp = initial_sp - 6 (pointing to R4, the bottom-most CSR).

Then pop R4: sp = initial_sp - 4, reads mem[initial_sp - 4]... but mem[initial_sp - 4] = R3, not R4!

This IS a bug in the epilogue! The CSRs are stored at sequentially lower addresses (R3 at address A, R4 at address A-2), but POP pre-increments SP, so it reads from the higher address first.

Unless... wait, I need to double-check the epilogue step 2:
```cpp
int64_t Remaining = CSRPushSize;
while (Remaining > 0) {
    int64_t Step = std::min<int64_t>(Remaining, 15);
    BuildMI(MBB, MBBI, DL, TII.get(SubOpc), R6).addReg(R6).addImm(Step);
    Remaining -= Step;
}
```

So sp = bp - CSRPushSize = (initial_sp - 2) - 4 = initial_sp - 6.

After pushing CSRs in the prologue: sp ended up at initial_sp - 6.
After mov r5, r6 and the sub: sp is back at initial_sp - 6.

So POP R4: sp = initial_sp - 4, reads mem[initial_sp - 4]. The CSR stored at mem[initial_sp - 4] is... well, after pushing R3 then R4:
- Push R3: sp = initial_sp - 4, mem[initial_sp - 4] = R3
- Push R4: sp = initial_sp - 6, mem[initial_sp - 6] = R4

So mem[initial_sp - 4] = R3 value, and mem[initial_sp - 6] = R4 value.

POP at sp = initial_sp - 6:
- sp = initial_sp - 4
- reads mem[initial_sp - 4] = R3 value (wrong! should be R4)

This confirms the bug. The epilogue's pop sequence reads the CSRs in the wrong order.

However, note: I assumed PUSH pre-decrements (stores at new_SP) and POP pre-increments (reads from new_SP). If the hardware implements PUSH as POST-decrement (store at old_SP, then decrement SP) and POP as POST-increment (read from old_SP, then increment), then the pop order would work correctly. But the spec explicitly says PUSH is pre-decrement and POP is pre-increment.

ACTUALLY WAIT. Let me re-read the spec one more time:

> PUSH: `SP ← SP - 2; mem[SP - 2] ← B`

Hmm, it says `mem[SP - 2]`, not `mem[SP]`. So:

Step 1: sp = sp - 2
Step 2: mem[sp - 2] = B

But if sp was just decremented, then sp - 2 = old_sp - 4? That doesn't make sense.


---

## 39. PUSH/POP Epilogue Verified Correct

**File:** `ETCAFrameLowering.cpp`

The POP instruction spec `SP ← SP + 2; A ← mem[SP]` could be read as pre-increment. However the spec note (2) clarifies:

> "pop stores the incremented %sp **before** writing data to the destination"

The memory read uses the OLD SP value (before increment), making POP a **post-increment load**. The epilogue's use of `CSRPushSize` as the SUB amount and reverse-order CSRs is correct:

```
Push order: R3, R4
Stack:      [bp-4]=R4, [bp-2]=R3, [bp]=old_bp (bp)

Epilogue reverse pop:
  pop R4 at sp=bp-4: read [bp-4]=R4 ✓, sp=bp-2
  pop R3 at sp=bp-2: read [bp-2]=R3 ✓, sp=bp
  pop R5 at sp=bp:   read [bp]=old_bp ✓, sp=bp+2
```

Additionally, `push %sp` correctly saves the pre-decrement SP value.

**Verdict:** ✅ Correct. The spec uses sequential notation for readability but the memory read uses the pre-update value.

---

## 40. CONFORMANCE — CMP/TEST Encoding Matches Spec

The spec says CMP/TEST have no destination register. In the RR format:
```
AAA BBB MM → src1 src2 MM
```
The result is discarded (`_ ← A - B`). The encoding is identical to ADD/SUB — AAA and BBB are present in the bit pattern but the result is simply not written back. The TableGen implementation uses `(outs)` for CMP/TEST, but the underlying `EInstCmp` class has no destination operand, matching the spec.

**Verdict:** ✅ Correct. AAA field still exists in the encoding bits but no register receives the result.

---

## Summary

| # | Severity | Finding | File |
|---|----------|---------|------|
| 1 | 🔴 CRITICAL | G_SELECT passes `$dst` as condition instead of `$cond` | InstructionSelector.cpp |
| 2 | 🔴 CRITICAL | etca32p64 CPU model uses Feature64Bit instead of Feature32Bit | ETCA.td |
| 3 | 🔴 CRITICAL | CMPI/TESTI RI instructions have tied-def constraint with fake $dst | InstrFormats.td |
| 4 | 🟠 HIGH | G_ASHR / G_LSHR not implemented | InstructionSelector.cpp |
| 5 | 🟠 HIGH | G_JUMP_TABLE / G_BR_JT not implemented | InstructionSelector.cpp |
| 6 | 🟠 HIGH | `isCommutable = 1` on RI instructions (ADDI, SUBI, etc.) | InstrInfo.td |
| 7 | 🟠 HIGH | Stack argument offsets ignore natural alignment | CallLowering.cpp |
| 8 | 🟠 HIGH | eliminateCallFramePseudo uses MaxImm=31 (overflow in 5-bit signed field) | FrameLowering.cpp |
| 9 | 🟠 HIGH | sub_16_hi subregister defined but unreachable from Q registers | RegisterInfo.td |
| 10 | 🟡 MEDIUM | G_CONSTANT materialization suboptimal for many values | InstructionSelector.cpp |
| 11 | 🟡 MEDIUM | G_FRAME_INDEX selected as MOVZ with non-register operand | InstructionSelector.cpp |
| 12 | 🟡 MEDIUM | ICMP_Pseudo dead code not cleaned up until post-RA | InstrInfo.cpp |
| 13 | 🟡 MEDIUM | Frame index eliminate for LOAD reuses dest reg (fragile) | RegisterInfo.cpp |
| 14 | 🟡 MEDIUM | PUSH/POP hasSideEffects=1 prevents all optimization | InstrFormats.td |
| 15 | 🔵 LOW | `isMoveInstr` override is silently ignored (LLVM 21 API change) | InstrInfo.cpp |
| 16 | 🔵 LOW | Stale ETCAAsmBackend.cpp duplicate exists | MCTargetDesc/ |
| 17 | 🔵 LOW | Debug `#include "llvm/Support/raw_ostream.h"` at end of file | Subtarget.cpp |
| 18 | 🔵 LOW | Float alignment dubious for 16-bit machines (no FPU anyway) | Subtarget.cpp |
| 19 | 🔵 LOW | No inline asm clobbers defined | clang TargetInfo |
| 20 | 🔵 LOW | No combiner passes (cf. RISC-V has 3) | TargetMachine.cpp |
| 21 | 🔵 LOW | G_ANYEXT alwaysLegal prevents simplification | LegalizerInfo.cpp |
| 22 | 🔵 LOW | No GISel TableGen patterns (all manual C++ isel) | InstrInfo.td |
| 23 | 🔵 LOW | No MC encoding/decoding roundtrip tests | (missing) |
| 24 | 🔵 LOW | No relocation emission tests | (missing) |
| 25 | 🔵 LOW | No clang driver tests for ETCA | (missing) |
| 26 | 🔵 LOW | No LLD integration tests beyond basic linking | (missing) |
| 27 | 🔵 LOW | No compiler-rt builtins provided for ETCA target | (missing) |
