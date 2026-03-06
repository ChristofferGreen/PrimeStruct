#pragma once

#include <optional>
#include <string>

namespace primec::emitter {

enum class EmitterLifecycleHelperKind { Create, Destroy, Copy, Move };

struct EmitterLifecycleHelperMatch {
  std::string parentPath;
  EmitterLifecycleHelperKind kind = EmitterLifecycleHelperKind::Create;
  std::string placement;
};

std::optional<EmitterLifecycleHelperMatch> runEmitterEmitSetupLifecycleHelperMatchStep(const std::string &fullPath);

} // namespace primec::emitter
