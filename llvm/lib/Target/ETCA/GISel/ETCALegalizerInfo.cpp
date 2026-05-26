//===- ETCALegalizerInfo.cpp - ETCA Legalizer ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the legalization rules for GlobalISel on ETCA.
//
// The ETCa ISA is natively variable width.  Legal types depend on the
// subtarget's word size (set via -mattr=+32bit/+64bit) and enabled
// extensions:
//   - 16-bit word (generic default):  i16 is legal; i8, i32, i64 are narrowed.
//   - 32-bit word (word size >= 32):  i16 and i32 are legal; i8, i64 are
//   narrowed.
//   - 64-bit word (word size >= 64):  i16, i32, and i64 are legal; i8 is
//   narrowed.
//   - BYTE extension:                 i8 becomes legal (MinLegal = s8).
//
// == Design ==
//
// Two tiers of type legality:
//
// Tier 1 — Data-flow ops (G_ZEXT, G_SEXT, G_TRUNC, G_MERGE_VALUES,
//          G_UNMERGE_VALUES, G_CONSTANT, G_LOAD, G_STORE, G_PHI,
//          G_IMPLICIT_DEF, G_FRAME_INDEX, G_GLOBAL_VALUE, G_PTR_ADD): s32 and
//          s64 are ALWAYS legal because the instruction selector handles these
//          manually (e.g., G_ZEXT uses MOVZ at source width + COPY bridge).
//          Making these illegal would cause the legalizer to create
//          G_MERGE_VALUES of illegal result types, leading to infinite regress.
//
// Tier 2 — Computation ops (G_ADD, G_SUB, G_AND, G_OR, G_XOR, G_SHL, G_ICMP,
//          G_SELECT):
//          Only legal for types with hardware TableGen patterns, conditioned
//          on HasDW (s32) / HasQW (s64).  On generic (16-bit only), s32/s64
//          computations are narrowed via splitting which produces helper
//          instructions (G_UADDE, G_UADDO, G_UMULH) on the legal narrow types.
//
// Tier 3 — Helper/narrowing ops (G_UADDE, G_UADDO, G_USUBE, G_USUBO,
//          G_UMULH, G_SMULH):
//          Legal for the same narrow types as Tier 2 but with s1 carry type.
//          These appear only as artifacts of the narrowing process and must
//          themselves be legal.
//
// For memory: pointers are PtrSize bits wide.  Loads/stores of legal scalar
// sizes are legal at their natural alignment.
//===----------------------------------------------------------------------===//

#include "ETCALegalizerInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/GlobalISel/LegalizerHelper.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "etca-legalizer"

using namespace llvm;

