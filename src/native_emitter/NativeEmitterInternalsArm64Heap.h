#pragma once

// The arm64 slot heap (HeapAlloc, HeapAllocBytes, HeapRealloc, HeapFree): three routines emitted once after the
// program's functions, which every heap instruction calls with BL.
//
// A block keeps the 16-byte header it always had (a magic word, then the block's total bytes, header included), so
// realloc and the text helpers read the same layout. A block of up to HeapPoolMaxSlots slots, header included, is
// pooled: it is rounded up to a size class (exact up to 8 slots, then four classes per doubling) and carved from
// 256 KiB chunks mapped on demand, which blocks of every class share; a freed pooled block goes on its class's free
// list and the next allocation of that class takes it back, zeroed. A larger block has a mapping of its own, as every
// block had before, and is unmapped when freed. The allocator state (the chunk cursor and end, then one free-list head
// per class) lives in the writable data segment, at HeapStateBytes after the import slots.
//
// Calling convention: the routines read their arguments from x0-x2 and return in x0; they use x0-x17 (never x18,
// x19-x28 or sp), so the template code's state (frame, operand stack and value-stack cache registers) survives, and
// the caller's link register is already saved in its frame. A double free, or a realloc of a freed block, writes a
// message to stderr and exits with code 3, as the VM's heap faults do.

inline void Arm64Emitter::emitHeapAlloc() {
  emitPopReg(1);
  emitMovImm64(2, HeapHeaderMagic);
  heapAllocCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
  emitPushReg(0);
}

inline void Arm64Emitter::emitHeapAllocBytes() {
  emitPopReg(1);
  // Whole slots: (bytes + IrSlotBytes - 1) >> 4, so a count of 0 stays 0 (no allocation).
  static_assert(IrSlotBytes == 16, "emitHeapAllocBytes shifts by log2(IrSlotBytes)");
  emitAddRegImm(1, 1, static_cast<uint16_t>(IrSlotBytes - 1));
  emit(0xD344FC21u); // LSR x1, x1, #4
  emitMovImm64(2, HeapHeaderMagicBytes);
  heapAllocCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
  emitPushReg(0);
}

inline void Arm64Emitter::emitHeapFree() {
  emitPopReg(0);
  heapFreeCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitHeapRealloc() {
  emitPopReg(1);
  emitPopReg(0);
  heapReallocCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
  emitPushReg(0);
}

namespace arm64_heap {

inline uint32_t cmpImm(uint8_t rn, uint16_t imm) { // CMP Xn, #imm
  return 0xF100001Fu | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) |
         (static_cast<uint32_t>(rn) << 5);
}
inline uint32_t clz(uint8_t rd, uint8_t rn) {
  return 0xDAC01000u | (static_cast<uint32_t>(rn) << 5) | rd;
}
inline uint32_t lslv(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x9AC02000u | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd;
}
inline uint32_t lsrv(uint8_t rd, uint8_t rn, uint8_t rm) {
  return 0x9AC02400u | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd;
}
inline uint32_t
lslImm(uint8_t rd, uint8_t rn, uint8_t shift) { // UBFM Xd, Xn, #(64 - shift), #(63 - shift)
  return 0xD3400000u | (static_cast<uint32_t>((64 - shift) & 63) << 16) |
         (static_cast<uint32_t>(63 - shift) << 10) | (static_cast<uint32_t>(rn) << 5) | rd;
}
inline uint32_t lsrImm(uint8_t rd, uint8_t rn, uint8_t shift) { // UBFM Xd, Xn, #shift, #63
  return 0xD340FC00u | (static_cast<uint32_t>(shift) << 16) | (static_cast<uint32_t>(rn) << 5) | rd;
}
inline uint32_t cbnz(uint8_t rt, int32_t imm19) {
  return 0xB5000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | rt;
}
inline uint32_t stpZeroPostIndex16(uint8_t rn) { // STP XZR, XZR, [Xn], #16
  return 0xA8817C1Fu | (static_cast<uint32_t>(rn) << 5);
}

} // namespace arm64_heap

