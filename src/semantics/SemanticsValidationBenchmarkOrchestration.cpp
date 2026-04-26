#include "SemanticsValidationBenchmarkOrchestration.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#include <unistd.h>
#endif

namespace primec::semantics {
namespace {

thread_local bool gDisableSemanticAllocatorReliefForBenchmark = false;

void relieveSemanticAllocatorPressure() {
#if defined(__APPLE__)
  (void)malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__linux__) && defined(__GLIBC__)
  (void)malloc_trim(0);
#endif
}

uint64_t saturatingSubtract(uint64_t after, uint64_t before) {
  return after > before ? (after - before) : 0;
}

} // namespace

bool SemanticValidationBenchmarkRuntime::hasPhaseCounters() const {
  return phaseCounters != nullptr;
}

bool SemanticValidationBenchmarkRuntime::usesAllocatorSampling() const {
  return allocationCountersEnabled || rssCheckpointsEnabled;
}

SemanticValidationBenchmarkRuntime makeSemanticValidationBenchmarkRuntime(
    const SemanticValidationBenchmarkConfig *benchmarkConfig,
    const SemanticValidationBenchmarkObserver *benchmarkObserver) {
  SemanticValidationBenchmarkRuntime runtime;
  if (benchmarkConfig != nullptr) {
    runtime.definitionValidationWorkerCount = benchmarkConfig->definitionValidationWorkerCount;
    runtime.disableMethodTargetMemoization = benchmarkConfig->disableMethodTargetMemoization;
    runtime.graphLocalAutoLegacyKeyShadow = benchmarkConfig->graphLocalAutoLegacyKeyShadow;
    runtime.graphLocalAutoLegacySideChannelShadow = benchmarkConfig->graphLocalAutoLegacySideChannelShadow;
    runtime.disableGraphLocalAutoDependencyScratchPmr =
        benchmarkConfig->disableGraphLocalAutoDependencyScratchPmr;
  }
  if (benchmarkObserver != nullptr) {
    runtime.phaseCounters = benchmarkObserver->phaseCounters;
    runtime.allocationCountersEnabled = benchmarkObserver->allocationCountersEnabled;
    runtime.rssCheckpointsEnabled = benchmarkObserver->rssCheckpointsEnabled;
  }
  return runtime;
}

ScopedSemanticAllocatorReliefDisable::ScopedSemanticAllocatorReliefDisable(bool enabled)
    : previous_(gDisableSemanticAllocatorReliefForBenchmark) {
  if (enabled) {
    gDisableSemanticAllocatorReliefForBenchmark = true;
  }
}

ScopedSemanticAllocatorReliefDisable::~ScopedSemanticAllocatorReliefDisable() {
  gDisableSemanticAllocatorReliefForBenchmark = previous_;
}

SemanticValidationBenchmarkPhase::SemanticValidationBenchmarkPhase(
    const SemanticValidationBenchmarkRuntime &runtime)
    : runtime_(runtime) {}

void SemanticValidationBenchmarkPhase::captureBefore() {
  if (runtime_.hasPhaseCounters() && runtime_.allocationCountersEnabled) {
    allocationBefore_ = captureProcessAllocationSample();
  }
  if (runtime_.hasPhaseCounters() && runtime_.rssCheckpointsEnabled) {
    rssBefore_ = captureProcessRssSample();
  }
}

void SemanticValidationBenchmarkPhase::captureAfter() {
  if (runtime_.hasPhaseCounters() && runtime_.allocationCountersEnabled) {
    allocationAfter_ = captureProcessAllocationSample();
    hasAllocationAfter_ = true;
  }
  if (runtime_.hasPhaseCounters() && runtime_.rssCheckpointsEnabled) {
    rssAfter_ = captureProcessRssSample();
    hasRssAfter_ = true;
  }
}

void SemanticValidationBenchmarkPhase::publish(uint64_t callsVisited, uint64_t peakLocalMapSize) {
  if (!runtime_.hasPhaseCounters()) {
    return;
  }
  runtime_.phaseCounters->validation.callsVisited = callsVisited;
  runtime_.phaseCounters->validation.peakLocalMapSize = peakLocalMapSize;
  if (runtime_.allocationCountersEnabled && hasAllocationAfter_) {
    populateAllocationDelta(runtime_.phaseCounters->validation, allocationBefore_, allocationAfter_);
  }
  if (runtime_.rssCheckpointsEnabled && hasRssAfter_) {
    populateRssCheckpoints(runtime_.phaseCounters->validation, rssBefore_, rssAfter_);
  }
}

