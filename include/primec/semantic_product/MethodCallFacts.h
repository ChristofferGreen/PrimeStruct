#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "primec/StdlibSurfaceRegistry.h"
#include "primec/SymbolInterner.h"

namespace primec {

struct SemanticProgram;

struct SemanticProgramMethodCallTarget {
  std::string scopePath;
  std::string methodName;
  std::string receiverTypeText;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId methodNameId = InvalidSymbolId;
  SymbolId receiverTypeTextId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
  std::optional<StdlibSurfaceId> stdlibSurfaceId;
};

std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram);
std::optional<SymbolId> semanticProgramLookupPublishedMethodCallTargetId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
std::optional<StdlibSurfaceId> semanticProgramMethodCallTargetStdlibSurfaceId(
    const SemanticProgramMethodCallTarget &entry);
std::optional<StdlibSurfaceId> semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
std::string_view semanticProgramMethodCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramMethodCallTarget &entry);

} // namespace primec
