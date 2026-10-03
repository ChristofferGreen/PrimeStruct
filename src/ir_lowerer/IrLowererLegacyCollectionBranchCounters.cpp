#include "primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h"

#include "primec/support/CompileArena.h"

#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include "primec/support/BenchmarkSink.h"

namespace primec::ir_lowerer {

namespace {

bool computeEnvEnabled() {
  const char *env =
      std::getenv("PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS");
  return env != nullptr && env[0] != '\0';
}

// Optional aggregation sink for whole-suite evidence-gathering runs (e.g.
// PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS=1 exported
// across an entire ctest invocation, where hundreds of primec/test-binary
// processes each hold their own in-process counters and none of their
// stdout/stderr is otherwise captured by the test runner for passing
// cases). When set, every report/divergence line normally written to
// stderr is ALSO appended, as a single atomic write() (safe against
// interleaving from concurrent processes sharing the file, since each line
// stays well under PIPE_BUF), to the file this env var names.
std::string logFileSinkPath() {
  const char *env = std::getenv(
      "PRIMEC_BENCHMARK_IR_LOWERER_LEGACY_COLLECTION_BRANCH_COUNTERS_LOG_FILE");
  return env != nullptr ? std::string(env) : std::string();
}

void appendLineToLogFileSink(const std::string &line) {
  // TODO-5235: built via systemHeapValue() so this magic static's backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string path = primec::systemHeapValue(logFileSinkPath);
  if (path.empty()) {
    return;
  }
  std::string withNewline = line;
  if (withNewline.empty() || withNewline.back() != '\n') {
    withNewline += '\n';
  }
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    return;
  }
  ssize_t written = 0;
  while (written < static_cast<ssize_t>(withNewline.size())) {
    const ssize_t result =
        ::write(fd, withNewline.data() + written, withNewline.size() - written);
    if (result <= 0) {
      break;
    }
    written += result;
  }
  ::close(fd);
}

bool &enabledFlag() {
  static bool enabled = computeEnvEnabled();
  return enabled;
}

LegacyCollectionBranchCounters &counters() {
  static LegacyCollectionBranchCounters instance;
  return instance;
}

bool &atexitRegistered() {
  static bool registered = false;
  return registered;
}

void atexitReportHandler() {
  emitLegacyCollectionBranchCountersReport();
}

} // namespace

namespace {

void ensureAtexitReportRegisteredIfEnabled() {
  if (enabledFlag() && !atexitRegistered()) {
    std::atexit(&atexitReportHandler);
    atexitRegistered() = true;
  }
}

} // namespace

void setLegacyCollectionBranchCountersEnabled(bool enabled) {
  enabledFlag() = enabledFlag() || enabled;
  ensureAtexitReportRegisteredIfEnabled();
}

bool legacyCollectionBranchCountersEnabled() {
  ensureAtexitReportRegisteredIfEnabled();
  return enabledFlag();
}

void resetLegacyCollectionBranchCounters() {
  counters() = LegacyCollectionBranchCounters{};
}

const LegacyCollectionBranchCounters &legacyCollectionBranchCounters() {
  return counters();
}

void recordLegacyCollectionBranchHitCollectionVectorMetadataMethodPath() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorMetadataMethodPathHits;
}

void recordLegacyCollectionBranchHitCollectionVectorOwnerPath() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathHits;
}

void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathSite() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathTargetPathSiteHits;
}

void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackResolved() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathTargetPathFallbackResolvedHits;
}

void recordLegacyCollectionBranchHitCollectionVectorOwnerPathReceiverTypeSite() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathReceiverTypeSiteHits;
}

void emitLegacyCollectionBranchCountersReport() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  const LegacyCollectionBranchCounters &c = counters();
  const std::string line =
      "[benchmark-ir-lowerer-legacy-collection-branch-counters] "
      "{\"schema\":\"primestruct_ir_lowerer_legacy_collection_branch_counters_v2\","
      "\"collection_vector_metadata_method_path_hits\":" +
      std::to_string(c.collectionVectorMetadataMethodPathHits) + ","
      "\"collection_vector_owner_path_hits\":" + std::to_string(c.collectionVectorOwnerPathHits) + ","
      "\"collection_vector_owner_path_target_path_site_hits\":" +
      std::to_string(c.collectionVectorOwnerPathTargetPathSiteHits) + ","
      "\"collection_vector_owner_path_target_path_fallback_resolved_hits\":" +
      std::to_string(c.collectionVectorOwnerPathTargetPathFallbackResolvedHits) + ","
      "\"collection_vector_owner_path_receiver_type_site_hits\":" +
      std::to_string(c.collectionVectorOwnerPathReceiverTypeSiteHits) + "}";
  primec::support::emitBenchmarkLine(line);
  appendLineToLogFileSink(line);
}

} // namespace primec::ir_lowerer
