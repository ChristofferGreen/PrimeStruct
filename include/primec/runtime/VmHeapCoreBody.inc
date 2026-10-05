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
  static constexpr uint64_t MaxSlots = (OffsetMask + 1) / SlotBytes;
  static constexpr uint16_t LiveBit = 0x8000;

  // One value per slot, and per slot `LiveBit | generation` while an allocation holds it, or the
  // generation it was last allocated with once freed (zero if never allocated). A live address
  // is Tag | generation << 48 | slot * 16, so `states[slot] == address >> 48` exactly when the
  // address names a live slot with the generation it was allocated with.
  std::vector<uint64_t> slots;
  std::vector<uint16_t> states;

  // The slot a tagged, 16-byte aligned address names, or null when it is outside the heap,
  // free, or from an earlier allocation of the slot.
  uint64_t *slotAt(uint64_t address) {
    const uint64_t index = (address & OffsetMask) / SlotBytes;
    if (index >= states.size() ||
        states[index] != static_cast<uint16_t>(address >> GenerationShift)) {
      return nullptr;
    }
    return &slots[index];
  }

  // `count` zeroed slots, reusing freed ones (the smallest free run that fits, else the free run
  // at the end of the heap extended, else new slots). Zero slots allocate nothing: address 0.
  bool allocate(uint64_t count, uint64_t & address, std::string & error) {
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
      if (count > MaxSlots || base > MaxSlots - count) {
        error = "VM heap allocation overflow";
        return false;
      }
      if (tailFree != 0) {
        takeFree(base, tailFree);
        reused = true;
      }
      slots.resize(static_cast<size_t>(base + count), 0);
      states.resize(static_cast<size_t>(base + count), 0);
    }
    if (reused) {
      // A generation no earlier allocation of these slots had (until it wraps).
      for (uint64_t i = base; i < base + count; ++i) {
        generation =
            std::max<uint64_t>(generation, states[static_cast<size_t>(i)] & GenerationMask);
      }
      generation = (generation + 1) & GenerationMask;
    }
    for (uint64_t i = base; i < base + count; ++i) {
      states[static_cast<size_t>(i)] = static_cast<uint16_t>(LiveBit | generation);
    }
    live_[base] = std::make_pair(count, generation);
    address = Tag | (generation << GenerationShift) | (base * SlotBytes);
    return true;
  }

  // Frees the allocation that starts at `address` (0 is a no-op); its slots are zeroed.
  bool release(uint64_t address, std::string & error) {
    if (address == 0) {
      return true;
    }
    const auto found = liveAllocationAt(address);
    if (found == live_.end()) {
      error = "invalid heap free address in IR: " + std::to_string(address);
      return false;
    }
    releaseAllocation(found);
    return true;
  }

  // A new allocation of `count` slots holding the old one's leading values; the old one is freed.
  bool reallocate(uint64_t address, uint64_t count, uint64_t & newAddress, std::string & error) {
    if (address == 0) {
      return allocate(count, newAddress, error);
    }
    if (count == 0) {
      if (!release(address, error)) {
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
    if (!allocate(count, newAddress, error)) {
      return false;
    }
    const uint64_t newBase = (newAddress & OffsetMask) / SlotBytes;
    const uint64_t copied = std::min(oldCount, count);
    for (uint64_t i = 0; i < copied; ++i) {
      slots[static_cast<size_t>(newBase + i)] = slots[static_cast<size_t>(oldBase + i)];
    }
    releaseAllocation(live_.find(oldBase));
    return true;
  }

private:
  using LiveMap = std::map<uint64_t, std::pair<uint64_t, uint64_t>>;

  // Live allocations by first slot: slot count and generation.
  LiveMap live_;
  // Free runs, coalesced: by first slot, and by (length, first slot) for best fit.
  std::map<uint64_t, uint64_t> freeByBase_;
  std::set<std::pair<uint64_t, uint64_t>> freeBySize_;

  LiveMap::iterator liveAllocationAt(uint64_t address) {
    if ((address & Tag) == 0 || address % SlotBytes != 0) {
      return live_.end();
    }
    const auto found = live_.find((address & OffsetMask) / SlotBytes);
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
      slots[static_cast<size_t>(i)] = 0;
      states[static_cast<size_t>(i)] = generation;
    }
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
