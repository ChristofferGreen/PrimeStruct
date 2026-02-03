#pragma once

#include <string>

namespace primec {

class IncludeResolver {
public:
  bool expandIncludes(const std::string &inputPath, std::string &source, std::string &error);

private:
  bool expandIncludesInternal(const std::string &baseDir, std::string &source, std::string &error);
};

} // namespace primec
