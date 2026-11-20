#pragma once

#include "primec/frontend/ExpandedSource.h"

#include <map>
#include <optional>
#include <string>

namespace primec {

// The text of every source unit of an import-expanded source, cut out of the expanded text through the segments: a
// diagnostic's span in a file is a line of that file, and the snippet under it is a line of that file's text, not of the
// expanded text the parser read.
struct SourceUnitTexts {
  // The primary file's own text. Unset unless the source also has other units: a source without imports keeps the
  // expanded text, which is already the primary text.
  std::optional<std::string> primary;
  // Imported, stdlib and library units by display path (the `file` of a span).
  std::map<std::string, std::string> byFile;
};

// `flattenedText` is the text the segments' flattened ranges address (ExpandedSource::text).
SourceUnitTexts sourceUnitTexts(const ExpandedSource &source, const std::string &flattenedText);

} // namespace primec
