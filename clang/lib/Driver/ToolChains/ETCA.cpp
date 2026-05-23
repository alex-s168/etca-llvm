//===--- ETCA.cpp - ETCA ToolChain Implementations --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA is a custom 16-bit RISC ISA targeting bare-metal embedded systems.
// The ToolChain provides:
//   - GCC-compatible include path discovery
//   - Default CPU selection (generic = 16-bit)
//   - ELF linking support (delegated to etca-elf-ld or lld)
//   - No built-in CRT or libc support (bare-metal)
//
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Types.h"
#include "clang/Options/Options.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/TargetParser/SubtargetFeature.h"
#include "llvm/TargetParser/Triple.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

//===----------------------------------------------------------------------===//
// Helper: extract pointer/word sizes from -mattr= or individual -m feature
// flags
//===----------------------------------------------------------------------===//

static void parseETCAFeatures(const ArgList &Args, unsigned &WordSize,
                              unsigned &PtrSize) {
  // Parse individual ETCA -m feature flags to determine word and pointer
  // sizes.  Default is 16-bit word + 16-bit pointer.
  WordSize = 16;
  PtrSize = 16;

  if (Args.hasArg(options::OPT_m64bit))
    WordSize = 64;
  else if (Args.hasArg(options::OPT_m32bit))
    WordSize = 32;

  if (Args.hasArg(options::OPT_mptr64))
    PtrSize = 64;
  else if (Args.hasArg(options::OPT_mptr32))
    PtrSize = 32;
}

//===----------------------------------------------------------------------===//
// ETCA ToolChain
//===----------------------------------------------------------------------===//

ETCAToolChain::ETCAToolChain(const Driver &D, const llvm::Triple &Triple,
                             const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  // Try to find a GCC installation for ETCA (e.g., etca-elf-gcc).
  // This gives us access to etca-elf-ld, crt objects, and system includes.
  GCCInstallation.init(Triple, Args);

  if (GCCInstallation.isValid()) {
    // Add GCC bin directory to program paths (for etca-elf-ld, etc.)
    SmallString<128> GCCBinPath;
    llvm::sys::path::append(GCCBinPath, GCCInstallation.getParentLibPath(),
                            "..", "bin");
    addPathIfExists(D, GCCBinPath, getProgramPaths());

    // Add GCC lib directory to file paths
    SmallString<128> GCCLibPath;
    llvm::sys::path::append(GCCLibPath, GCCInstallation.getInstallPath(),
                            GCCInstallation.getMultilib().gccSuffix());
    addPathIfExists(D, GCCLibPath, getFilePaths());
  }
}

void ETCAToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                              ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc) ||
      DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // If we have a GCC installation, add its include path
  if (GCCInstallation.isValid()) {
    SmallString<128> IncludeDir;
    llvm::sys::path::append(IncludeDir, GCCInstallation.getParentLibPath(),
                            "..", GCCInstallation.getTriple().str(), "include");
    if (llvm::sys::fs::is_directory(IncludeDir))
      addSystemInclude(DriverArgs, CC1Args, IncludeDir);
  }

  // Add the ELF-specific include path for the target triple
  SmallString<128> Dir;
  if (!getDriver().SysRoot.empty())
    Dir = getDriver().SysRoot;
  else
    llvm::sys::path::append(Dir, getDriver().Dir, "..");

  llvm::sys::path::append(Dir, getTripleString(), "include");
  if (llvm::sys::fs::is_directory(Dir))
    addSystemInclude(DriverArgs, CC1Args, Dir);
}

