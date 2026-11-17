#pragma once

#include "primec/frontend/ExpandedSource.h"

#include <string>
#include <vector>

namespace primec {

// The files a compile read: the primary input, every import, and the stdlib files that were included. Absolute,
// lexically normalized, sorted bytewise, without duplicates. Generated separator text is not a file.
std::vector<std::string> collectSourceDependencies(const ExpandedSource &source);

// Writes collectSourceDependencies to `path`, one per line. On failure sets `error` and returns false.
bool writeEmitDepsFile(const std::string &path, const ExpandedSource &source, std::string &error);

} // namespace primec