// x4 = the size class index and x5 = the class's slot count for x3 slots (2 to HeapPoolMaxSlots, header included):
// x3 itself up to 8 slots (classes 0-6), then four classes per doubling: for 2^b < x3 <= 2^(b+1), the smallest
// 2^b + q * 2^(b-2) (q = 1 to 4) that holds x3, class 4b - 6 + q. Uses x6-x8.
inline void Arm64Emitter::emitHeapPoolClass() {
  using namespace arm64_heap;
  emit(cmpImm(3, 8));
  const size_t jumpBig = currentWordIndex();
  emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Hi)));
  emitSubRegImm(4, 3, 2);
  emitMovReg(5, 3);
  const size_t jumpDone = currentWordIndex();
  emit(encodeB(0));
  patchWord(jumpBig,
            encodeBCond(static_cast<int32_t>(currentWordIndex() - jumpBig),
                        static_cast<uint8_t>(CondCode::Hi)));
  emitSubRegImm(6, 3, 1);
  emit(clz(6, 6));
  emit(encodeMovz(7, 63, 0));
  emitSubReg(7, 7, 6);    // b
  emitSubRegImm(8, 7, 2); // b - 2
  emit(encodeMovz(6, 1, 0));
  emit(lslv(5, 6, 7)); // 2^b
  emit(lslv(6, 6, 8)); // the step, 2^(b-2)
  emitSubReg(4, 3, 5);
  emitAddReg(4, 4, 6);
  emitSubRegImm(4, 4, 1);
  emit(lsrv(4, 4, 8)); // q
  emit(lslv(6, 4, 8));
  emitAddReg(5, 5, 6); // 2^b + q * step
  emit(lslImm(7, 7, 2));
  emitAddReg(4, 7, 4);
  emitSubRegImm(4, 4, 6); // 4b - 6 + q
  patchWord(jumpDone, encodeB(static_cast<int32_t>(currentWordIndex() - jumpDone)));
}

// x9 = the address of the allocator state (patched once the data segment's address is known).
inline void Arm64Emitter::emitHeapStateAddress() {
  heapStateFixups_.push_back(currentWordIndex());
  emit(encodeAdrp(9, 0));
  emitAddRegImm(9, 9, 0);
}

