#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace primec {

class ImportResolver {
public:
  bool expandImports(const std::string &inputPath,
                      std::string &source,
                      std::string &error,
                      const std::vector<std::string> &importPaths = {});

private:
  bool expandImportsInternal(const std::string &baseDir,
                              std::string &source,
                              std::unordered_set<std::string> &expanded,
                              std::string &error,
                              const std::vector<std::filesystem::path> &importRoots);
};

} // namespace primec
