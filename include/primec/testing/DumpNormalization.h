#pragma once

#include <string>

namespace primec::testing {

// Compiler dumps (notably `--dump-stage type-graph`) carry wall-clock values:
// `*_ms=N`, `*_ms_max=N`, and `*_over=true|false` budget flags derived from them.
// They differ between runs, so any test that compares two dumps (or a dump with
// a golden) must normalize with this helper first; scripts/check_dump_comparisons.py
// enforces that. Timing numbers become `N` and `*_over` flags become `B`.
inline std::string stripDumpTimings(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    const size_t ms = text.find("_ms", i);
    const size_t over = text.find("_over=", i);
    if (ms == std::string::npos && over == std::string::npos) {
      out.append(text, i, std::string::npos);
      break;
    }
    if (over != std::string::npos && (ms == std::string::npos || over < ms)) {
      const size_t valueStart = over + 6;
      out.append(text, i, valueStart - i);
      size_t j = valueStart;
      while (j < text.size() && text[j] >= 'a' && text[j] <= 'z') {
        ++j;
      }
      out += "B";
      i = j;
      continue;
    }
    size_t j = ms + 3;
    if (text.compare(j, 4, "_max") == 0) {
      j += 4;
    }
    if (j < text.size() && text[j] == '=') {
      out.append(text, i, j + 1 - i);
      ++j;
      while (j < text.size() && text[j] >= '0' && text[j] <= '9') {
        ++j;
      }
      out += "N";
      i = j;
    } else {
      out.append(text, i, j - i);
      i = j;
    }
  }
  return out;
}

} // namespace primec::testing
