#pragma once

#include <string>

namespace primec {

struct TextFilterOptions {
  bool implicitI32Suffix = true;
};

class TextFilterPipeline {
public:
  bool apply(const std::string &input,
             std::string &output,
             std::string &error,
             const TextFilterOptions &options = {}) const;
};

} // namespace primec