ETCALegalizerInfo::ETCALegalizerInfo(const ETCASubtarget &ST) {
  using namespace TargetOpcode;

  unsigned PS = ST.getPtrSize();
  bool HasByte = ST.hasByte();
  bool HasDW = ST.hasDW();
  bool HasQW = ST.hasQW();

  const LLT s1 = LLT::scalar(1);
  const LLT s8 = LLT::scalar(8);
  const LLT s16 = LLT::scalar(16);
  const LLT s32 = LLT::scalar(32);
  const LLT s64 = LLT::scalar(64);
  const LLT p0 = LLT::pointer(0, PS);

  // Minimum legal scalar — i8 if BYTE extension, otherwise i16.
  LLT MinLegal = HasByte ? s8 : s16;
  MinLegalSize = MinLegal.getSizeInBits();

  //===----------------------------------------------------------------===//
  // Tier 1 helpers — register all scalar types for data-flow ops
  //===----------------------------------------------------------------===//

  // All possible scalar types up to 64 bits are always valid for data-flow.
  auto dataFlowTypes = [&](auto &B) {
    if (HasByte)
      B.legalFor({s8});
    B.legalFor({s16, s32, s64});
  };

  // For computational ops, only register types that have hardware support.
  auto computeTypes = [&](auto &B) {
    if (HasByte)
      B.legalFor({s8});
    B.legalFor({s16});
    if (HasDW)
      B.legalFor({s32});
    if (HasQW)
      B.legalFor({s64});
  };

  // The maximum legal type for computational ops.
  LLT MaxComp = HasQW ? s64 : (HasDW ? s32 : s16);

  //===----------------------------------------------------------------===//
  // Arithmetic and logical operations — TIER 2 (subtarget-gated)
  //===----------------------------------------------------------------===//

  auto &ArithActions = getActionDefinitionsBuilder(
      {G_ADD, G_SUB, G_AND, G_OR, G_XOR, G_SHL, G_ASHR, G_LSHR});
  computeTypes(ArithActions);
  ArithActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  ArithActions.clampScalar(0, MinLegal, MaxComp);

  //===----------------------------------------------------------------===//
  // ICMP — TIER 2 (subtarget-gated on operand type, result is always s1/i16)
  //===----------------------------------------------------------------===//

  auto &IcmpActions = getActionDefinitionsBuilder(G_ICMP);
  // ICMP result is always s16 on ETCA (condition flags set in CCR, then
  // compared).  The operand type is computation-gated.
  if (HasByte)
    IcmpActions.legalFor({{s16, s8}});
  IcmpActions.legalFor({{s16, s16}});
  IcmpActions.legalFor(HasDW, {{s16, s32}});
  IcmpActions.legalFor(HasQW, {{s16, s64}});
  // Pointer comparisons are always legal (comparing addresses is the
  // same as comparing integers of the pointer width).  The instruction
  // selector handles p0 by using the pointer size to pick the right CMP
  // opcode (CMP32 for ptr32, CMP64 for ptr64, CMP for ptr16).
  IcmpActions.legalFor({{s16, p0}});
  IcmpActions.clampScalar(0, s16, s64);
  IcmpActions.clampScalar(1, MinLegal, MaxComp);

  //===----------------------------------------------------------------===//
  // SELECT — TIER 2 (subtarget-gated on data type)
  //===----------------------------------------------------------------===//

  auto &SelectActions = getActionDefinitionsBuilder(G_SELECT);
  // SELECT result is computation-gated; condition is always s16.
  if (HasByte)
    SelectActions.legalFor({{s8, s16}});
  SelectActions.legalFor({{s16, s16}});
  SelectActions.legalFor(HasDW, {{s32, s16}});
  SelectActions.legalFor(HasQW, {{s64, s16}});
  // Pointer-typed SELECT (e.g. select between two pointers) is always legal.
  // p0 size matches the pointer width (16/32/64) and is handled by the
  // instruction selector's G_SELECT handler via getRCForType.
  SelectActions.legalFor({{p0, s16}});
  SelectActions.clampScalar(0, MinLegal, MaxComp);
  SelectActions.clampScalar(1, s16, s64);

  //===----------------------------------------------------------------===//
  // BRCOND — always handles s16 condition
  //===----------------------------------------------------------------===//

  auto &BrcondActions = getActionDefinitionsBuilder(G_BRCOND);
  BrcondActions.legalFor({s16});
  BrcondActions.clampScalar(0, s16, s64);

  //===----------------------------------------------------------------===//
  // Constants — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &ConstActions = getActionDefinitionsBuilder(G_CONSTANT);
  dataFlowTypes(ConstActions);
  ConstActions.legalFor({p0});
  ConstActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  ConstActions.clampScalar(0, MinLegal, s64);

  //===----------------------------------------------------------------===//
  // Load and Store — TIER 1 (data-flow)
  //
  // In addition to scalar types, pointer types (p0) must be legal for
  // loads/stores of pointer values (common in alloca-based code where
  // the address of an alloca is stored to a pointer variable).
  //===----------------------------------------------------------------===//

  // Memory type for pointer-sized loads/stores.
  LLT PtrMemTy = LLT::scalar(PS);
  unsigned PtrAlign = PS / 8;

  auto &LoadActions = getActionDefinitionsBuilder(G_LOAD);
  if (HasByte)
    LoadActions.legalForTypesWithMemDesc({{s8, p0, s8, 1}});
  LoadActions.legalForTypesWithMemDesc({{s16, p0, s16, 2}});
  LoadActions.legalForTypesWithMemDesc({{s32, p0, s32, 4}});
  LoadActions.legalForTypesWithMemDesc({{s64, p0, s64, 8}});
  // Pointer loads: result type p0, addr p0, memory is PS-bit scalar
  LoadActions.legalForTypesWithMemDesc({{p0, p0, PtrMemTy, PtrAlign}});
  // Sub-MinLegal types (e.g., s1 from i1 loads) can't use widenScalar
  // because LLVM's generic widenScalar for G_LOAD returns UnableToLegalize.
  // Handle them via custom legalization that loads MinLegal bits and
  // truncates.
  LoadActions.customIf([=](const LegalityQuery &Q) {
    return Q.Types[0].getSizeInBits() < MinLegal.getSizeInBits();
  });
  LoadActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  LoadActions.clampScalar(0, MinLegal, s64);

  auto &StoreActions = getActionDefinitionsBuilder(G_STORE);
  if (HasByte)
    StoreActions.legalForTypesWithMemDesc({{s8, p0, s8, 1}});
  StoreActions.legalForTypesWithMemDesc({{s16, p0, s16, 2}});
  StoreActions.legalForTypesWithMemDesc({{s32, p0, s32, 4}});
  StoreActions.legalForTypesWithMemDesc({{s64, p0, s64, 8}});
  // Pointer stores: value type p0, addr p0, memory is PS-bit scalar
  StoreActions.legalForTypesWithMemDesc({{p0, p0, PtrMemTy, PtrAlign}});
  // Sub-MinLegal types — widen via custom legalization (same reasoning
  // as G_LOAD: the generic widenScalar for G_STORE isn't supported).
  StoreActions.customIf([=](const LegalityQuery &Q) {
    return Q.Types[0].getSizeInBits() < MinLegal.getSizeInBits();
  });
  StoreActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  StoreActions.clampScalar(0, MinLegal, s64);

  //===----------------------------------------------------------------===//
  // Implicit def, Frame index, Global value — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &ImpDefActions = getActionDefinitionsBuilder(G_IMPLICIT_DEF);
  dataFlowTypes(ImpDefActions);
  ImpDefActions.legalFor({p0});
  ImpDefActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  ImpDefActions.clampScalar(0, MinLegal, s64);

  getActionDefinitionsBuilder(G_FRAME_INDEX).legalFor({p0});

  getActionDefinitionsBuilder(G_GLOBAL_VALUE).legalFor({p0});

  //===---------------------------------------------------------------===//
  // INTTOPTR / PTRTOINT — TIER 1 (always-legal)
  //
  // G_INTTOPTR: int-to-pointer conversion (result is p0, src is scalar).
  // G_PTRTOINT: pointer-to-int conversion (result is scalar, src is p0).
  // These are register-class changes at the MIR level and the
  // instruction selector emits COPY for both.  They must be always-legal
  // because the IR's pointer type may differ from the target's
  // (e.g., when -mattr changes the pointer size but the IR was compiled
  // with a different datalayout).
  //===---------------------------------------------------------------===//

  getActionDefinitionsBuilder({G_INTTOPTR, G_PTRTOINT}).alwaysLegal();

  //===----------------------------------------------------------------===//
  // Pointer arithmetic — TIER 1 (data-flow)
  //
  // The offset type must match what the ALU can handle (s16 always,
  // s32 with HasDW, s64 with HasQW).  Pointer arithmetic decomposes
  // into word-size ADD instructions, so this mirrors computation gating.
  //===----------------------------------------------------------------===//

  auto &PtrAddActions = getActionDefinitionsBuilder(G_PTR_ADD);
  PtrAddActions.legalFor({{p0, s16}});
  PtrAddActions.legalFor(HasDW, {{p0, s32}});
  PtrAddActions.legalFor(HasQW, {{p0, s64}});
  PtrAddActions.widenScalarToNextPow2(1, MinLegal.getSizeInBits());
  PtrAddActions.clampScalar(1, MinLegal, MaxComp);

  //===----------------------------------------------------------------===//
  // Extensions and truncations — TIER 1 (always-legal)
  //
  // G_ZEXT, G_SEXT, and G_TRUNC are handled manually by the instruction
  // selector for ALL width combinations (s8/s16/s32/s64).  The selector
  // emits MOVZ/MOVS at the source width + COPY bridge for cross-width
  // extensions, so the legalizer must leave these untouched.
  //
  // We use alwaysLegal() because these have multiple type indices (result
  // and source) and we need ALL of them to be legal for all data-flow
  // scalar types.
  //===----------------------------------------------------------------===//

  getActionDefinitionsBuilder(G_ZEXT).alwaysLegal();
  getActionDefinitionsBuilder(G_SEXT).alwaysLegal();
  getActionDefinitionsBuilder(G_TRUNC).alwaysLegal();

  // G_ANYEXT is created by the legalizer's built-in combine step when
  // simplifying G_ZEXT/G_SEXT chains.  The instruction selector handles
  // it like G_ZEXT (MOVZ at source width + COPY bridge).
  getActionDefinitionsBuilder(G_ANYEXT).alwaysLegal();

  // G_FREEZE is a pass-through that marks a value as non-poison.
  // It behaves like a COPY at the MIR level and the instruction
  // selector emits a MOVZ.  Make it always-legal for all scalar
  // and pointer types.
  getActionDefinitionsBuilder(G_FREEZE).alwaysLegal();

  //===----------------------------------------------------------------===//
  // PHI — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &PhiActions = getActionDefinitionsBuilder(G_PHI);
  dataFlowTypes(PhiActions);
  PhiActions.legalFor({p0});
  PhiActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  PhiActions.clampScalar(0, MinLegal, s64);

  //===----------------------------------------------------------------===//
  // MUL/DIV/REM/SHIFT — libcalls (available for all widths via software)
  //
  // These use runtime library calls (__mulhi3, __divhi3, __modhi3, etc.)
  // provided by compiler-rt.  The libcall routing is set via libcallFor(),
  // and the actual Libcall→LibcallImpl name mapping is established in
  // ETCASubtarget::initLibcallLoweringInfo().
  //
  // G_ASHR and G_LSHR are also lowered to libcalls (__ashrhi3, __lshrhi3,
  // etc.) because the base ETCa ISA has no shift-right instruction.
  // G_SHL is handled inline by the instruction selector (using SLO/ADD for
  // constant shifts, or libcalls for variable shifts).
  //===----------------------------------------------------------------===//

  auto SetupLibcall = [&](unsigned Opc) {
    auto &B = getActionDefinitionsBuilder(Opc);
    B.libcallFor({s16});
    B.libcallFor(HasDW, {s32});
    B.libcallFor(HasQW, {s64});
    B.clampScalar(0, s16, MaxComp);
  };

  SetupLibcall(G_MUL);
  SetupLibcall(G_UDIV);
  SetupLibcall(G_SDIV);
  SetupLibcall(G_UREM);
  SetupLibcall(G_SREM);
  // G_ASHR and G_LSHR are now handled inline (moved to ArithActions above)
  // for constant shifts.  Non-constant shifts are handled by the instruction
  // selector which emits libcalls directly.

  //===----------------------------------------------------------------===//
  // Memory intrinsics — lowered to libcalls (memcpy, memmove, memset)
  //
  // G_MEMCPY, G_MEMMOVE, G_MEMSET are lowered to calls to the standard
  // library functions.  G_MEMCPY_INLINE is lowered to an inline sequence
  // of loads and stores.
  //===----------------------------------------------------------------===//

  //===----------------------------------------------------------------===//
  // Jump tables
  //===----------------------------------------------------------------===//

  // G_JUMP_TABLE materializes the address of a jump table (always pointer).
  getActionDefinitionsBuilder(G_JUMP_TABLE).legalFor({p0});

  // G_BRINDIRECT is used for indirect jumps via register (jmpr).
  // Mark it legal for pointer types (the selectWidth in the AsmParser
  // and the JMPR instruction handle all pointer widths via register class).
  getActionDefinitionsBuilder(G_BRINDIRECT).legalFor({p0});

  // G_BRJT branches via a jump table entry.  We expand it in the legalizer
  // into: load entry from table, then G_BRINDIRECT.
  auto &BrjtActions = getActionDefinitionsBuilder(G_BRJT);
  BrjtActions.customFor({{p0, s16}});
  BrjtActions.customFor(HasDW, {{p0, s32}});
  BrjtActions.customFor(HasQW, {{p0, s64}});
  BrjtActions.clampScalar(1, MinLegal, MaxComp);

  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE, G_MEMSET}).libcall();
  getActionDefinitionsBuilder(G_MEMCPY_INLINE).lower();

  //===----------------------------------------------------------------===//
  // Narrow/widen helper ops — TIER 3
  //
  // These instructions are produced by the legalizer when narrowing wide
  // computation ops.  They must be legal for their narrow types (plus carry
  // flag type s1).  Their operand types match the computation-gated types.
  //===----------------------------------------------------------------===//

  // G_MERGE_VALUES / G_UNMERGE_VALUES — TIER 1 (always-legal).
  // These combine/split multi-register values and the instruction selector
  // handles them.  They must accept all width combinations.
  getActionDefinitionsBuilder(G_MERGE_VALUES).alwaysLegal();
  getActionDefinitionsBuilder(G_UNMERGE_VALUES).alwaysLegal();

  // G_UADDE/G_USUBE/G_UADDO/G_USUBO — custom expansion
  //
  // The instruction selector cannot select these carry-producing ops.
  // Instead, expand them in the legalizer to simpler ops:
  //   G_UADDO(a,b) → G_ADD(a,b), G_ICMP(ult, sum, a)
  //   G_USUBO(a,b) → G_SUB(a,b), G_ICMP(ult, a, b)
  //   G_UADDE(a,b,ci) → G_ADD(a,b), carry, then add ci, combine carries
  //   G_USUBE(a,b,bi) → G_SUB(a,b), borrow, then sub bi, combine borrows
  //
  // These expanded ops (G_ADD, G_SUB, G_ICMP) are all handled by the
  // instruction selector (or lowered further).
  //
  // Only the narrow (legal) types are marked custom.  Wider types go
  // through the generic narrowScalar first, which produces narrow
  // versions that our custom handler expands.

  auto setupCarryOp = [&](auto &B) {
    if (HasByte)
      B.customFor({{s8, s1}});
    B.customFor({{s16, s1}});
    B.customFor(HasDW, {{s32, s1}});
    B.customFor(HasQW, {{s64, s1}});
  };

  auto &UaddeActions = getActionDefinitionsBuilder(G_UADDE);
  setupCarryOp(UaddeActions);
  UaddeActions.clampScalar(0, MinLegal, MaxComp);

  auto &UsubeActions = getActionDefinitionsBuilder(G_USUBE);
  setupCarryOp(UsubeActions);
  UsubeActions.clampScalar(0, MinLegal, MaxComp);

  auto &UaddoActions = getActionDefinitionsBuilder(G_UADDO);
  setupCarryOp(UaddoActions);
  UaddoActions.clampScalar(0, MinLegal, MaxComp);

  auto &UsuboActions = getActionDefinitionsBuilder(G_USUBO);
  setupCarryOp(UsuboActions);
  UsuboActions.legalFor({{s16, s1}});
  UsuboActions.legalFor(HasDW, {{s32, s1}});
  UsuboActions.legalFor(HasQW, {{s64, s1}});
  UsuboActions.clampScalar(0, MinLegal, MaxComp);

  // G_UMULH / G_SMULH: single-type operations, created by the legalizer
  // during narrowing.  The input/output types match the narrow computation
  // type, so make them always-legal.
  getActionDefinitionsBuilder(G_UMULH).alwaysLegal();
  getActionDefinitionsBuilder(G_SMULH).alwaysLegal();

  //===----------------------------------------------------------------===//
  // Min/Max — lowered to G_ICMP + G_SELECT
  //
  // ETCa has no dedicated min/max instructions.  These are expanded by the
  // generic LegalizerHelper::lowerMinMax() into:
  //   G_ICMP (pred, a, b) + G_SELECT (cmp, a, b)
  // Both G_ICMP and G_SELECT are already fully supported with subtarget
  // gating (HasByte/HasDW/HasQW), so no additional plumbing is needed.
  //
  // We use plain .lower() without type clamping.  The lowered G_ICMP and
  // G_SELECT are then legalized by their own rules (which include proper
  // widenScalarToNextPow2 + clampScalar for each subtarget configuration).
  // This avoids the need for a narrowScalar implementation for G_SMIN,
  // which the generic LegalizerHelper does not provide.
  //===----------------------------------------------------------------===//

  getActionDefinitionsBuilder({G_SMIN, G_SMAX, G_UMIN, G_UMAX}).lower();

  //===----------------------------------------------------------------===//
  // Absolute value and absolute difference — lowered via generic helper
  //
  // G_ABS, G_ABDS, G_ABDU: lowered via the generic LegalizerHelper which
  // decomposes into G_ADD, G_SUB, G_ICMP, G_SELECT, G_ASHR, G_XOR,
  // G_CONSTANT — all of which are already handled.
  //
  // These use plain .lower() — no computeTypes() call before, because
  // .legalFor() would conflict with .lower() on the same type set.
  // The widenScalarToNextPow2 + clampScalar ensure only valid types are
  // lowered for each subtarget configuration.
  //===----------------------------------------------------------------===//

  auto &AbsActions = getActionDefinitionsBuilder(G_ABS);
  AbsActions.lower();
  AbsActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  AbsActions.clampScalar(0, MinLegal, MaxComp);

  auto &AbdsActions = getActionDefinitionsBuilder({G_ABDS, G_ABDU});
  AbdsActions.lower();
  AbdsActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  AbdsActions.clampScalar(0, MinLegal, MaxComp);

  //===----------------------------------------------------------------===//
  // Saturating arithmetic — custom lowering (MinMax-style)
  //
  // All four sat ops (G_UADDSAT, G_USUBSAT, G_SADDSAT, G_SSUBSAT) use
  // custom lowering that directly emits the MinMax expansion.
  //
  // The generic LegalizerHelper's .lower() falls through to an
  // AddoSubo strategy that produces G_UADDO/G_USUBO/G_SADDO/G_SSUBO.
  // Since ETCA has no hardware carry/overflow detection, the instruction
  // selector cannot select these overflow ops.  By implementing the
  // MinMax expansion directly in custom lowering, we avoid producing
  // unselectable G_UADDO/G_SADDO nodes.
  //
  // Expansions:
  //   uadd.sat(a, b) → a + umin(~a, b)
  //   usub.sat(a, b) → a - umin(a, b)
  //   sadd.sat(a, b) → a + smin(smax(lo, b), hi)
  //                     lo = 0x8000... - smin(a, 0)
  //                     hi = 0x7fff... - smax(a, 0)
  //   ssub.sat(a, b) → a - smin(smax(lo, b), hi)
  //                     lo = smax(a, -1) - 0x7fff...
  //                     hi = smin(a, -1) - 0x8000...
  //
  // G_SMIN, G_SMAX, G_UMIN, G_UMAX are lowered to G_ICMP+G_SELECT by
  // their own rules, so the expansion is fully decomposed into legal ops.
  //===----------------------------------------------------------------===//

  auto isLegalType = [=](const LegalityQuery &Q) -> bool {
    unsigned Size = Q.Types[0].getSizeInBits();
    return Size >= MinLegal.getSizeInBits() && Size <= MaxComp.getSizeInBits();
  };

  auto &UAddSatActions = getActionDefinitionsBuilder(G_UADDSAT);
  UAddSatActions.customIf(isLegalType);
  UAddSatActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  UAddSatActions.clampScalar(0, MinLegal, MaxComp);

  auto &USubSatActions = getActionDefinitionsBuilder(G_USUBSAT);
  USubSatActions.customIf(isLegalType);
  USubSatActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  USubSatActions.clampScalar(0, MinLegal, MaxComp);

  auto &SAddSatActions = getActionDefinitionsBuilder(G_SADDSAT);
  SAddSatActions.customIf(isLegalType);
  SAddSatActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  SAddSatActions.clampScalar(0, MinLegal, MaxComp);

  auto &SSubSatActions = getActionDefinitionsBuilder(G_SSUBSAT);
  SSubSatActions.customIf(isLegalType);
  SSubSatActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  SSubSatActions.clampScalar(0, MinLegal, MaxComp);

  //===----------------------------------------------------------------===//
  // Mark unhandled ops as unsupported (will cause GISel abort)
  //===----------------------------------------------------------------===//

  // Finalize the legalization tables.  This must be called after all
  // getActionDefinitionsBuilder calls are complete.
  getLegacyLegalizerInfo().computeTables();
}

