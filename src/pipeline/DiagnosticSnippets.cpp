#include "primec/pipeline/DiagnosticSnippets.h"

#include <cstddef>
#include <vector>

namespace primec {

namespace {

std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  size_t start = 0;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == '\n') {
      lines.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  return lines;
}

// The lines of one unit, indexed by original line - 1; a line no segment covers stays empty.
using UnitLines = std::vector<std::string>;

void placeSegment(const SourceSegment &segment,
                  const std::vector<std::string> &flattened,
                  UnitLines &unit) {
  if (segment.flattenedStartLine <= 0 || segment.originalStartLine <= 0) {
    return;
  }
  // The end is exclusive: a segment that ends at column 1 does not include its last line.
  const int lastLine =
      segment.flattenedEndColumn <= 1 ? segment.flattenedEndLine - 1 : segment.flattenedEndLine;
  for (int line = segment.flattenedStartLine; line <= lastLine; ++line) {
    if (static_cast<size_t>(line) > flattened.size()) {
      return;
    }
    const size_t original =
        static_cast<size_t>(segment.originalStartLine + (line - segment.flattenedStartLine));
    if (unit.size() < original) {
      unit.resize(original);
    }
    if (unit[original - 1].empty()) {
      unit[original - 1] = flattened[static_cast<size_t>(line) - 1];
    }
  }
}

std::string joinLines(const UnitLines &lines) {
  std::string text;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i != 0) {
      text.push_back('\n');
    }
    text += lines[i];
  }
  return text;
}

} // namespace

SourceUnitTexts sourceUnitTexts(const ExpandedSource &source, const std::string &flattenedText) {
  SourceUnitTexts texts;
  bool hasOtherUnit = false;
  for (const SourceUnit &unit : source.units) {
    hasOtherUnit =
        hasOtherUnit || (unit.kind != SourceUnitKind::Primary && !unit.displayPath.empty());
  }
  if (!hasOtherUnit) {
    return texts;
  }
  const std::vector<std::string> flattened = splitLines(flattenedText);
  std::map<std::size_t, UnitLines> byUnit;
  for (const SourceSegment &segment : source.segments) {
    placeSegment(segment, flattened, byUnit[segment.unitId]);
  }
  for (const SourceUnit &unit : source.units) {
    const auto found = byUnit.find(unit.id);
    if (found == byUnit.end()) {
      continue;
    }
    if (unit.kind == SourceUnitKind::Primary) {
      texts.primary = joinLines(found->second);
    } else if (!unit.displayPath.empty()) {
      texts.byFile.emplace(unit.displayPath, joinLines(found->second));
    }
  }
  return texts;
}

} // namespace primec
