#pragma once

#include "primec/frontend/ExpandedSource.h"

#include <string>
#include <vector>

namespace primec {

// One explicit stdlib `import` of user code that `--allow-import` does not list.
struct ImportAllowlistViolation {
  std::string message;
  int line = 0;
  int column = 0;
};

// True when `importPath` (`/std/file/*`, `/std/file/x`, `/std/file`) lies at or below one of `allowedPrefixes`.
// Matching is on path components: `/std/file` allows `/std/file`, `/std/file/*` and `/std/file/x`, never
// `/std/filesystem`. A trailing `/*` or `/` on either side is ignored.
bool importPathAllowed(const std::string &importPath, const std::vector<std::string> &allowedPrefixes);

// Checks every explicit `import /std/...` written in user source units (the primary file and imported project
// files) against `allowedPrefixes`. Imports inside stdlib units are trusted, as are imports that do not name a
// stdlib path. `stdlibRoots` are import roots whose `std/` folder holds the stdlib: an `import<...>` file below
// one counts as stdlib. Violations are returned in source order.
std::vector<ImportAllowlistViolation>
findImportAllowlistViolations(const ExpandedSource &source,
                              const std::vector<std::string> &allowedPrefixes,
                              const std::vector<std::string> &stdlibRoots);

} // namespace primec
