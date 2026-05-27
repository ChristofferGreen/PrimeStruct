#include "primec/IrPreparation.h"

#include "primec/AstMemory.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"

#include <cstdlib>
#include <iostream>

namespace primec {
namespace {

void emitPostIrPreparationAstHeapEstimate(const Program &program) {
  if (std::getenv("PRIMEC_BENCHMARK_COMPILE_AST_HEAP_ESTIMATE") == nullptr) {
    return;
  }

  const ProgramHeapEstimateStats stats = estimateProgramHeap(program);
  std::cerr << "[benchmark-ast-heap-estimate] "
            << "{\"stage\":\"post-ir-preparation-release\""
            << ",\"definitions\":" << stats.definitions
            << ",\"executions\":" << stats.executions
            << ",\"exprs\":" << stats.exprs
            << ",\"transforms\":" << stats.transforms
            << ",\"strings\":" << stats.strings
            << ",\"dynamic_bytes\":" << stats.dynamicBytes
            << "}\n";
}

bool validateRuntimeReflectionBackendSupport(const SemanticProgram &semanticProgram,
                                             const Options &options,
                                             std::string &error) {
  std::string_view reflectionPath;
  if (!semanticProgramResolvePublishedLowererRuntimeReflectionPath(
          semanticProgram, reflectionPath, error)) {
    return false;
  }
  if (reflectionPath.empty()) {
    return true;
  }

  const IrBackendCapabilitySupport support =
      queryIrBackendCapabilitySupport(options, IrBackendCapability::RuntimeReflection);
  if (support.supported) {
    return true;
  }

  const std::string targetName =
      support.targetName.empty() ? "selected" : std::string(support.targetName);
  error = "runtime reflection support unavailable for " + targetName + " target: " +
          std::string(reflectionPath);
  return false;
}

} // namespace

const std::vector<IrPreparationPhaseManifestEntry> &irPreparationPhaseManifest() {
  static const std::vector<IrPreparationPhaseManifestEntry> Manifest = {
      {"semantic-product-preflight",
       IrPreparationPhaseOwnership::CompilePipelineAstAndSemanticProduct,
       IrPreparationPhaseOwnership::CompilePipelineAstAndSemanticProduct,
       IrPreparationPhaseAction::ValidatesOnly,
       false,
       "program AST, semantic product pointer, options, backend capability profile, and validation target",
       "a missing semantic product or unsupported runtime-reflection capability invalidates IR preparation before lowering; no IR module is owned",
       "prepareIrModule callers and lowerer preflight"},
      {"lower-semantic-product-to-ir",
       IrPreparationPhaseOwnership::CompilePipelineAstAndSemanticProduct,
       IrPreparationPhaseOwnership::IrPreparationLoweredIr,
       IrPreparationPhaseAction::CreatesOutput,
       false,
       "semantic canonical AST bodies, semantic-product facts, entry effects, and validation target",
       "missing or stale semantic-product facts invalidate lowering and report a Lowering failure",
       "IR validation, optional inlining, and backend emitters"},
      {"validate-lowered-ir",
       IrPreparationPhaseOwnership::IrPreparationLoweredIr,
       IrPreparationPhaseOwnership::IrPreparationValidatedIr,
       IrPreparationPhaseAction::ValidatesOnly,
       false,
       "lowered IR module and selected IR validation target",
       "validation failure invalidates backend consumers and the optional inline phase",
       "backend emitters and inline-ir-calls when enabled"},
      {"inline-ir-calls",
       IrPreparationPhaseOwnership::IrPreparationValidatedIr,
       IrPreparationPhaseOwnership::IrPreparationInlinedIr,
       IrPreparationPhaseAction::MutatesOutput,
       true,
       "validated IR module and Options::inlineIrCalls",
       "inlining mutates the IR module and invalidates the prior validation result",
       "validate-inlined-ir"},
      {"validate-inlined-ir",
       IrPreparationPhaseOwnership::IrPreparationInlinedIr,
       IrPreparationPhaseOwnership::IrPreparationValidatedIr,
       IrPreparationPhaseAction::ValidatesOnly,
       true,
       "inlined IR module and selected IR validation target",
       "failure keeps inlined IR from reaching backend consumers",
       "backend emitters after inline-ir-calls"},
      {"release-lowered-ast-bodies",
       IrPreparationPhaseOwnership::CompilerAstStorage,
       IrPreparationPhaseOwnership::IrPreparationValidatedIr,
       IrPreparationPhaseAction::ReleasesInputStorage,
       false,
       "validated IR module and lowered AST body storage",
       "releases lowered AST bodies after IR preparation; downstream consumers must use IR, semantic product facts, and source maps",
       "heap benchmarking, CLI/backend output, and VM/native/IR emit paths"},
  };
  return Manifest;
}

bool prepareIrModule(Program &program,
                     const SemanticProgram *semanticProgram,
                     const Options &options,
                     IrValidationTarget validationTarget,
                     IrModule &ir,
                     IrPreparationFailure &failure,
                     const ExpandedSource *expandedSource) {
  failure = {};
  std::string error;
  DiagnosticSink diagnosticSink(&failure.diagnosticInfo);
  diagnosticSink.reset();

  if (semanticProgram == nullptr) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = "semantic product is required for IR preparation";
    diagnosticSink.setSummary(failure.message);
    return false;
  }
  if (!validateRuntimeReflectionBackendSupport(*semanticProgram, options, error)) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = std::move(error);
    diagnosticSink.setSummary(failure.message);
    return false;
  }

  IrLowerer lowerer;
  if (!lowerer.lower(program,
                     semanticProgram,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     ir,
                     error,
                     &failure.diagnosticInfo,
                     validationTarget,
                     expandedSource)) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = std::move(error);
    return false;
  }

  if (!validateIrModule(ir, validationTarget, error)) {
    failure.stage = IrPreparationFailureStage::Validation;
    failure.message = std::move(error);
    diagnosticSink.setSummary(failure.message);
    return false;
  }

  if (options.inlineIrCalls) {
    if (!inlineIrModuleCalls(ir, error)) {
      failure.stage = IrPreparationFailureStage::Inlining;
      failure.message = std::move(error);
      diagnosticSink.setSummary(failure.message);
      return false;
    }
    if (!validateIrModule(ir, validationTarget, error)) {
      failure.stage = IrPreparationFailureStage::Validation;
      failure.message = std::move(error);
      diagnosticSink.setSummary(failure.message);
      return false;
    }
  }

  releaseLoweredAstBodies(program);
  emitPostIrPreparationAstHeapEstimate(program);

  return true;
}

} // namespace primec
