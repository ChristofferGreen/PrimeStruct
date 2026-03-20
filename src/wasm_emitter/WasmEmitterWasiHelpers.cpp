#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmBlockTypeVoid = 0x40;
constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpUnreachable = 0x00;
constexpr uint8_t WasmOpCall = 0x10;
constexpr uint8_t WasmOpDrop = 0x1a;
constexpr uint8_t WasmOpBlock = 0x02;
constexpr uint8_t WasmOpLoop = 0x03;
constexpr uint8_t WasmOpBr = 0x0c;
constexpr uint8_t WasmOpBrIf = 0x0d;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpLocalTee = 0x22;
constexpr uint8_t WasmOpI32Store = 0x36;
constexpr uint8_t WasmOpI32Store8 = 0x3a;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpI64Eqz = 0x50;
constexpr uint8_t WasmOpI64Eq = 0x51;
constexpr uint8_t WasmOpI64LtS = 0x53;
constexpr uint8_t WasmOpI64GeU = 0x5a;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Sub = 0x6b;
constexpr uint8_t WasmOpI64Sub = 0x7d;
constexpr uint8_t WasmOpI64DivU = 0x80;
constexpr uint8_t WasmOpI64RemU = 0x82;
constexpr uint8_t WasmOpI32WrapI64 = 0xa7;

constexpr uint32_t WasiPreopenDirFd = 3u;
constexpr uint32_t WasmDecimalBase = 10u;
constexpr uint32_t WasmAsciiZero = 48u;
constexpr uint32_t WasmAsciiMinus = 45u;

void appendI32MemArg(uint32_t offset, std::vector<uint8_t> &out) {
  appendU32Leb(2, out);
  appendU32Leb(offset, out);
}

} // namespace

void emitWasiWritePtrLen(uint32_t fd,
                         uint32_t ptrLocal,
                         uint32_t lenLocal,
                         const WasmRuntimeContext &runtime,
                         std::vector<uint8_t> &out,
                         bool dropResult) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
  if (dropResult) {
    out.push_back(WasmOpDrop);
  }
}

void emitWasiWriteLiteral(uint32_t fd,
                          uint32_t ptr,
                          uint32_t len,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          bool dropResult) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(ptr), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(len), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
  if (dropResult) {
    out.push_back(WasmOpDrop);
  }
}

void emitWasiWriteLiteralFromFdLocal(uint32_t fdLocal,
                                     uint32_t ptr,
                                     uint32_t len,
                                     const WasmRuntimeContext &runtime,
                                     std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(ptr), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(len), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(fdLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
}

void emitWasiWritePtrLenFromFdLocal(uint32_t fdLocal,
                                    uint32_t ptrLocal,
                                    uint32_t lenLocal,
                                    const WasmRuntimeContext &runtime,
                                    std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(fdLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
}

void emitWasiReadByteFromFdLocal(uint32_t fdLocal,
                                 const WasmRuntimeContext &runtime,
                                 std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.byteScratchAddr), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(fdLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdReadImportIndex, out);
}

void emitWasiWriteDecimalFromI64Local(uint32_t fdLocal,
                                      uint32_t valueLocal,
                                      uint32_t remLocal,
                                      uint32_t ptrLocal,
                                      uint32_t lenLocal,
                                      uint32_t negLocal,
                                      bool signedValue,
                                      const WasmRuntimeContext &runtime,
                                      std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.numericScratchAddr + runtime.numericScratchBytes), out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(ptrLocal, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(lenLocal, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(negLocal, out);

  if (signedValue) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
    out.push_back(WasmOpI64LtS);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(negLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
    out.push_back(WasmOpLocalGet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpI64Sub);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpEnd);
  }

  out.push_back(WasmOpBlock);
  out.push_back(WasmBlockTypeVoid);
  out.push_back(WasmOpLoop);
  out.push_back(WasmBlockTypeVoid);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(WasmDecimalBase), out);
  out.push_back(WasmOpI64RemU);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(remLocal, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Sub);
  out.push_back(WasmOpLocalTee);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(remLocal, out);
  out.push_back(WasmOpI32WrapI64);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasmAsciiZero), out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpI32Store8);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(lenLocal, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(WasmDecimalBase), out);
  out.push_back(WasmOpI64DivU);
  out.push_back(WasmOpLocalTee);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Eqz);
  out.push_back(WasmOpI32Eqz);
  out.push_back(WasmOpBrIf);
  appendU32Leb(0, out);

  out.push_back(WasmOpEnd);
  out.push_back(WasmOpEnd);

  if (signedValue) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(negLocal, out);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);

    out.push_back(WasmOpLocalGet);
    appendU32Leb(ptrLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpI32Sub);
    out.push_back(WasmOpLocalTee);
    appendU32Leb(ptrLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(static_cast<int32_t>(WasmAsciiMinus), out);
    out.push_back(WasmOpI32Store8);
    appendI32MemArg(0, out);

    out.push_back(WasmOpLocalGet);
    appendU32Leb(lenLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpI32Add);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(lenLocal, out);
    out.push_back(WasmOpEnd);
  }

  emitWasiWritePtrLenFromFdLocal(fdLocal, ptrLocal, lenLocal, runtime, out);
}

