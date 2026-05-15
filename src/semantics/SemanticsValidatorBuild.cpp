#include "SemanticsValidator.h"

#include <array>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace primec::semantics {

bool SemanticsValidator::buildDefinitionMaps() {
  auto failBuildDefinitionMapDiagnostic = [&](std::string message,
                                              const Definition *def = nullptr) -> bool {
    if (def != nullptr) {
      return failDefinitionDiagnostic(*def, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  defaultEffectSet_.clear();
  entryDefaultEffectSet_.clear();
  defMap_.clear();
  returnKinds_.clear();
  returnStructs_.clear();
  returnBindings_.clear();
  graphLocalAutoFacts_.clear();
  graphLocalAutoScopePathInterner_.clear();
  if (graphLocalAutoBenchmarkShadow_ != nullptr) {
    graphLocalAutoBenchmarkShadow_->clear();
  }
  pilotRoutingSemanticCollectorsValid_ = false;
  mergedWorkerPublicationFactsValid_ = false;
  mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = false;
  mergedWorkerPublicationSeedStringsValid_ = false;
  collectedDirectCallTargets_.clear();
  collectedMethodCallTargets_.clear();
  collectedBridgePathChoices_.clear();
  collectedCallableSummaries_.clear();
  mergedWorkerPublicationFacts_ = {};
  mergedWorkerPublicationSeedStrings_.clear();
  onErrorSnapshotFactCacheValid_ = false;
  onErrorSnapshotCache_.clear();
  structNames_.clear();
  publicDefinitions_.clear();
  paramsByDef_.clear();
  localBindingMemoRevisionByIdentity_.clear();
  inferExprReturnKindMemo_.clear();
  inferStructReturnMemo_.clear();
  structFieldReturnKindMemo_.clear();
  currentValidationState_ = {};
  sumNames_.clear();

  for (const auto &effect : defaultEffects_) {
    if (!isEffectName(effect)) {
      return failBuildDefinitionMapDiagnostic("invalid default effect: " +
                                              effect);
    }
    defaultEffectSet_.insert(effect);
  }
  for (const auto &effect : entryDefaultEffects_) {
    if (!isEffectName(effect)) {
      return failBuildDefinitionMapDiagnostic(
          "invalid entry default effect: " + effect);
    }
    entryDefaultEffectSet_.insert(effect);
  }
  std::unordered_set<std::string> explicitStructs;
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isStructDefinition = [&](const Definition &def, bool &isExplicitOut) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        isExplicitOut = false;
        return false;
      }
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      isExplicitOut = true;
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      isExplicitOut = false;
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        isExplicitOut = false;
        return false;
      }
    }
    isExplicitOut = false;
    return true;
  };
  for (const auto &declaration : validationPlan_->definitionPrepass.declarationsInStableOrder) {
    const Definition &def = program_.definitions[declaration.stableIndex];
    DefinitionContextScope definitionScope(*this, def);
    if (isSumDefinition(def)) {
      sumNames_.insert(def.fullPath);
    }
    bool isExplicit = false;
    if (isStructDefinition(def, isExplicit)) {
      structNames_.insert(def.fullPath);
      if (isExplicit) {
        explicitStructs.insert(def.fullPath);
      }
    }
  }

  auto isLifecycleHelperName = [](const std::string &fullPath) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
        {"CreateStack", "stack"},
        {"DestroyStack", "stack"},
        {"CreateHeap", "heap"},
        {"DestroyHeap", "heap"},
        {"CreateBuffer", "buffer"},
        {"DestroyBuffer", "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      return true;
    }
    return false;
  };
  auto isStructHelperDefinition = [&](const Definition &def) -> bool {
    if (!def.isNested) {
      return false;
    }
    if (structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    return structNames_.count(parent) > 0;
  };
  auto validateTemplateParameters = [&](const Definition &def) -> bool {
    if (!def.templateArgIsPack.empty() &&
        def.templateArgIsPack.size() != def.templateArgs.size()) {
      return failDefinitionDiagnostic(def,
                                      "template parameter metadata mismatch on " +
                                          def.fullPath);
    }
    std::unordered_set<std::string> seen;
    bool sawPack = false;
    for (std::size_t index = 0; index < def.templateArgs.size(); ++index) {
      const std::string &name = def.templateArgs[index];
      if (!seen.insert(name).second) {
        return failDefinitionDiagnostic(def,
                                        "duplicate template parameter: " + name);
      }
      const bool isPack = index < def.templateArgIsPack.size() &&
                          def.templateArgIsPack[index];
      if (isPack) {
        if (sawPack) {
          return failDefinitionDiagnostic(def,
                                          "only one type pack parameter is supported");
        }
        sawPack = true;
      } else if (sawPack) {
        return failDefinitionDiagnostic(def, "type pack parameter must be last");
      }
    }
    std::unordered_set<std::string> seenPackBindings;
    for (const TemplatePackBinding &binding : def.templatePackBindings) {
      if (binding.parameterName.empty()) {
        return failDefinitionDiagnostic(def,
                                        "template pack binding is missing a parameter name");
      }
      if (!seenPackBindings.insert(binding.parameterName).second) {
        return failDefinitionDiagnostic(def,
                                        "duplicate template pack binding: " +
                                            binding.parameterName);
      }
    }
    return true;
  };
  std::vector<SemanticDiagnosticRecord> transformDiagnosticRecords;
  for (const auto &declaration : validationPlan_->definitionPrepass.declarationsInStableOrder) {
    const Definition &def = program_.definitions[declaration.stableIndex];
    DefinitionContextScope definitionScope(*this, def);
    if (!validateTemplateParameters(def)) {
      return false;
    }
    if (defMap_.count(def.fullPath) > 0) {
      return failBuildDefinitionMapDiagnostic("duplicate definition: " +
                                              def.fullPath,
                                              &def);
    }
    if (def.fullPath.find('/', 1) == std::string::npos) {
      const std::string rootName = def.fullPath.substr(1);
      if ((mathImportAll_ || mathImports_.count(rootName) > 0) && isMathBuiltinName(rootName)) {
        return failBuildDefinitionMapDiagnostic(
            "import creates name conflict: " + rootName,
            &def);
      }
    }
    const bool isStructHelper = isStructHelperDefinition(def);
    const bool isLifecycleHelper = isLifecycleHelperName(def.fullPath);
    bool definitionTransformError = false;
    if (!validateDefinitionBuildTransforms(def,
                                           isStructHelper,
                                           isLifecycleHelper,
                                           explicitStructs,
                                           &transformDiagnosticRecords,
                                           definitionTransformError)) {
      return false;
    }
    if (definitionTransformError) {
      continue;
    }
    defMap_[def.fullPath] = &def;
  }
  if (!finalizeCollectedStructuredDiagnostics(transformDiagnosticRecords)) {
    return false;
  }

  if (!buildImportAliases()) {
    return false;
  }

  if (!buildDefinitionReturnKinds(explicitStructs)) {
    return false;
  }

  if (!validateLifecycleHelperDefinitions()) {
    return false;
  }

  if (!buildParameters()) {
    return false;
  }
  rebuildCallResolutionFamilyIndexes();

  for (const auto &declaration : validationPlan_->definitionPrepass.declarationsInStableOrder) {
    const Definition &def = program_.definitions[declaration.stableIndex];
    DefinitionContextScope definitionContextScope(*this, def);
    ValidationContext context;
    if (!makeDefinitionValidationContext(def, context)) {
      return false;
    }
  }

  return true;
}

}  // namespace primec::semantics