SemanticValidatorLifetimeBenchmark SemanticValidatorLifetimeBenchmark::fromEnvironment() {
  SemanticValidatorLifetimeBenchmark benchmark;
  benchmark.enabled_ = std::getenv("PRIMEC_BENCHMARK_SEMANTIC_VALIDATOR_LIFETIME") != nullptr;
  return benchmark;
}

void SemanticValidatorLifetimeBenchmark::captureBefore() {
  if (!enabled_) {
    return;
  }
  allocationBefore_ = captureProcessAllocationSample();
  rssBefore_ = captureProcessRssSample();
}

void SemanticValidatorLifetimeBenchmark::captureAfterRun() {
  if (!enabled_) {
    return;
  }
  allocationAfterRun_ = captureProcessAllocationSample();
  rssAfterRun_ = captureProcessRssSample();
}

void SemanticValidatorLifetimeBenchmark::captureAfterDestroyAndReport() {
  if (!enabled_) {
    return;
  }
  allocationAfterDestroy_ = captureProcessAllocationSample();
  rssAfterDestroy_ = captureProcessRssSample();
  std::cerr << "[benchmark-semantic-validator-lifetime] "
            << "{\"schema\":\"primestruct_semantic_validator_lifetime_v1\""
            << ",\"allocation_before_bytes\":" << allocationBefore_.allocatedBytes
            << ",\"allocation_after_run_bytes\":" << allocationAfterRun_.allocatedBytes
            << ",\"allocation_after_destroy_bytes\":" << allocationAfterDestroy_.allocatedBytes
            << ",\"rss_before_bytes\":" << rssBefore_.residentBytes
            << ",\"rss_after_run_bytes\":" << rssAfterRun_.residentBytes
            << ",\"rss_after_destroy_bytes\":" << rssAfterDestroy_.residentBytes
            << "}" << std::endl;
}

void maybeRelieveSemanticAllocatorPressure() {
  if (gDisableSemanticAllocatorReliefForBenchmark) {
    return;
  }
  if (std::getenv("PRIMEC_DISABLE_SEMANTIC_ALLOCATOR_RELIEF") != nullptr) {
    return;
  }
  relieveSemanticAllocatorPressure();
}

ProcessAllocationSample captureProcessAllocationSample() {
  ProcessAllocationSample sample;
#if defined(__APPLE__)
  malloc_statistics_t stats{};
  malloc_zone_statistics(nullptr, &stats);
  sample.valid = true;
  sample.allocationCount = static_cast<uint64_t>(stats.blocks_in_use);
  sample.allocatedBytes = static_cast<uint64_t>(stats.size_in_use);
#elif defined(__linux__)
  const struct mallinfo2 info = mallinfo2();
  sample.valid = true;
  sample.allocationCount = info.ordblks > 0 ? static_cast<uint64_t>(info.ordblks) : 0;
  sample.allocatedBytes = info.uordblks > 0 ? static_cast<uint64_t>(info.uordblks) : 0;
#endif
  return sample;
}

ProcessRssSample captureProcessRssSample() {
  ProcessRssSample sample;
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  const kern_return_t status = task_info(mach_task_self(),
                                         MACH_TASK_BASIC_INFO,
                                         reinterpret_cast<task_info_t>(&info),
                                         &count);
  if (status == KERN_SUCCESS) {
    sample.valid = true;
    sample.residentBytes = static_cast<uint64_t>(info.resident_size);
  }
#elif defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t residentPages = 0;
  if (statm.good()) {
    statm.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
    if (statm >> residentPages) {
      const long pageSize = sysconf(_SC_PAGESIZE);
      if (pageSize > 0) {
        sample.valid = true;
        sample.residentBytes = residentPages * static_cast<uint64_t>(pageSize);
      }
    }
  }
#endif
  return sample;
}

void populateAllocationDelta(SemanticPhaseCounterSnapshot &snapshot,
                             const ProcessAllocationSample &before,
                             const ProcessAllocationSample &after) {
  if (!before.valid || !after.valid) {
    return;
  }
  snapshot.allocationCount = saturatingSubtract(after.allocationCount, before.allocationCount);
  snapshot.allocatedBytes = saturatingSubtract(after.allocatedBytes, before.allocatedBytes);
}

void populateRssCheckpoints(SemanticPhaseCounterSnapshot &snapshot,
                            const ProcessRssSample &before,
                            const ProcessRssSample &after) {
  if (!before.valid || !after.valid) {
    return;
  }
  snapshot.rssBeforeBytes = before.residentBytes;
  snapshot.rssAfterBytes = after.residentBytes;
}

} // namespace primec::semantics
