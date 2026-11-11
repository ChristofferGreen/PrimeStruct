// The VM heap allocator, written once and used twice (see VmHeapCore.h): compiled into the VM,
// the debugger and the native JIT's runtime, and pasted as text into the programs the C++
// backends generate. It must stay self-contained: standard headers only, no preprocessor lines,
// balanced parentheses.
PRIMEC_VM_HEAP_CORE_BODY(class VmHeapCore {
public:
  static constexpr uint64_t SlotBytes = 16;
  static constexpr uint64_t Tag = uint64_t{1} << 63;
  static constexpr unsigned GenerationShift = 48;
  static constexpr uint64_t GenerationMask = 0x7FFF;
  static constexpr uint64_t OffsetMask = (uint64_t{1} << GenerationShift) - 1;
  // Arenas (programs whose tasks run on threads, VmHeapArenas.h): the arena number is address bits 41 to 46, inside
  // the offset field, so an arena addresses 2^41 bytes of slots and 2^41 bytes of byte regions. The VM has arena 0
  // alone, whose addresses carry no arena bits.
  static constexpr unsigned ArenaShift = 41;
  static constexpr unsigned MaxArenas = 64;
  static constexpr uint64_t ArenaSpan = uint64_t{1} << ArenaShift;
  static constexpr uint64_t MaxSlots = ArenaSpan / SlotBytes;
  static constexpr uint16_t LiveBit = 0x8000;

  // The storage of the slots and their states: memory from calloc, which is zero already, so growing the vectors
  // writes nothing (a value-initialized element is left as it is). A large block comes as zero pages from the system,
  // which cost nothing until they are written: the parts of an allocation a program never stores to stay free.
  // From MappedBytes on the block is mapped without reserving memory for it, so an arena can reserve the address
  // space its storage will ever need (shareAsArena).
  template <typename T>
  struct ZeroedAllocator {
    using value_type = T;
    ZeroedAllocator() = default;
    template <typename U>
    ZeroedAllocator(const ZeroedAllocator<U> &) {}
    static constexpr std::size_t MappedBytes = std::size_t{1} << 30;
    T *allocate(std::size_t count) {
      void *memory = nullptr;
      if (count >= MappedBytes / sizeof(T)) {
        memory = mmap(nullptr, count * sizeof(T), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
        if (memory == MAP_FAILED) {
          memory = nullptr;
        }
      } else {
        memory = std::calloc(count, sizeof(T));
      }
      if (memory == nullptr) {
        throw std::bad_alloc();
      }
      return static_cast<T *>(memory);
    }
    void deallocate(T *memory, std::size_t count) {
      if (count >= MappedBytes / sizeof(T)) {
        munmap(memory, count * sizeof(T));
      } else {
        std::free(memory);
      }
    }
    template <typename U>
    void construct(U *) {}
    template <typename U, typename... Args>
    void construct(U *place, Args &&...args) {
      ::new (static_cast<void *>(place)) U(std::forward<Args>(args)...);
    }
    template <typename U>
    bool operator==(const ZeroedAllocator<U> &) const { return true; }
    template <typename U>
    bool operator!=(const ZeroedAllocator<U> &) const { return false; }
  };

  // One value per slot, and per slot `LiveBit | generation` while an allocation holds it, or the
  // generation it was last allocated with once freed (zero if never allocated). A live address
  // is Tag | generation << 48 | slot * 16, so `states[slot] == address >> 48` exactly when the
  // address names a live slot with the generation it was allocated with.
  std::vector<uint64_t, ZeroedAllocator<uint64_t>> slots;
  std::vector<uint16_t, ZeroedAllocator<uint16_t>> states;

  // The slot a tagged, 16-byte aligned address names, or null when it is outside the heap,
  // free, or from an earlier allocation of the slot. The state is read as a relaxed atomic (and written as one), so
  // another arena's thread may ask too; an address of another arena is answered by that arena's table.
  uint64_t *slotAt(uint64_t address) {
    const uint64_t index = (address & OffsetMask) / SlotBytes - slotBase_;
    if (index < slotBound_ && __atomic_load_n(states.data() + index, __ATOMIC_RELAXED) ==
                                  static_cast<uint16_t>(address >> GenerationShift)) [[likely]] {
      // A caller's null test of this path is one more branch in every slot access, so say what holds.
      uint64_t *slot = slots.data() + index;
      if (slot == nullptr) {
        __builtin_unreachable();
      }
      return slot;
    }
    return slotAtRemote(address);
  }

  // ---- arenas ----
  // The arena's lock, taken only when there is one (never in the VM).
  struct Guard {
    std::mutex *lock;
    explicit Guard(std::mutex *held) : lock(held) {
      if (lock != nullptr) {
        lock->lock();
      }
    }
    ~Guard() {
      if (lock != nullptr) {
        lock->unlock();
      }
    }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
  };

  // Makes this heap arena `index` of `table` (VmHeapArenas): its storage moves, for the last time, into a
  // reservation of `slotCapacity` slots and `byteCapacity` bytes, every use of its maps takes `lock` from now on, and
  // blocks of other arenas are found through `table`. Called while no other thread uses this heap; arena 0 may hold
  // blocks already, any other arena must be empty. `companion` is the arena's string heap.
  bool shareAsArena(unsigned index, VmHeapCore **table, std::mutex *lock, void *companion, uint64_t slotCapacity,
                    uint64_t byteCapacity, std::string & error) {
    if (index >= MaxArenas || (index != 0 && (!slots.empty() || !bytes.empty()))) {
      error = "invalid thread heap arena";
      return false;
    }
    slotCapacity = std::min(slotCapacity, MaxSlots);
    byteCapacity = std::min(byteCapacity, ArenaSpan);
    try {
      slots.reserve(static_cast<size_t>(slotCapacity));
      states.reserve(slots.capacity());
      bytes.reserve(static_cast<size_t>(byteCapacity));
      byteStates.reserve((bytes.capacity() + GranuleBytes - 1) >> GranuleShift);
    } catch (const std::bad_alloc &) {
      error = "cannot reserve address space for a thread heap";
      return false;
    }
    arena_ = index;
    arenaBits_ = static_cast<uint64_t>(index) << ArenaShift;
    slotBase_ = arenaBits_ / SlotBytes;
    byteBase_ = arenaBits_;
    slotBound_ = std::min(slots.capacity(), states.capacity());
    granuleBound_ = byteStates.capacity();
    pinned_ = true;
    lock_ = lock;
    companion_ = companion;
    arenas_ = table;
    __atomic_store_n(table + index, this, __ATOMIC_RELEASE);
    return true;
  }

  unsigned arena() const { return arena_; }
  uint64_t slowByteReads() const {
    return __atomic_load_n(&slowByteReads_, __ATOMIC_RELAXED);
  }
  void *companion() const { return companion_; }

  // Whether a heap address belongs to another arena.
  bool isRemote(uint64_t address) const {
    return arenas_ != nullptr && (address & Tag) != 0 && ((address >> ArenaShift) & (MaxArenas - 1)) != arena_;
  }
  // The heap of the arena an address names (null when there is no such arena); only for a heap that is an arena.
  VmHeapCore *arenaOf(uint64_t address) const {
    return __atomic_load_n(arenas_ + ((address >> ArenaShift) & (MaxArenas - 1)), __ATOMIC_ACQUIRE);
  }

  // Frees a block of another arena: it goes on that arena's queue, and its owner frees it before its next
  // allocation (drainRemoteFrees), where a block that is not live fails with the usual message. `text` marks a
  // string heap block.
  bool releaseRemote(uint64_t address, bool text, std::string & error) {
    VmHeapCore *owner = arenaOf(address);
    if (owner == nullptr) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    const Guard guard(owner->lock_);
    owner->remoteFrees_.emplace_back(address, text);
    __atomic_store_n(&owner->hasRemoteFrees_, true, __ATOMIC_RELAXED);
    return true;
  }
  // Whether the heap is one of several arenas: only then does it lock, and can it have remote frees.
  bool threaded() const { return lock_ != nullptr; }
  bool hasRemoteFrees() const { return __atomic_load_n(&hasRemoteFrees_, __ATOMIC_RELAXED); }
  void takeRemoteFrees(std::vector<std::pair<uint64_t, bool>> & out) {
    const Guard guard(lock_);
    out.swap(remoteFrees_);
    __atomic_store_n(&hasRemoteFrees_, false, __ATOMIC_RELAXED);
  }

  // `count` zeroed slots, reusing freed ones (the smallest free run that fits, else the free run
  // at the end of the heap extended, else new slots). Zero slots allocate nothing: address 0.
  bool allocate(uint64_t count, uint64_t & address, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return allocateOwn(count, address, error);
    }
    const Guard guard(lock_);
    return allocateOwn(count, address, error);
  }
  // `count` zeroed bytes (address 0 for a count of 0).
  bool allocateBytes(uint64_t count, uint64_t & address, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return allocateBytesOwn(count, address, error);
    }
    const Guard guard(lock_);
    return allocateBytesOwn(count, address, error);
  }
  // A new byte region of `count` bytes holding the old one's leading bytes; the old one is freed.
  bool reallocateBytes(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return reallocateBytesOwn(address, count, newAddress, error);
    }
    if (isRemote(address)) {
      return reallocateRemote(address, count, newAddress, error);
    }
    const Guard guard(lock_);
    return reallocateBytesOwn(address, count, newAddress, error);
  }
  bool releaseBytes(uint64_t address, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return releaseBytesOwn(address, error);
    }
    if (isRemote(address)) {
      return releaseRemote(address, false, error);
    }
    const Guard guard(lock_);
    return releaseBytesOwn(address, error);
  }
  // Frees the allocation that starts at `address` (0 is a no-op); its slots are zeroed. A byte
  // region address frees that region.
  bool release(uint64_t address, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return releaseOwn(address, error);
    }
    if (isRemote(address)) {
      return releaseRemote(address, false, error);
    }
    const Guard guard(lock_);
    return releaseOwn(address, error);
  }
  // A new allocation of `count` slots holding the old one's leading values; the old one is freed.
  bool reallocate(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    if (lock_ == nullptr) [[likely]] {
      return reallocateOwn(address, count, newAddress, error);
    }
    if (isRemote(address)) {
      return reallocateRemote(address, count, newAddress, error);
    }
    const Guard guard(lock_);
    return reallocateOwn(address, count, newAddress, error);
  }

