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

// The place of a file that a function step writes: `path` (relative to the output directory) as an absolute, normalized
// path inside `outDir`. False, with `writes outside the output directory: <path>` (or `writes an empty path`) in `error`,
// when `path` is empty or absolute, leaves the output directory (`..`), reaches outside it through a symbolic link of a
// folder on the way, or names a symbolic link (a write through it would land elsewhere). Nothing is created.
bool resolveStepOutputPath(const std::string &path,
                           const std::string &outDir,
                           std::string &resolved,
                           std::string &error);

} // namespace primec
