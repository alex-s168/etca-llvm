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
// subtarget's word size and enabled extensions:
//   - 16-bit word (generic):       i16 is legal; i8, i32, i64 are narrowed.
//   - 32-bit word (etca32):        i16 and i32 are legal; i8, i64 are narrowed.
//   - 64-bit word (etca64, etc.):  i16, i32, and i64 are legal; i8 is narrowed.
//   - BYTE extension:              i8 becomes legal (MinLegal = s8).
//
// == Design ==
//
// Two tiers of type legality:
//
// Tier 1 — Data-flow ops (G_ZEXT, G_SEXT, G_TRUNC, G_MERGE_VALUES,
//          G_UNMERGE_VALUES, G_CONSTANT, G_LOAD, G_STORE, G_PHI, G_IMPLICIT_DEF,
//          G_FRAME_INDEX, G_GLOBAL_VALUE, G_PTR_ADD):
//          s32 and s64 are ALWAYS legal because the instruction selector handles
//          these manually (e.g., G_ZEXT uses MOVZ at source width + COPY bridge).
//          Making these illegal would cause the legalizer to create G_MERGE_VALUES
//          of illegal result types, leading to infinite regress.
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
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "etca-legalizer"

using namespace llvm;

ETCALegalizerInfo::ETCALegalizerInfo(const ETCASubtarget &ST) {
  using namespace TargetOpcode;

  unsigned PS = ST.getPtrSize();
  bool HasByte = ST.hasByte();
  bool HasDW   = ST.hasDW();
  bool HasQW   = ST.hasQW();

  const LLT s1  = LLT::scalar(1);
  const LLT s8  = LLT::scalar(8);
  const LLT s16 = LLT::scalar(16);
  const LLT s32 = LLT::scalar(32);
  const LLT s64 = LLT::scalar(64);
  const LLT p0  = LLT::pointer(0, PS);

  // Minimum legal scalar — i8 if BYTE extension, otherwise i16.
  LLT MinLegal = HasByte ? s8 : s16;

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
      {G_ADD, G_SUB, G_AND, G_OR, G_XOR, G_SHL});
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
  ConstActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  ConstActions.clampScalar(0, MinLegal, s64);

  //===----------------------------------------------------------------===//
  // Load and Store — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &LoadActions = getActionDefinitionsBuilder(G_LOAD);
  if (HasByte)
    LoadActions.legalForTypesWithMemDesc({{s8, p0, s8, 1}});
  LoadActions.legalForTypesWithMemDesc({{s16, p0, s16, 2}});
  LoadActions.legalForTypesWithMemDesc({{s32, p0, s32, 4}});
  LoadActions.legalForTypesWithMemDesc({{s64, p0, s64, 8}});
  LoadActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  LoadActions.clampScalar(0, MinLegal, s64);

  auto &StoreActions = getActionDefinitionsBuilder(G_STORE);
  if (HasByte)
    StoreActions.legalForTypesWithMemDesc({{s8, p0, s8, 1}});
  StoreActions.legalForTypesWithMemDesc({{s16, p0, s16, 2}});
  StoreActions.legalForTypesWithMemDesc({{s32, p0, s32, 4}});
  StoreActions.legalForTypesWithMemDesc({{s64, p0, s64, 8}});
  StoreActions.widenScalarToNextPow2(0, MinLegal.getSizeInBits());
  StoreActions.clampScalar(0, MinLegal, s64);

  //===----------------------------------------------------------------===//
  // Implicit def, Frame index, Global value — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &ImpDefActions = getActionDefinitionsBuilder(G_IMPLICIT_DEF);
  dataFlowTypes(ImpDefActions);
  ImpDefActions.clampScalar(0, MinLegal, s64);

  getActionDefinitionsBuilder(G_FRAME_INDEX)
      .legalFor({p0});

  getActionDefinitionsBuilder(G_GLOBAL_VALUE)
      .legalFor({p0});

  //===----------------------------------------------------------------===//
  // Pointer arithmetic — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  getActionDefinitionsBuilder(G_PTR_ADD)
      .legalFor({{p0, s16}})
      .clampScalar(1, s16, s64);

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

  //===----------------------------------------------------------------===//
  // PHI — TIER 1 (data-flow)
  //===----------------------------------------------------------------===//

  auto &PhiActions = getActionDefinitionsBuilder(G_PHI);
  dataFlowTypes(PhiActions);

  //===----------------------------------------------------------------===//
  // MUL/DIV/REM — libcalls (available for all widths via software)
  //
  // These use runtime library calls (__mulhi3, __divhi3, __modhi3, etc.)
  // provided by compiler-rt.  The libcall routing is set via libcallFor(),
  // and the actual Libcall→LibcallImpl name mapping is established in
  // ETCASubtarget::initLibcallLoweringInfo().
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

  // G_UADDE/G_USUBE: carries are s1
  auto &UaddeActions = getActionDefinitionsBuilder(G_UADDE);
  if (HasByte)
    UaddeActions.legalFor({{s8, s1}});
  UaddeActions.legalFor({{s16, s1}});
  UaddeActions.legalFor(HasDW, {{s32, s1}});
  UaddeActions.legalFor(HasQW, {{s64, s1}});
  UaddeActions.clampScalar(0, MinLegal, MaxComp);

  auto &UsubeActions = getActionDefinitionsBuilder(G_USUBE);
  if (HasByte)
    UsubeActions.legalFor({{s8, s1}});
  UsubeActions.legalFor({{s16, s1}});
  UsubeActions.legalFor(HasDW, {{s32, s1}});
  UsubeActions.legalFor(HasQW, {{s64, s1}});
  UsubeActions.clampScalar(0, MinLegal, MaxComp);

  // G_UADDO/G_USUBO: carry-out is s1
  auto &UaddoActions = getActionDefinitionsBuilder(G_UADDO);
  if (HasByte)
    UaddoActions.legalFor({{s8, s1}});
  UaddoActions.legalFor({{s16, s1}});
  UaddoActions.legalFor(HasDW, {{s32, s1}});
  UaddoActions.legalFor(HasQW, {{s64, s1}});
  UaddoActions.clampScalar(0, MinLegal, MaxComp);

  auto &UsuboActions = getActionDefinitionsBuilder(G_USUBO);
  if (HasByte)
    UsuboActions.legalFor({{s8, s1}});
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
  // Mark unhandled ops as unsupported (will cause GISel abort)
  //===----------------------------------------------------------------===//
}