bool ETCALegalizerInfo::legalizeCustom(
    LegalizerHelper &Helper, MachineInstr &MI,
    LostDebugLocObserver &LocObserver) const {
  MachineIRBuilder &MIRBuilder = Helper.MIRBuilder;

  switch (MI.getOpcode()) {
  default:
    return false;
  case TargetOpcode::G_BRJT:
    return legalizeBRJT(MI, MIRBuilder);
  case TargetOpcode::G_LOAD:
    return legalizeSubMinLegalLoad(MI, MIRBuilder);
  case TargetOpcode::G_STORE:
    return legalizeSubMinLegalStore(MI, MIRBuilder);
  case TargetOpcode::G_UADDSAT:
    return legalizeUAddSat(MI, MIRBuilder);
  case TargetOpcode::G_USUBSAT:
    return legalizeUSubSat(MI, MIRBuilder);
  case TargetOpcode::G_SADDSAT:
    return legalizeSAddSat(MI, MIRBuilder);
  case TargetOpcode::G_SSUBSAT:
    return legalizeSSubSat(MI, MIRBuilder);
  case TargetOpcode::G_UADDO:
    return legalizeUAddo(MI, MIRBuilder);
  case TargetOpcode::G_USUBO:
    return legalizeUSubo(MI, MIRBuilder);
  case TargetOpcode::G_UADDE:
    return legalizeUAdde(MI, MIRBuilder);
  case TargetOpcode::G_USUBE:
    return legalizeUSube(MI, MIRBuilder);
  }
}