private:
  // The first slot of this arena in the offset field, and the slots a state can be read for: the heap's size, or
  // once the storage is reserved its capacity (states past the size are zero, which no address matches).
  uint64_t slotBase_ = 0;
  uint64_t slotBound_ = 0;
  uint64_t byteBase_ = 0;
  uint64_t granuleBound_ = 0;
  uint64_t arenaBits_ = 0;
  // The last other arena whose bytes this heap read, so a byte of another thread's block is answered in line like
  // one of this arena's own. An arena's storage is reserved once and lives as long as the arenas do (VmHeapArenas),
  // so its offsets, bounds and storage never change; one pointer is the whole cache, so threads that share this heap
  // (the last arena) can overwrite it without a torn state.
  VmHeapCore *remoteOwner_ = nullptr;
  // Byte accesses that neither this arena's granule states nor the remote cache answered (a fault, a granule a block
  // only partly fills in another arena, or the first read of an arena): a counting test reads it.
  uint64_t slowByteReads_ = 0;
  unsigned arena_ = 0;
  bool pinned_ = false;
  bool hasRemoteFrees_ = false;
  std::mutex *lock_ = nullptr;
  VmHeapCore **arenas_ = nullptr;
  void *companion_ = nullptr;
  std::vector<std::pair<uint64_t, bool>> remoteFrees_;

  // Out of line and cold, so slotAt's own answer is the straight path of its caller; and pure (it only reads), so
  // a caller's loop need not read again what it knew before the call.
  [[gnu::noinline, gnu::cold, gnu::pure]] uint64_t *slotAtRemote(uint64_t address) const {
    if (arenas_ == nullptr || (address & Tag) == 0 || isByteAddress(address)) {
      return nullptr;
    }
    VmHeapCore *owner = arenaOf(address);
    if (owner == nullptr || owner == this) {
      return nullptr;
    }
    const uint64_t index = (address & OffsetMask) / SlotBytes - owner->slotBase_;
    if (index < owner->slotBound_ && __atomic_load_n(owner->states.data() + index, __ATOMIC_RELAXED) ==
                                         static_cast<uint16_t>(address >> GenerationShift)) {
      return owner->slots.data() + index;
    }
    return nullptr;
  }

  // The heap that answers for a byte address: this one, or the address's arena (null when there is none).
  VmHeapCore *ownerOf(uint64_t address) {
    return isRemote(address) ? arenaOf(address) : this;
  }

  // Grows a block of another arena: a new block here with its leading values, and the old one freed remotely.
  bool reallocateRemote(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    VmHeapCore *owner = arenaOf(address);
    const bool isBytes = isByteAddress(address);
    uint64_t oldBase = 0;
    uint64_t oldCount = 0;
    bool found = false;
    if (owner != nullptr) {
      const Guard guard(owner->lock_);
      const auto block = isBytes ? owner->liveByteRegionAt(address) : owner->liveAllocationAt(address);
      if (block != (isBytes ? owner->byteLive_.end() : owner->live_.end())) {
        found = true;
        oldBase = block->first;
        oldCount = block->second.first;
      }
    }
    if (!found) {
      error = "invalid heap realloc address in IR: " + std::to_string(address);
      return false;
    }
    newAddress = 0;
    if (count != 0) {
      const Guard guard(lock_);
      if (!(isBytes ? allocateBytesOwn(count, newAddress, error) : allocateOwn(count, newAddress, error))) {
        return false;
      }
      const uint64_t copied = std::min(oldCount, count);
      if (isBytes) {
        std::copy(owner->bytes.data() + oldBase, owner->bytes.data() + oldBase + copied,
                  bytes.data() + byteOffsetOf(newAddress));
      } else {
        std::copy(owner->slots.data() + oldBase, owner->slots.data() + oldBase + copied,
                  slots.data() + slotIndexOf(newAddress));
      }
    }
    return releaseRemote(address, false, error);
  }

  uint64_t slotIndexOf(uint64_t address) const { return (address & OffsetMask) / SlotBytes - slotBase_; }
  uint64_t byteOffsetOf(uint64_t address) const { return (address & ByteOffsetMask) - byteBase_; }

  bool allocateOwn(uint64_t count, uint64_t & address, std::string & error) {
    if (count == 0) {
      address = 0;
      return true;
    }
    uint64_t base = 0;
    uint64_t generation = 0;
    bool reused = false;
    const auto fit = freeBySize_.lower_bound(std::make_pair(count, uint64_t{0}));
    if (fit != freeBySize_.end()) {
      base = fit->second;
      takeFree(base, count);
      reused = true;
    } else {
      base = slots.size();
      uint64_t tailFree = 0;
      if (!freeByBase_.empty()) {
        const auto last = std::prev(freeByBase_.end());
        if (last->first + last->second == slots.size()) {
          base = last->first;
          tailFree = last->second;
        }
      }
      if (count > MaxSlots || base > MaxSlots - count || (pinned_ && base + count > slotBound_)) {
        error = "VM heap allocation overflow";
        return false;
      }
      if (tailFree != 0) {
        takeFree(base, tailFree);
        reused = true;
      }
      growStorage(base + count);
    }
    if (reused) {
      // A generation no earlier allocation of these slots had (until it wraps).
      for (uint64_t i = base; i < base + count; ++i) {
        generation =
            std::max<uint64_t>(generation, states[static_cast<size_t>(i)] & GenerationMask);
      }
      generation = (generation + 1) & GenerationMask;
    }
    fillStates(states.data() + base, count, static_cast<uint16_t>(LiveBit | generation));
    live_[base] = std::make_pair(count, generation);
    address = Tag | (generation << GenerationShift) | arenaBits_ | (base * SlotBytes);
    return true;
  }

