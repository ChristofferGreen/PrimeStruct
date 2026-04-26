#pragma once

#include "primec/SemanticsBenchmark.h"

#include <cstdint>

namespace primec::semantics {

struct ProcessAllocationSample {
  bool valid = false;
  uint64_t allocationCount = 0;
  uint64_t allocatedBytes = 0;
};

struct ProcessRssSample {
  bool valid = false;
  uint64_t residentBytes = 0;
};

struct SemanticValidationBenchmarkRuntime {
  uint32_t definitionValidationWorkerCount = 1;
  SemanticPhaseCounters *phaseCounters = nullptr;
  bool allocationCountersEnabled = false;
  bool rssCheckpointsEnabled = false;
  bool disableMethodTargetMemoization = false;
  bool graphLocalAutoLegacyKeyShadow = false;
  bool graphLocalAutoLegacySideChannelShadow = false;
  bool disableGraphLocalAutoDependencyScratchPmr = false;

  bool hasPhaseCounters() const;
  bool usesAllocatorSampling() const;
};

SemanticValidationBenchmarkRuntime makeSemanticValidationBenchmarkRuntime(
    const SemanticValidationBenchmarkConfig *benchmarkConfig,
    const SemanticValidationBenchmarkObserver *benchmarkObserver);

class ScopedSemanticAllocatorReliefDisable {
public:
  explicit ScopedSemanticAllocatorReliefDisable(bool enabled);
  ~ScopedSemanticAllocatorReliefDisable();

private:
  bool previous_ = false;
};

class SemanticValidationBenchmarkPhase {
public:
  explicit SemanticValidationBenchmarkPhase(const SemanticValidationBenchmarkRuntime &runtime);

  void captureBefore();
  void captureAfter();
  void publish(uint64_t callsVisited, uint64_t peakLocalMapSize);

private:
  const SemanticValidationBenchmarkRuntime &runtime_;
  ProcessAllocationSample allocationBefore_{};
  ProcessAllocationSample allocationAfter_{};
  ProcessRssSample rssBefore_{};
  ProcessRssSample rssAfter_{};
  bool hasAllocationAfter_ = false;
  bool hasRssAfter_ = false;
};

class SemanticValidatorLifetimeBenchmark {
public:
  static SemanticValidatorLifetimeBenchmark fromEnvironment();

  void captureBefore();
  void captureAfterRun();
  void captureAfterDestroyAndReport();

private:
  bool enabled_ = false;
  ProcessAllocationSample allocationBefore_{};
  ProcessAllocationSample allocationAfterRun_{};
  ProcessAllocationSample allocationAfterDestroy_{};
  ProcessRssSample rssBefore_{};
  ProcessRssSample rssAfterRun_{};
  ProcessRssSample rssAfterDestroy_{};
};

void maybeRelieveSemanticAllocatorPressure();
ProcessAllocationSample captureProcessAllocationSample();
ProcessRssSample captureProcessRssSample();
void populateAllocationDelta(SemanticPhaseCounterSnapshot &snapshot,
                             const ProcessAllocationSample &before,
                             const ProcessAllocationSample &after);
void populateRssCheckpoints(SemanticPhaseCounterSnapshot &snapshot,
                            const ProcessRssSample &before,
                            const ProcessRssSample &after);

} // namespace primec::semantics
