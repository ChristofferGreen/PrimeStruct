// Precompiled-header source for primec's generated-C++ ("--emit=exe") output.
//
// IrToCppEmitter.cpp emits a fixed set of standard-library #includes at the
// top of every generated program (a subset of these, chosen by which
// runtime helpers the specific program needs - see moduleUsesF32Helpers()
// and friends in IrToCppEmitter.cpp). This header is the superset of all of
// them, precompiled once at build time (see the primec_generated_cpp_pch
// CMake target) so ExternalTooling.cpp's clang++ invocation can pass
// `-include-pch` and skip re-parsing/re-instantiating these headers on every
// exe-mode compile. Measured locally: ~3.5x wall-clock reduction on the
// clang step for a trivial generated program (includes dominate; the
// generated program body itself is cheap to compile by comparison).
//
// Keep this header's include list a superset of IrToCppEmitter.cpp's -
// clang tolerates a generated .cpp re-including a subset of what's already
// in the PCH (the guards make the re-includes no-ops), but the reverse
// (a generated .cpp needing something NOT in the PCH) would just fall back
// to parsing it fresh, so this only needs to stay a superset to keep the
// speedup, not for correctness.
#pragma once

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>
