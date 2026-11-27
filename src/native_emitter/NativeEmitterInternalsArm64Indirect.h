#pragma once

// Calls through function addresses on arm64 (docs/spec/vm-design.md, Function addresses). `FunctionAddress f` is the
// constant `f + 1`. A `CallIndirect p` takes the address as the deepest of `p + 1` value stack slots: the code moves
// the address into x16, shifts the `p` parameters up over its slot and calls the dispatch routine with `bl`. The
// routine (emitted once after the functions) turns the address into an index, faults with exit code 3 when it
// names no emitted function, and jumps with `br` through a table of entry offsets that follows it, so the callee
// returns to the call site (the link register is not touched) and finds its parameters on the value stack as after a
// `Call`. The callee's parameter count is not checked: a call that passes another count than the callee takes is a
// program the compiler does not produce (the VM faults on it).

namespace arm64_indirect {
inline constexpr uint8_t AddressRegister = 16;
inline constexpr uint8_t ScratchRegister = 17;
inline constexpr uint8_t ValueStackRegister = 28;
inline constexpr uint64_t RuntimeErrorExitCode = 3;
} // namespace arm64_indirect

inline bool Arm64Emitter::emitIndirectCall(uint64_t parameterCount, bool returnsValue, std::string &error) {
  using namespace arm64_indirect;
  // The slot offsets are the immediate of a load and a store (at most 4095 doublewords).
  if ((parameterCount + 1) * 16 > 4095 * 8) {
    error = "native backend: an indirect call passes too many parameters";
    return false;
  }
  flushValueStackCache();
  emit(encodeLdrRegBase(AddressRegister, ValueStackRegister, static_cast<uint16_t>(parameterCount * 16)));
  for (uint64_t slot = parameterCount; slot-- > 0;) {
    emit(encodeLdrRegBase(ScratchRegister, ValueStackRegister, static_cast<uint16_t>(slot * 16)));
    emit(encodeStrRegBase(ScratchRegister, ValueStackRegister, static_cast<uint16_t>((slot + 1) * 16)));
  }
  emit(encodeAddRegImm(ValueStackRegister, ValueStackRegister, 16));
  indirectCallSites_.push_back(currentWordIndex());
  emit(encodeBl(0));
  if (returnsValue) {
    emitPushReg0();
  }
  return true;
}

// Emits the dispatch routine and its table of `functionOffsets` (code word indices of the functions that are
// emitted: `emitted[i]`), and points every call site at the routine.
inline void Arm64Emitter::emitIndirectCallRuntime(const std::vector<size_t> &functionOffsets,
                                                  const std::vector<bool> &emitted) {
  using namespace arm64_indirect;
  constexpr uint32_t BrBase = 0xD61F0000u;
  constexpr uint32_t LdrswExtendedBase = 0xB8A07800u; // ldrsw xt, [xn, xm, lsl #2]
  constexpr uint8_t HighOrSame = 2;
  constexpr uint8_t Zr = 31;
  const uint8_t address = AddressRegister;
  const uint8_t scratch = ScratchRegister;
  const size_t start = currentWordIndex();
  // Address 0 wraps to the largest index, so one unsigned compare rejects it with the addresses past the table.
  emit(encodeSubRegImm(address, address, 1));
  emitMovImm64(scratch, functionOffsets.size());
  emit(encodeSubsReg(Zr, address, scratch));
  const size_t outOfRange = currentWordIndex();
  emit(0);
  const size_t tableLoad = currentWordIndex();
  emit(0);
  emit(LdrswExtendedBase | (static_cast<uint32_t>(address) << 16) | (static_cast<uint32_t>(scratch) << 5) | address);
  const size_t notEmitted = currentWordIndex();
  emit(0);
  emit(encodeAddReg(scratch, scratch, address));
  emit(BrBase | (static_cast<uint32_t>(scratch) << 5));
  const size_t fault = currentWordIndex();
  emitTrap(static_cast<int32_t>(RuntimeErrorExitCode));
  const size_t table = currentWordIndex();
  for (size_t index = 0; index < functionOffsets.size(); ++index) {
    const int64_t bytes = emitted[index] ? (static_cast<int64_t>(functionOffsets[index]) - static_cast<int64_t>(table)) * 4 : 0;
    emit(static_cast<uint32_t>(static_cast<int32_t>(bytes)));
  }
  patchWord(outOfRange, encodeBCond(static_cast<int32_t>(fault) - static_cast<int32_t>(outOfRange), HighOrSame));
  patchWord(tableLoad, encodeAdr(scratch, static_cast<int32_t>((table - tableLoad) * 4)));
  patchWord(notEmitted, encodeCbz(address, static_cast<int32_t>(fault) - static_cast<int32_t>(notEmitted)));
  for (const size_t site : indirectCallSites_) {
    patchCall(site, static_cast<int32_t>(start) - static_cast<int32_t>(site));
  }
}
