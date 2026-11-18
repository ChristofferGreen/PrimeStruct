#pragma once

#include "primec/frontend/ExpandedSource.h"

#include <string>
#include <vector>

namespace primec {
class ProcessRunner;

// The interface of a library archive as the resolver reads it (`--library`): an `import</root>` of a namespace root
// the library owns splices `text` once, as a source unit of kind SourceUnitKind::Library.
struct LibraryInterfaceUnit {
  std::string name;
  // The archive path as given on the command line (diagnostics and --emit-deps name it).
  std::string displayPath;
  std::string interfaceHash;
  // The owned namespace roots, `/ui`.
  std::vector<std::string> roots;
  std::string text;
};

class ImportResolver {
public:
  explicit ImportResolver(const ProcessRunner *processRunner = nullptr);

  // Libraries whose interface `import</root>` finds before it looks for a file. Not owned; must outlive the calls.
  void setLibraryInterfaces(const std::vector<LibraryInterfaceUnit> *libraries) {
    libraries_ = libraries;
  }

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
  const std::vector<LibraryInterfaceUnit> *libraries_ = nullptr;
};

} // namespace primec
