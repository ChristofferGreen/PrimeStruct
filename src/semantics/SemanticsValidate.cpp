#include <cstdio>
#include "primec/support/CompileArena.h"
#include "primec/semantics/Semantics.h"
#include "primec/semantics/SemanticsBenchmark.h"
#include "primec/semantics/SemanticValidationPlan.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateBuiltinSoaRewrites.h"
#include "SemanticsValidateCompileTimeIf.h"
#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateExperimentalSoaFieldViewRewrites.h"
#include "SemanticsValidateExperimentalSoaMethodRewrites.h"
#include "SemanticsValidateKeyValueRewrites.h"
#include "SemanticsValidateOmittedStructInitializers.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateSoaBindingExtraction.h"
#include "SemanticsValidateTransforms.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "RequirementPredicateFacts.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidationBenchmarkOrchestration.h"
#include "SemanticsValidationPublicationOrchestration.h"
#include "SemanticsValidator.h"
#include "TypeResolutionGraphPreparation.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);

namespace {

std::string returnKindSnapshotName(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Unknown:
      return "unknown";
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Void:
      return "void";
    case ReturnKind::Array:
      return "array";
  }
  return "unknown";
}

std::string bindingTypeTextForSnapshot(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

uint64_t hashSemanticNodePath(const std::string &path) {
  constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
  constexpr uint64_t FnvPrime = 1099511628211ull;

  uint64_t hash = FnvOffsetBasis;
  for (unsigned char ch : path) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= FnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

std::string makeIndexedSemanticNodePath(const std::string &base, const char *segment, size_t index) {
  return base + "/" + segment + "[" + std::to_string(index) + "]";
}

void assignExprSemanticNodeIds(Expr &expr, const std::string &path) {
  expr.semanticNodeId = hashSemanticNodePath(path);
  for (size_t i = 0; i < expr.args.size(); ++i) {
    assignExprSemanticNodeIds(expr.args[i], makeIndexedSemanticNodePath(path, "arg", i));
  }
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(expr.bodyArguments[i], makeIndexedSemanticNodePath(path, "body", i));
  }
}

void assignDefinitionSemanticNodeIds(Definition &def) {
  const std::string basePath = "definition:" + def.fullPath;
  def.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    assignExprSemanticNodeIds(def.parameters[i], makeIndexedSemanticNodePath(basePath, "parameter", i));
  }
  for (size_t i = 0; i < def.statements.size(); ++i) {
    assignExprSemanticNodeIds(def.statements[i], makeIndexedSemanticNodePath(basePath, "statement", i));
  }
  if (def.returnExpr.has_value()) {
    assignExprSemanticNodeIds(*def.returnExpr, basePath + "/return");
  }
}

void assignExecutionSemanticNodeIds(Execution &exec) {
  const std::string basePath = "execution:" + exec.fullPath;
  exec.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.arguments[i], makeIndexedSemanticNodePath(basePath, "argument", i));
  }
  for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.bodyArguments[i], makeIndexedSemanticNodePath(basePath, "body", i));
  }
}

void assignSemanticNodeIds(Program &program) {
  for (auto &def : program.definitions) {
    assignDefinitionSemanticNodeIds(def);
  }
  for (auto &exec : program.executions) {
    assignExecutionSemanticNodeIds(exec);
  }
}

template <typename CaptureFn>
bool runTypeResolutionSnapshot(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &semanticTransforms,
    CaptureFn &&capture) {
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }
  capture(validator);
  return true;
}

} // namespace
}


