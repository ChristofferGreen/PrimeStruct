#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/testing/SemanticsGraphHelpers.h"

namespace primec {

struct ResolvedType {
  std::string text;
  bool concrete = true;
};

using SubstMap = std::unordered_map<std::string, std::string>;

struct TemplateRootInfo {
  std::string fullPath;
  std::vector<std::string> params;
};

struct TemplateArgumentBinding {
  SubstMap mapping;
  std::unordered_set<std::string> integerParameters;
  std::vector<TemplatePackBinding> packBindings;
};

struct HelperOverloadEntry {
  std::string internalPath;
  std::string sourceKey;
  size_t parameterCount = 0;
  size_t variadicMinArgumentCount = 0;
  bool isVariadic = false;
  bool hasRequirementTransform = false;
};

struct GenericTypeOverloadEntry {
  std::string internalPath;
  size_t templateParameterCount = 0;
};

struct ExplicitTemplateArgInferenceFact {
  std::string resolvedTypeText;
  bool resolvedConcrete = false;
};

struct ImplicitTemplateArgInferenceFact {
  std::vector<std::string> inferredArgs;
};

struct Context {
  explicit Context(Program &inputProgram)
      : program(inputProgram) {}

  Program &program;
  std::unordered_map<std::string, Definition> sourceDefs;
  // Lazily-built, sorted view of sourceDefs' keys, used by
  // hasDefinitionFamilyPath()/hasTemplatedDefinitionFamilyPath() (see
  // TemplateMonomorphMethodTargets.h) to answer prefix-existence queries in
  // O(log N) instead of a linear scan over sourceDefs. Must be invalidated
  // (sourceDefsFamilyPathIndexValid = false) at every sourceDefs mutation
  // site - see initializeTemplateMonomorphSourceDefinitions() and
  // TemplateMonomorphTemplateSpecialization.h's clone-insertion loop.
  // mutable so read-only query helpers taking `const Context &` can still
  // build/cache it (several call sites, e.g. resolveMethodCallTemplateTarget,
  // only have a const Context&).
  mutable std::set<std::string> sourceDefsFamilyPathIndex;
  mutable bool sourceDefsFamilyPathIndexValid = false;
  std::unordered_set<std::string> templateDefs;
  std::unordered_map<std::string, std::string> directImportAliases;
  std::unordered_map<std::string, std::string> transitiveImportAliases;
  std::unordered_map<std::string, std::string> stdlibScopedImportAliases;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<std::string>> directImportAliasTargets;
  std::unordered_map<std::string, std::vector<std::string>> transitiveImportAliasTargets;
  std::unordered_map<std::string, std::vector<std::string>> stdlibScopedImportAliasTargets;
  std::unordered_map<std::string, std::vector<std::string>> importAliasTargets;
  std::unordered_map<std::string, std::vector<HelperOverloadEntry>> helperOverloads;
  std::unordered_map<std::string, std::string> helperOverloadInternalToPublic;
  std::unordered_map<std::string, std::string> helperOverloadDefinitionIdentity;
  std::unordered_map<std::string, std::vector<GenericTypeOverloadEntry>> genericTypeOverloads;
  std::unordered_map<std::string, std::string> genericTypeOverloadInternalToPublic;
  std::unordered_map<std::string, std::string> specializationCache;
  std::unordered_set<std::string> outputPaths;
  std::vector<Definition> outputDefs;
  std::vector<Execution> outputExecs;
  std::string currentDefinitionPath;
  std::unordered_set<std::string> implicitTemplateDefs;
  std::unordered_map<std::string, std::vector<std::string>> implicitTemplateParams;
  std::unordered_set<std::string> returnInferenceStack;
  bool collectExplicitTemplateArgFactsForTesting = false;
  std::vector<semantics::ExplicitTemplateArgResolutionFactForTesting> explicitTemplateArgFactsForTesting;
  std::unordered_map<std::string, ExplicitTemplateArgInferenceFact> explicitTemplateArgInferenceFacts;
  uint64_t explicitTemplateArgInferenceFactHitsForTesting = 0;
  bool collectImplicitTemplateArgFactsForTesting = false;
  std::vector<semantics::ImplicitTemplateArgResolutionFactForTesting> implicitTemplateArgFactsForTesting;
  std::unordered_map<std::string, ImplicitTemplateArgInferenceFact> implicitTemplateArgInferenceFacts;
  uint64_t implicitTemplateArgInferenceFactHitsForTesting = 0;
  const Definition *currentRewriteDefinition = nullptr;
  mutable std::string requirementOverloadSelectionError;
};

using LocalTypeMap = std::unordered_map<std::string, semantics::BindingInfo>;

const std::set<std::string> &sourceDefsFamilyPathIndex(const Context &ctx);
bool anySourceDefStartsWith(const Context &ctx, const std::string &prefix);
bool isArgsPackParameterExpr(const Expr &param);
bool definitionHasVariadicParameter(const Definition &def);
bool definitionHasTypePackParameter(const Definition &def);
bool isStructDefinition(const Definition &def);

} // namespace primec
