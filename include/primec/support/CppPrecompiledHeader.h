#pragma once

#include "primec/support/ProcessRunner.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

// A precompiled header of the standard headers that every `optexe` and optimized `exe` program parses
// (`optCppRuntimeHeaders()`), built on first use and kept in a cache directory inside the build directory.
//
// A cached header is never reused when anything that shapes it changed: the file name is a hash of the C++
// compiler's predefined macros under the compile flags (which carry its version, target and optimization level),
// of the flags themselves and of the header text. A header that is missing, unreadable or half written is
// rebuilt; a header is written under a unique name and renamed into place, so concurrent compiles never see a
// partial file. The header text it was built from stays next to it as `pch-<key>.h`: the compiler refuses a
// precompiled header whose source file is gone or changed.

// The key a precompiled header is stored under: a hash of the compiler identity (its predefined macros), the
// flags and the header text. Different input, different key.
std::string cppPrecompiledHeaderKey(std::string_view compilerIdentity,
                                    const std::vector<std::string> &flags,
                                    std::string_view headerText);

// The path of a valid precompiled header for `headerText` compiled with `flags` (the flags of the compile that
// will use it, without the input and output), building it into `cacheDir` when it is not there yet. Returns an
// empty path when the cache directory cannot be used or the compiler cannot build the header; the caller then
// compiles without it.
std::filesystem::path ensureCppPrecompiledHeader(const ProcessRunner &runner,
                                                 const std::filesystem::path &cacheDir,
                                                 const std::vector<std::string> &flags,
                                                 std::string_view headerText);

// True when every extra compiler argument only links (`-l`, `-L`, `-Wl,`, `-framework`, `-rpath`, archives and
// objects): an argument that can change how headers are read (`-D`, `-I`, `-std`, `-include`) or a source of
// another language would not match the precompiled header.
bool linkArgsAllowPrecompiledHeader(const std::vector<std::string> &linkArgs);

} // namespace primec
