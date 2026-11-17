#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// The synchronization-order log of a `--vm-threads` run (docs/spec/vm-design.md, Synchronization record and replay):
// `--vm-record-sync <path>` writes it, `--vm-replay-sync <path>` reads it. It holds the events that order tasks
// against each other, in the order they happened, and nothing else: no host input or output, no instruction trace.

enum class SyncEventKind : uint8_t {
  End = 0, // the trailer, never an event
  Spawn = 1,
  Wait = 2,
  Lock = 3,
  Unlock = 4,
  ConditionWait = 5,
  ConditionNotify = 6,
  AtomicLoad = 7,
  AtomicStore = 8,
  AtomicAdd = 9,
  AtomicExchange = 10,
  AtomicCompareExchange = 11,
};

// One event: the task that did it (its number, the main task is 1 and spawned tasks count up from 2 in the order of
// their spawn events) and what it did.
struct SyncEvent {
  uint32_t task = 0;
  SyncEventKind kind = SyncEventKind::End;
};

inline constexpr uint32_t SyncLogVersion = 1;

// The event an opcode of the scheduler's family is, or End for any other opcode.
SyncEventKind syncEventKindOf(IrOpcode op);
const char *syncEventKindName(SyncEventKind kind);

// A fingerprint of the module a log belongs to (FNV-1a of its serialized form), so a log is refused for another
// program or for the same program compiled differently (another -O level changes the IR).
bool fingerprintIrModule(const IrModule &module, uint64_t &fingerprint, std::string &error);

// Writes the log of one run. Events arrive from every task thread; they are appended under one mutex, which is the
// ordering point of the record: a lock event is noted after the lock is taken and an unlock event before it is
// released, so the log orders two tasks on the same lock; an atomic operation holds the mutex across the operation
// itself (`orderMutex`), so atomic operations are logged in the order they ran. The log is buffered and written in
// blocks.
class SyncLogRecorder {
public:
  // Opens `path` for writing; null with `error` set when it cannot be created.
  static std::unique_ptr<SyncLogRecorder>
  open(const std::string &path, uint64_t fingerprint, std::string &error);
  ~SyncLogRecorder();
  SyncLogRecorder(const SyncLogRecorder &) = delete;
  SyncLogRecorder &operator=(const SyncLogRecorder &) = delete;

  // Appends one event.
  void note(uint64_t task, SyncEventKind kind);
  // The ordering point. An atomic operation holds it across the operation and appends with `noteLocked`.
  std::mutex &orderMutex() {
    return mutex_;
  }
  void noteLocked(uint64_t task, SyncEventKind kind);

  // Writes the trailer and closes the file. False with `error` set when a write failed; call once the run ended.
  bool finish(std::string &error);

private:
  SyncLogRecorder() = default;
  void flushLocked();

  std::mutex mutex_;
  std::FILE *file_ = nullptr;
  std::string path_;
  std::vector<uint8_t> buffer_;
  uint64_t count_ = 0;
  bool failed_ = false;
};

// Writes a log with no events (a module with no tasks has no order to record).
bool writeEmptySyncLog(const std::string &path, uint64_t fingerprint, std::string &error);

// The sync log of a run of a module that spawns no tasks: a record is written with no events and a replay only checks
// that its file belongs to the module, since there is no order to follow. True when there is nothing to do or it went
// well.
bool handleTaskFreeSyncLog(const IrModule &module, const VmSyncLogPaths *paths, std::string &error);

// Reads the log at `path` and checks it belongs to `fingerprint`. The diagnostics name the file and say what is wrong
// (not a log, an unknown version, truncated, another program).
bool readSyncLog(const std::string &path,
                 uint64_t fingerprint,
                 std::vector<SyncEvent> &events,
                 std::string &error);

} // namespace primec::vm_detail
