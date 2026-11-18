#include "EmitDeps.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace primec {

std::vector<std::string> collectSourceDependencies(const ExpandedSource &source) {
  std::vector<std::string> files;
  for (const SourceUnit &unit : source.units) {
    if (unit.kind != SourceUnitKind::Primary && unit.kind != SourceUnitKind::Import &&
        unit.kind != SourceUnitKind::Stdlib && unit.kind != SourceUnitKind::Library) {
      continue;
    }
    if (unit.displayPath.empty()) {
      continue;
    }
    std::error_code ec;
    std::filesystem::path path = std::filesystem::absolute(unit.displayPath, ec);
    if (ec) {
      path = unit.displayPath;
    }
    files.push_back(path.lexically_normal().string());
  }
  std::sort(files.begin(), files.end());
  files.erase(std::unique(files.begin(), files.end()), files.end());
  return files;
}

bool writeEmitDepsFile(const std::string &path, const ExpandedSource &source, std::string &error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "failed to write --emit-deps file: " + path;
    return false;
  }
  for (const std::string &file : collectSourceDependencies(source)) {
    out << file << '\n';
  }
  out.flush();
  if (!out) {
    error = "failed to write --emit-deps file: " + path;
    return false;
  }
  return true;
}

} // namespace primec