public:
  // ---- byte regions ----
  // A byte region is `count` zeroed bytes. Its address is Tag | generation << 48 | ByteRegionBit |
  // byte offset, so `address + i` names byte i, and the offset field is far beyond any slot index:
  // a slot access to a byte address misses in slotAt, and a byte access to a slot address misses in
  // byteAt, which is how the VM faults on mixing the two. Free runs are reused best fit and
  // coalesced like slot runs.
  static constexpr uint64_t ByteRegionBit = uint64_t{1} << 47;
  static constexpr uint64_t ByteOffsetMask = ByteRegionBit - 1;

  std::vector<uint8_t, ZeroedAllocator<uint8_t>> bytes;

  // One state per 64-byte granule of `bytes`, which answers a byte access with one load and compare and writes
  // nothing (the slots' `states` do the same for slots). While the bytes of exactly one live region lie in a granule,
  // its state is that region's `address >> 47` (Tag, generation and ByteRegionBit) in bits 15 to 31, the first and
  // one past the last of its bytes in the granule (lo in bits 0 to 5, hi in bits 8 to 14), and GranuleStartBit when
  // the region begins in the granule. A granule that holds no live byte, or bytes of two regions, is 0, and the
  // live-region map answers for it, exactly. (64 bytes rather than 16: a quarter of the state writes on allocation and
  // free, and 6% of the byte storage.)
  static constexpr unsigned GranuleShift = 6;
  static constexpr uint64_t GranuleBytes = uint64_t{1} << GranuleShift;
  static constexpr uint32_t GranuleStartBit = 0x80;
  static constexpr uint32_t GranuleRangeMask = 0x7F;
  static constexpr uint32_t FullGranule =
      static_cast<uint32_t>(GranuleBytes << 8); // the whole granule: lo 0, hi 64, no start bit
  std::vector<uint32_t, ZeroedAllocator<uint32_t>> byteStates;

  static bool isByteAddress(uint64_t address) {
    return (address & Tag) != 0 && (address & ByteRegionBit) != 0;
  }

  // The byte a byte-region address names, or null when the address is not one, is outside every
  // live region, or comes from an earlier allocation of those bytes.
  uint8_t *byteAt(uint64_t address) {
    const uint64_t offset = (address & ByteOffsetMask) - byteBase_;
    const uint64_t granule = offset >> GranuleShift;
    if (granule < granuleBound_ &&
        (__atomic_load_n(byteStates.data() + granule, __ATOMIC_RELAXED) | GranuleStartBit) ==
            ((static_cast<uint32_t>(address >> 47) << 15) | FullGranule | GranuleStartBit)) {
      uint8_t *byte = bytes.data() + offset;
      if (byte == nullptr) {
        __builtin_unreachable();
      }
      return byte;
    }
    // A granule of the last other arena read that its region fills (the common case of a long block another thread
    // allocated), answered like byteAt's own check.
    if (VmHeapCore *remote = __atomic_load_n(&remoteOwner_, __ATOMIC_RELAXED); remote != nullptr) {
      const uint64_t remoteOffset = (address & ByteOffsetMask) - remote->byteBase_;
      const uint64_t remoteGranule = remoteOffset >> GranuleShift;
      if (remoteGranule < remote->granuleBound_ &&
          (__atomic_load_n(remote->byteStates.data() + remoteGranule, __ATOMIC_RELAXED) |
           GranuleStartBit) ==
              ((static_cast<uint32_t>(address >> 47) << 15) | FullGranule | GranuleStartBit)) {
        return remote->bytes.data() + remoteOffset;
      }
    }
    return byteAtEdge(address);
  }

  // The first of `count` bytes starting at a byte-region address, or null unless every one of them lies in the one
  // live region of that address. A short span is checked granule by granule; a long one asks the map once, which
  // costs less than walking its granules.
  static constexpr uint64_t SpanGranuleCheckBytes = 128;
  uint8_t *byteSpan(uint64_t address, uint64_t count) {
    VmHeapCore *owner = arenas_ == nullptr ? this : ownerOf(address);
    return owner == nullptr ? nullptr : owner->byteSpanOwn(address, count);
  }

