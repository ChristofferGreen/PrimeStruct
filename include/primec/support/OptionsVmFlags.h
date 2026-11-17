#pragma once

#include <string>

namespace primec {
struct Options;

enum class VmFlagResult {
  NotVmFlag, // `arg` is not a VM task flag; the caller goes on with its other flags
  Consumed,  // the flag (and its value, advancing `index`) was read into `out`
  Error,     // the flag is malformed; `error` says why
};

// The VM's task flags of primec and primevm (docs/spec/vm-design.md, Tasks, Tasks on threads and Synchronization
// record and replay): `--vm-schedule-seed`, `--vm-threads`, `--vm-record-sync` and `--vm-replay-sync`. `arg` is
// argv[index]; a flag with a separate value reads argv[index + 1] and advances `index` past it.
VmFlagResult parseVmFlag(
    const std::string &arg, int argc, char **argv, int &index, Options &out, std::string &error);

// What can only be checked once every flag is read: a record needs `--vm-threads`, a replay cannot be combined with
// `--vm-threads` or a record, and `PRIMESTRUCT_VM_SCHEDULE_SEED` gives the seed a flag did not.
bool validateVmFlags(Options &out, std::string &error);

} // namespace primec
