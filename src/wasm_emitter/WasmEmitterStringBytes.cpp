#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpI32Load = 0x28;
constexpr uint8_t WasmOpI32Load8U = 0x2d;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI32Shl = 0x74;
constexpr uint8_t WasmOpI64ExtendI32U = 0xad;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI64GeU = 0x5a;
constexpr uint8_t WasmOpI32WrapI64 = 0xa7;

} // namespace

void emitWasmStringByteLoad(uint32_t positionLocal,
                            uint32_t pointer,
                            uint32_t length,
                            const WasmRuntimeContext &runtime,
                            std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalSet);
  appendU32Leb(positionLocal, out);
  // The VM compares the position as an unsigned 64-bit value, so a negative `i32` position is past the end too.
  out.push_back(WasmOpLocalGet);
  appendU32Leb(positionLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(length), out);
  out.push_back(WasmOpI64GeU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  emitWasmRuntimeFault(
      runtime, runtime.stringByteMessageAddr, runtime.stringByteMessageLength, out);
  out.push_back(WasmOpEnd);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(positionLocal, out);
  out.push_back(WasmOpI32WrapI64);
  out.push_back(WasmOpI32Load8U);
  appendU32Leb(0, out);
  appendU32Leb(pointer, out);
}

void emitWasmStringLengthLoad(uint32_t indexLocal,
                              uint32_t count,
                              const WasmRuntimeContext &runtime,
                              std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalSet);
  appendU32Leb(indexLocal, out);
  // The VM compares the index as an unsigned 64-bit value, so a negative index is past the table too.
  out.push_back(WasmOpLocalGet);
  appendU32Leb(indexLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(count), out);
  out.push_back(WasmOpI64GeU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  emitWasmRuntimeFault(
      runtime, runtime.stringIndexMessageAddr, runtime.stringIndexMessageLength, out);
  out.push_back(WasmOpEnd);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(indexLocal, out);
  out.push_back(WasmOpI32WrapI64);
  out.push_back(WasmOpI32Const);
  appendS32Leb(2, out);
  out.push_back(WasmOpI32Shl);
  out.push_back(WasmOpI32Load);
  appendU32Leb(0, out);
  appendU32Leb(runtime.stringLengthTableAddr, out);
  out.push_back(WasmOpI64ExtendI32U);
}

} // namespace primec
