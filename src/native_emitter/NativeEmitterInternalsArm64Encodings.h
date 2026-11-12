#pragma once

// Instruction encodings and layout constants of the concurrency support of the arm64 emitter: the task block and
// the thread entry (arm64_tasks), the atomic instructions (arm64_atomics), the per-thread allocator states
// (arm64_theap) and the wait state of locks and conditions (arm64_sync). Included before the heap routines, which use
// them.

namespace arm64_tasks {

constexpr uint64_t BlockResult = 0;
constexpr uint64_t BlockThread = 8;
constexpr uint64_t BlockArgc = 16;
constexpr uint64_t BlockArgv = 24;
constexpr uint64_t BlockDone =
    32; // set with the gate held when the thread's function has returned (synchronized)
constexpr uint64_t BlockArguments = 40;
constexpr uint64_t ThreadStackBytes = 8ull * 1024ull * 1024ull;
constexpr uint32_t SubSpValueStack =
    0xD14403FFu; // SUB sp, sp, #256, lsl #12 (the 1 MiB value stack)
constexpr uint32_t AddSpValueStack = 0x914403FFu; // ADD sp, sp, #256, lsl #12
constexpr uint64_t SavedRegisterBytes = 112; // x19-x30, then the block pointer, 16-byte aligned

inline uint32_t stpOffset(uint8_t rt, uint8_t rt2, uint8_t rn, uint32_t offsetBytes) {
  return 0xA9000000u | (((offsetBytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t ldpOffset(uint8_t rt, uint8_t rt2, uint8_t rn, uint32_t offsetBytes) {
  return 0xA9400000u | (((offsetBytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t
stpPreIndexSp(uint8_t rt, uint8_t rt2, uint32_t bytes) { // STP rt, rt2, [sp, #-bytes]!
  return 0xA9800000u | ((static_cast<uint32_t>(-static_cast<int32_t>(bytes) / 8) & 0x7Fu) << 15) |
         (static_cast<uint32_t>(rt2) << 10) | (31u << 5) | rt;
}
inline uint32_t
ldpPostIndexSp(uint8_t rt, uint8_t rt2, uint32_t bytes) { // LDP rt, rt2, [sp], #bytes
  return 0xA8C00000u | (((bytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (31u << 5) | rt;
}
inline uint32_t casalW(uint8_t compare, uint8_t desired, uint8_t address) {
  return 0x88E0FC00u | (static_cast<uint32_t>(compare) << 16) |
         (static_cast<uint32_t>(address) << 5) | desired;
}
inline uint32_t stlrXzr(uint8_t address) { // STLR xzr, [Xn]
  return 0xC89FFC1Fu | (static_cast<uint32_t>(address) << 5);
}
inline uint32_t movSpToReg(uint8_t rd) { // ADD Xd, sp, #0
  return 0x910003E0u | rd;
}
inline uint32_t addSpImm(uint16_t imm) { // ADD sp, sp, #imm
  return 0x910003FFu | (static_cast<uint32_t>(imm & 0xFFFu) << 10);
}
inline uint32_t subSpImm(uint16_t imm) { // SUB sp, sp, #imm
  return 0xD10003FFu | (static_cast<uint32_t>(imm & 0xFFFu) << 10);
}
inline uint32_t cbnzW(uint8_t rt, int32_t imm19) { // CBNZ Wt
  return 0x35000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | rt;
}

} // namespace arm64_tasks

namespace arm64_text {

// The string heap's counters in the data segment's state, after the allocator state and the lock and wait state of a
// threaded or synchronized module: the memory limit (0 for none), the bytes of live text blocks, the blocks handed
// out, the live blocks above the VM's large threshold and their bytes, and the pool chunks mapped.
constexpr uint64_t StatOffset = HeapStateBytes + 192;
constexpr uint64_t LimitOffset = StatOffset;
constexpr uint64_t HeldOffset = StatOffset + 8;
constexpr uint64_t AllocationsOffset = StatOffset + 16;
constexpr uint64_t LargeBlocksOffset = StatOffset + 24;
constexpr uint64_t LargeBytesOffset = StatOffset + 32;
constexpr uint64_t ChunksOffset = StatOffset + 40;
constexpr uint64_t StatBytes = 48;
constexpr uint64_t LargeThreshold = 64 * 1024;

} // namespace arm64_text

namespace arm64_theap {

constexpr uint64_t RemoteHeadOffset = HeapStateBytes;     // in every allocator state
constexpr uint64_t PoolLinkOffset = HeapStateBytes + 8;   // in every allocator state
constexpr uint64_t LockOffset = HeapStateBytes + 16;      // in the data segment's state only
constexpr uint64_t KeyOffset = HeapStateBytes + 24;       // the pthread key (a 64-bit word)
constexpr uint64_t MainTokenOffset = HeapStateBytes + 32; // TPIDRRO_EL0 of the main thread
constexpr uint64_t PoolHeadOffset = HeapStateBytes + 40;
constexpr uint64_t RuntimeBytes = 48; // what a threaded or synchronized module adds
constexpr uint64_t StatePageBytes = 4096;
constexpr uint8_t ChunkShift = 18; // HeapPoolChunkBytes = 1 << ChunkShift

static_assert(HeapPoolChunkBytes == (1ull << ChunkShift), "chunks are aligned to their size");

} // namespace arm64_theap

namespace arm64_atomics {

inline uint32_t ldar(uint8_t rt, uint8_t rn) {
  return 0xC8DFFC00u | (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t stlr(uint8_t rt, uint8_t rn) {
  return 0xC89FFC00u | (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t casal(uint8_t compare, uint8_t desired, uint8_t address) {
  return 0xC8E0FC00u | (static_cast<uint32_t>(compare) << 16) |
         (static_cast<uint32_t>(address) << 5) | desired;
}
inline uint32_t swpal(uint8_t value, uint8_t old, uint8_t address) {
  return 0xF8E08000u | (static_cast<uint32_t>(value) << 16) |
         (static_cast<uint32_t>(address) << 5) | old;
}
inline uint32_t ldaddal(uint8_t delta, uint8_t old, uint8_t address) {
  return 0xF8E00000u | (static_cast<uint32_t>(delta) << 16) |
         (static_cast<uint32_t>(address) << 5) | old;
}
inline uint32_t sxtw(uint8_t rd, uint8_t rn) {
  return 0x93407C00u | (static_cast<uint32_t>(rn) << 5) | rd;
}

} // namespace arm64_atomics

namespace arm64_sync {

constexpr uint64_t GateOffset = 0;
constexpr uint64_t ChangedOffset = 64;
constexpr uint64_t SleepersOffset = 112;
constexpr uint64_t LiveOffset = 120;
constexpr uint64_t BlockedOffset = 128;
constexpr uint64_t RoundOffset = 136;
constexpr uint64_t MutexSignature = 0x32AAABA7ull; // _PTHREAD_MUTEX_SIG_init
constexpr uint64_t CondSignature = 0x3CB0B1BBull;  // _PTHREAD_COND_SIG_init
constexpr uint32_t MrsThreadPointer = 0xD53BD060u; // MRS x0, TPIDRRO_EL0
constexpr uint32_t AndClearLowBits = 0x927DF000u;  // AND x0, x0, #~7 (rn and rd in bits 5 and 0)

} // namespace arm64_sync
