#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "CondensationDag.h"
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

enum class TypeResolutionGraphInvalidationEditFamily {
  LocalBinding,
  ControlFlow,
  InitializerShape,
  DefinitionSignature,
  ImportAlias,
  ReceiverType,
};

enum class TypeResolutionGraphInvalidationPropagation {
  DefinitionLocal,
  CrossDefinition,
};

struct TypeResolutionGraphInvalidationContract {
  TypeResolutionGraphInvalidationEditFamily editFamily =
      TypeResolutionGraphInvalidationEditFamily::LocalBinding;
  std::string_view name;
  TypeResolutionGraphInvalidationPropagation propagation =
      TypeResolutionGraphInvalidationPropagation::DefinitionLocal;
  std::string_view immediateInvalidations;
  std::string_view lazyRevisits;
  std::string_view diagnosticDiscards;
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
  uint64_t prepareMillis = 0;
  uint64_t buildMillis = 0;
  uint64_t prepareMaxMillis = 0;
  uint64_t buildMaxMillis = 0;
  bool prepareOverBudget = false;
  bool buildOverBudget = false;
  uint64_t invalidationLocalBindingCount = 0;
  uint64_t invalidationControlFlowCount = 0;
  uint64_t invalidationInitializerShapeCount = 0;
  uint64_t invalidationDefinitionSignatureCount = 0;
  uint64_t invalidationImportAliasCount = 0;
  uint64_t invalidationReceiverTypeCount = 0;
  uint64_t explicitTemplateArgInferenceFactHitCount = 0;
  uint64_t implicitTemplateArgInferenceFactHitCount = 0;
};

std::string_view typeResolutionNodeKindName(TypeResolutionNodeKind kind);
std::string_view typeResolutionEdgeKindName(TypeResolutionEdgeKind kind);
std::string_view typeResolutionGraphInvalidationEditFamilyName(
    TypeResolutionGraphInvalidationEditFamily editFamily);
std::string_view typeResolutionGraphInvalidationPropagationName(
    TypeResolutionGraphInvalidationPropagation propagation);
const std::vector<TypeResolutionGraphInvalidationContract> &
typeResolutionGraphInvalidationContracts();
const TypeResolutionGraphInvalidationContract *
typeResolutionGraphInvalidationContract(
    TypeResolutionGraphInvalidationEditFamily editFamily);
uint64_t typeResolutionGraphInvalidationCount(
    const TypeResolutionGraph &graph,
    TypeResolutionGraphInvalidationEditFamily editFamily);
TypeResolutionGraph buildTypeResolutionGraph(const Program &program);
bool buildTypeResolutionGraphForProgram(Program program,
                                        const std::string &entryPath,
                                        const std::vector<std::string> &semanticTransforms,
                                        std::string &error,
                                        TypeResolutionGraph &out);
CondensationDag computeTypeResolutionDependencyDag(const TypeResolutionGraph &graph);
std::string formatTypeResolutionGraph(const TypeResolutionGraph &graph);

} // namespace primec::semantics
