#pragma once

// Calls through function addresses on arm64 (docs/spec/vm-design.md, Function addresses). `FunctionAddress f` is the
// constant `f + 1`. A `CallIndirect p` takes the address as the deepest of `p + 1` value stack slots: the code moves
// the address into x16, shifts the `p` parameters up over its slot, puts `p` in x17 and calls the dispatch routine
// with `bl`. The routine (emitted once after the functions) turns the address into an index, faults with exit code 3
// when it names no emitted function or a function that takes another parameter count than x17, and jumps with `br`
// through a table that follows it (one doubleword per function: the entry offset, then the parameter count), so the
// callee returns to the call site (the link register is not touched) and finds its parameters on the value stack as
// after a `Call`. The routine borrows x15 and gives it back before the jump.

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
  emit(encodeMovz(ScratchRegister, static_cast<uint16_t>(parameterCount), 0));
  indirectCallSites_.push_back(currentWordIndex());
  emit(encodeBl(0));
  if (returnsValue) {
    emitPushReg0();
  }
  return true;
}

// Emits the dispatch routine and its table (for each function: the entry offset from the table, as a word index of
// `functionOffsets`, and the parameter count of `parameterCounts`; a function that is not emitted, `emitted[i]`, gets
// offset 0 and a count no call passes), and points every call site at the routine.
inline void Arm64Emitter::emitIndirectCallRuntime(const std::vector<size_t> &functionOffsets,
                                                  const std::vector<uint32_t> &parameterCounts,
                                                  const std::vector<bool> &emitted) {
  using namespace arm64_indirect;
  constexpr uint32_t BrBase = 0xD61F0000u;
  constexpr uint32_t LsrBy32 = 0xD360FC00u;        // ubfm xd, xn, #32, #63
  constexpr uint32_t SignExtendWord = 0x93407C00u; // sbfm xd, xn, #0, #31
  constexpr uint8_t HighOrSame = 2;
  constexpr uint8_t NotEqual = 1;
  constexpr uint8_t Zr = 31;
  constexpr uint8_t Borrowed = 15;
  constexpr uint32_t StrBorrowedPreIndex = 0xF81F0FEFu;  // str x15, [sp, #-16]!
  constexpr uint32_t LdrBorrowedPostIndex = 0xF84107EFu; // ldr x15, [sp], #16
  constexpr uint32_t NoCall = 0xFFFFFFFFu;
  const uint8_t address = AddressRegister;
  const uint8_t count = ScratchRegister;
  const size_t start = currentWordIndex();
  struct FaultBranch {
    size_t word;
    bool compareZero;
    uint8_t condition;
    uint8_t reg;
  };
  std::vector<FaultBranch> faultBranches;
  const auto branchToFault = [&](uint8_t condition, bool compareZero, uint8_t reg) {
    faultBranches.push_back({currentWordIndex(), compareZero, condition, reg});
    emit(0);
  };
  emit(StrBorrowedPreIndex);
  // Address 0 wraps to the largest index, so one unsigned compare rejects it with the addresses past the table.
  emit(encodeSubRegImm(address, address, 1));
  emitMovImm64(Borrowed, functionOffsets.size());
  emit(encodeSubsReg(Zr, address, Borrowed));
  branchToFault(HighOrSame, false, 0);
  for (int doubling = 0; doubling < 3; ++doubling) {
    emit(encodeAddReg(address, address, address));
  }
  const size_t firstTableLoad = currentWordIndex();
  emit(0);
  emit(encodeAddReg(Borrowed, Borrowed, address));
  emit(encodeLdrRegBase(address, Borrowed, 0));
  emit(LsrBy32 | (static_cast<uint32_t>(address) << 5) | Borrowed);
  emit(encodeSubsReg(Zr, Borrowed, count));
  branchToFault(NotEqual, false, 0);
  emit(SignExtendWord | (static_cast<uint32_t>(address) << 5) | address);
  branchToFault(0, true, address);
  const size_t secondTableLoad = currentWordIndex();
  emit(0);
  emit(encodeAddReg(address, address, Borrowed));
  emit(LdrBorrowedPostIndex);
  emit(BrBase | (static_cast<uint32_t>(address) << 5));
  const size_t fault = currentWordIndex();
  emitTrap(static_cast<int32_t>(RuntimeErrorExitCode));
  const size_t table = currentWordIndex();
  for (size_t index = 0; index < functionOffsets.size(); ++index) {
    const int64_t bytes =
        emitted[index]
            ? (static_cast<int64_t>(functionOffsets[index]) - static_cast<int64_t>(table)) * 4
            : 0;
    emit(static_cast<uint32_t>(static_cast<int32_t>(bytes)));
    emit(emitted[index] ? parameterCounts[index] : NoCall);
  }
  for (const FaultBranch &branch : faultBranches) {
    const int32_t offsetWords = static_cast<int32_t>(fault) - static_cast<int32_t>(branch.word);
    patchWord(branch.word,
              branch.compareZero ? encodeCbz(branch.reg, offsetWords)
                                 : encodeBCond(offsetWords, branch.condition));
  }
  patchWord(firstTableLoad,
            encodeAdr(Borrowed, static_cast<int32_t>((table - firstTableLoad) * 4)));
  patchWord(secondTableLoad,
            encodeAdr(Borrowed, static_cast<int32_t>((table - secondTableLoad) * 4)));
  for (const size_t site : indirectCallSites_) {
    patchCall(site, static_cast<int32_t>(start) - static_cast<int32_t>(site));
  }
}
