#pragma once

#include <string>
#include <vector>

namespace primec {

struct TextFilterOptions {
  std::vector<std::string> enabledFilters = {"collections", "operators", "implicit-utf8"};

  bool hasFilter(const std::string &name) const {
    for (const auto &filter : enabledFilters) {
      if (filter == name) {
        return true;
      }
    }
    return false;
  }
};

class TextFilterPipeline {
public:
  bool apply(const std::string &input,
             std::string &output,
             std::string &error,
             const TextFilterOptions &options = {}) const;
};

} // namespace primec
