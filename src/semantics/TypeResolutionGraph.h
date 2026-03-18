#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

enum class TypeResolutionNodeKind {
  DefinitionReturn,
  CallConstraint,
  LocalAuto,
};

enum class TypeResolutionEdgeKind {
  Dependency,
  Requirement,
};

struct TypeResolutionGraphNode {
  uint32_t id = 0;
  TypeResolutionNodeKind kind = TypeResolutionNodeKind::DefinitionReturn;
  std::string label;
  std::string scopePath;
  std::string resolvedPath;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct TypeResolutionGraphEdge {
  uint32_t sourceId = 0;
  uint32_t targetId = 0;
  TypeResolutionEdgeKind kind = TypeResolutionEdgeKind::Dependency;
};

struct TypeResolutionGraph {
  std::vector<TypeResolutionGraphNode> nodes;
  std::vector<TypeResolutionGraphEdge> edges;
};

std::string_view typeResolutionNodeKindName(TypeResolutionNodeKind kind);
std::string_view typeResolutionEdgeKindName(TypeResolutionEdgeKind kind);
TypeResolutionGraph buildTypeResolutionGraph(const Program &program);

} // namespace primec::semantics
