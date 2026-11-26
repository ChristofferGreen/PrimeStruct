#pragma once

// String arenas on the arm64 native backend (`StringArena`, `__pstext_arena_begin` and `__pstext_arena_end`).
//
// While an arena is open every text block comes from a bump region of the innermost arena instead of the pool, and
// ending the arena gives all of its blocks back at once. The regions are mapped from the system (arm64_arena in
// NativeEmitterInternalsArm64Encodings.h gives the layout); the data segment's state holds the innermost arena (its
// first region is its record, which links to the enclosing arena) and the list of regions free to reuse.
//
// A block of an arena has its own header magic (HeapHeaderMagicArena): freeing it does nothing. Ending the arena
// writes the free magic into every block it made and takes all access away from its regions (`mprotect` with no
// access), so text used after its arena ended is a fault: a read, a write or a free of it stops the program with
// the VM's text for a byte address outside any live region (exit code 3), instead of reading or corrupting what a
// later arena wrote there. A region of the standard size stays mapped, protected, on the free list; the next arena
// that takes it gives its access back first (a use of the old text after that sees the new arena's text, as the
// VM's recycled chunks do). A larger region, made for one block that did not fit, is unmapped, so a loop of arenas
// with large text runs in bounded memory; a use of such text faults the same way. The handler that reports the
// fault (`SIGBUS` and `SIGSEGV`) is installed when the first region is mapped.
//
// A threaded module gives every thread arenas of its own, as the VM gives every task thread a text heap of its own:
// the innermost arena and the list of free regions live in the allocator state of the running thread (the same
// offsets in every state, found through the lookup the pool uses), and the counters stay the program's, changed with
// atomic adds. A thread's free regions stay with its state when the thread ends, so the next thread that takes the
// state reuses them. A block of an arena has its own header magic, so a thread that frees one (a `String` that went to
// another thread) ignores it, as the owner's arena end gives it back. A threaded module that also has a
// `SharedString` (its owner count is shared between threads and the last owner may be on another thread than the
// arena that holds the text) keeps its arenas counting only (`arenaCountOnly`): its text blocks stay in the pool.
//
// The three routines are emitted once after the functions and called with BL from the template code. They use x0-x11
// and x16 and leave everything else alone; the allocation reads x12 (the text size) as the caller keeps it.

// x9 = the arena state of the running thread: the data segment's state, or in a threaded module the thread's own
// (clobbers x11 as well, every other register survives).
inline void Arm64Emitter::emitArenaStateAddress() {
  if (threaded_) {
    emitHeapResolveState();
  } else {
    emitHeapStateAddress();
  }
}

