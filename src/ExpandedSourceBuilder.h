#pragma once

#include "primec/ExpandedSource.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace primec {

class ExpandedSourceBuilder {
public:
  struct Cursor {
    int line = 1;
    int column = 1;
  };

  explicit ExpandedSourceBuilder(ExpandedSource &source);

  std::size_t addUnit(SourceUnitKind kind,
                      std::string displayPath,
                      std::string moduleKey = {},
                      int originalStartLine = 0,
                      int originalStartColumn = 0);

  void appendSegment(std::size_t unitId,
                     std::string_view text,
                     int originalStartLine,
                     int originalStartColumn);

  std::size_t appendGenerated(std::string_view text, std::string moduleKey = {});
  std::size_t textSize() const;
  bool textEndsWithNewline() const;

private:
  void advance(std::string_view text);
  void updateUnitRange(SourceUnit &unit, const SourceSegment &segment);

  ExpandedSource &source_;
  Cursor cursor_;
};

} // namespace primec
