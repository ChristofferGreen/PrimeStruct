#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

enum class SourceUnitKind {
  Primary,
  Import,
  Stdlib,
  Generated,
  Unknown,
};

inline std::string_view sourceUnitKindName(SourceUnitKind kind) {
  switch (kind) {
    case SourceUnitKind::Primary:
      return "primary";
    case SourceUnitKind::Import:
      return "import";
    case SourceUnitKind::Stdlib:
      return "stdlib";
    case SourceUnitKind::Generated:
      return "generated";
    case SourceUnitKind::Unknown:
      return "unknown";
  }
  return "unknown";
}

struct SourceSegment {
  std::size_t unitId = 0;
  // Flattened ranges are 1-based and end-exclusive.
  int flattenedStartLine = 0;
  int flattenedStartColumn = 0;
  int flattenedEndLine = 0;
  int flattenedEndColumn = 0;
  int originalStartLine = 0;
  int originalStartColumn = 0;
};

struct SourceUnit {
  std::size_t id = 0;
  SourceUnitKind kind = SourceUnitKind::Unknown;
  std::string displayPath = {};
  std::string moduleKey = {};
  // Flattened ranges aggregate this unit's segments and are 1-based/end-exclusive.
  int flattenedStartLine = 0;
  int flattenedStartColumn = 0;
  int flattenedEndLine = 0;
  int flattenedEndColumn = 0;
  int originalStartLine = 0;
  int originalStartColumn = 0;
};

struct ExpandedSource {
  std::string text;
  std::vector<SourceUnit> units = {};
  std::vector<SourceSegment> segments = {};
};

} // namespace primec
