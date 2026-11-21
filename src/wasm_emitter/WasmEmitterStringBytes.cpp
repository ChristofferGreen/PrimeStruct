#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpI32Load8U = 0x2d;
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

} // namespace primec
