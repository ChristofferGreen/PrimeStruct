#pragma once

#include <string>

namespace primec {

// The output of a copy step (`Build.copy`): `output` as an absolute, normalized path inside `outDir`. False, with the
// problem in `error`, when `output` is empty, absolute, or leaves the output directory (`..`).
bool resolveCopyDestination(const std::string &output,
                            const std::string &outDir,
                            std::string &destination,
                            std::string &error);

// Copies the file `source` to `destination`: the target is removed first, so the copy is a new file (the kernel keeps
// the signature of the pages of an old one) and its directory is created. False, with the problem in `error`, when
// `source` is not a regular file or the copy fails.
bool copyBuildFile(const std::string &source, const std::string &destination, std::string &error);

} // namespace primec
