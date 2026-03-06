#include "EmitterEmitSetupLifecycleHelperStep.h"

#include <array>
#include <string_view>

namespace primec::emitter {

namespace {
struct HelperSuffixInfo {
  std::string_view suffix;
  EmitterLifecycleHelperKind kind;
  std::string_view placement;
};
} // namespace

std::optional<EmitterLifecycleHelperMatch> runEmitterEmitSetupLifecycleHelperMatchStep(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 10> suffixes = {{
      {"Create", EmitterLifecycleHelperKind::Create, ""},
      {"Destroy", EmitterLifecycleHelperKind::Destroy, ""},
      {"Copy", EmitterLifecycleHelperKind::Copy, ""},
      {"Move", EmitterLifecycleHelperKind::Move, ""},
      {"CreateStack", EmitterLifecycleHelperKind::Create, "stack"},
      {"DestroyStack", EmitterLifecycleHelperKind::Destroy, "stack"},
      {"CreateHeap", EmitterLifecycleHelperKind::Create, "heap"},
      {"DestroyHeap", EmitterLifecycleHelperKind::Destroy, "heap"},
      {"CreateBuffer", EmitterLifecycleHelperKind::Create, "buffer"},
      {"DestroyBuffer", EmitterLifecycleHelperKind::Destroy, "buffer"},
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
    EmitterLifecycleHelperMatch match;
    match.parentPath = fullPath.substr(0, suffixStart - 1);
    match.kind = info.kind;
    match.placement = std::string(info.placement);
    return match;
  }
  return std::nullopt;
}

} // namespace primec::emitter
