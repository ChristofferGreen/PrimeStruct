#include <cstdio>
#include "SemanticsHelpers.h"
#include "TemplateMonomorphContext.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
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
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFinalOrchestration.h"
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
