#include <cstdio>
#include "SemanticsHelpers.h"
#include "TemplateMonomorphContext.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CompileArena.h"
#include "RequirementPredicateFacts.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/ast/Ast.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

ResolvedType resolveTypeString(std::string input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error);

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack);

bool inferImplicitTemplateArgs(const Definition &def,
                               const Expr &callExpr,
                               const LocalTypeMap &locals,
                               const std::vector<ParameterInfo> &params,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               bool allowMathBare,
                               std::vector<std::string> &outArgs,
                               std::string &error);

bool resolvesExperimentalKeyValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx);

#include "TemplateMonomorphSourceDefinitionSetup.h"

std::string resolveCalleePath(const Expr &expr,
                              const std::string &namespacePrefix,
                              const Context &ctx,
                              const LocalTypeMap *locals = nullptr,
                              const std::vector<ParameterInfo> *params = nullptr);

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut);

bool inferBindingTypeForMonomorph(const Expr &initializer,
                                  const std::vector<ParameterInfo> &params,
                                  const LocalTypeMap &locals,
                                  bool allowMathBare,
                                  Context &ctx,
                                  BindingInfo &infoOut);

bool resolveFieldBindingTarget(const Expr &target,
                               const std::vector<ParameterInfo> &params,
                               const LocalTypeMap &locals,
                               bool allowMathBare,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               BindingInfo &bindingOut);

bool extractExperimentalKeyValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                 const Context &ctx,
                                                                 std::vector<std::string> &templateArgsOut);

bool extractCollectionVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                    const Context &ctx,
                                                                    std::vector<std::string> &templateArgsOut);

bool extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                       const Context &ctx,
                                                                       std::vector<std::string> &templateArgsOut);

#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         const std::vector<TemplateArgument> *resolvedArgDetails,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut);

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut);

bool rewriteExpr(Expr &expr,
                 const SubstMap &mapping,
                 const std::unordered_set<std::string> &allowedParams,
                 const std::string &namespacePrefix,
                 Context &ctx,
                 std::string &error,
                 const LocalTypeMap &locals,
                 const std::vector<ParameterInfo> &params,
                 bool allowMathBare);

bool usesStdlibScopedImportAliases(const std::string &namespacePrefix,
                                   const Context &ctx) {
  auto isStdlibOwnedPath = [](const std::string &path) {
    if (path.rfind("/std/", 0) == 0) {
      return true;
    }
    if (path.size() <= 1 || path.front() != '/') {
      return false;
    }
    const size_t nextSlash = path.find('/', 1);
    const std::string rootName =
        nextSlash == std::string::npos ? path.substr(1) : path.substr(1, nextSlash - 1);
    return isRootBuiltinName(rootName) || rootName == "string" || rootName == "Result" ||
           rootName == "Maybe" || rootName == "Buffer" || rootName == "ImageError" ||
           rootName == "ContainerError" || rootName == "GfxError";
  };
  return isStdlibOwnedPath(namespacePrefix) ||
         (namespacePrefix.empty() && isStdlibOwnedPath(ctx.currentDefinitionPath));
}

const std::unordered_map<std::string, std::string> &scopedImportAliasesForNamespace(
    const std::string &namespacePrefix,
    const Context &ctx) {
  if (usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    return ctx.stdlibScopedImportAliases;
  }
  return ctx.importAliases;
}

const std::unordered_map<std::string, std::vector<std::string>> &scopedImportAliasTargetsForNamespace(
    const std::string &namespacePrefix,
    const Context &ctx) {
  if (usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    return ctx.stdlibScopedImportAliasTargets;
  }
  return ctx.importAliasTargets;
}

const std::string *lookupScopedImportAliasForNamespace(std::string_view name,
                                                       const std::string &namespacePrefix,
                                                       const Context &ctx) {
  const std::string key(name);
  if (auto aliasIt = ctx.directImportAliases.find(key);
      aliasIt != ctx.directImportAliases.end()) {
    return &aliasIt->second;
  }
  if (auto aliasIt = ctx.transitiveImportAliases.find(key);
      aliasIt != ctx.transitiveImportAliases.end()) {
    return &aliasIt->second;
  }
  if (!usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
    if (auto aliasIt = ctx.importAliases.find(key);
        aliasIt != ctx.importAliases.end()) {
      return &aliasIt->second;
    }
  }
  return nullptr;
}

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphTypeResolution.h"
#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphExpressionRewrite.h"