bool ETCALegalizerInfo::legalizeSubMinLegalLoad(
    MachineInstr &MI, MachineIRBuilder &MIRBuilder) const {
  // Widen a sub-MinLegal G_LOAD to the minimum legal scalar type.
  // Load MinLegal bits, then truncate to the original (narrow) type.
  Register Dst = MI.getOperand(0).getReg();
  Register Addr = MI.getOperand(1).getReg();
  LLT DstTy = MIRBuilder.getMRI()->getType(Dst);
  unsigned DstBits = DstTy.getSizeInBits();

  // Safety: only handle types smaller than MinLegalSize.
  if (DstBits >= MinLegalSize)
    return false;

  // Create a new MMO with the widened memory type so that the
  // legalizeInstrStep → legalForTypesWithMemDesc rules match.
  // The original MMO has a narrow MemoryType (e.g., s8 for a 1-byte i1)
  // which doesn't match the legal-width rule (s16, p0, s16, 2).
  MachineMemOperand &OldMMO = **MI.memoperands_begin();
  MachineFunction &MF = MIRBuilder.getMF();
  LLT WideTy = LLT::scalar(MinLegalSize);
  auto *NewMMO = MF.getMachineMemOperand(
      OldMMO.getPointerInfo(), OldMMO.getFlags(), WideTy, OldMMO.getBaseAlign(),
      OldMMO.getAAInfo());
  auto WideLoad = MIRBuilder.buildLoad(WideTy, Addr, *NewMMO);
  MIRBuilder.buildTrunc(Dst, WideLoad);
  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeSubMinLegalStore(
    MachineInstr &MI, MachineIRBuilder &MIRBuilder) const {
  // Widen a sub-MinLegal G_STORE to the minimum legal scalar type.
  // Extend the value to MinLegal bits (anyext — upper bits are undefined),
  // then store the wider type.
  Register Val = MI.getOperand(0).getReg();
  Register Addr = MI.getOperand(1).getReg();
  LLT ValTy = MIRBuilder.getMRI()->getType(Val);
  unsigned ValBits = ValTy.getSizeInBits();

  if (ValBits >= MinLegalSize)
    return false;

  // Create a new MMO with the widened memory type so the legalizer knows
  // this is a legal-width memory access.
  MachineMemOperand &OldMMO = **MI.memoperands_begin();
  MachineFunction &MF = MIRBuilder.getMF();
  LLT WideTy = LLT::scalar(MinLegalSize);
  auto *NewMMO = MF.getMachineMemOperand(
      OldMMO.getPointerInfo(), OldMMO.getFlags(), WideTy, OldMMO.getBaseAlign(),
      OldMMO.getAAInfo());
  auto WideVal = MIRBuilder.buildAnyExt(WideTy, Val);
  MIRBuilder.buildStore(WideVal, Addr, *NewMMO);
  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeBRJT(MachineInstr &MI,
                                     MachineIRBuilder &MIRBuilder) const {
  // G_BRJT operands: [tablePtr (p0), JTI (imm), index (scalar)]
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  MachineFunction &MF = MIRBuilder.getMF();
  const MachineJumpTableInfo *MJTI = MF.getJumpTableInfo();
  if (!MJTI)
    return false;

  Register PtrReg = MI.getOperand(0).getReg();
  Register IndexReg = MI.getOperand(2).getReg();

  LLT PtrTy = MRI.getType(PtrReg);
  LLT IndexTy = MRI.getType(IndexReg);
  unsigned EntrySize = MJTI->getEntrySize(MF.getDataLayout());
  unsigned EntryAlign = MJTI->getEntryAlignment(MF.getDataLayout());

  // Shift index by log2(EntrySize) to get byte offset.
  // EntrySize is always a power of 2 (2 for 16-bit ptrs, 4 for 32-bit, etc.).
  Register ScaledIndex;
  if (EntrySize > 1) {
    auto ShiftAmt = MIRBuilder.buildConstant(IndexTy, Log2_32(EntrySize));
    ScaledIndex = MIRBuilder.buildShl(IndexTy, IndexReg, ShiftAmt).getReg(0);
  } else {
    ScaledIndex = IndexReg;
  }

  // Compute table address + offset.
  auto LoadAddr = MIRBuilder.buildPtrAdd(PtrTy, PtrReg, ScaledIndex);

  // Load the jump target address from the table.
  // The entry kind determines how the address is encoded.
  // For ETCa (embedded, absolute addressing), we use EK_BlockAddress.
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getJumpTable(MF), MachineMemOperand::MOLoad,
      EntrySize, Align(EntryAlign));

  Register TargetReg;
  switch (MJTI->getEntryKind()) {
  default:
    return false;
  case MachineJumpTableInfo::EK_BlockAddress:
    // Each entry is a plain absolute address of the target block.
    // Load the pointer-sized entry.
    TargetReg = MIRBuilder.buildLoad(PtrTy, LoadAddr, *MMO).getReg(0);
    break;
  case MachineJumpTableInfo::EK_LabelDifference32:
  case MachineJumpTableInfo::EK_Custom32: {
    // For PIC-style entries, load a 32-bit PC-relative delta and add the
    // table base to get the absolute address.
    LLT LoadTy = LLT::scalar(32);
    auto Loaded = MIRBuilder.buildLoad(LoadTy, LoadAddr, *MMO);
    auto LoadedSExt = MIRBuilder.buildSExt(PtrTy, Loaded);
    TargetReg = MIRBuilder.buildPtrAdd(PtrTy, PtrReg, LoadedSExt).getReg(0);
    break;
  }
  }

  // Indirect branch to the target address.
  MIRBuilder.buildBrIndirect(TargetReg);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUAddSat(MachineInstr &MI,
                                        MachineIRBuilder &MIRBuilder) const {
  // Expand G_UADDSAT(a, b) → a + umin(~a, b)
  //   ~a = xor a, -1  (bitwise NOT)
  //   umin(~a, b) → (via lowering) icmp + select
  //   result = a + umin(...)
  //
  // This avoids G_UADDO which the instruction selector cannot select.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  LLT Ty = MRI.getType(Dst);

  auto NegOne = MIRBuilder.buildConstant(Ty, -1);
  auto NotA = MIRBuilder.buildXor(Ty, LHS, NegOne);
  auto Min = MIRBuilder.buildUMin(Ty, NotA, RHS);
  MIRBuilder.buildAdd(Dst, LHS, Min);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUSubSat(MachineInstr &MI,
                                        MachineIRBuilder &MIRBuilder) const {
  // Expand G_USUBSAT(a, b) → a - umin(a, b)
  //   umin(a, b) → (via lowering) icmp + select
  //   result = a - umin(...)
  //
  // This avoids G_USUBO which the instruction selector cannot select.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  LLT Ty = MRI.getType(Dst);

  auto Min = MIRBuilder.buildUMin(Ty, LHS, RHS);
  MIRBuilder.buildSub(Dst, LHS, Min);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeSAddSat(MachineInstr &MI,
                                        MachineIRBuilder &MIRBuilder) const {
  // Expand G_SADDSAT(a, b) using MinMax strategy:
  //   hi = 0x7fff... - smax(a, 0)
  //   lo = 0x8000... - smin(a, 0)
  //   result = a + smin(smax(lo, b), hi)
  //
  // This avoids G_SADDO/G_SSUBO which ETCA doesn't support.
  // The G_SMIN/G_SMAX nodes produced here will be lowered to
  // G_ICMP+G_SELECT by the legalizer in a subsequent pass.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  LLT Ty = MRI.getType(Dst);
  unsigned NumBits = Ty.getScalarSizeInBits();

  auto MaxVal = MIRBuilder.buildConstant(Ty, APInt::getSignedMaxValue(NumBits));
  auto MinVal = MIRBuilder.buildConstant(Ty, APInt::getSignedMinValue(NumBits));
  auto Zero = MIRBuilder.buildConstant(Ty, 0);

  // hi = 0x7fff... - smax(a, 0)
  auto SMaxA0 = MIRBuilder.buildSMax(Ty, LHS, Zero);
  auto Hi = MIRBuilder.buildSub(Ty, MaxVal, SMaxA0);

  // lo = 0x8000... - smin(a, 0)
  auto SMinA0 = MIRBuilder.buildSMin(Ty, LHS, Zero);
  auto Lo = MIRBuilder.buildSub(Ty, MinVal, SMinA0);

  // RHSClamped = smin(smax(lo, b), hi)
  auto SMaxLoRHS = MIRBuilder.buildSMax(Ty, Lo, RHS);
  auto RHSClamped = MIRBuilder.buildSMin(Ty, SMaxLoRHS, Hi);

  // result = a + RHSClamped
  MIRBuilder.buildAdd(Dst, LHS, RHSClamped);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeSSubSat(MachineInstr &MI,
                                        MachineIRBuilder &MIRBuilder) const {
  // Expand G_SSUBSAT(a, b) using MinMax strategy:
  //   lo = smax(a, -1) - 0x7fff...
  //   hi = smin(a, -1) - 0x8000...
  //   result = a - smin(smax(lo, b), hi)
  //
  // This avoids G_SADDO/G_SSUBO which ETCA doesn't support.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register Dst = MI.getOperand(0).getReg();
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  LLT Ty = MRI.getType(Dst);
  unsigned NumBits = Ty.getScalarSizeInBits();

  auto MaxVal = MIRBuilder.buildConstant(Ty, APInt::getSignedMaxValue(NumBits));
  auto MinVal = MIRBuilder.buildConstant(Ty, APInt::getSignedMinValue(NumBits));
  auto NegOne = MIRBuilder.buildConstant(Ty, -1);

  // lo = smax(a, -1) - 0x7fff...
  auto SMaxAN1 = MIRBuilder.buildSMax(Ty, LHS, NegOne);
  auto Lo = MIRBuilder.buildSub(Ty, SMaxAN1, MaxVal);

  // hi = smin(a, -1) - 0x8000...
  auto SMinAN1 = MIRBuilder.buildSMin(Ty, LHS, NegOne);
  auto Hi = MIRBuilder.buildSub(Ty, SMinAN1, MinVal);

  // RHSClamped = smin(smax(lo, b), hi)
  auto SMaxLoRHS = MIRBuilder.buildSMax(Ty, Lo, RHS);
  auto RHSClamped = MIRBuilder.buildSMin(Ty, SMaxLoRHS, Hi);

  // result = a - RHSClamped
  MIRBuilder.buildSub(Dst, LHS, RHSClamped);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUAddo(MachineInstr &MI,
                                      MachineIRBuilder &MIRBuilder) const {
  // Expand G_UADDO(a, b) → {sum, carry}:
  //   sum = ADD a, b
  //   carry = ICMP_ULT(sum, a)
  //
  // G_ICMP result type is s16 on ETCA.  Since G_UADDO's carry is s1,
  // we emit the ICMP with an s16 temporary and truncate to s1.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register DstSum = MI.getOperand(0).getReg();
  Register DstCarry = MI.getOperand(1).getReg();
  Register Src1 = MI.getOperand(2).getReg();
  Register Src2 = MI.getOperand(3).getReg();

  // sum = a + b
  MIRBuilder.buildAdd(DstSum, Src1, Src2);

  // carry = (sum < a): ICMP produces s16, truncate to s1
  auto Cmp =
      MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), DstSum, Src1);
  MIRBuilder.buildTrunc(DstCarry, Cmp);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUSubo(MachineInstr &MI,
                                      MachineIRBuilder &MIRBuilder) const {
  // Expand G_USUBO(a, b) → {diff, borrow}:
  //   diff = SUB a, b
  //   borrow = ICMP_ULT(a, b)
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register DstDiff = MI.getOperand(0).getReg();
  Register DstBorrow = MI.getOperand(1).getReg();
  Register Src1 = MI.getOperand(2).getReg();
  Register Src2 = MI.getOperand(3).getReg();

  // diff = a - b
  MIRBuilder.buildSub(DstDiff, Src1, Src2);

  // borrow = (a < b): ICMP produces s16, truncate to s1
  auto Cmp =
      MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), Src1, Src2);
  MIRBuilder.buildTrunc(DstBorrow, Cmp);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUAdde(MachineInstr &MI,
                                      MachineIRBuilder &MIRBuilder) const {
  // Expand G_UADDE(a, b, carry_in) → {sum, carry}:
  //   sum1 = ADD a, b
  //   sum = ADD sum1, ZEXT(carry_in)
  //   carry = ICMP_ULT(sum, sum1) | ICMP_ULT(sum1, a)
  //
  // ICMP produces s16 on ETCA; carry is s1.  Truncate after OR.
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register DstSum = MI.getOperand(0).getReg();
  Register DstCarry = MI.getOperand(1).getReg();
  Register Src1 = MI.getOperand(2).getReg();
  Register Src2 = MI.getOperand(3).getReg();
  Register CarryIn = MI.getOperand(4).getReg();
  LLT Ty = MRI.getType(DstSum);

  // sum1 = a + b — use buildAdd which creates a properly-typed vreg
  auto Sum1MI = MIRBuilder.buildAdd(Ty, Src1, Src2);

  // carry1 = (sum1 < a) — as s16
  auto Cmp1 =
      MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), Sum1MI, Src1);

  // sum = sum1 + zext(carry_in)
  auto CarryInExt = MIRBuilder.buildZExt(Ty, CarryIn);
  MIRBuilder.buildAdd(DstSum, Sum1MI, CarryInExt);

  // carry2 = (sum < sum1) — as s16
  auto Cmp2 =
      MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), DstSum, Sum1MI);

  // carry = (carry1 | carry2), truncated to s1
  auto Combined = MIRBuilder.buildOr(LLT::scalar(16), Cmp1, Cmp2);
  MIRBuilder.buildTrunc(DstCarry, Combined);

  MI.eraseFromParent();
  return true;
}

