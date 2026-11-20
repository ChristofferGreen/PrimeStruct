#include "primec/support/TextFilterOffsetMap.h"

#include <algorithm>

namespace primec {

namespace {

std::vector<std::uint32_t> lineStartsOf(const std::string &text) {
  std::vector<std::uint32_t> starts{0};
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      starts.push_back(static_cast<std::uint32_t>(i + 1));
    }
  }
  return starts;
}

} // namespace

bool TextFilterOffsetMap::build(const std::string &source, const TextFilterOptions &options) {
  filtered_.clear();
  origins_.clear();
  filteredLineStarts_.clear();
  sourceLineStarts_.clear();
  sourceSize_ = source.size();
  std::string error;
  if (!TextFilterPipeline().applyWithOrigins(source, filtered_, origins_, error, options)) {
    filtered_.clear();
    origins_.clear();
    return false;
  }
  filteredLineStarts_ = lineStartsOf(filtered_);
  sourceLineStarts_ = lineStartsOf(source);
  return true;
}

std::size_t TextFilterOffsetMap::sourceOffsetOf(std::size_t filteredOffset) const {
  if (filteredOffset >= origins_.size()) {
    return sourceSize_;
  }
  if (origins_[filteredOffset] != TextFilterSynthesizedOrigin) {
    return origins_[filteredOffset];
  }
  for (std::size_t next = filteredOffset + 1; next < origins_.size(); ++next) {
    if (origins_[next] != TextFilterSynthesizedOrigin) {
      return origins_[next];
    }
  }
  for (std::size_t previous = filteredOffset; previous > 0; --previous) {
    if (origins_[previous - 1] != TextFilterSynthesizedOrigin) {
      return static_cast<std::size_t>(origins_[previous - 1]) + 1;
    }
  }
  return sourceSize_;
}

bool TextFilterOffsetMap::mapToSource(int line,
                                      int column,
                                      int &sourceLine,
                                      int &sourceColumn) const {
  if (origins_.empty() || line <= 0 || column <= 0 ||
      static_cast<std::size_t>(line) > filteredLineStarts_.size()) {
    return false;
  }
  const std::size_t filteredOffset = std::min<std::size_t>(
      static_cast<std::size_t>(filteredLineStarts_[static_cast<std::size_t>(line) - 1]) +
          static_cast<std::size_t>(column) - 1,
      filtered_.size());
  const std::size_t offset = sourceOffsetOf(filteredOffset);
  const auto next = std::upper_bound(sourceLineStarts_.begin(), sourceLineStarts_.end(), offset);
  const std::size_t lineIndex = static_cast<std::size_t>(next - sourceLineStarts_.begin()) - 1;
  sourceLine = static_cast<int>(lineIndex) + 1;
  sourceColumn = static_cast<int>(offset - sourceLineStarts_[lineIndex]) + 1;
  return true;
}

} // namespace primec
