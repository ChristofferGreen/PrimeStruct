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

struct SemanticProgramDirectCallTarget {
  std::string scopePath;
  std::string callName;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  uint64_t provenanceHandle = 0;
  SymbolId scopePathId = InvalidSymbolId;
  SymbolId callNameId = InvalidSymbolId;
  SymbolId resolvedPathId = InvalidSymbolId;
  std::optional<StdlibSurfaceId> stdlibSurfaceId;
};

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram);
std::optional<SymbolId> semanticProgramLookupPublishedDirectCallTargetId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
std::optional<StdlibSurfaceId> semanticProgramDirectCallTargetStdlibSurfaceId(
    const SemanticProgramDirectCallTarget &entry);
std::optional<StdlibSurfaceId> semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
    const SemanticProgram &semanticProgram,
    uint64_t semanticNodeId);
std::string_view semanticProgramDirectCallTargetResolvedPath(
    const SemanticProgram &semanticProgram,
    const SemanticProgramDirectCallTarget &entry);

} // namespace primec
