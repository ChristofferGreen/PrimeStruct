// The per-thread heap arenas of generated programs, written once and used twice (see VmHeapArenas.h). It must stay
// self-contained: standard headers (algorithm, mutex, string, utility, vector), VmHeapCore and VmTextHeap only, no
// preprocessor lines, balanced parentheses.
PRIMEC_VM_HEAP_ARENAS_BODY(
// One thread's heap and string heap. `lock` guards the core's maps and its remote-free queue, `textLock` the string
// heap; the owner takes them only once a second arena exists.
struct VmHeapArena {
  VmHeapCore core;
  VmTextHeap text;
  std::mutex lock;
  std::mutex textLock;
  unsigned index = 0;
  unsigned owners = 0;
};

class VmHeapArenas {
public:
  // The storage each arena reserves once threads exist (address space, not memory: pages cost nothing until
  // written). An arena that outgrows its reservation fails with the heap's overflow error.
  static constexpr uint64_t DefaultSlotCapacity = uint64_t{1} << 32;
  static constexpr uint64_t DefaultByteCapacity = uint64_t{1} << 36;

  explicit VmHeapArenas(uint64_t slotCapacity = DefaultSlotCapacity,
                        uint64_t byteCapacity = DefaultByteCapacity,
                        unsigned maxArenas = VmHeapCore::MaxArenas)
      : slotCapacity_(slotCapacity), byteCapacity_(byteCapacity),
        maxArenas_(std::min(std::max(maxArenas, 2u), VmHeapCore::MaxArenas)) {
    arenas_[0] = new VmHeapArena();
    arenas_[0]->owners = 1;
    count_ = 1;
  }
  VmHeapArenas(const VmHeapArenas &) = delete;
  VmHeapArenas &operator=(const VmHeapArenas &) = delete;
  ~VmHeapArenas() {
    for (unsigned i = 0; i < count_; ++i) {
      delete arenas_[i];
    }
  }

  // Arena 0, the main thread's: until `acquire` is first called it is a plain heap with the VM's addresses, storage
  // that grows by moving, and no locks.
  VmHeapArena &main() { return *arenas_[0]; }

  // An arena for a thread about to start, called by the thread that starts it (so the first call, which moves
  // arena 0's storage into its reservation, runs while the main thread is the only one). An arena whose thread
  // ended is used again; past the last arena, threads share the last one, whose every operation is behind its
  // locks and whose string arenas are off. Null with `error` set when the address space cannot be reserved.
  VmHeapArena *acquire(std::string &error) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!shared_) {
      if (!share(*arenas_[0], 0, error)) {
        return nullptr;
      }
      shared_ = true;
    }
    for (unsigned i = 1; i < count_; ++i) {
      if (arenas_[i]->owners == 0) {
        arenas_[i]->owners = 1;
        return arenas_[i];
      }
    }
    if (count_ < maxArenas_) {
      VmHeapArena *arena = new VmHeapArena();
      if (!share(*arena, count_, error)) {
        delete arena;
        return nullptr;
      }
      if (count_ + 1 == maxArenas_) {
        arena->text.disableArenas();
      }
      arena->owners = 1;
      arenas_[count_] = arena;
      ++count_;
      return arena;
    }
    ++arenas_[count_ - 1]->owners;
    return arenas_[count_ - 1];
  }

  // The thread that held `arena` ended. Its live blocks stay valid, and blocks freed for it wait for its next owner.
  void release(VmHeapArena *arena) {
    std::lock_guard<std::mutex> guard(mutex_);
    --arena->owners;
  }

  unsigned arenaCount() {
    std::lock_guard<std::mutex> guard(mutex_);
    return count_;
  }

private:
  bool share(VmHeapArena &arena, unsigned index, std::string &error) {
    arena.index = index;
    arena.text.setLock(&arena.textLock);
    return arena.core.shareAsArena(index, table_, &arena.lock, &arena.text, slotCapacity_, byteCapacity_, error);
  }

  std::mutex mutex_;
  uint64_t slotCapacity_;
  uint64_t byteCapacity_;
  unsigned maxArenas_;
  unsigned count_ = 0;
  bool shared_ = false;
  VmHeapArena *arenas_[VmHeapCore::MaxArenas] = {};
  // The cores by arena number, read by every core without a lock (VmHeapCore::arenaOf).
  VmHeapCore *table_[VmHeapCore::MaxArenas] = {};
};
)
