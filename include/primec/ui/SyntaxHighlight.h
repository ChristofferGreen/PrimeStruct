#pragma once

// Syntax highlighting for the native UI text views (docs/NativeUiPlan.md). The lexers are small, line-tolerant scanners (no parsing, no
// state across edits): they turn UTF-8 text into style runs over byte ranges,
// which ps_ui_text_view_highlight applies through ps_ui_text_view_add_style.
// Runs never overlap, start and end on code point boundaries, and are ordered.

#include <cstdint>
#include <string_view>
#include <vector>

#include "primec/ui/NativeUi.h"

namespace primec::ui {

enum class Language { None, PrimeStruct, Cpp, Python, Json, Markdown };

// Colours are 0xRRGGBB mid-tones that read on both light and dark backgrounds.
namespace highlight_colors {
constexpr int32_t Keyword = 0xC9569A;
constexpr int32_t Type = 0x3C9FD0;
constexpr int32_t String = 0x3FA04F;
constexpr int32_t Number = 0xD9822B;
constexpr int32_t Comment = 0x8A8F98;
constexpr int32_t Heading = 0x3C9FD0;
constexpr int32_t Code = 0x3FA04F;
constexpr int32_t Link = 0xD9822B;
constexpr int32_t Marker = 0xC9569A;
constexpr int32_t Strong = 0xE0A53E;
} // namespace highlight_colors

struct HighlightRun {
  int32_t start = 0;
  int32_t end = 0;
  int32_t rgb = 0;
  int32_t flags = 0;
  bool operator==(const HighlightRun &other) const = default;
};

// By the file name's extension, case-insensitively: .prime .ps; .c .h .cc .cpp
// .cxx .hpp .hh .hxx .m .mm; .py .pyw; .json; .md .markdown. None otherwise.
Language languageForPath(std::string_view path);

std::vector<HighlightRun> highlight(Language language, std::string_view text);

// Texts longer than this are shown unstyled (restyling runs on every edit).
constexpr size_t MaxHighlightBytes = 65536;

// Replaces the styles of `view` with the highlighting of its text for the language
// of `path` (unknown extension: styles cleared). Used by the backends to
// implement ps_ui_text_view_highlight on top of the public style calls.
bool applyHighlight(uint64_t view, const char *path);

} // namespace primec::ui
