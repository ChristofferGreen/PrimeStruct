#include "WasmEmitterInternal.h"

#include <cstdint>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpI32Load8U = 0x2d;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI32LtU = 0x49;
constexpr uint8_t WasmValueTypeI32 = 0x7f;

// One WASI errno and the POSIX number the VM reports for the same condition. The numbers in the second column are the
// ones Linux and macOS share; an errno whose number differs between them (EAGAIN, ENAMETOOLONG, ELOOP, ENOTEMPTY,
// ENOSYS, EDQUOT, ENOTSUP, ESTALE) and the WASI-only ENOTCAPABLE are not mapped and pass through.
struct WasiErrnoMapping {
  uint8_t wasi;
  uint8_t posix;
};

constexpr WasiErrnoMapping WasiErrnoMappings[] = {
    {1, 7},    // E2BIG
    {2, 13},   // EACCES
    {8, 9},    // EBADF
    {10, 16},  // EBUSY
    {12, 10},  // ECHILD
    {18, 33},  // EDOM
    {20, 17},  // EEXIST
    {21, 14},  // EFAULT
    {22, 27},  // EFBIG
    {27, 4},   // EINTR
    {28, 22},  // EINVAL
    {29, 5},   // EIO
    {31, 21},  // EISDIR
    {33, 24},  // EMFILE
    {34, 31},  // EMLINK
    {41, 23},  // ENFILE
    {43, 19},  // ENODEV
    {44, 2},   // ENOENT
    {45, 8},   // ENOEXEC
    {48, 12},  // ENOMEM
    {51, 28},  // ENOSPC
    {54, 20},  // ENOTDIR
    {59, 25},  // ENOTTY
    {60, 6},   // ENXIO
    {63, 1},   // EPERM
    {64, 32},  // EPIPE
    {68, 34},  // ERANGE
    {69, 30},  // EROFS
    {70, 29},  // ESPIPE
    {71, 3},   // ESRCH
    {74, 26},  // ETXTBSY
    {75, 18},  // EXDEV
};

// The WASI errno numbers run 0 to 76 (`ENOTCAPABLE`).
constexpr size_t WasiErrnoCount = 77;

} // namespace

const std::vector<uint8_t> &wasiErrnoTable() {
  static const std::vector<uint8_t> table = [] {
    std::vector<uint8_t> entries(WasiErrnoCount);
    for (size_t i = 0; i < WasiErrnoCount; ++i) {
      entries[i] = static_cast<uint8_t>(i);
    }
    for (const WasiErrnoMapping &mapping : WasiErrnoMappings) {
      entries[mapping.wasi] = mapping.posix;
    }
    return entries;
  }();
  return table;
}

void emitMapWasiErrno(uint32_t scratchLocal, const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalSet);
  appendU32Leb(scratchLocal, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(scratchLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasiErrnoCount), out);
  out.push_back(WasmOpI32LtU);
  out.push_back(WasmOpIf);
  out.push_back(WasmValueTypeI32);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(scratchLocal, out);
  out.push_back(WasmOpI32Load8U);
  appendI8MemArg(runtime.errnoTableAddr, out);
  out.push_back(WasmOpElse);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(scratchLocal, out);
  out.push_back(WasmOpEnd);
}

} // namespace primec
