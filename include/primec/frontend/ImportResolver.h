#pragma once

#include "primec/frontend/ExpandedSource.h"

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

  // Expands imports for source text supplied by the caller. `displayPath` names
  // the primary unit in diagnostics; relative imports resolve against its
  // parent directory (or the current directory when it has none).
  bool expandImportsFromSource(const std::string &displayPath,
                               const std::string &content,
                               ExpandedSource &source,
                               std::string &error,
                               const std::vector<std::string> &importPaths = {});

private:
  const ProcessRunner *processRunner_ = nullptr;
};

} // namespace primec
