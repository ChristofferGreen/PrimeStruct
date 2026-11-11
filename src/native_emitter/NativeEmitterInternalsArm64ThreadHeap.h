#pragma once

// Per-thread allocator states of a threaded module (a module with TaskSpawn): each thread allocates from a state of
// its own, so the heap routines take no lock (docs/MultithreadingPrototype.md, Heaps).
//
// A state is the allocator state the heap always had (chunk cursor and end, one free-list head per size class) plus
// a remote-free head and a pool link. The main thread keeps the data segment's state; a spawned thread takes one
// from a pool (a free list of mmap'd pages that is never unmapped, because a block can outlive its thread) when it
// starts and gives it back when it ends, so a thread that starts later continues with the chunks and free lists of
// one that ended. The state of the running thread is found through the thread pointer (TPIDRRO_EL0, which the main
// thread compared with at startup) and a pthread key (`pthread_getspecific`) for every other thread.
//
// Chunks are mapped aligned to their size (256 KiB) and start with a word that holds their owner's state, so the
// owner of a pooled block is found by masking its address. Freeing a block of another thread's chunk marks it free
// and pushes it onto the owner's remote-free list with a compare-and-swap; the owner takes that list back with an
// exchange when its free list of a size class is empty, and sorts the blocks into their classes. Large blocks (their
// own mapping) are unmapped by whoever frees them. The lock word of the data segment only guards the pool.

// x9 = the allocator state of the running thread. Entered with BL; the caller keeps its link register in x16 (heap
// routines have no other free register for it). Clobbers x9 and x11; every other register survives.
inline void Arm64Emitter::emitHeapResolveState() {
  heapCurStateCalls_.push_back(currentWordIndex() + 1);
  emitMovReg(16, 30);
  emit(encodeBl(0));
  emitMovReg(30, 16);
}

