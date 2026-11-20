#pragma once

#include "primec/support/TextFilterPipeline.h"

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

// Maps a position of the text-filtered source back to the text that was filtered (the source the user wrote, after
// import expansion). The filter rewrites operators into calls, so a column or a snippet taken from the filtered text
// is not the user's. The map is built from the filter run itself: every filtered byte knows the source offset it was
// copied from, and a byte the filter wrote (`plus(`, `, `, `)`, a type suffix) takes the position of the next copied
// byte, so a rewritten call reports the start of the expression it came from.
class TextFilterOffsetMap {
public:
  // Filters `source` with `options` again and records where each filtered byte came from. False when the filter
  // fails; the map then maps nothing.
  bool build(const std::string &source, const TextFilterOptions &options);

  // The 1-based line and column in the source of 1-based `line` and `column` in the filtered text. False when the
  // map holds nothing or the line does not exist.
  bool mapToSource(int line, int column, int &sourceLine, int &sourceColumn) const;

  // The filtered text the map was built for (what the parser saw).
  const std::string &filteredText() const {
    return filtered_;
  }

private:
  std::size_t sourceOffsetOf(std::size_t filteredOffset) const;

  std::string filtered_;
  std::vector<std::uint32_t> origins_;
  std::vector<std::uint32_t> filteredLineStarts_;
  std::vector<std::uint32_t> sourceLineStarts_;
  std::size_t sourceSize_ = 0;
};

} // namespace primec
