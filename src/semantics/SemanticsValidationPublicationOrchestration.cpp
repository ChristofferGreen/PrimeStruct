#include "SemanticsValidationPublicationOrchestration.h"

#include "SemanticPublicationBuilders.h"
#include "SemanticsValidator.h"

#include <utility>

namespace primec::semantics {
namespace {

SemanticProgram buildSemanticProgramAfterValidation(
    const Program &program,
    const std::string &entryPath,
    SemanticsValidator &validator,
    const SemanticProductBuildConfig *buildConfig) {
  auto publicationSurface =
      validator.takeSemanticPublicationSurfaceForSemanticProduct(buildConfig);
  maybeRelieveSemanticAllocatorPressure();
  SemanticProgram semanticProgram = buildSemanticProgramFromPublicationSurface(
      program,
      entryPath,
      std::move(publicationSurface),
      buildConfig);
  maybeRelieveSemanticAllocatorPressure();
  return semanticProgram;
}

uint64_t semanticProgramFactCountForValidationPublication(
    const SemanticProgram &semanticProgram) {
  return static_cast<uint64_t>(
      semanticProgram.definitions.size() +
      semanticProgram.executions.size() +
      semanticProgram.directCallTargets.size() +
      semanticProgram.methodCallTargets.size() +
      semanticProgram.bridgePathChoices.size() +
      semanticProgram.callableSummaries.size() +
      semanticProgram.typeMetadata.size() +
      semanticProgram.structFieldMetadata.size() +
      semanticProgram.collectionSpecializations.size() +
      semanticProgram.bindingFacts.size() +
      semanticProgram.returnFacts.size() +
      semanticProgram.localAutoFacts.size() +
      semanticProgram.queryFacts.size() +
      semanticProgram.tryFacts.size() +
      semanticProgram.onErrorFacts.size());
}

} // namespace

void publishSemanticProgramAfterValidation(
    const Program &program,
    const std::string &entryPath,
    SemanticsValidator &validator,
    const SemanticProductBuildConfig *buildConfig,
    const SemanticValidationBenchmarkRuntime &benchmarkRuntime,
    SemanticProgram &semanticProgramOut) {
  ProcessAllocationSample semanticProductAllocationBefore;
  ProcessRssSample semanticProductRssBefore;
  if (benchmarkRuntime.phaseCounters != nullptr &&
      benchmarkRuntime.allocationCountersEnabled) {
    semanticProductAllocationBefore = captureProcessAllocationSample();
  }
  if (benchmarkRuntime.phaseCounters != nullptr &&
      benchmarkRuntime.rssCheckpointsEnabled) {
    semanticProductRssBefore = captureProcessRssSample();
  }

  semanticProgramOut = buildSemanticProgramAfterValidation(
      program,
      entryPath,
      validator,
      buildConfig);

  if (benchmarkRuntime.phaseCounters == nullptr) {
    return;
  }

  benchmarkRuntime.phaseCounters->semanticProductBuild.callsVisited = 1;
  benchmarkRuntime.phaseCounters->semanticProductBuild.factsProduced =
      semanticProgramFactCountForValidationPublication(semanticProgramOut);
  if (benchmarkRuntime.allocationCountersEnabled) {
    const auto semanticProductAllocationAfter = captureProcessAllocationSample();
    populateAllocationDelta(benchmarkRuntime.phaseCounters->semanticProductBuild,
                            semanticProductAllocationBefore,
                            semanticProductAllocationAfter);
  }
  if (benchmarkRuntime.rssCheckpointsEnabled) {
    const auto semanticProductRssAfter = captureProcessRssSample();
    populateRssCheckpoints(benchmarkRuntime.phaseCounters->semanticProductBuild,
                           semanticProductRssBefore,
                           semanticProductRssAfter);
  }
}

} // namespace primec::semantics
