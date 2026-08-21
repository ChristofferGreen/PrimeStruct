#include "primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h"

#include "primec/support/CompileArena.h"

#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>

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

void recordLegacyCollectionBranchHitStructSlotLayoutVector() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().structSlotLayoutVectorBranchHits;
}

void recordLegacyCollectionBranchHitStructSlotLayoutSoa() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().structSlotLayoutSoaBranchHits;
}

void recordLegacyCollectionBranchHitUninitializedStructInferenceDuplicate() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().uninitializedStructInferenceDuplicateHits;
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

void recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackNullptr() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathTargetPathFallbackNullptrHits;
}

void recordLegacyCollectionBranchHitCollectionVectorOwnerPathReceiverTypeSite() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().collectionVectorOwnerPathReceiverTypeSiteHits;
}

namespace {

std::string jsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

} // namespace

void recordLegacyCollectionBranchStructSlotLayoutDivergence(
    const std::string &site,
    const std::string &hardcodedStructPath,
    int32_t hardcodedSlotCount,
    bool genericResolved,
    const std::string &genericStructPath,
    int32_t genericSlotCount) {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  ++counters().structSlotLayoutDivergenceCount;
  std::string line = "[benchmark-ir-lowerer-legacy-collection-branch-divergence] "
                      "{\"schema\":\"primestruct_ir_lowerer_legacy_collection_branch_divergence_v1\","
                      "\"site\":\"" + jsonEscape(site) + "\","
                      "\"hardcoded_struct_path\":\"" + jsonEscape(hardcodedStructPath) + "\","
                      "\"hardcoded_slot_count\":" + std::to_string(hardcodedSlotCount) + ","
                      "\"generic_resolved\":" + (genericResolved ? "true" : "false") + ","
                      "\"generic_struct_path\":\"" + jsonEscape(genericStructPath) + "\","
                      "\"generic_slot_count\":" + std::to_string(genericSlotCount) + "}";
  std::cerr << line << "\n";
  appendLineToLogFileSink(line);
}

void emitLegacyCollectionBranchCountersReport() {
  if (!legacyCollectionBranchCountersEnabled()) {
    return;
  }
  const LegacyCollectionBranchCounters &c = counters();
  const std::string line =
      "[benchmark-ir-lowerer-legacy-collection-branch-counters] "
      "{\"schema\":\"primestruct_ir_lowerer_legacy_collection_branch_counters_v1\","
      "\"struct_slot_layout_vector_branch_hits\":" + std::to_string(c.structSlotLayoutVectorBranchHits) + ","
      "\"struct_slot_layout_soa_branch_hits\":" + std::to_string(c.structSlotLayoutSoaBranchHits) + ","
      "\"uninitialized_struct_inference_duplicate_hits\":" +
      std::to_string(c.uninitializedStructInferenceDuplicateHits) + ","
      "\"collection_vector_metadata_method_path_hits\":" +
      std::to_string(c.collectionVectorMetadataMethodPathHits) + ","
      "\"collection_vector_owner_path_hits\":" + std::to_string(c.collectionVectorOwnerPathHits) + ","
      "\"collection_vector_owner_path_target_path_site_hits\":" +
      std::to_string(c.collectionVectorOwnerPathTargetPathSiteHits) + ","
      "\"collection_vector_owner_path_target_path_fallback_resolved_hits\":" +
      std::to_string(c.collectionVectorOwnerPathTargetPathFallbackResolvedHits) + ","
      "\"collection_vector_owner_path_target_path_fallback_nullptr_hits\":" +
      std::to_string(c.collectionVectorOwnerPathTargetPathFallbackNullptrHits) + ","
      "\"collection_vector_owner_path_receiver_type_site_hits\":" +
      std::to_string(c.collectionVectorOwnerPathReceiverTypeSiteHits) + ","
      "\"struct_slot_layout_divergence_count\":" + std::to_string(c.structSlotLayoutDivergenceCount) + "}";
  std::cerr << line << "\n";
  appendLineToLogFileSink(line);
}

} // namespace primec::ir_lowerer
