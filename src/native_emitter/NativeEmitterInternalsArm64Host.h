#pragma once

// Calls of imported C functions (CallHost): the arguments are popped into the
// AAPCS64 argument registers by their host kinds, the function is called through its dyld-bound pointer
// slot (loaded by an ADRP/LDR pair the image builder patches once the data segment's address is known),
// and the result is pushed. The callee keeps x19-x28, which hold the emitter's state (argc, argv, the
// frame and operand stack pointers and the value-stack cache); sp is 16-byte aligned between instructions.

inline size_t Arm64Emitter::emitHostSlotLoad(uint8_t reg) {
  const size_t index = currentWordIndex();
  emit(encodeAdrp(reg, 0));
  emit(encodeLdrRegBase(reg, reg, 0));
  return index;
}

// The text a host returned (x0, a NUL-terminated C string) copied into a fresh string-heap block; the block
// address is pushed (0 for a null or empty string, or when no block can be mapped).
inline void Arm64Emitter::emitHostTextResult() {
  emitMovReg(13, 0);
  emitMovImm64(8, 0);
  emitCompareRegZero(13);
  const size_t jumpNull = emitCondBranchPlaceholder(CondCode::Eq);
  emitMovImm64(14, 0);
  const size_t lengthLoop = currentWordIndex();
  emitAddReg(1, 13, 14);
  emit(encodeLdrbRegBase(2, 1, 0));
  emitCompareRegZero(2);
  const size_t jumpLengthEnd = emitCondBranchPlaceholder(CondCode::Eq);
  emitAddRegImm(14, 14, 1);
  const size_t jumpLengthBack = emitJumpPlaceholder();
  patchJump(jumpLengthBack, static_cast<int32_t>(lengthLoop - jumpLengthBack));
  const size_t lengthEnd = currentWordIndex();
  patchCondBranch(jumpLengthEnd, static_cast<int32_t>(lengthEnd - jumpLengthEnd), CondCode::Eq);
  emitCompareRegZero(14);
  const size_t jumpEmpty = emitCondBranchPlaceholder(CondCode::Eq);
  emitAddRegImm(0, 14, 1);
  emitPushReg(0);
  emitTextAlloc();
  emitPopReg(8);
  emitCompareRegZero(8);
  const size_t jumpNoBlock = emitCondBranchPlaceholder(CondCode::Eq);
  emitMovImm64(9, 0);
  const size_t copyLoop = currentWordIndex();
  emitCompareReg(9, 14);
  const size_t jumpCopied = emitCondBranchPlaceholder(CondCode::Hs);
  emitAddReg(10, 13, 9);
  emit(encodeLdrbRegBase(11, 10, 0));
  emitAddReg(12, 8, 9);
  emit(encodeStrbRegBase(11, 12, 0));
  emitAddRegImm(9, 9, 1);
  const size_t jumpCopyBack = emitJumpPlaceholder();
  patchJump(jumpCopyBack, static_cast<int32_t>(copyLoop - jumpCopyBack));
  const size_t done = currentWordIndex();
  patchCondBranch(jumpNull, static_cast<int32_t>(done - jumpNull), CondCode::Eq);
  patchCondBranch(jumpEmpty, static_cast<int32_t>(done - jumpEmpty), CondCode::Eq);
  patchCondBranch(jumpNoBlock, static_cast<int32_t>(done - jumpNoBlock), CondCode::Eq);
  patchCondBranch(jumpCopied, static_cast<int32_t>(done - jumpCopied), CondCode::Hs);
  emitPushReg(8);
}

inline bool Arm64Emitter::emitHostCall(const HostCallSite &site, std::string &error) {
  const std::vector<IrHostValueKind> &parameters = *site.parameters;
  std::vector<uint8_t> registerOf(parameters.size(), 0);
  uint8_t integerRegisters = 0;
  uint8_t floatRegisters = 0;
  for (size_t i = 0; i < parameters.size(); ++i) {
    const IrHostValueKind kind = parameters[i];
    if (kind == IrHostValueKind::Void) {
      error = "native backend found a void host parameter";
      return false;
    }
    const bool isFloat = kind == IrHostValueKind::F32 || kind == IrHostValueKind::F64;
    uint8_t &next = isFloat ? floatRegisters : integerRegisters;
    if (next >= 8) {
      error = "native backend host calls take at most 8 integer and 8 floating-point arguments";
      return false;
    }
    registerOf[i] = next++;
  }
  if (site.returnKind == IrHostValueKind::String) {
    error = "native backend host calls cannot return a string literal index";
    return false;
  }

  flushValueStackCache();
  // The last argument is on top of the stack.
  for (size_t i = parameters.size(); i-- > 0;) {
    const uint8_t reg = registerOf[i];
    switch (parameters[i]) {
    case IrHostValueKind::F32:
      emitPopReg(15);
      emit(0x1E270000u | (15u << 5) | reg); // FMOV S<reg>, W15
      break;
    case IrHostValueKind::F64:
      emitPopReg(15);
      emit(0x9E670000u | (15u << 5) | reg); // FMOV D<reg>, X15
      break;
    case IrHostValueKind::String: {
      // A string-table index: the address of its NUL-terminated text. The table address needs x1, which an
      // earlier argument may hold, so x1 waits in x14.
      emitPopReg(15);
      emitMovReg(14, 1);
      site.stringTableFixups->push_back(emitAdrPlaceholder(1));
      emitMovImm64(10, site.stringTableDelta);
      emitSubReg(10, 1, 10);
      emitMovImm64(11, 8);
      emitMulReg(11, 15, 11);
      emitAddReg(12, 1, 11);
      emit(encodeLdrRegBase(12, 12, 0));
      emitAddReg(13, 10, 12);
      emitMovReg(1, 14);
      emitMovReg(reg, 13);
      break;
    }
    default:
      emitPopReg(reg);
      break;
    }
  }

  *site.slotFixup = emitHostSlotLoad(16);
  emit(0xD63F0200u); // BLR x16

  switch (site.returnKind) {
  case IrHostValueKind::Void:
    break;
  case IrHostValueKind::I32:
    emit(0x93407C00u); // SXTW x0, w0
    emitPushReg(0);
    break;
  case IrHostValueKind::Bool:
    emit(0x53001C00u); // UXTB w0, w0
    emitCompareRegZero(0);
    emit(0x9A9F07E0u); // CSET x0, NE
    emitPushReg(0);
    break;
  case IrHostValueKind::U8:
    emit(0x53001C00u); // UXTB w0, w0
    emitPushReg(0);
    break;
  case IrHostValueKind::F32:
    emit(0x1E260000u); // FMOV W0, S0
    emitPushReg(0);
    break;
  case IrHostValueKind::F64:
    emit(0x9E660000u); // FMOV X0, D0
    emitPushReg(0);
    break;
  case IrHostValueKind::Text:
    emitHostTextResult();
    break;
  default: // I64, U64
    emitPushReg(0);
    break;
  }
  return true;
}
