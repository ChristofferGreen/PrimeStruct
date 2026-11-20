#pragma once

#include <cstdint>
#include <sys/types.h>

namespace primec {

// Cumulative CPU time used by the process `pid` and every process below it, the time of children that already exited
// included, in a unit private to the platform (a monotone count, only ever compared with an earlier reading). A driver
// such as `clang++` runs its compiler and linker as child processes, so its own time says nothing about progress.
// Returns false when the platform cannot read it (no libproc or /proc) or `pid` is gone.
bool processTreeCpuTime(pid_t pid, std::uint64_t &out);

} // namespace primec