inline void Arm64Emitter::emitArenaBegin() {
  if (arenaCountOnly()) {
    if (textStats_) {
      emitHeapStateAddress();
      emitAddRegImm(2, 9, static_cast<uint16_t>(arm64_text::ArenaDepthOffset));
      emitMovImm64(1, 1);
      emit(arm64_atomics::ldaddal(1, 3, 2));
    }
    return;
  }
  arenaCalls_.emplace_back(currentWordIndex(), ArenaRoutine::Begin);
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitArenaEnd() {
  if (arenaCountOnly()) {
    if (textStats_) {
      emitHeapStateAddress();
      emitAddRegImm(2, 9, static_cast<uint16_t>(arm64_text::ArenaDepthOffset));
      emitMovImm64(1, ~0ull);
      emit(arm64_atomics::ldaddal(1, 3, 2));
    }
    return;
  }
  arenaCalls_.emplace_back(currentWordIndex(), ArenaRoutine::End);
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitArenaRuntime() {
  using namespace arm64_arena;
  if (arenaCalls_.empty()) {
    return;
  }
  const size_t firstStateCall = heapCurStateCalls_.size();
  const auto here = [&]() { return currentWordIndex(); };
  const auto branch = [&](CondCode cond) {
    const size_t at = here();
    emit(encodeBCond(0, static_cast<uint8_t>(cond)));
    return at;
  };
  const auto bind = [&](size_t at, CondCode cond) {
    patchWord(at, encodeBCond(static_cast<int32_t>(here() - at), static_cast<uint8_t>(cond)));
  };
  const auto jump = [&]() {
    const size_t at = here();
    emit(encodeB(0));
    return at;
  };
  const auto bindJump = [&](size_t at) {
    patchWord(at, encodeB(static_cast<int32_t>(here() - at)));
  };
  const auto cbz = [&](uint8_t reg) {
    const size_t at = here();
    emit(encodeCbz(reg, 0));
    return at;
  };
  const auto bindCbz = [&](size_t at, uint8_t reg) {
    patchWord(at, encodeCbz(reg, static_cast<int32_t>(here() - at)));
  };
  const auto cbnz = [&](uint8_t reg) {
    const size_t at = here();
    emit(arm64_heap::cbnz(reg, 0));
    return at;
  };
  const auto bindCbnz = [&](size_t at, uint8_t reg) {
    patchWord(at, arm64_heap::cbnz(reg, static_cast<int32_t>(here() - at)));
  };
  const auto load = [&](uint8_t reg, uint8_t base, uint64_t offset) {
    emit(encodeLdrRegBase(reg, base, static_cast<uint16_t>(offset)));
  };
  const auto store = [&](uint8_t reg, uint8_t base, uint64_t offset) {
    emit(encodeStrRegBase(reg, base, static_cast<uint16_t>(offset)));
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
  // mprotect(region in `regionReg`, the standard region size, `prot`): the syscall changes x0, x1 and x2.
  const auto emitMprotect = [&](uint8_t regionReg, uint64_t prot) {
    emitMovReg(0, regionReg);
    emitMovImm64(1, RegionBytes);
    emitMovImm64(2, prot);
    emitMovImm64(16, SysMprotect);
    emit(encodeSvc());
  };
  // `counter` (a state offset of the program's counters) changes by the value of `delta`: plain loads and stores in a
  // module without threads; in a threaded module an atomic add on the data segment's counter (x9 is the thread's own
  // state there), through `address` and `discard`.
  const auto addToCounter = [&](uint64_t counter,
                                uint8_t delta,
                                uint8_t scratch,
                                bool subtract,
                                uint8_t address,
                                uint8_t discard) {
    if (threaded_) {
      emitHeapStateAddress(address);
      emitAddRegImm(address, address, static_cast<uint16_t>(counter));
      if (subtract) {
        emitMovImm64(discard, 0);
        emitSubReg(discard, discard, delta);
        emit(arm64_atomics::ldaddal(discard, discard, address));
      } else {
        emit(arm64_atomics::ldaddal(delta, discard, address));
      }
      return;
    }
    load(scratch, 9, counter);
    if (subtract) {
      emitSubReg(scratch, scratch, delta);
    } else {
      emitAddReg(scratch, scratch, delta);
    }
    store(scratch, 9, counter);
  };
  bool usesRoutine[3] = {false, false, false};
  for (const auto &call : arenaCalls_) {
    usesRoutine[static_cast<size_t>(call.second)] = true;
  }
  size_t entries[3] = {0, 0, 0};
  // The SUB sp of each handler installation; the ADR that names the handler follows it.
  std::vector<size_t> handlerInstalls;

  // ---- begin: a region (a free one, or a new mapping) that becomes the innermost arena's record ----
  if (usesRoutine[static_cast<size_t>(ArenaRoutine::Begin)]) {
    entries[static_cast<size_t>(ArenaRoutine::Begin)] = here();
    emitArenaStateAddress();
    load(0, 9, arm64_text::ArenaFreeRegionsOffset);
    const size_t noFreeRegion = cbz(0);
    emitMovReg(11, 0);
    // The free region gets its access back before its link is read.
    emitMprotect(11, MmapProtReadWrite);
    emitMovReg(0, 11);
    load(1, 0, NextOffset);
    store(1, 9, arm64_text::ArenaFreeRegionsOffset);
    emitMovImm64(1, 1);
    const size_t gotRegion = jump();
    bindCbz(noFreeRegion, 0);
    emitMovImm64(10, RegionBytes);
    emitMmap(10);
    const size_t mapFailed = branch(CondCode::Hs);
    emitMovReg(11, 0);
    handlerInstalls.push_back(here());
    // A `struct __sigaction` (the handler, its trampoline, the mask and the flags) on the stack.
    emit(encodeSubSpImm(32));
    emit(encodeAdr(2, 0)); // patched to the handler
    store(2, 31, 0);
    store(2, 31, 8);
    emitMovImm64(3, 0);
    store(3, 31, 16);
    for (const uint64_t signal : {SignalBus, SignalSegv}) {
      emitMovImm64(0, signal);
      emitAddRegImm(1, 31, 0);
      emitMovImm64(2, 0);
      emitMovImm64(16, SysSigaction);
      emit(encodeSvc());
    }
    emit(encodeAddSpImm(32));
    emitMovReg(0, 11);
    emitMovImm64(1, 0);
    bindJump(gotRegion);
    // x0 = the region, x1 = whether it was used before.
    emitArenaStateAddress();
    load(2, 9, arm64_text::ArenaCurrentOffset);
    store(2, 0, ParentOffset);
    emitMovImm64(3, 0);
    store(3, 0, NextOffset);
    store(3, 0, UsedOffset);
    emitMovImm64(4, RegionBytes);
    store(4, 0, BytesOffset);
    emitAddRegImm(5, 0, static_cast<uint16_t>(HeaderBytes));
    store(5, 0, BumpOffset);
    emitAddReg(6, 0, 4);
    store(6, 0, EndOffset);
    store(0, 0, NewestOffset);
    store(1, 0, DirtyOffset);
    store(0, 9, arm64_text::ArenaCurrentOffset);
    if (textStats_) {
      emitMovImm64(1, 1);
      addToCounter(arm64_text::ArenaDepthOffset, 1, 2, false, 2, 3);
    }
    emit(encodeRet());
    bind(mapFailed, CondCode::Hs);
    emitFaultMessage("string arena could not be made: out of memory\n");
  }

  // ---- end: every block of the innermost arena becomes free, its standard regions go on the free list, larger ones are unmapped ----
  if (usesRoutine[static_cast<size_t>(ArenaRoutine::End)]) {
    entries[static_cast<size_t>(ArenaRoutine::End)] = here();
    emitArenaStateAddress();
    load(1, 9, arm64_text::ArenaCurrentOffset);
    const size_t isOpen = cbnz(1);
    emitFaultMessage("string arena ended without one being open\n");
    bindCbnz(isOpen, 1);
    load(2, 1, ParentOffset);
    store(2, 9, arm64_text::ArenaCurrentOffset);
    load(2, 1, NewestOffset);
    load(3, 1, BumpOffset);
    store(3, 2, UsedOffset);
    emitMovImm64(4, 0); // text bytes the blocks held
    emitMovImm64(5, 0); // blocks above the large threshold
    emitMovImm64(6, 0); // and their bytes
    emitMovImm64(10, HeapHeaderMagicFree);
    emitMovImm64(11, arm64_text::LargeThreshold);
    const size_t regionLoop = here();
    load(7, 2, NextOffset);
    emitAddRegImm(0, 2, static_cast<uint16_t>(HeaderBytes));
    load(8, 2, UsedOffset);
    const size_t blockLoop = here();
    emitCompareReg(0, 8);
    const size_t blocksDone = branch(CondCode::Hs);
    load(3, 0, 8);
    store(10, 0, 0);
    if (textStats_) {
      emitSubRegImm(3, 3, static_cast<uint16_t>(IrSlotBytes)); // the text size
      emitAddReg(4, 4, 3);
      emitCompareReg(3, 11);
      const size_t small = branch(CondCode::Ls);
      emitAddRegImm(5, 5, 1);
      emitAddReg(6, 6, 3);
      bind(small, CondCode::Ls);
      emitAddRegImm(
          3,
          3,
          static_cast<uint16_t>(2 * IrSlotBytes - 1)); // the block's bytes, header and rounding
    } else {
      emitAddRegImm(3, 3, static_cast<uint16_t>(IrSlotBytes - 1));
    }
    emit(arm64_heap::lsrImm(3, 3, 4));
    emit(arm64_heap::lslImm(3, 3, 4));
    emitAddReg(0, 0, 3);
    const size_t again = jump();
    patchWord(again, encodeB(static_cast<int32_t>(blockLoop) - static_cast<int32_t>(again)));
    bind(blocksDone, CondCode::Hs);
    load(3, 2, BytesOffset);
    emitMovImm64(0, RegionBytes);
    emitCompareReg(3, 0);
    const size_t unmapRegion = branch(CondCode::Ne);
    load(0, 9, arm64_text::ArenaFreeRegionsOffset);
    store(0, 2, NextOffset);
    store(2, 9, arm64_text::ArenaFreeRegionsOffset);
    // Text of the arena used from here on faults; x7 still holds the older region.
    emitMprotect(2, MmapProtNone);
    const size_t regionDone = jump();
    // A larger region (made for one block that did not fit a standard one) goes back to the system: munmap(region,
    // bytes). The syscall changes x0 and x1 only; x7 still holds the older region.
    bind(unmapRegion, CondCode::Ne);
    emitMovReg(1, 3);
    emitMovReg(0, 2);
    emitMovImm64(16, SysMunmap);
    emit(encodeSvc());
    bindJump(regionDone);
    emitMovReg(2, 7);
    emit(arm64_heap::cbnz(2, static_cast<int32_t>(regionLoop) - static_cast<int32_t>(here())));
    if (textStats_) {
      addToCounter(arm64_text::HeldOffset, 4, 0, true, 7, 8);
      addToCounter(arm64_text::ArenaBytesOffset, 4, 0, true, 7, 8);
      addToCounter(arm64_text::LargeBlocksOffset, 5, 0, true, 7, 8);
      addToCounter(arm64_text::LargeBytesOffset, 6, 0, true, 7, 8);
      emitMovImm64(1, 1);
      addToCounter(arm64_text::ArenaDepthOffset, 1, 0, true, 7, 8);
    }
    emit(encodeRet());
  }

  // ---- alloc: x1 text slots (at least 1) -> x0 = the block's address, or 0; x12 = the text size ----
  if (usesRoutine[static_cast<size_t>(ArenaRoutine::Alloc)]) {
    entries[static_cast<size_t>(ArenaRoutine::Alloc)] = here();
    emitArenaStateAddress();
    emitMovReg(8, 1); // the slots, kept in x8 across the system call
    load(4, 9, arm64_text::ArenaCurrentOffset);
    emit(arm64_heap::lslImm(3, 8, 4));
    emitAddRegImm(3, 3, static_cast<uint16_t>(IrSlotBytes)); // the block's bytes, header included
    load(5, 4, BumpOffset);
    load(6, 4, EndOffset);
    emitAddReg(7, 5, 3);
    emitCompareReg(7, 6);
    const size_t fits = branch(CondCode::Ls);
    // The newest region is full: a free standard region, or a new mapping (as large as the block needs).
    emitAddRegImm(10, 3, static_cast<uint16_t>(HeaderBytes));
    emitMovImm64(6, RegionBytes);
    emitCompareReg(10, 6);
    const size_t oversize = branch(CondCode::Hi);
    emitMovReg(10, 6);
    load(0, 9, arm64_text::ArenaFreeRegionsOffset);
    const size_t noFreeRegion = cbz(0);
    emitMovReg(11, 0);
    // The free region gets its access back before its link is read.
    emitMprotect(11, MmapProtReadWrite);
    emitMovReg(0, 11);
    load(1, 0, NextOffset);
    store(1, 9, arm64_text::ArenaFreeRegionsOffset);
    emitMovImm64(1, 1);
    const size_t gotRegion = jump();
    bindCbz(noFreeRegion, 0);
    bind(oversize, CondCode::Hi);
    emitMmap(10);
    const size_t mapFailed = branch(CondCode::Hs);
    emitMovImm64(1, 0);
    bindJump(gotRegion);
    // x0 = the region, x10 = its size, x1 = whether it was used before: the arena moves on to it.
    emitArenaStateAddress();
    load(4, 9, arm64_text::ArenaCurrentOffset);
    emit(arm64_heap::lslImm(3, 8, 4));
    emitAddRegImm(3, 3, static_cast<uint16_t>(IrSlotBytes));
    load(5, 4, NewestOffset);
    load(6, 4, BumpOffset);
    store(6, 5, UsedOffset);
    store(5, 0, NextOffset);
    store(10, 0, BytesOffset);
    store(1, 0, DirtyOffset);
    emitMovImm64(2, 0);
    store(2, 0, UsedOffset);
    store(0, 4, NewestOffset);
    emitAddReg(6, 0, 10);
    store(6, 4, EndOffset);
    emitAddRegImm(5, 0, static_cast<uint16_t>(HeaderBytes));
    emitAddReg(7, 5, 3);
    bind(fits, CondCode::Ls);
    // x5 = the block, x7 = the bump after it.
    store(7, 4, BumpOffset);
    emitMovImm64(6, HeapHeaderMagicArena);
    store(6, 5, 0);
    store(3, 5, 8);
    emitAddRegImm(0, 5, static_cast<uint16_t>(IrSlotBytes));
    load(6, 4, NewestOffset);
    load(6, 6, DirtyOffset);
    const size_t clean = cbz(6);
    emitMovReg(7, 0);
    emitMovReg(6, 8);
    const size_t zeroLoop = here();
    emit(arm64_heap::stpZeroPostIndex16(7));
    emitSubRegImm(6, 6, 1);
    emit(arm64_heap::cbnz(6, static_cast<int32_t>(zeroLoop) - static_cast<int32_t>(here())));
    bindCbz(clean, 6);
    if (textStats_) {
      addToCounter(arm64_text::ArenaBytesOffset, 12, 1, false, 1, 3);
    }
    emit(encodeRet());
    bind(mapFailed, CondCode::Hs);
    emitMovImm64(0, 0);
    emit(encodeRet());
  }

  // ---- fault: the handler of SIGBUS and SIGSEGV (also its own trampoline); it reports the VM's text and exits ----
  if (!handlerInstalls.empty()) {
    const size_t handler = here();
    for (const size_t install : handlerInstalls) {
      const size_t adr = install + 1;
      patchWord(adr, encodeAdr(2, static_cast<int32_t>((handler - adr) * 4)));
    }
    emitFaultMessage("VM error: invalid byte address in IR\n");
  }

  for (size_t call = firstStateCall; call < heapCurStateCalls_.size(); ++call) {
    const size_t site = heapCurStateCalls_[call];
    patchWord(site, encodeBl(static_cast<int32_t>(heapCurState_) - static_cast<int32_t>(site)));
  }
  for (const auto &[site, routine] : arenaCalls_) {
    patchWord(site,
              encodeBl(static_cast<int32_t>(entries[static_cast<size_t>(routine)]) -
                       static_cast<int32_t>(site)));
  }
}
