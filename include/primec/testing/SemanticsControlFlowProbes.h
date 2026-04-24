#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

struct ExprCaptureSplitProbeSnapshotForTesting {
  std::vector<std::string> tokens;
};

struct LoopCountProbeSnapshotForTesting {
  std::optional<uint64_t> knownIterationCount;
  bool canIterateMoreThanOnce = false;
  bool isNegativeIntegerLiteral = false;
};

ExprCaptureSplitProbeSnapshotForTesting probeExprCaptureSplitForTesting(const std::string &text);
LoopCountProbeSnapshotForTesting probeLoopCountForTesting(const Expr &countExpr, bool allowBoolCount);

} // namespace primec::semantics

