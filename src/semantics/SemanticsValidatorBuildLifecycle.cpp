#include "SemanticsValidator.h"

#include <array>
#include <string_view>

namespace primec::semantics {

bool SemanticsValidator::validateLifecycleHelperDefinitions() {
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };

  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
        {"CreateStack", "stack"},
        {"DestroyStack", "stack"},
        {"CreateHeap", "heap"},
        {"DestroyHeap", "heap"},
        {"CreateBuffer", "buffer"},
        {"DestroyBuffer", "buffer"},
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
      parentOut = fullPath.substr(0, suffixStart - 1);
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };

  auto isCopyHelperName = [](const std::string &fullPath) -> bool {
    static constexpr std::string_view copySuffix = "/Copy";
    static constexpr std::string_view moveSuffix = "/Move";
    if (fullPath.size() <= copySuffix.size()) {
      return false;
    }
    if (fullPath.compare(fullPath.size() - copySuffix.size(),
                         copySuffix.size(),
                         copySuffix.data(),
                         copySuffix.size()) == 0) {
      return true;
    }
    if (fullPath.size() <= moveSuffix.size()) {
      return false;
    }
    return fullPath.compare(fullPath.size() - moveSuffix.size(),
                            moveSuffix.size(),
                            moveSuffix.data(),
                            moveSuffix.size()) == 0;
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    auto failLifecycleDiagnostic = [&](std::string message) -> bool {
      return failDefinitionDiagnostic(def, std::move(message));
    };
    std::string parentPath;
    std::string placement;
    if (!isLifecycleHelper(def.fullPath, parentPath, placement)) {
      continue;
    }
    if (parentPath.empty() || structNames_.count(parentPath) == 0) {
      return failLifecycleDiagnostic(
          "lifecycle helper must be nested inside a struct: " + def.fullPath);
    }
    if (isCopyHelperName(def.fullPath)) {
      if (def.parameters.size() != 1) {
        return failLifecycleDiagnostic(
            "Copy/Move helpers require exactly one parameter: " +
            def.fullPath);
      }
    } else if (!def.parameters.empty()) {
      return failLifecycleDiagnostic(
          "lifecycle helpers do not accept parameters: " + def.fullPath);
    }
  }

  return true;
}

}  // namespace primec::semantics