void emitWasiWriteDynamicStringFromI64Local(uint32_t fdLocal,
                                            uint32_t stringIndexLocal,
                                            const WasmRuntimeContext &runtime,
                                            std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(stringIndexLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(runtime.stringPtrs.size()), out);
  out.push_back(WasmOpI64GeU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  out.push_back(WasmOpUnreachable);
  out.push_back(WasmOpEnd);

  out.push_back(WasmOpBlock);
  out.push_back(WasmBlockTypeVoid);
  for (size_t stringIndex = 0; stringIndex < runtime.stringPtrs.size(); ++stringIndex) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(stringIndexLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(static_cast<int64_t>(stringIndex), out);
    out.push_back(WasmOpI64Eq);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);
    emitWasiWriteLiteralFromFdLocal(fdLocal,
                                    runtime.stringPtrs[stringIndex],
                                    runtime.stringLens[stringIndex],
                                    runtime,
                                    out);
    out.push_back(WasmOpDrop);
    out.push_back(WasmOpBr);
    appendU32Leb(1, out);
    out.push_back(WasmOpEnd);
  }
  out.push_back(WasmOpUnreachable);
  out.push_back(WasmOpEnd);
}

void emitWasiPathOpenDynamicFromI64Local(uint32_t stringIndexLocal,
                                         uint32_t errorLocal,
                                         uint32_t oflags,
                                         uint64_t rightsBase,
                                         uint32_t fdflags,
                                         const WasmRuntimeContext &runtime,
                                         std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(stringIndexLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(runtime.stringPtrs.size()), out);
  out.push_back(WasmOpI64GeU);
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  out.push_back(WasmOpUnreachable);
  out.push_back(WasmOpEnd);

  out.push_back(WasmOpBlock);
  out.push_back(WasmBlockTypeVoid);
  for (size_t stringIndex = 0; stringIndex < runtime.stringPtrs.size(); ++stringIndex) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(stringIndexLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(static_cast<int64_t>(stringIndex), out);
    out.push_back(WasmOpI64Eq);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);
    emitWasiPathOpen(runtime.stringPtrs[stringIndex],
                     runtime.stringLens[stringIndex],
                     oflags,
                     rightsBase,
                     fdflags,
                     runtime,
                     out);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(errorLocal, out);
    out.push_back(WasmOpBr);
    appendU32Leb(1, out);
    out.push_back(WasmOpEnd);
  }
  out.push_back(WasmOpUnreachable);
  out.push_back(WasmOpEnd);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(errorLocal, out);
}

void emitWasiPathOpen(uint32_t pathPtr,
                      uint32_t pathLen,
                      uint32_t oflags,
                      uint64_t rightsBase,
                      uint32_t fdflags,
                      const WasmRuntimeContext &runtime,
                      std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasiPreopenDirFd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(pathPtr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(pathLen), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(oflags), out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(rightsBase), out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(0, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fdflags), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.openedFdAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.pathOpenImportIndex, out);
}

} // namespace primec