std::string
ETCAToolChain::ComputeEffectiveClangTriple(const ArgList &Args,
                                           types::ID InputType) const {
  // Start with the base triple (e.g., "etca-unknown-elf").
  std::string TripleStr = getTripleString().str();

  // Determine pointer size from -mattr features.
  // We encode the pointer size in the OS field via a numeric suffix:
  //   etca-unknown-elf    → 16-bit pointer (default)
  //   etca-unknown-elf32  → 32-bit pointer
  //   etca-unknown-elf64  → 64-bit pointer
  //
  // The LLVM MC layer (ETCAMCAsmInfo factory) reads this suffix to set
  // CodePointerSize and CalleeSaveStackSlotSize correctly.
  unsigned PtrSize, WordSize;
  parseETCAFeatures(Args, WordSize, PtrSize);

  // If the pointer size doesn't match the default (16-bit), append the
  // size to the OS name. The base triple is e.g. "etca-unknown-elf".
  if (PtrSize != 16) {
    // Rebuild triple string: "etca-unknown-elf" + "32" or "64"
    TripleStr =
        (getArchName().str() + "-unknown-elf" + (PtrSize == 32 ? "32" : "64"));
  }

  return TripleStr;
}

void ETCAToolChain::addClangTargetOptions(
    const ArgList &DriverArgs, ArgStringList &CC1Args,
    Action::OffloadKind DeviceOffloadKind) const {
  // Don't override default init-array behavior for bare-metal ETCA.
  // lld on ETCA supports .init_array by default.
}

Tool *ETCAToolChain::buildLinker() const {
  return new tools::ETCA::Linker(*this);
}

//===----------------------------------------------------------------------===//
// ETCA Linker
//===----------------------------------------------------------------------===//

void ETCA::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {
  const ToolChain &ToolChain = getToolChain();

  // Determine which linker to use (handles -fuse-ld=lld, -fuse-ld=bfd, etc.)
  bool LinkerIsLLD = false;
  std::string Linker = ToolChain.GetLinkerPath(&LinkerIsLLD);

  // Also detect LLD from the resolved path (GetLinkerPath only sets
  // LinkerIsLLD when -fuse-ld=lld is explicit; the default "ld.lld"
  // path needs filename detection too).
  if (!LinkerIsLLD) {
    StringRef LinkerName = llvm::sys::path::filename(Linker);
    LinkerIsLLD = LinkerName.contains("lld");
  }

  ArgStringList CmdArgs;

  // Determine emulation from -mattr features.
  // The pointer size determines whether we need 64-bit or 32-bit ELF.
  unsigned PtrSize, WordSize;
  parseETCAFeatures(Args, WordSize, PtrSize);
  StringRef Emulation;
  if (LinkerIsLLD) {
    // LLD emulation flags: ptr64 → elf64etca, ptr16/ptr32 → elf32etca
    if (PtrSize == 64)
      Emulation = "elf64etca";
    else
      Emulation = "elf32etca";
  } else {
    // Binutils emulation flags:
    //   ptr64  → elf64_etca
    //   ptr32  → elf32_etca
    //   ptr16  → elf16_etca
    if (PtrSize == 64)
      Emulation = "elf64_etca";
    else if (PtrSize == 32)
      Emulation = "elf32_etca";
    else
      Emulation = "elf16_etca";
  }

  if (!Emulation.empty()) {
    CmdArgs.push_back(Args.MakeArgString(Twine("-m") + Emulation));
  }

  // Output file
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Garbage collect unused sections
  if (!Args.hasArg(options::OPT_r))
    CmdArgs.push_back("--gc-sections");

  // Library search paths
  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  // Linker inputs
  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  // Standard libraries (if not disabled)
  if (!Args.hasArg(options::OPT_nostdlib) && !Args.hasArg(options::OPT_r) &&
      !Args.hasArg(options::OPT_nodefaultlibs)) {
    // On bare-metal, we link -lc if it's available
    if (!Args.hasArg(options::OPT_nolibc)) {
      CmdArgs.push_back("--start-group");
      CmdArgs.push_back("-lc");
      CmdArgs.push_back("-lgcc");
      CmdArgs.push_back("--end-group");
    }
  }

  // User-specified -T linker script or other flags
  Args.AddAllArgs(CmdArgs, options::OPT_T);
  Args.AddAllArgs(CmdArgs, options::OPT_s);
  Args.AddAllArgs(CmdArgs, options::OPT_t);
  Args.AddAllArgs(CmdArgs, options::OPT_u);
  Args.AddAllArgs(CmdArgs, options::OPT_n);

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(), Args.MakeArgString(Linker),
      CmdArgs, Inputs, Output));
}
