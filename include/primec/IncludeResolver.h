#pragma once

#include <string>
#include <unordered_set>

namespace primec {

class IncludeResolver {
public:
  bool expandIncludes(const std::string &inputPath, std::string &source, std::string &error);

private:
  bool expandIncludesInternal(const std::string &baseDir,
                              std::string &source,
                              std::unordered_set<std::string> &expanded,
                              std::string &error);
};

} // namespace primec
