#include "VmSyncLog.h"

#include "primec/ir/IrSerializer.h"

#include <cerrno>
#include <cstring>

namespace primec::vm_detail {

namespace {

constexpr char Magic[8] = {'P', 'S', 'S', 'Y', 'N', 'C', 'L', 'G'};
constexpr size_t HeaderBytes = 24;
constexpr size_t EventBytes = 8;
constexpr size_t TrailerBytes = EventBytes + 8;
constexpr size_t FlushBytes = size_t{1} << 16;

void putU32(std::vector<uint8_t> &out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>(value >> shift));
  }
}

void putU64(std::vector<uint8_t> &out, uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<uint8_t>(value >> shift));
  }
}

uint64_t getLe(const uint8_t *bytes, int count) {
  uint64_t value = 0;
  for (int i = 0; i < count; ++i) {
    value |= static_cast<uint64_t>(bytes[i]) << (8 * i);
  }
  return value;
}

void putHeader(std::vector<uint8_t> &out, uint64_t fingerprint) {
  out.insert(out.end(), Magic, Magic + sizeof(Magic));
  putU32(out, SyncLogVersion);
  putU32(out, 0);
  putU64(out, fingerprint);
}

void putTrailer(std::vector<uint8_t> &out, uint64_t count) {
  putU32(out, 0);
  putU32(out, 0);
  putU64(out, count);
}

} // namespace

SyncEventKind syncEventKindOf(IrOpcode op) {
  switch (op) {
  case IrOpcode::TaskSpawn:
    return SyncEventKind::Spawn;
  case IrOpcode::TaskWait:
    return SyncEventKind::Wait;
  case IrOpcode::MutexLock:
    return SyncEventKind::Lock;
  case IrOpcode::MutexUnlock:
    return SyncEventKind::Unlock;
  case IrOpcode::ConditionWait:
    return SyncEventKind::ConditionWait;
  case IrOpcode::ConditionNotify:
    return SyncEventKind::ConditionNotify;
  case IrOpcode::AtomicLoad:
    return SyncEventKind::AtomicLoad;
  case IrOpcode::AtomicStore:
    return SyncEventKind::AtomicStore;
  case IrOpcode::AtomicAdd:
    return SyncEventKind::AtomicAdd;
  case IrOpcode::AtomicExchange:
    return SyncEventKind::AtomicExchange;
  case IrOpcode::AtomicCompareExchange:
    return SyncEventKind::AtomicCompareExchange;
  default:
    return SyncEventKind::End;
  }
}

const char *syncEventKindName(SyncEventKind kind) {
  switch (kind) {
  case SyncEventKind::Spawn:
    return "spawn";
  case SyncEventKind::Wait:
    return "wait";
  case SyncEventKind::Lock:
    return "lock";
  case SyncEventKind::Unlock:
    return "unlock";
  case SyncEventKind::ConditionWait:
    return "condition wait";
  case SyncEventKind::ConditionNotify:
    return "condition notify";
  case SyncEventKind::AtomicLoad:
    return "atomic load";
  case SyncEventKind::AtomicStore:
    return "atomic store";
  case SyncEventKind::AtomicAdd:
    return "atomic add";
  case SyncEventKind::AtomicExchange:
    return "atomic exchange";
  case SyncEventKind::AtomicCompareExchange:
    return "atomic compare-exchange";
  default:
    return "end";
  }
}

bool fingerprintIrModule(const IrModule &module, uint64_t &fingerprint, std::string &error) {
  std::vector<uint8_t> bytes;
  if (!serializeIr(module, bytes, error)) {
    return false;
  }
  uint64_t hash = 0xCBF29CE484222325ull;
  for (const uint8_t byte : bytes) {
    hash = (hash ^ byte) * 0x100000001B3ull;
  }
  fingerprint = hash;
  return true;
}

std::unique_ptr<SyncLogRecorder>
SyncLogRecorder::open(const std::string &path, uint64_t fingerprint, std::string &error) {
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
    error = "cannot write the sync record " + path + ": " + std::strerror(errno);
    return nullptr;
  }
  std::unique_ptr<SyncLogRecorder> recorder(new SyncLogRecorder());
  recorder->file_ = file;
  recorder->path_ = path;
  recorder->buffer_.reserve(FlushBytes + EventBytes);
  putHeader(recorder->buffer_, fingerprint);
  return recorder;
}

