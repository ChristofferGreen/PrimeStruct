#pragma once

#include <string>

namespace primec {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string entryPath = "/main";
};
} // namespace primec
