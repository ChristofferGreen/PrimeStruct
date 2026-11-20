#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpUnreachable = 0x00;
constexpr uint8_t WasmOpCall = 0x10;
constexpr uint8_t WasmOpDrop = 0x1a;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpI32Store = 0x36;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpI64Eqz = 0x50;
constexpr uint8_t WasmOpI32DivS = 0x6d;
constexpr uint8_t WasmOpI64DivS = 0x7f;
constexpr uint8_t WasmOpI64DivU = 0x80;
constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint32_t StderrFd = 2;
constexpr int32_t RuntimeErrorExitCode = 3;

void appendI32Const(int32_t value, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(value, out);
}

void appendStoreI32(uint32_t address, uint32_t value, std::vector<uint8_t> &out) {
  appendI32Const(static_cast<int32_t>(address), out);
  appendI32Const(static_cast<int32_t>(value), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);
}

// The runtime-error sequence of the WASI profile: the VM's message to stderr, then `proc_exit(3)`.
void appendDivisionFault(const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  appendStoreI32(runtime.iovAddr, runtime.divisionMessageAddr, out);
  appendStoreI32(runtime.iovAddr + 4, runtime.divisionMessageLength, out);
  appendI32Const(static_cast<int32_t>(StderrFd), out);
  appendI32Const(static_cast<int32_t>(runtime.iovAddr), out);
  appendI32Const(1, out);
  appendI32Const(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
  out.push_back(WasmOpDrop);
  appendI32Const(RuntimeErrorExitCode, out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.procExitImportIndex, out);
  out.push_back(WasmOpUnreachable);
}

} // namespace

WasmCodeBody buildWasmDivisionHelperBody(WasmDivisionOp kind, const WasmRuntimeContext &runtime) {
  WasmCodeBody body;
  appendU32Leb(0, body.localDecls);
  std::vector<uint8_t> &out = body.instructions;
  const bool is32 = kind == WasmDivisionOp::I32;
  out.push_back(WasmOpLocalGet);
  appendU32Leb(1, out);
  out.push_back(is32 ? WasmOpI32Eqz : WasmOpI64Eqz);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  appendDivisionFault(runtime, out);
  out.push_back(WasmOpEnd);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(0, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(1, out);
  out.push_back(is32 ? WasmOpI32DivS : kind == WasmDivisionOp::I64 ? WasmOpI64DivS : WasmOpI64DivU);
  out.push_back(WasmOpEnd);
  return body;
}

} // namespace primec