SyncLogRecorder::~SyncLogRecorder() {
  if (file_ != nullptr) {
    std::fclose(file_);
  }
}

void SyncLogRecorder::note(uint64_t task, SyncEventKind kind) {
  std::lock_guard<std::mutex> lock(mutex_);
  noteLocked(task, kind);
}

void SyncLogRecorder::noteLocked(uint64_t task, SyncEventKind kind) {
  putU32(buffer_, static_cast<uint32_t>(task));
  buffer_.push_back(static_cast<uint8_t>(kind));
  buffer_.push_back(0);
  buffer_.push_back(0);
  buffer_.push_back(0);
  ++count_;
  if (buffer_.size() >= FlushBytes) {
    flushLocked();
  }
}

void SyncLogRecorder::flushLocked() {
  if (!buffer_.empty() && std::fwrite(buffer_.data(), 1, buffer_.size(), file_) != buffer_.size()) {
    failed_ = true;
  }
  buffer_.clear();
}

bool SyncLogRecorder::finish(std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  putTrailer(buffer_, count_);
  flushLocked();
  const bool closed = std::fclose(file_) == 0;
  file_ = nullptr;
  if (failed_ || !closed) {
    error = "cannot write the sync record " + path_;
    return false;
  }
  return true;
}

bool writeEmptySyncLog(const std::string &path, uint64_t fingerprint, std::string &error) {
  std::unique_ptr<SyncLogRecorder> recorder = SyncLogRecorder::open(path, fingerprint, error);
  return recorder != nullptr && recorder->finish(error);
}

bool handleTaskFreeSyncLog(const IrModule &module,
                           const VmSyncLogPaths *paths,
                           std::string &error) {
  if (paths == nullptr || (paths->record.empty() && paths->replay.empty())) {
    return true;
  }
  uint64_t fingerprint = 0;
  if (!fingerprintIrModule(module, fingerprint, error)) {
    return false;
  }
  if (!paths->record.empty()) {
    return writeEmptySyncLog(paths->record, fingerprint, error);
  }
  std::vector<SyncEvent> events;
  return readSyncLog(paths->replay, fingerprint, events, error);
}

bool readSyncLog(const std::string &path,
                 uint64_t fingerprint,
                 std::vector<SyncEvent> &events,
                 std::string &error) {
  events.clear();
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    error = "cannot read the sync record " + path + ": " + std::strerror(errno);
    return false;
  }
  std::vector<uint8_t> bytes;
  uint8_t block[4096];
  size_t got = 0;
  while ((got = std::fread(block, 1, sizeof(block), file)) > 0) {
    bytes.insert(bytes.end(), block, block + got);
  }
  std::fclose(file);
  const std::string name = "the sync record " + path;
  if (bytes.size() < HeaderBytes || std::memcmp(bytes.data(), Magic, sizeof(Magic)) != 0) {
    error = name + " is not a sync record";
    return false;
  }
  const uint32_t version = static_cast<uint32_t>(getLe(bytes.data() + 8, 4));
  if (version != SyncLogVersion) {
    error = name + " has format version " + std::to_string(version) + " (this VM reads version " +
            std::to_string(SyncLogVersion) + ")";
    return false;
  }
  if (getLe(bytes.data() + 16, 8) != fingerprint) {
    error =
        name +
        " was recorded from a different program (or the same program compiled with other options)";
    return false;
  }
  const size_t body = bytes.size() - HeaderBytes;
  if (body < TrailerBytes || (body - TrailerBytes) % EventBytes != 0) {
    error = name + " is truncated";
    return false;
  }
  const size_t count = (body - TrailerBytes) / EventBytes;
  const uint8_t *trailer = bytes.data() + HeaderBytes + count * EventBytes;
  if (trailer[4] != 0 || getLe(trailer + EventBytes, 8) != count) {
    error = name + " is truncated";
    return false;
  }
  events.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t *record = bytes.data() + HeaderBytes + i * EventBytes;
    const uint8_t kind = record[4];
    if (kind < static_cast<uint8_t>(SyncEventKind::Spawn) ||
        kind > static_cast<uint8_t>(SyncEventKind::AtomicCompareExchange)) {
      error = name + " holds an unknown event kind " + std::to_string(kind);
      return false;
    }
    events.push_back({static_cast<uint32_t>(getLe(record, 4)), static_cast<SyncEventKind>(kind)});
  }
  return true;
}

} // namespace primec::vm_detail
