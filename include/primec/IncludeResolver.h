#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace primec {

class IncludeResolver {
public:
  bool expandIncludes(const std::string &inputPath,
                      std::string &source,
                      std::string &error,
                      const std::vector<std::string> &includePaths = {});

private:
  bool expandIncludesInternal(const std::string &baseDir,
                              std::string &source,
                              std::unordered_set<std::string> &expanded,
                              std::string &error,
                              const std::vector<std::filesystem::path> &includeRoots);
};

} // namespace primec