private:
  // Writes `count` states. Other threads read the states of a shared heap, so its writes are atomic ones; a heap
  // that is not shared has no such reader, and its plain stores are a fill the compiler vectorises.
  template <typename State>
  void fillStates(State *first, uint64_t count, uint64_t bits) {
    const State value = static_cast<State>(bits);
    if (lock_ == nullptr) [[likely]] {
      std::fill(first, first + count, value);
      return;
    }
    for (uint64_t i = 0; i < count; ++i) {
      __atomic_store_n(first + i, value, __ATOMIC_RELAXED);
    }
  }

  uint32_t granuleState(uint64_t granule) const {
    return __atomic_load_n(byteStates.data() + granule, __ATOMIC_RELAXED);
  }

  uint8_t *byteSpanOwn(uint64_t address, uint64_t count) {
    const uint64_t offset = byteOffsetOf(address);
    if (count > 1 && count <= SpanGranuleCheckBytes) {
      const uint64_t last = offset + count - 1;
      const uint64_t first = offset >> GranuleShift;
      const uint64_t lastGranule = last >> GranuleShift;
      if (first <= lastGranule && lastGranule < granuleBound_) {
        const uint32_t key = static_cast<uint32_t>(address >> 47);
        const uint32_t full = (key << 15) | FullGranule;
        uint32_t state = granuleState(first);
        bool fits =
            (state >> 15) == key && (offset & (GranuleBytes - 1)) >= (state & GranuleRangeMask);
        if (fits && lastGranule != first) {
          fits = ((state >> 8) & GranuleRangeMask) == GranuleBytes;
          for (uint64_t g = first + 1; fits && g < lastGranule; ++g) {
            fits = granuleState(g) == full;
          }
          state = granuleState(lastGranule);
          fits =
              fits && (state >> 15) == key && (state & (GranuleStartBit | GranuleRangeMask)) == 0;
        }
        if (fits && (last & (GranuleBytes - 1)) < ((state >> 8) & GranuleRangeMask)) {
          return bytes.data() + offset;
        }
      }
    }
    uint8_t *byte = byteAtOwn(address);
    if (byte == nullptr || count <= 1) {
      return byte;
    }
    if (lock_ == nullptr) [[likely]] {
      return spanFits(offset, count) ? byte : nullptr;
    }
    const Guard guard(lock_);
    return spanFits(offset, count) ? byte : nullptr;
  }

  // Whether the live region that holds the byte at `offset` holds `count` bytes from there on.
  bool spanFits(uint64_t offset, uint64_t count) const {
    auto region = byteLive_.upper_bound(offset);
    if (region == byteLive_.begin()) {
      return false;
    }
    --region;
    return offset < region->first + region->second.first && count <= region->first + region->second.first - offset;
  }