bool rewriteExecution(Execution &exec, Context &ctx, std::string &error) {
  return rewriteExecutionEntry(exec, ctx, error);
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         const std::vector<TemplateArgument> *resolvedArgDetails,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  auto defIt = ctx.sourceDefs.find(basePath);
  if (defIt == ctx.sourceDefs.end()) {
    error = "unknown template definition: " + basePath;
    return false;
  }
  const Definition &baseDef = defIt->second;
  if (baseDef.templateArgs.empty()) {
    error = "template arguments are only supported on templated definitions: " +
            helperOverloadDisplayPath(basePath, ctx);
    return false;
  }
  TemplateArgumentBinding templateBinding;
  if (!bindTemplateArguments(baseDef,
                             resolvedArgs,
                             resolvedArgDetails,
                             helperOverloadDisplayPath(basePath, ctx),
                             templateBinding,
                             error)) {
    return false;
  }
  if (!definitionHasTypePackParameter(baseDef) &&
      baseDef.templateArgs.size() != resolvedArgs.size()) {
    std::ostringstream out;
    out << "template argument count mismatch for " << helperOverloadDisplayPath(basePath, ctx) << ": expected "
        << baseDef.templateArgs.size()
        << ", got " << resolvedArgs.size();
    error = out.str();
    return false;
  }
  const std::string key = basePath + "<" + joinMangledTemplateArgs(resolvedArgs, resolvedArgDetails) + ">";
  auto cacheIt = ctx.specializationCache.find(key);
  if (cacheIt != ctx.specializationCache.end()) {
    specializedPathOut = cacheIt->second;
    return true;
  }

  const size_t lastSlash = basePath.find_last_of('/');
  const std::string baseName = lastSlash == std::string::npos ? basePath : basePath.substr(lastSlash + 1);
  const std::string suffix = mangleTemplateArgs(resolvedArgs, resolvedArgDetails);
  const std::string specializedName = baseName + suffix;
  const std::string specializedBasePath = (lastSlash == std::string::npos)
                                              ? specializedName
                                              : basePath.substr(0, lastSlash + 1) + specializedName;
  if (ctx.sourceDefs.count(specializedBasePath) > 0) {
    error = "template specialization conflicts with existing definition: " + specializedBasePath;
    return false;
  }
  ctx.specializationCache.emplace(key, specializedBasePath);
  if (!specializeTemplateDefinitionFamily(
          basePath, templateBinding, specializedBasePath, specializedName, key, ctx, error)) {
    return false;
  }

  specializedPathOut = specializedBasePath;
  return true;
}

bool instantiateTemplate(const std::string &basePath,
                         const std::vector<std::string> &resolvedArgs,
                         Context &ctx,
                         std::string &error,
                         std::string &specializedPathOut) {
  return instantiateTemplate(basePath, resolvedArgs, nullptr, ctx, error, specializedPathOut);
}

#include "TemplateMonomorphFinalOrchestration.h"

} // namespace

bool monomorphizeTemplates(Program &program,
                           const std::string &entryPath,
                           std::string &error,
                           uint64_t *explicitTemplateArgFactHitCountOut,
                           uint64_t *implicitTemplateArgFactHitCountOut) {
  Context ctx = makeTemplateMonomorphContext(program);

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  program.definitions = std::move(ctx.outputDefs);
  program.executions = std::move(ctx.outputExecs);
  if (explicitTemplateArgFactHitCountOut != nullptr) {
    *explicitTemplateArgFactHitCountOut = ctx.explicitTemplateArgInferenceFactHitsForTesting;
  }
  if (implicitTemplateArgFactHitCountOut != nullptr) {
    *implicitTemplateArgFactHitCountOut = ctx.implicitTemplateArgInferenceFactHitsForTesting;
  }
  return true;
}

bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error) {
  uint64_t ignoredExplicitTemplateArgFactHits = 0;
  uint64_t ignoredImplicitTemplateArgFactHits = 0;
  return monomorphizeTemplates(program,
                               entryPath,
                               error,
                               &ignoredExplicitTemplateArgFactHits,
                               &ignoredImplicitTemplateArgFactHits);
}

bool collectExplicitTemplateArgResolutionFactsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    std::vector<ExplicitTemplateArgResolutionFactForTesting> &out) {
  out.clear();
  Context ctx = makeTemplateMonomorphContext(program);
  ctx.collectExplicitTemplateArgFactsForTesting = true;

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  out = std::move(ctx.explicitTemplateArgFactsForTesting);
  return true;
}

bool collectImplicitTemplateArgResolutionFactsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    std::vector<ImplicitTemplateArgResolutionFactForTesting> &out) {
  out.clear();
  Context ctx = makeTemplateMonomorphContext(program);
  ctx.collectImplicitTemplateArgFactsForTesting = true;

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  out = std::move(ctx.implicitTemplateArgFactsForTesting);
  return true;
}

bool collectExplicitTemplateArgFactConsumptionMetricsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    ExplicitTemplateArgFactConsumptionMetricsForTesting &out) {
  out = {};
  Context ctx = makeTemplateMonomorphContext(program);

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  out.hitCount = ctx.explicitTemplateArgInferenceFactHitsForTesting;
  return true;
}

bool collectImplicitTemplateArgFactConsumptionMetricsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    ImplicitTemplateArgFactConsumptionMetricsForTesting &out) {
  out = {};
  Context ctx = makeTemplateMonomorphContext(program);

  if (!applyImplicitAutoTemplates(program, ctx, error)) {
    return false;
  }

  std::unordered_set<std::string> templateRoots;
  if (!initializeTemplateMonomorphSourceDefinitions(ctx, entryPath, templateRoots, error)) {
    return false;
  }

  buildImportAliases(ctx);

  if (!rewriteMonomorphizedDefinitions(ctx, templateRoots, error)) {
    return false;
  }
  if (!rewriteMonomorphizedExecutions(ctx, error)) {
    return false;
  }

  out.hitCount = ctx.implicitTemplateArgInferenceFactHitsForTesting;
  return true;
}

void classifyTemplatedFallbackQueryTypeTextForTesting(
    const std::string &queryTypeText,
    TemplatedFallbackQueryStateEnvelopeSnapshotForTesting &out) {
  TemplatedFallbackQueryStateAdapterData adapter;
  adapter.queryTypeText = queryTypeText;
  populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(queryTypeText, adapter);
  out.hasResultType = adapter.hasResultType;
  out.resultTypeHasValue = adapter.resultTypeHasValue;
  out.resultValueType = std::move(adapter.resultValueType);
  out.resultErrorType = std::move(adapter.resultErrorType);
  out.mismatchDiagnostic = std::move(adapter.mismatchDiagnostic);
}

} // namespace primec::semantics
