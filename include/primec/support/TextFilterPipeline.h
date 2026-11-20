#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

struct TextTransformRule {
  std::string path;
  bool wildcard = false;
  bool recursive = false;
  std::vector<std::string> transforms;
};

struct TextFilterOptions {
  std::vector<std::string> enabledFilters = {"collections", "operators", "implicit-utf8"};
  std::vector<TextTransformRule> rules;
  bool allowEnvelopeTransforms = true;

  bool hasFilter(const std::string &name) const {
    for (const auto &filter : enabledFilters) {
      if (filter == name) {
        return true;
      }
    }
    return false;
  }
};

// The origin of a filtered byte that no source byte produced: an inserted call name, parenthesis or type suffix.
constexpr std::uint32_t TextFilterSynthesizedOrigin = 0xFFFFFFFFu;

class TextFilterPipeline {
public:
  bool apply(const std::string &input,
             std::string &output,
             std::string &error,
             const TextFilterOptions &options = {}) const;

  // The same filtering, and for each byte of `output` the offset in `input` it was copied from
  // (`TextFilterSynthesizedOrigin` for a byte the filter wrote). Slower than `apply`: it tracks every write.
  bool applyWithOrigins(const std::string &input,
                        std::string &output,
                        std::vector<std::uint32_t> &origins,
                        std::string &error,
                        const TextFilterOptions &options = {}) const;
};

} // namespace primec
