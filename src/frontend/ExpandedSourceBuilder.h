#pragma once

#include "primec/frontend/ExpandedSource.h"
#include "primec/frontend/ImportResolver.h"

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

  // Appends the interface of `library` (`--library`) as a source unit of kind SourceUnitKind::Library, unless the
  // source holds one for that library already (`import</root>` and the stdlib pass can both ask). Returns whether it
  // appended.
  bool appendLibraryInterfaceOnce(const LibraryInterfaceUnit &library);

private:
  void advance(std::string_view text);
  void updateUnitRange(SourceUnit &unit, const SourceSegment &segment);

  ExpandedSource &source_;
  Cursor cursor_;
};

} // namespace primec
