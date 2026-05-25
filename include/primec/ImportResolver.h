#pragma once

#include "primec/ExpandedSource.h"

#include <string>
#include <vector>

namespace primec {
class ProcessRunner;

class ImportResolver {
public:
  explicit ImportResolver(const ProcessRunner *processRunner = nullptr);

  bool expandImports(const std::string &inputPath,
                      std::string &source,
                      std::string &error,
                      const std::vector<std::string> &importPaths = {});

  bool expandImports(const std::string &inputPath,
                      ExpandedSource &source,
                      std::string &error,
                      const std::vector<std::string> &importPaths = {});

private:
  const ProcessRunner *processRunner_ = nullptr;
};

} // namespace primec
