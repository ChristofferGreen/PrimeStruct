#pragma once

#include "NativeEmitterInternals.h"
#include "NativeEmitterInternalsX64.h"
#include "primec/backend/NativeEmitter.h"

#include <string>
#include <vector>

namespace primec::native_emitter {

struct NativeEmitterFunctionLayout {
  size_t localCount = 0;
  bool needsPrintScratch = false;
  bool needsArgc = false;
  bool needsArgv = false;
  uint32_t argcLocalIndex = 0;
  uint32_t argvLocalIndex = 0;
  uint32_t framePointerLocalIndex = 0;
  uint32_t linkLocalIndex = 0;
  uint32_t scratchSlots = 0;
  uint32_t scratchBytes = 0;
  uint32_t scratchOffset = 0;
  uint64_t localsSize = 0;
  uint64_t frameSize = 0;
};

struct NativeEmitterBranchFixup {
  size_t codeIndex = 0;
  size_t functionIndex = 0;
  size_t targetInst = 0;
  bool isConditional = false;
};

struct NativeEmitterCallFixup {
  size_t codeIndex = 0;
  size_t targetFunctionIndex = 0;
};

struct NativeEmitterStringFixup {
  size_t codeIndex = 0;
  uint32_t stringIndex = 0;
};

// The C functions the program calls (CallHost, arm64 native only): their names in slot order (the
// pointer slots of the image's data segment, see MachOData) and the code that loads each slot.
struct NativeEmitterHostFixup {
  size_t codeIndex = 0; // word index of the ADRP of the slot load (the LDR follows it)
  size_t slot = 0;
};

struct NativeEmitterHostImports {
  std::vector<std::string> symbols;
  std::vector<NativeEmitterHostFixup> fixups;
};

// Templated so the exact same IR-dispatch logic drives whichever
// concrete emitter this platform builds - see NativeEmitterFunctionEmit.cpp
// for the definition and the platform-gated explicit instantiation.
// EmitterT is always deduced from the `emitter` argument at the call
// site (NativeEmitterEmit.cpp), so callers never need to name it.
template <typename EmitterT>
bool emitNativeFunctions(const IrModule &module,
                         size_t entryIndex,
                         const std::vector<NativeEmitterFunctionLayout> &layouts,
                         const std::vector<size_t> &emitOrder,
                         EmitterT &emitter,
                         std::vector<NativeEmitterBranchFixup> &branchFixups,
                         std::vector<NativeEmitterCallFixup> &callFixups,
                         std::vector<NativeEmitterStringFixup> &stringFixups,
                         std::vector<size_t> &stringTableFixups,
                         uint64_t stringTableOffsetDelta,
                         uint64_t stringOffsetTableSize,
                         std::vector<size_t> &functionOffsets,
                         std::vector<std::vector<size_t>> &instOffsets,
                         NativeEmitterHostImports &hostImports,
                         NativeEmitterInstrumentation *instrumentation,
                         std::string &error);

} // namespace primec::native_emitter