// The entry function of a threaded module: the main thread's token and the pthread key of the thread states.
inline void Arm64Emitter::emitThreadHeapInit() {
  threadHeapInitCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

inline void
Arm64Emitter::emitThreadHeapRuntime(std::vector<std::pair<size_t, std::string>> &slotLoads) {
  using namespace arm64_theap;
  using namespace arm64_atomics;
  using namespace arm64_tasks;
  if (!threaded_) {
    return;
  }
  const auto callImport = [&](const char *symbol) {
    const size_t at = emitHostSlotLoad(16);
    slotLoads.emplace_back(at, symbol);
    emit(0xD63F0200u); // BLR x16
  };
  const auto here = [&]() { return currentWordIndex(); };
  const auto bind = [&](size_t at, CondCode cond) {
    patchWord(at, encodeBCond(static_cast<int32_t>(here() - at), static_cast<uint8_t>(cond)));
  };
  // The pool's lock (x9, x11 and x12 are clobbered).
  const auto lockPool = [&]() {
    emitHeapLockAddress();
    const size_t spin = here();
    emit(encodeMovz(11, 0, 0));
    emit(encodeMovz(12, 1, 0));
    emit(casalW(11, 12, 9));
    emit(cbnzW(11, static_cast<int32_t>(spin) - static_cast<int32_t>(here())));
  };
  const auto unlockPool = [&]() {
    emitHeapLockAddress();
    emit(stlrXzr(9));
  };

  // ---- init: the main thread's token and the pthread key ----
  const size_t initIndex = here();
  emit(subSpImm(16));
  emit(encodeStrRegBase(30, 31, 0));
  emitHeapStateAddress();
  emit(0xD53BD060u | 10u); // MRS x10, TPIDRRO_EL0
  emit(encodeStrRegBase(10, 9, MainTokenOffset));
  emitAddRegImm(0, 9, static_cast<uint16_t>(KeyOffset));
  emit(encodeMovz(1, 0, 0));
  callImport("pthread_key_create");
  emit(encodeLdrRegBase(30, 31, 0));
  emit(addSpImm(16));
  emit(encodeRet());
  for (const size_t site : threadHeapInitCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(initIndex) - static_cast<int32_t>(site)));
  }

  // ---- current state: x9 = the running thread's allocator state ----
  const size_t curStateIndex = here();
  emitHeapStateAddress();
  emit(encodeLdrRegBase(11, 9, MainTokenOffset));
  emit(0xD53BD060u | 9u); // MRS x9, TPIDRRO_EL0
  emitCompareReg(9, 11);
  const size_t notMain = here();
  emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Ne)));
  emitHeapStateAddress();
  emit(encodeRet());
  bind(notMain, CondCode::Ne);
  // Another thread: its state is the value of the pthread key. Everything but x9 and x11 is kept.
  emit(subSpImm(144));
  emit(stpOffset(0, 1, 31, 0));
  emit(stpOffset(2, 3, 31, 16));
  emit(stpOffset(4, 5, 31, 32));
  emit(stpOffset(6, 7, 31, 48));
  emit(stpOffset(8, 10, 31, 64));
  emit(stpOffset(12, 13, 31, 80));
  emit(stpOffset(14, 15, 31, 96));
  emit(stpOffset(16, 17, 31, 112));
  emit(encodeStrRegBase(30, 31, 128));
  emitHeapStateAddress();
  emit(encodeLdrRegBase(0, 9, KeyOffset));
  callImport("pthread_getspecific");
  emitMovReg(9, 0);
  emit(ldpOffset(0, 1, 31, 0));
  emit(ldpOffset(2, 3, 31, 16));
  emit(ldpOffset(4, 5, 31, 32));
  emit(ldpOffset(6, 7, 31, 48));
  emit(ldpOffset(8, 10, 31, 64));
  emit(ldpOffset(12, 13, 31, 80));
  emit(ldpOffset(14, 15, 31, 96));
  emit(ldpOffset(16, 17, 31, 112));
  emit(encodeLdrRegBase(30, 31, 128));
  emit(addSpImm(144));
  emit(encodeRet());
  heapCurState_ = curStateIndex;

  // ---- drain: x9 = the state; the blocks other threads freed to it go onto its free lists. Keeps every register but
  // x3, x7, x8, x10 and x11 (what the caller needs of x1-x6 is saved); the caller saved its link register. ----
  const size_t drainIndex = here();
  emit(subSpImm(64));
  emit(stpOffset(30, 1, 31, 0));
  emit(stpOffset(2, 4, 31, 16));
  emit(stpOffset(5, 6, 31, 32));
  emitAddRegImm(10, 9, static_cast<uint16_t>(RemoteHeadOffset));
  emit(encodeLdrRegBase(1, 10, 0));
  const size_t drainEmpty = here();
  emit(encodeCbz(1, 0));
  emit(encodeMovz(2, 0, 0));
  emit(swpal(2, 1, 10)); // x1 = the whole list, the head is empty again
  const size_t drainLoop = here();
  const size_t drainEnd = here();
  emit(encodeCbz(1, 0));
  emit(encodeLdrRegBase(2, 1, 16)); // the next block of the list
  emit(encodeLdrRegBase(3, 1, 8));
  emit(arm64_heap::lsrImm(3, 3, 4));
  emitHeapPoolClass(); // x4 = the class
  emit(arm64_heap::lslImm(6, 4, 3));
  emitAddReg(6, 6, 9);
  emitAddRegImm(6, 6, 16);
  emit(encodeLdrRegBase(5, 6, 0));
  emit(encodeStrRegBase(5, 1, 16));
  emit(encodeStrRegBase(1, 6, 0));
  emitMovReg(1, 2);
  emit(encodeB(static_cast<int32_t>(drainLoop) - static_cast<int32_t>(here())));
  patchWord(drainEnd, encodeCbz(1, static_cast<int32_t>(here() - drainEnd)));
  patchWord(drainEmpty, encodeCbz(1, static_cast<int32_t>(here() - drainEmpty)));
  emit(ldpOffset(30, 1, 31, 0));
  emit(ldpOffset(2, 4, 31, 16));
  emit(ldpOffset(5, 6, 31, 32));
  emit(addSpImm(64));
  emit(encodeRet());
  heapDrain_ = drainIndex;

  // ---- acquire: -> x0 = a state for a starting thread (from the pool, else a fresh zeroed page) ----
  const size_t acquireIndex = here();
  lockPool();
  emitHeapStateAddress();
  emitAddRegImm(10, 9, static_cast<uint16_t>(PoolHeadOffset));
  emit(encodeLdrRegBase(0, 10, 0));
  const size_t poolEmpty = here();
  emit(encodeCbz(0, 0));
  emit(encodeLdrRegBase(1, 0, PoolLinkOffset));
  emit(encodeStrRegBase(1, 10, 0));
  const size_t acquired = here();
  emit(encodeB(0));
  patchWord(poolEmpty, encodeCbz(0, static_cast<int32_t>(here() - poolEmpty)));
  emitMovImm64(1, StatePageBytes);
  emitMovImm64(0, 0);
  emitMovImm64(2, MmapProtReadWrite);
  emitMovImm64(3, MmapFlagsPrivateAnon);
  emitMovImm64(4, ~0ull);
  emitMovImm64(5, 0);
  emitMovImm64(16, SysMmap);
  emit(encodeSvc());
  const size_t mapFailed = here();
  emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Hs)));
  patchWord(acquired, encodeB(static_cast<int32_t>(here() - acquired)));
  emitMovReg(13, 0);
  unlockPool();
  emitMovReg(0, 13);
  emit(encodeRet());
  bind(mapFailed, CondCode::Hs);
  {
    static constexpr std::string_view Text = "task spawn failed: no memory for a thread heap\n";
    const size_t site = here();
    emit(encodeAdr(1, 0));
    emit(encodeMovz(2, static_cast<uint16_t>(Text.size()), 0));
    emit(encodeMovz(0, 2, 0));
    emitMovImm64(16, SysWrite);
    emit(encodeSvc());
    emit(encodeMovz(0, 3, 0));
    emitMovImm64(16, SysExit);
    emit(encodeSvc());
    patchWord(site, encodeAdr(1, static_cast<int32_t>((here() - site) * 4)));
    for (size_t i = 0; i < Text.size(); i += 4) {
      uint32_t word = 0;
      for (size_t b = 0; b < 4 && i + b < Text.size(); ++b) {
        word |= static_cast<uint32_t>(static_cast<uint8_t>(Text[i + b])) << (8 * b);
      }
      emit(word);
    }
  }
  heapAcquire_ = acquireIndex;

  // ---- release: x0 = the state of an ending thread, which goes back to the pool ----
  const size_t releaseIndex = here();
  emitMovReg(13, 0);
  lockPool();
  emitHeapStateAddress();
  emitAddRegImm(10, 9, static_cast<uint16_t>(PoolHeadOffset));
  emit(encodeLdrRegBase(1, 10, 0));
  emit(encodeStrRegBase(1, 13, PoolLinkOffset));
  emit(encodeStrRegBase(13, 10, 0));
  unlockPool();
  emit(encodeRet());
  heapRelease_ = releaseIndex;
}
