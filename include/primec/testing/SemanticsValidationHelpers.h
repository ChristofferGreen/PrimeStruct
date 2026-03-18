#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

struct TypeResolutionGraphSnapshotNode {
  uint32_t id = 0;
  std::string kind;
  std::string label;
  std::string scopePath;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct TypeResolutionGraphSnapshotEdge {
  uint32_t sourceId = 0;
  uint32_t targetId = 0;
  std::string kind;
};

struct TypeResolutionGraphSnapshot {
  std::vector<TypeResolutionGraphSnapshotNode> nodes;
  std::vector<TypeResolutionGraphSnapshotEdge> edges;
};

std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep(const std::string &text);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                              bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);
bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr);
bool buildTypeResolutionGraphForTesting(Program program,
                                        const std::string &entryPath,
                                        std::string &error,
                                        TypeResolutionGraphSnapshot &out,
                                        const std::vector<std::string> &semanticTransforms = {});
bool dumpTypeResolutionGraphForTesting(Program program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       std::string &out,
                                       const std::vector<std::string> &semanticTransforms = {});

} // namespace primec::semantics