inline void Arm64Emitter::emitHeapRuntime() {
  using namespace arm64_heap;
  if (heapAllocCalls_.empty() && heapFreeCalls_.empty() && heapReallocCalls_.empty()) {
    return;
  }
  // Forward branches inside the routines: emitted with a zero offset and bound to the current word later.
  const auto branch = [&](CondCode cond) {
    const size_t at = currentWordIndex();
    emit(encodeBCond(0, static_cast<uint8_t>(cond)));
    return at;
  };
  const auto bind = [&](size_t at, CondCode cond) {
    patchWord(
        at, encodeBCond(static_cast<int32_t>(currentWordIndex() - at), static_cast<uint8_t>(cond)));
  };
  const auto jump = [&]() {
    const size_t at = currentWordIndex();
    emit(encodeB(0));
    return at;
  };
  const auto bindJump = [&](size_t at) {
    patchWord(at, encodeB(static_cast<int32_t>(currentWordIndex() - at)));
  };
  const auto bindCbz = [&](size_t at, uint8_t reg) {
    patchWord(at, encodeCbz(reg, static_cast<int32_t>(currentWordIndex() - at)));
  };
  const auto bindCbnz = [&](size_t at, uint8_t reg) {
    patchWord(at, cbnz(reg, static_cast<int32_t>(currentWordIndex() - at)));
  };
  const auto offsetTo = [&](size_t target) {
    return static_cast<int32_t>(target) - static_cast<int32_t>(currentWordIndex());
  };
  // mmap(0, sizeReg, read/write, private anonymous): the carry flag is set when it fails.
  const auto emitMmap = [&](uint8_t sizeReg) {
    emitMovReg(1, sizeReg);
    emitMovImm64(0, 0);
    emitMovImm64(2, MmapProtReadWrite);
    emitMovImm64(3, MmapFlagsPrivateAnon);
    emitMovImm64(4, ~0ull);
    emitMovImm64(5, 0);
    emitMovImm64(16, SysMmap);
    emit(encodeSvc());
  };
  // x6 = the address of class x4's free-list head (x9 = the allocator state).
  const auto emitClassHead = [&]() {
    emit(lslImm(6, 4, 3));
    emitAddReg(6, 6, 9);
    emitAddRegImm(6, 6, 16);
  };

  // ---- alloc: x1 slots (0 allocates nothing), x2 the magic of a mapped block of its kind -> x0 ----
  // Uses x0-x11 and x16.
  const size_t allocIndex = currentWordIndex();
  const size_t allocZero = currentWordIndex();
  emit(encodeCbz(1, 0));
  emitAddRegImm(3, 1, 1); // the slots with the header
  emitMovImm64(4, HeapPoolMaxSlots);
  emitCompareReg(3, 4);
  const size_t allocLarge = branch(CondCode::Hi);
  emitHeapPoolClass();
  emitHeapStateAddress();
  emitClassHead();
  emit(encodeLdrRegBase(7, 6, 0));
  const size_t allocBump = currentWordIndex();
  emit(encodeCbz(7, 0));
  // Reuse the class's first free block: unlink it (the link is its first slot) and zero its slots.
  emit(encodeLdrRegBase(8, 7, 16));
  emit(encodeStrRegBase(8, 6, 0));
  emitAddRegImm(8, 7, 16);
  emitSubRegImm(3, 5, 1);
  const size_t zeroLoop = currentWordIndex();
  emit(stpZeroPostIndex16(8));
  emitSubRegImm(3, 3, 1);
  emit(cbnz(3, offsetTo(zeroLoop)));
  const size_t allocReused = jump();
  // Carve the block from the current chunk, mapping a new chunk when it does not fit (and the first time).
  bindCbz(allocBump, 7);
  emit(encodeLdrRegBase(7, 9, 0));
  emit(encodeLdrRegBase(8, 9, 8));
  emit(lslImm(3, 5, 4));
  emitAddReg(6, 7, 3);
  emitCompareReg(6, 8);
  const size_t allocFits = branch(CondCode::Ls);
  emitMovReg(10, 2);
  emitMovReg(11, 5);
  emitMovImm64(7, HeapPoolChunkBytes);
  emitMmap(7);
  const size_t chunkFailed = branch(CondCode::Hs);
  emitMovReg(7, 0);
  emitMovImm64(8, HeapPoolChunkBytes);
  emitAddReg(8, 7, 8);
  emit(encodeStrRegBase(8, 9, 8));
  emitMovReg(2, 10);
  emitMovReg(5, 11);
  emit(lslImm(3, 5, 4));
  emitAddReg(6, 7, 3);
  bind(allocFits, CondCode::Ls);
  emit(encodeStrRegBase(6, 9, 0));
  bindJump(allocReused);
  // x7 = the block: a pooled header of the block's kind and the class's bytes.
  emitAddRegImm(8, 2, 2);
  emit(encodeStrRegBase(8, 7, 0));
  emit(lslImm(8, 5, 4));
  emit(encodeStrRegBase(8, 7, 8));
  emitAddRegImm(0, 7, 16);
  emit(encodeRet());
  // A large block: a mapping of its own.
  bind(allocLarge, CondCode::Hi);
  emit(lslImm(6, 3, 4));
  emitMovReg(10, 2);
  emitMmap(6);
  const size_t largeFailed = branch(CondCode::Hs);
  emit(encodeStrRegBase(10, 0, 0));
  emit(encodeStrRegBase(6, 0, 8));
  emitAddRegImm(0, 0, 16);
  emit(encodeRet());
  bindCbz(allocZero, 1);
  bind(chunkFailed, CondCode::Hs);
  bind(largeFailed, CondCode::Hs);
  emitMovImm64(0, 0);
  emit(encodeRet());

  // ---- free: x0 the address (0, and an address that is not a block, are ignored) -> x0 = 0 ----
  // Uses x0-x10 and x16; x10 is the block's header.
  const size_t freeIndex = currentWordIndex();
  const size_t freeNull = currentWordIndex();
  emit(encodeCbz(0, 0));
  emitSubRegImm(10, 0, 16);
  emit(encodeLdrRegBase(1, 10, 0));
  std::vector<size_t> freeUnmap;
  std::vector<size_t> freePool;
  for (const uint64_t magic : {HeapHeaderMagic, HeapHeaderMagicBytes}) {
    emitMovImm64(2, magic);
    emitCompareReg(1, 2);
    freeUnmap.push_back(branch(CondCode::Eq));
  }
  for (const uint64_t magic : {HeapHeaderMagicPooled, HeapHeaderMagicPooledBytes}) {
    emitMovImm64(2, magic);
    emitCompareReg(1, 2);
    freePool.push_back(branch(CondCode::Eq));
  }
  emitMovImm64(2, HeapHeaderMagicFree);
  emitCompareReg(1, 2);
  const size_t freeTwice = branch(CondCode::Eq);
  const size_t freeIgnored = jump();
  for (const size_t at : freeUnmap) {
    bind(at, CondCode::Eq);
  }
  emitMovReg(0, 10);
  emit(encodeLdrRegBase(1, 10, 8));
  emitMovImm64(16, SysMunmap);
  emit(encodeSvc());
  const size_t freeUnmapped = jump();
  for (const size_t at : freePool) {
    bind(at, CondCode::Eq);
  }
  emit(encodeLdrRegBase(3, 10, 8));
  emit(lsrImm(3, 3, 4));
  emitHeapPoolClass();
  emitHeapStateAddress();
  emitClassHead();
  emit(encodeLdrRegBase(8, 6, 0));
  emit(encodeStrRegBase(8, 10, 16));
  emit(encodeStrRegBase(10, 6, 0));
  emitMovImm64(2, HeapHeaderMagicFree);
  emit(encodeStrRegBase(2, 10, 0));
  bindCbz(freeNull, 0);
  bindJump(freeIgnored);
  bindJump(freeUnmapped);
  emitMovImm64(0, 0);
  emit(encodeRet());
  static constexpr std::string_view FreeMessage =
      "invalid heap free address: the block is already free\n";
  bind(freeTwice, CondCode::Eq);
  const size_t freeMessage = currentWordIndex();
  emit(encodeAdr(1, 0));
  emit(encodeMovz(2, static_cast<uint16_t>(FreeMessage.size()), 0));
  const size_t freeFault = jump();

  // ---- realloc: x0 the address (0 allocates slots), x1 the new count (slots, or bytes for a byte region) -> x0 ----
  // A new block of the old one's kind holding its leading slots; the old one is freed. The state lives in x12-x15
  // and x17, which alloc and free leave alone.
  const size_t reallocIndex = currentWordIndex();
  const size_t reallocNonNull = currentWordIndex();
  emit(cbnz(0, 0));
  emitMovImm64(2, HeapHeaderMagic);
  emit(encodeB(offsetTo(allocIndex)));
  bindCbnz(reallocNonNull, 0);
  emit(cbnz(1, 2));
  emit(encodeB(offsetTo(freeIndex))); // a count of 0 frees
  emitSubRegImm(13, 0, 16);
  emit(encodeLdrRegBase(11, 13, 0));
  emitMovReg(12, 1);
  std::vector<size_t> reallocSlots;
  std::vector<size_t> reallocBytes;
  emitMovImm64(2, HeapHeaderMagic);
  for (const uint64_t magic : {HeapHeaderMagic, HeapHeaderMagicPooled}) {
    emitMovImm64(3, magic);
    emitCompareReg(11, 3);
    reallocSlots.push_back(branch(CondCode::Eq));
  }
  emitMovImm64(2, HeapHeaderMagicBytes);
  for (const uint64_t magic : {HeapHeaderMagicBytes, HeapHeaderMagicPooledBytes}) {
    emitMovImm64(3, magic);
    emitCompareReg(11, 3);
    reallocBytes.push_back(branch(CondCode::Eq));
  }
  emitMovImm64(3, HeapHeaderMagicFree);
  emitCompareReg(11, 3);
  const size_t reallocFreed = branch(CondCode::Eq);
  emitMovImm64(0, 0); // not a block
  emit(encodeRet());
  for (const size_t at : reallocBytes) {
    bind(at, CondCode::Eq);
  }
  // A byte region counts bytes: the new size is the slots that hold them.
  emitAddRegImm(12, 12, static_cast<uint16_t>(IrSlotBytes - 1));
  emit(lsrImm(12, 12, 4));
  for (const size_t at : reallocSlots) {
    bind(at, CondCode::Eq);
  }
  emit(encodeLdrRegBase(14, 13, 8));
  emit(lsrImm(14, 14, 4));
  emitSubRegImm(14, 14, 1); // the old block's slots
  emitMovReg(15, 0);
  emitMovReg(17, 30);
  emitMovReg(1, 12);
  emit(encodeBl(offsetTo(allocIndex)));
  emitMovReg(30, 17);
  const size_t reallocAllocated = currentWordIndex();
  emit(cbnz(0, 0));
  emit(encodeRet()); // no memory: 0, and the old block stays
  bindCbnz(reallocAllocated, 0);
  // Copy as many leading slots as both blocks hold.
  emitCompareReg(14, 12);
  emit(encodeBCond(2, static_cast<uint8_t>(CondCode::Ls)));
  emitMovReg(14, 12);
  emitMovReg(8, 15);
  emitMovReg(9, 0);
  const size_t copyDone = currentWordIndex();
  emit(encodeCbz(14, 0));
  const size_t copyLoop = currentWordIndex();
  emit(encodeLdrRegBase(10, 8, 0));
  emit(encodeStrRegBase(10, 9, 0));
  emit(encodeLdrRegBase(10, 8, 8));
  emit(encodeStrRegBase(10, 9, 8));
  emitAddRegImm(8, 8, 16);
  emitAddRegImm(9, 9, 16);
  emitSubRegImm(14, 14, 1);
  emit(cbnz(14, offsetTo(copyLoop)));
  bindCbz(copyDone, 14);
  emitMovReg(13, 0);
  emitMovReg(0, 15);
  emit(encodeBl(offsetTo(freeIndex)));
  emitMovReg(0, 13);
  emitMovReg(30, 17);
  emit(encodeRet());
  static constexpr std::string_view ReallocMessage =
      "invalid heap realloc address: the block is free\n";
  bind(reallocFreed, CondCode::Eq);
  const size_t reallocMessage = currentWordIndex();
  emit(encodeAdr(1, 0));
  emit(encodeMovz(2, static_cast<uint16_t>(ReallocMessage.size()), 0));

  // ---- the heap fault: the message in x1 and x2 goes to stderr, then the program exits with 3 ----
  bindJump(freeFault);
  emit(encodeMovz(0, 2, 0));
  emitMovImm64(16, SysWrite);
  emit(encodeSvc());
  emit(encodeMovz(0, 3, 0));
  emitMovImm64(16, SysExit);
  emit(encodeSvc());
  const auto emitMessage = [&](std::string_view text, size_t site) {
    patchWord(site, encodeAdr(1, static_cast<int32_t>((currentWordIndex() - site) * 4)));
    for (size_t i = 0; i < text.size(); i += 4) {
      uint32_t word = 0;
      for (size_t b = 0; b < 4 && i + b < text.size(); ++b) {
        word |= static_cast<uint32_t>(static_cast<uint8_t>(text[i + b])) << (8 * b);
      }
      emit(word);
    }
  };
  emitMessage(FreeMessage, freeMessage);
  emitMessage(ReallocMessage, reallocMessage);

  // A threaded module calls the routines through entries that hold the heap lock (realloc's own calls of alloc and
  // free go to the routines directly, under its lock).
  size_t allocEntry = allocIndex;
  size_t freeEntry = freeIndex;
  size_t reallocEntry = reallocIndex;
  if (threaded_) {
    allocEntry = emitLockedHeapEntry(allocIndex);
    freeEntry = emitLockedHeapEntry(freeIndex);
    reallocEntry = emitLockedHeapEntry(reallocIndex);
  }
  for (const size_t site : heapAllocCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(allocEntry) - static_cast<int32_t>(site)));
  }
  for (const size_t site : heapFreeCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(freeEntry) - static_cast<int32_t>(site)));
  }
  for (const size_t site : heapReallocCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(reallocEntry) - static_cast<int32_t>(site)));
  }
}