bool ETCALegalizerInfo::legalizeUSube(MachineInstr &MI,
                                      MachineIRBuilder &MIRBuilder) const {
  // Expand G_USUBE(a, b, borrow_in) → {diff, borrow}:
  //   diff1 = SUB a, b
  //   diff = SUB diff1, ZEXT(borrow_in)
  //   borrow = ICMP_ULT(a, b) | ICMP_ULT(diff1, ZEXT(borrow_in))
  MachineRegisterInfo &MRI = *MIRBuilder.getMRI();
  Register DstDiff = MI.getOperand(0).getReg();
  Register DstBorrow = MI.getOperand(1).getReg();
  Register Src1 = MI.getOperand(2).getReg();
  Register Src2 = MI.getOperand(3).getReg();
  Register BorrowIn = MI.getOperand(4).getReg();
  LLT Ty = MRI.getType(DstDiff);

  // diff1 = a - b
  auto Diff1MI = MIRBuilder.buildSub(Ty, Src1, Src2);

  // borrow1 = (a < b)
  auto Cmp1 =
      MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), Src1, Src2);

  // diff = diff1 - zext(borrow_in)
  auto BorrowInExt = MIRBuilder.buildZExt(Ty, BorrowIn);
  MIRBuilder.buildSub(DstDiff, Diff1MI, BorrowInExt);

  // borrow2 = (diff1 < zext(borrow_in))
  auto Cmp2 = MIRBuilder.buildICmp(CmpInst::ICMP_ULT, LLT::scalar(16), Diff1MI,
                                   BorrowInExt);

  // borrow = (borrow1 | borrow2), truncated to s1
  auto Combined = MIRBuilder.buildOr(LLT::scalar(16), Cmp1, Cmp2);
  MIRBuilder.buildTrunc(DstBorrow, Combined);

  MI.eraseFromParent();
  return true;
}