namespace {

struct SemanticValidationManifestExecutionState {
  Program &program;
  const std::string &entryPath;
  std::string &error;
  const std::vector<std::string> &defaultEffects;
  const std::vector<std::string> &entryDefaultEffects;
  const std::vector<std::string> &semanticTransforms;
  SemanticDiagnosticInfo *diagnosticInfo = nullptr;
  bool collectDiagnostics = false;
  SemanticProgram *semanticProgramOut = nullptr;
  const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr;
  const semantics::SemanticValidationBenchmarkRuntime &benchmarkRuntime;
  semantics::SemanticValidationBenchmarkPhase &validationBenchmark;
  semantics::SemanticValidatorLifetimeBenchmark &validatorLifetimeBenchmark;
  std::unique_ptr<semantics::SemanticsValidator> validator;
  semantics::SemanticsValidator::ValidationCounters validationCounters;
  bool validatorPassCompleted = false;
  bool semanticProductPublicationCompleted = false;
  const std::unordered_set<std::string> *lazyStdlibModuleKeys = nullptr;
};

bool validateSemanticValidationManifestExecutableShape(std::string &error) {
  const auto &manifest = semantics::semanticValidationPassManifest();
  if (manifest.empty()) {
    error = "semantic validation manifest is empty";
    return false;
  }

  std::vector<semantics::SemanticValidationPassId> ids;
  std::vector<std::string_view> names;
  bool sawValidator = false;
  bool sawPublication = false;
  for (const auto &pass : manifest) {
    if (pass.name.empty()) {
      error = "semantic validation manifest contains an unnamed pass";
      return false;
    }
    if (std::find(names.begin(), names.end(), pass.name) != names.end()) {
      error = "semantic validation manifest has duplicate pass name: " +
              std::string(pass.name);
      return false;
    }
    names.push_back(pass.name);
    if (std::find(ids.begin(), ids.end(), pass.id) != ids.end()) {
      error = "semantic validation manifest has duplicate executable pass id: " +
              std::string(pass.name);
      return false;
    }
    ids.push_back(pass.id);

    if (sawPublication) {
      error = "semantic validation manifest has pass after publication: " +
              std::string(pass.name);
      return false;
    }
    if (pass.kind == semantics::SemanticValidationPassKind::Publication &&
        !sawValidator) {
      error = "semantic validation manifest reached publication before validator: " +
              std::string(pass.name);
      return false;
    }
    if (pass.id == semantics::SemanticValidationPassId::ValidatorPasses) {
      if (pass.kind != semantics::SemanticValidationPassKind::Validation) {
        error = "semantic validation manifest validator pass has wrong kind";
        return false;
      }
      sawValidator = true;
    } else if (pass.kind == semantics::SemanticValidationPassKind::Validation) {
      error = "semantic validation manifest has unexpected validation pass: " +
              std::string(pass.name);
      return false;
    }
    if (pass.id == semantics::SemanticValidationPassId::SemanticProductPublication) {
      if (pass.kind != semantics::SemanticValidationPassKind::Publication) {
        error = "semantic validation manifest publication pass has wrong kind";
        return false;
      }
      sawPublication = true;
    } else if (pass.kind == semantics::SemanticValidationPassKind::Publication) {
      error = "semantic validation manifest has unexpected publication pass: " +
              std::string(pass.name);
      return false;
    }
  }

  if (!sawValidator) {
    error = "semantic validation manifest is missing validator-passes";
    return false;
  }
  if (!sawPublication) {
    error = "semantic validation manifest is missing semantic-product-publication";
    return false;
  }
  return true;
}

bool runSemanticValidationManifestValidatorPass(
    SemanticValidationManifestExecutionState &state) {
  if (state.validator != nullptr || state.validatorPassCompleted) {
    state.error = "semantic validation manifest attempted to run validator twice";
    return false;
  }

  state.validatorLifetimeBenchmark.captureBefore();
  state.validationBenchmark.captureBefore();
  state.validator = std::make_unique<semantics::SemanticsValidator>(
      state.program,
      state.entryPath,
      state.error,
      state.defaultEffects,
      state.entryDefaultEffects,
      state.diagnosticInfo,
      state.collectDiagnostics,
      state.benchmarkRuntime.definitionValidationWorkerCount,
      state.benchmarkRuntime.hasPhaseCounters(),
      state.benchmarkRuntime.disableMethodTargetMemoization,
      state.benchmarkRuntime.graphLocalAutoLegacyKeyShadow,
      state.benchmarkRuntime.graphLocalAutoLegacySideChannelShadow,
      state.benchmarkRuntime.disableGraphLocalAutoDependencyScratchPmr,
      nullptr,
      state.lazyStdlibModuleKeys);
  try {
    if (!state.validator->run()) {
      return false;
    }
  } catch (const std::exception &ex) {
    state.error = std::string("semantic validator exception: ") + ex.what();
    return false;
  }

  state.validationCounters = state.validator->validationCounters();
  eraseCompileTimeTypeBindings(state.program);
  state.validatorPassCompleted = true;
  return true;
}

bool runSemanticValidationManifestPublicationPass(
    SemanticValidationManifestExecutionState &state) {
  if (!state.validatorPassCompleted || state.validator == nullptr) {
    state.error =
        "semantic validation manifest reached publication without validator state";
    return false;
  }
  if (state.semanticProductPublicationCompleted) {
    state.error =
        "semantic validation manifest attempted to publish semantic product twice";
    return false;
  }

  semantics::SemanticPublicationSurface publicationSurface;
  if (state.semanticProgramOut != nullptr) {
    publicationSurface =
        state.validator->takeSemanticPublicationSurfaceForSemanticProduct(
            state.semanticProductBuildConfig);
  }
  semantics::maybeRelieveSemanticAllocatorPressure();
  state.validationBenchmark.captureAfter();
  state.validatorLifetimeBenchmark.captureAfterRun();
  if (state.semanticProgramOut != nullptr) {
    semantics::publishSemanticProgramAfterValidation(
        state.program,
        state.entryPath,
        std::move(publicationSurface),
        state.semanticProductBuildConfig,
        state.benchmarkRuntime,
        *state.semanticProgramOut);
  }
  state.validator.reset();
  state.semanticProductPublicationCompleted = true;
  return true;
}

bool runSemanticValidationManifestPass(
    const semantics::SemanticValidationPassManifestEntry &pass,
    SemanticValidationManifestExecutionState &state) {
  using PassId = semantics::SemanticValidationPassId;
  switch (pass.id) {
    case PassId::SemanticTransformRules:
      return semantics::applySemanticTransforms(
          state.program, state.semanticTransforms, state.error);
    case PassId::ExperimentalGfxConstructors:
      return semantics::rewriteExperimentalGfxConstructors(state.program, state.error);
    case PassId::ReflectionGeneratedHelpers:
      return semantics::rewriteReflectionGeneratedHelpers(state.program, state.error);
    case PassId::BuiltinSoaConversionMethods:
      return rewriteBuiltinSoaConversionMethods(state.program, state.error);
    case PassId::BuiltinSoaToAosCalls:
      return rewriteBuiltinSoaToAosCalls(state.program, state.error);
    case PassId::BuiltinSoaHelperReturnMetadata:
      return validateBuiltinSoaHelperReturnMetadataRequirements(
          state.program, state.error);
    case PassId::BuiltinSoaAccessCalls:
      return rewriteBuiltinSoaAccessCalls(state.program, state.error);
    case PassId::BuiltinSoaCountCalls:
      return rewriteBuiltinSoaCountCalls(state.program, state.error);
    case PassId::BuiltinSoaMutatorCalls:
      return rewriteBuiltinSoaMutatorCalls(state.program, state.error);
    case PassId::ExperimentalSoaInlineBorrowMethods:
      return rewriteExperimentalSoaInlineBorrowMethods(state.program, state.error);
    case PassId::ExperimentalSoaSamePathHelperMethods:
      return rewriteExperimentalSoaSamePathHelperMethods(state.program, state.error);
    case PassId::ExperimentalSoaToAosMethods:
      return rewriteExperimentalSoaToAosMethods(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewIndexes:
      return rewriteExperimentalSoaFieldViewIndexes(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewHelpers:
      return rewriteExperimentalSoaFieldViewHelpers(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewCarrierIndexes:
      return rewriteExperimentalSoaFieldViewCarrierIndexes(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewAssignTargets:
      return rewriteExperimentalSoaFieldViewAssignTargets(state.program, state.error);
    case PassId::BorrowedExperimentalMapMethods:
      return rewriteBorrowedExperimentalKeyValueMethods(state.program, state.error);
    case PassId::ExperimentalMapValueMethods:
      return rewriteExperimentalKeyValueValueMethods(state.program, state.error);
    case PassId::BuiltinMapInsertMethods:
      return rewriteBuiltinKeyValueInsertMethods(state.program, state.error);
    case PassId::CompileTimeBranchPruning:
      return rewriteCompileTimeIfBranches(state.program, true, state.error);
    case PassId::TemplateMonomorphization:
      try {
        if (!semantics::monomorphizeTemplates(
                state.program, state.entryPath, state.error)) {
          return false;
        }
        return semantics::rewriteReflectionGeneratedHelpersForPackSpecializations(
            state.program, state.error);
      } catch (const std::exception &ex) {
        state.error = std::string("template monomorphization exception: ") +
                      ex.what();
        return false;
      }
    case PassId::CompileTimeSpecializedBranchPruning:
      return rewriteCompileTimeIfBranches(state.program, false, state.error);
    case PassId::ReflectionMetadataQueries:
      return semantics::rewriteReflectionMetadataQueries(state.program, state.error);
    case PassId::ConvertConstructors:
      return semantics::rewriteConvertConstructors(state.program, state.error);
    case PassId::ValidatorPasses:
      return runSemanticValidationManifestValidatorPass(state);
    case PassId::OmittedStructInitializers:
      if (!state.validatorPassCompleted) {
        state.error =
            "semantic validation manifest reached omitted initializer rewrite before validator";
        return false;
      }
      return rewriteOmittedStructInitializers(state.program, state.error);
    case PassId::SemanticNodeIdAssignment:
      if (!state.validatorPassCompleted || state.validator == nullptr) {
        state.error =
            "semantic validation manifest reached node-id assignment without validator state";
        return false;
      }
      semantics::assignSemanticNodeIds(state.program);
      state.validator->invalidatePilotRoutingSemanticCollectors();
      return true;
    case PassId::SemanticProductPublication:
      return runSemanticValidationManifestPublicationPass(state);
  }

  state.error = "semantic validation manifest has no runner for pass: " +
                std::string(pass.name);
  return false;
}

bool runSemanticValidationManifest(SemanticValidationManifestExecutionState &state) {
  if (!validateSemanticValidationManifestExecutableShape(state.error)) {
    return false;
  }

  using PassKind = semantics::SemanticValidationPassKind;

  // Internal stdlib modules (internal_*) are in canonical form and never need
  // compatibility rewrites. Deferring them from those passes avoids redundant
  // AST traversals on thousands of internal definitions per pass — the
  // 4,842-line internal_soa_storage.prime alone adds >30s on wildcard imports
  // like `import /std/collections/*` without this filter.
  auto isInternalStdlibDef = [](const Definition &def) {
    return def.namespacePrefix.find("/internal_") != std::string::npos;
  };

  std::vector<Definition> deferredInternalDefs;
  bool internalDefsInProgram = true;

  auto deferInternalDefs = [&]() {
    if (!internalDefsInProgram) {
      return;
    }
    auto &defs = state.program.definitions;
    auto pivot = std::stable_partition(defs.begin(), defs.end(),
                                       [&](const Definition &def) {
                                         return !isInternalStdlibDef(def);
                                       });
    if (pivot == defs.end()) {
      return;
    }
    deferredInternalDefs.insert(deferredInternalDefs.end(),
                                std::make_move_iterator(pivot),
                                std::make_move_iterator(defs.end()));
    defs.erase(pivot, defs.end());
    internalDefsInProgram = false;
  };

  auto restoreInternalDefs = [&]() {
    if (internalDefsInProgram || deferredInternalDefs.empty()) {
      return;
    }
    auto &defs = state.program.definitions;
    defs.insert(defs.end(),
                std::make_move_iterator(deferredInternalDefs.begin()),
                std::make_move_iterator(deferredInternalDefs.end()));
    deferredInternalDefs.clear();
    internalDefsInProgram = true;
  };

  for (const auto &pass : semantics::semanticValidationPassManifest()) {
    if (state.semanticProductPublicationCompleted) {
      state.error = "semantic validation manifest has pass after publication: " +
                    std::string(pass.name);
      return false;
    }
    if (pass.kind == PassKind::CompatibilityRewrite) {
      deferInternalDefs();
    } else {
      restoreInternalDefs();
    }
    if (!runSemanticValidationManifestPass(pass, state)) {
      return false;
    }
  }

  restoreInternalDefs();

  if (!state.validatorPassCompleted) {
    state.error = "semantic validation manifest is missing validator-passes";
    return false;
  }
  if (!state.semanticProductPublicationCompleted) {
    state.error =
        "semantic validation manifest is missing semantic-product-publication";
    return false;
  }
  return true;
}

bool runSemanticValidation(Program &program,
                           const std::string &entryPath,
                           std::string &error,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           const std::vector<std::string> &semanticTransforms,
                           SemanticDiagnosticInfo *diagnosticInfo,
                           bool collectDiagnostics,
                           SemanticProgram *semanticProgramOut,
                           const SemanticProductBuildConfig *semanticProductBuildConfig,
                           const SemanticValidationBenchmarkConfig *benchmarkConfig,
                           const SemanticValidationBenchmarkObserver *benchmarkObserver,
                           const std::unordered_set<std::string> *lazyStdlibModuleKeys) {
  const auto benchmarkRuntime =
      semantics::makeSemanticValidationBenchmarkRuntime(benchmarkConfig, benchmarkObserver);

  error.clear();
  if (benchmarkRuntime.phaseCounters != nullptr) {
    *benchmarkRuntime.phaseCounters = {};
  }
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool validationSucceeded = false;
  struct ValidationDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &validationSucceeded;

    ~ValidationDiagnosticScope() {
      if (!validationSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } validationDiagnosticScope{diagnosticSink, error, validationSucceeded};
  const semantics::ScopedSemanticAllocatorReliefDisable scopedBenchmarkAllocatorReliefDisable(
      benchmarkRuntime.usesAllocatorSampling());
  auto validatorLifetimeBenchmark = semantics::SemanticValidatorLifetimeBenchmark::fromEnvironment();
  semantics::SemanticValidationBenchmarkPhase validationBenchmark(benchmarkRuntime);
  SemanticValidationManifestExecutionState manifestState{
      program,
      entryPath,
      error,
      defaultEffects,
      entryDefaultEffects,
      semanticTransforms,
      diagnosticInfo,
      collectDiagnostics,
      semanticProgramOut,
      semanticProductBuildConfig,
      benchmarkRuntime,
      validationBenchmark,
      validatorLifetimeBenchmark,
      nullptr,
      {},
      false,
      false,
      lazyStdlibModuleKeys,
  };
  if (!runSemanticValidationManifest(manifestState)) {
    return false;
  }

  validatorLifetimeBenchmark.captureAfterDestroyAndReport();
  validationBenchmark.publish(manifestState.validationCounters.callsVisited,
                              manifestState.validationCounters.peakLocalMapSize);
  error.clear();
  validationSucceeded = true;
  return true;
}

} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &entryDefaultEffects,
                         const std::vector<std::string> &semanticTransforms,
                         SemanticDiagnosticInfo *diagnosticInfo,
                         bool collectDiagnostics,
                         SemanticProgram *semanticProgramOut,
                         const SemanticProductBuildConfig *semanticProductBuildConfig,
                         const std::unordered_set<std::string> *lazyStdlibModuleKeys) const {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               nullptr,
                               nullptr,
                               lazyStdlibModuleKeys);
}

bool validateSemanticsForBenchmark(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::vector<std::string> &semanticTransforms,
    SemanticDiagnosticInfo *diagnosticInfo,
    bool collectDiagnostics,
    SemanticProgram *semanticProgramOut,
    const SemanticProductBuildConfig *semanticProductBuildConfig,
    const SemanticValidationBenchmarkConfig &benchmarkConfig,
    const SemanticValidationBenchmarkObserver &benchmarkObserver,
    const std::unordered_set<std::string> *lazyStdlibModuleKeys) {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               &benchmarkConfig,
                               &benchmarkObserver,
                               lazyStdlibModuleKeys);
}

bool semantics::computeTypeResolutionReturnSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionReturnSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.returnResolutionSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionReturnSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.kind),
          entry.structPath,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

std::string semantics::runSemanticsReturnKindNameStep(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &error) {
  return returnKindSnapshotName(getReturnKind(def, structNames, importAliases, error));
}

bool semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionLocalBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.localAutoBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionLocalBindingSnapshotEntry{
          entry.scopePath,
          entry.bindingName,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
          entry.initializerResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerBinding),
          bindingTypeTextForSnapshot(entry.initializerReceiverBinding),
          entry.initializerQueryTypeText,
          entry.initializerResultHasValue,
          entry.initializerResultValueType,
          entry.initializerResultErrorType,
          entry.initializerHasTry,
          entry.initializerTryOperandResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerTryOperandBinding),
          bindingTypeTextForSnapshot(entry.initializerTryOperandReceiverBinding),
          entry.initializerTryOperandQueryTypeText,
          entry.initializerTryValueType,
          entry.initializerTryErrorType,
          entry.initializerHasTry ? returnKindSnapshotName(entry.initializerTryContextReturnKind) : std::string{},
          entry.initializerTryOnErrorHandlerPath,
          entry.initializerTryOnErrorErrorType,
          entry.initializerTryOnErrorBoundArgCount,
          entry.initializerDirectCallResolvedPath,
          entry.initializerDirectCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerDirectCallReturnKind)
              : std::string{},
          entry.initializerMethodCallResolvedPath,
          entry.initializerMethodCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerMethodCallReturnKind)
              : std::string{},
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryCallSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryCallSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.typeText.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryCallSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.typeText,
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.binding.typeName.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryResultTypeSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (!entry.hasResultType) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryResultTypeSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionTryValueSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionTryValueSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.tryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionTryValueSnapshotEntry{
          entry.scopePath,
          entry.operandResolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.operandBinding),
          bindingTypeTextForSnapshot(entry.operandReceiverBinding),
          entry.operandQueryTypeText,
          entry.valueType,
          entry.errorType,
          returnKindSnapshotName(entry.contextReturnKind),
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

bool semantics::computeTypeResolutionCallBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionCallBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.callBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionCallBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryReceiverBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.receiverBinding.typeName.empty()) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryReceiverBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.receiverBinding),
      });
    }
  });
}

bool semantics::computeTypeResolutionOnErrorSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionOnErrorSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.onErrorFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionOnErrorSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.returnKind),
          entry.handlerPath,
          entry.errorType,
          entry.boundArgCount,
          entry.returnResultHasValue,
          entry.returnResultValueType,
          entry.returnResultErrorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionValidationContextSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionValidationContextSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.takeCollectedCallableSummariesForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.isExecution) {
        continue;
      }
      out.entries.push_back(TypeResolutionValidationContextSnapshotEntry{
          entry.fullPath,
          returnKindSnapshotName(entry.returnKind),
          entry.isCompute,
          entry.isUnsafe,
          entry.activeEffects,
          entry.hasResultType,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
          entry.hasOnError,
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

} // namespace primec