public:

  // Live byte-region bytes, for tests and instrumentation.
  uint64_t liveByteCount() const {
    const Guard guard(lock_);
    return liveBytes_;
  }
  // Live slot allocations (vector buffers, boxes), for tests and instrumentation.
  uint64_t liveSlotAllocationCount() const {
    const Guard guard(lock_);
    return live_.size();
  }

private:
  bool allocateBytesOwn(uint64_t count, uint64_t & address, std::string & error) {
    if (count == 0) {
      address = 0;
      return true;
    }
    // A region occupies a multiple of 8 bytes, so every region starts at a multiple of 8 (the storage does too): a
    // 64-bit value in a byte region, such as the owner count of a SharedString, is aligned for the atomic opcodes.
    // The region itself is exactly `count` bytes; the bytes after it up to the multiple belong to no region.
    const uint64_t footprint = (count + 7) & ~uint64_t{7};
    uint64_t base = 0;
    const auto fit = byteFreeBySize_.lower_bound(std::make_pair(footprint, uint64_t{0}));
    if (fit != byteFreeBySize_.end()) {
      base = fit->second;
      takeByteFree(base, footprint);
    } else {
      base = bytes.size();
      uint64_t tailFree = 0;
      if (!byteFreeByBase_.empty()) {
        const auto last = std::prev(byteFreeByBase_.end());
        if (last->first + last->second == bytes.size()) {
          base = last->first;
          tailFree = last->second;
        }
      }
      if (footprint > ArenaSpan || base > ArenaSpan - footprint || (pinned_ && base + footprint > bytes.capacity())) {
        error = "VM heap allocation overflow";
        return false;
      }
      if (tailFree != 0) {
        takeByteFree(base, tailFree);
      }
      bytes.resize(static_cast<size_t>(base + footprint));
      byteStates.resize(static_cast<size_t>((bytes.size() + GranuleBytes - 1) >> GranuleShift));
      if (!pinned_) {
        granuleBound_ = byteStates.size();
      }
    }
    byteGeneration_ = (byteGeneration_ + 1) & GenerationMask;
    byteLive_[base] = std::make_pair(count, byteGeneration_);
    liveBytes_ += count;
    address = Tag | ByteRegionBit | (byteGeneration_ << GenerationShift) | arenaBits_ | base;
    markGranules(base, count, static_cast<uint32_t>(address >> 47));
    return true;
  }

  bool reallocateBytesOwn(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    if (count == 0) {
      if (!releaseBytesOwn(address, error)) {
        return false;
      }
      newAddress = 0;
      return true;
    }
    const auto found = liveByteRegionAt(address);
    if (found == byteLive_.end()) {
      error = "invalid heap realloc address in IR: " + std::to_string(address);
      return false;
    }
    const uint64_t oldBase = found->first;
    const uint64_t oldCount = found->second.first;
    if (!allocateBytesOwn(count, newAddress, error)) {
      return false;
    }
    const uint64_t newBase = byteOffsetOf(newAddress);
    const uint64_t copied = std::min(oldCount, count);
    for (uint64_t i = 0; i < copied; ++i) {
      bytes[static_cast<size_t>(newBase + i)] = bytes[static_cast<size_t>(oldBase + i)];
    }
    releaseByteRegion(byteLive_.find(oldBase));
    return true;
  }

  bool releaseBytesOwn(uint64_t address, std::string & error) {
    const auto found = liveByteRegionAt(address);
    if (found == byteLive_.end()) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    releaseByteRegion(found);
    return true;
  }

  bool releaseOwn(uint64_t address, std::string & error) {
    if (address == 0) {
      return true;
    }
    if (isByteAddress(address)) {
      return releaseBytesOwn(address, error);
    }
    const auto found = liveAllocationAt(address);
    if (found == live_.end()) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    releaseAllocation(found);
    return true;
  }

  bool reallocateOwn(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    if (address == 0) {
      return allocateOwn(count, newAddress, error);
    }
    if (isByteAddress(address)) {
      return reallocateBytesOwn(address, count, newAddress, error);
    }
    if (count == 0) {
      if (!releaseOwn(address, error)) {
        return false;
      }
      newAddress = 0;
      return true;
    }
    const auto found = liveAllocationAt(address);
    if (found == live_.end()) {
      error = "invalid heap realloc address in IR: " + std::to_string(address);
      return false;
    }
    const uint64_t oldBase = found->first;
    const uint64_t oldCount = found->second.first;
    if (count > oldCount && growInPlace(found, count, newAddress)) {
      return true;
    }
    if (!allocateOwn(count, newAddress, error)) {
      return false;
    }
    const uint64_t newBase = slotIndexOf(newAddress);
    const uint64_t copied = std::min(oldCount, count);
    for (uint64_t i = 0; i < copied; ++i) {
      slots[static_cast<size_t>(newBase + i)] = slots[static_cast<size_t>(oldBase + i)];
    }
    releaseAllocation(live_.find(oldBase));
    return true;
  }

  using LiveMap = std::map<uint64_t, std::pair<uint64_t, uint64_t>>;

  // Live allocations by first slot: slot count and generation.
  LiveMap live_;
  // Free runs, coalesced: by first slot, and by (length, first slot) for best fit.
  std::map<uint64_t, uint64_t> freeByBase_;
  std::set<std::pair<uint64_t, uint64_t>> freeBySize_;

  // The slot storage holds at least `size` slots (new ones zero). It is reserved ahead in large steps (ReservedSlots,
  // then doubling), so a growing heap is rarely copied and leaves no buffers of earlier sizes behind in the allocator;
  // a reservation costs memory only where slots are written.
  static constexpr uint64_t ReservedSlots = uint64_t{1} << 20;
  void growStorage(uint64_t size) {
    if (size > slots.capacity()) {
      const uint64_t reserved =
          std::max(std::max(size, static_cast<uint64_t>(slots.capacity()) * 2), ReservedSlots);
      slots.reserve(static_cast<size_t>(reserved));
      states.reserve(static_cast<size_t>(reserved));
    }
    slots.resize(static_cast<size_t>(size));
    states.resize(static_cast<size_t>(size));
    if (!pinned_) {
      slotBound_ = size;
    }
  }

  // Grows a live allocation to `count` slots where it is when the slots after it are free (a free run long enough,
  // or one that reaches the end of the heap, which then grows): its values stay, the new slots are zero, and all of
  // them take a generation none of them had, so the old address faults as after a moving realloc.
  bool growInPlace(LiveMap::iterator allocation, uint64_t count, uint64_t & address) {
    const uint64_t base = allocation->first;
    const uint64_t end = base + allocation->second.first;
    const uint64_t extra = count - allocation->second.first;
    uint64_t runLength = 0;
    const auto run = freeByBase_.find(end);
    if (run != freeByBase_.end()) {
      runLength = run->second;
    }
    if (runLength < extra && end + runLength != slots.size()) {
      return false;
    }
    if (count > MaxSlots || base > MaxSlots - count || (pinned_ && base + count > slotBound_)) {
      return false;
    }
    if (runLength != 0) {
      takeFree(end, std::min(runLength, extra));
    }
    if (base + count > slots.size()) {
      growStorage(base + count);
    }
    uint64_t generation = 0;
    for (uint64_t i = base; i < base + count; ++i) {
      generation = std::max<uint64_t>(generation, states[static_cast<size_t>(i)] & GenerationMask);
    }
    generation = (generation + 1) & GenerationMask;
    fillStates(states.data() + base, count, static_cast<uint16_t>(LiveBit | generation));
    allocation->second = std::make_pair(count, generation);
    address = Tag | (generation << GenerationShift) | arenaBits_ | (base * SlotBytes);
    return true;
  }

  LiveMap::iterator liveAllocationAt(uint64_t address) {
    if ((address & Tag) == 0 || address % SlotBytes != 0) {
      return live_.end();
    }
    const auto found = live_.find(slotIndexOf(address));
    if (found == live_.end() ||
        found->second.second != ((address >> GenerationShift) & GenerationMask)) {
      return live_.end();
    }
    return found;
  }

  void releaseAllocation(LiveMap::iterator allocation) {
    uint64_t base = allocation->first;
    uint64_t count = allocation->second.first;
    const uint16_t generation = static_cast<uint16_t>(allocation->second.second);
    for (uint64_t i = base; i < base + count; ++i) {
      if (slots[static_cast<size_t>(i)] != 0) {
        slots[static_cast<size_t>(i)] = 0; // a slot never written stays an untouched page
      }
    }
    fillStates(states.data() + base, count, generation);
    live_.erase(allocation);
    const auto next = freeByBase_.find(base + count);
    if (next != freeByBase_.end()) {
      count += next->second;
      freeBySize_.erase(std::make_pair(next->second, next->first));
      freeByBase_.erase(next);
    }
    const auto after = freeByBase_.lower_bound(base);
    if (after != freeByBase_.begin()) {
      const auto previous = std::prev(after);
      if (previous->first + previous->second == base) {
        base = previous->first;
        count += previous->second;
        freeBySize_.erase(std::make_pair(previous->second, previous->first));
        freeByBase_.erase(previous);
      }
    }
    freeByBase_[base] = count;
    freeBySize_.insert(std::make_pair(count, base));
  }

  // Live byte regions by first byte offset: byte count and generation. Free byte runs, coalesced,
  // by first byte and by (length, first byte).
  LiveMap byteLive_;
  std::map<uint64_t, uint64_t> byteFreeByBase_;
  std::set<std::pair<uint64_t, uint64_t>> byteFreeBySize_;
  uint64_t byteGeneration_ = 0;
  uint64_t liveBytes_ = 0;

  LiveMap::iterator liveByteRegionAt(uint64_t address) {
    if (!isByteAddress(address)) {
      return byteLive_.end();
    }
    const auto found = byteLive_.find(byteOffsetOf(address));
    if (found == byteLive_.end() ||
        found->second.second != ((address >> GenerationShift) & GenerationMask)) {
      return byteLive_.end();
    }
    return found;
  }

  // A byte access byteAt's one compare did not answer (a granule its region only partly fills, or a fault): the granule state answers while the bytes of one region alone lie in it, else the map does.
  // With arenas, the address's own arena answers, and the map is read behind its lock.
  // This arena's granule is asked first and in line: short strings live in partly filled granules.
  uint8_t *byteAtEdge(uint64_t address) {
    const uint64_t offset = byteOffsetOf(address);
    const uint64_t granule = offset >> GranuleShift;
    if (granule < granuleBound_) {
      const uint32_t state = granuleState(granule);
      const uint32_t within = static_cast<uint32_t>(address & (GranuleBytes - 1));
      if ((state >> 15) == (address >> 47) &&
          within - (state & GranuleRangeMask) <
              ((state >> 8) & GranuleRangeMask) - (state & GranuleRangeMask)) {
        return bytes.data() + offset;
      }
    }
    return byteAtSlow(address);
  }

  [[gnu::noinline]] uint8_t *byteAtSlow(uint64_t address) {
    __atomic_fetch_add(&slowByteReads_, 1, __ATOMIC_RELAXED);
    if (arenas_ == nullptr) {
      return byteAtOwn(address);
    }
    VmHeapCore *owner = ownerOf(address);
    if (owner != nullptr && owner != this && owner->pinned_) {
      __atomic_store_n(&remoteOwner_, owner, __ATOMIC_RELAXED);
    }
    return owner == nullptr ? nullptr : owner->byteAtOwn(address);
  }

  uint8_t *byteAtOwn(uint64_t address) {
    const uint64_t offset = byteOffsetOf(address);
    const uint64_t granule = offset >> GranuleShift;
    if (granule < granuleBound_) [[likely]] {
      const uint32_t state = granuleState(granule);
      const uint32_t within = static_cast<uint32_t>(address & (GranuleBytes - 1));
      if ((state >> 15) == (address >> 47) &&
          within - (state & GranuleRangeMask) <
              ((state >> 8) & GranuleRangeMask) - (state & GranuleRangeMask)) [[likely]] {
        return bytes.data() + offset;
      }
    }
    return byteAtMap(address, offset);
  }

  // The byte at `offset` by the map of live regions: the answer for a granule whose state cannot tell.
  [[gnu::noinline]] uint8_t *byteAtMap(uint64_t address, uint64_t offset) {
    if (!isByteAddress(address)) {
      return nullptr;
    }
    const Guard guard(lock_);
    auto region = byteLive_.upper_bound(offset);
    if (region == byteLive_.begin()) {
      return nullptr;
    }
    --region;
    if (offset >= region->first + region->second.first ||
        region->second.second != ((address >> GenerationShift) & GenerationMask)) {
      return nullptr;
    }
    return bytes.data() + offset;
  }

  // The granule states of the `count` bytes at `base`, which just became live with `key` (an address >> 47) or,
  // with key 0, free. A granule wholly inside the run takes its state directly; the first and the last one may hold
  // bytes of a neighbour and are worked out from the map.
  void markGranules(uint64_t base, uint64_t count, uint32_t key) {
    const uint64_t first = base >> GranuleShift;
    const uint64_t lastGranule = (base + count - 1) >> GranuleShift;
    const uint32_t full = key == 0 ? 0 : (key << 15) | FullGranule;
    if (lastGranule > first + 1) {
      fillStates(byteStates.data() + first + 1, lastGranule - first - 1, full);
    }
    refreshGranule(first);
    if (lastGranule != first) {
      refreshGranule(lastGranule);
    }
  }

  void refreshGranule(uint64_t granule) {
    const uint64_t begin = granule << GranuleShift;
    const uint64_t end = begin + GranuleBytes;
    auto region = byteLive_.upper_bound(begin);
    if (region != byteLive_.begin() &&
        std::prev(region)->first + std::prev(region)->second.first > begin) {
      --region;
    }
    uint32_t state = 0;
    if (region != byteLive_.end() && region->first < end) {
      const auto next = std::next(region);
      if (next == byteLive_.end() || next->first >= end) {
        const uint64_t regionEnd = region->first + region->second.first;
        const uint64_t lo = region->first > begin ? region->first - begin : 0;
        const uint64_t hi = (regionEnd < end ? regionEnd : end) - begin;
        const uint64_t key =
            (Tag | ByteRegionBit | (region->second.second << GenerationShift)) >> 47;
        state = static_cast<uint32_t>((key << 15) | (hi << 8) | lo) |
                (region->first >= begin ? GranuleStartBit : 0);
      }
    }
    fillStates(byteStates.data() + granule, 1, state);
  }

  void releaseByteRegion(LiveMap::iterator region) {
    uint64_t base = region->first;
    const uint64_t regionBytes = region->second.first;
    uint64_t count = (regionBytes + 7) & ~uint64_t{7};
    for (uint64_t i = base; i < base + regionBytes; ++i) {
      bytes[static_cast<size_t>(i)] = 0;
    }
    liveBytes_ -= regionBytes;
    byteLive_.erase(region);
    markGranules(base, regionBytes, 0);
    const auto next = byteFreeByBase_.find(base + count);
    if (next != byteFreeByBase_.end()) {
      count += next->second;
      byteFreeBySize_.erase(std::make_pair(next->second, next->first));
      byteFreeByBase_.erase(next);
    }
    const auto after = byteFreeByBase_.lower_bound(base);
    if (after != byteFreeByBase_.begin()) {
      const auto previous = std::prev(after);
      if (previous->first + previous->second == base) {
        base = previous->first;
        count += previous->second;
        byteFreeBySize_.erase(std::make_pair(previous->second, previous->first));
        byteFreeByBase_.erase(previous);
      }
    }
    byteFreeByBase_[base] = count;
    byteFreeBySize_.insert(std::make_pair(count, base));
  }

  // Takes `count` bytes from the start of the free run at `base`; the rest stays free.
  void takeByteFree(uint64_t base, uint64_t count) {
    const auto run = byteFreeByBase_.find(base);
    const uint64_t length = run->second;
    byteFreeBySize_.erase(std::make_pair(length, base));
    byteFreeByBase_.erase(run);
    if (length > count) {
      byteFreeByBase_[base + count] = length - count;
      byteFreeBySize_.insert(std::make_pair(length - count, base + count));
    }
  }

  // Takes `count` slots from the start of the free run at `base`; the rest stays free.
  void takeFree(uint64_t base, uint64_t count) {
    const auto run = freeByBase_.find(base);
    const uint64_t length = run->second;
    freeBySize_.erase(std::make_pair(length, base));
    freeByBase_.erase(run);
    if (length > count) {
      freeByBase_[base + count] = length - count;
      freeBySize_.insert(std::make_pair(length - count, base + count));
    }
  }
};)
