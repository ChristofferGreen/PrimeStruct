#pragma once

#include <cstdint>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// Which local slots of a function can be reached through memory, and so must
// stay in the frame instead of being promoted to a register.
//
// Soundness argument. Local addresses are frame-relative byte offsets: the VM
// resolves LoadIndirect/StoreIndirect against the *current* frame's locals (or
// the tagged heap), and native code addresses the frame directly. A pointer to
// a slot of this function's frame can therefore only originate from an
// AddressOfLocal executed in this function. From there, user code may add any
// byte offset, negative ones included (`plus(location(v), 8i32)` is legal and
// the VM resolves whatever slot the sum names), and the address can travel
// through locals, calls and memory. This version does no pointer provenance
// analysis, so once any AddressOfLocal appears every slot of the function is
// treated as reachable. A function that never takes an address has no
// memory-reachable slot at all, with one exception: FileReadByte stores into
// the local its immediate names, a write the analysis cannot see as a
// StoreLocal, so that slot is pinned.
//
// `lowestAddressedSlot` records the smallest AddressOfLocal immediate so a
// later, more precise analysis (for example one that proves offsets are
// non-negative constants) can relax the rule without changing this interface.
struct IrLocalEscapeInfo {
  // Highest local index used by LoadLocal, StoreLocal, AddressOfLocal or
  // FileReadByte, plus one (the number of slots a frame needs).
  uint32_t localCount = 0;
  // Pinned slot indices, ascending and unique.
  std::vector<uint32_t> pinnedSlots;
  bool addressTaken = false;
  // Smallest AddressOfLocal immediate; meaningful only when `addressTaken`.
  uint32_t lowestAddressedSlot = 0;

  bool isPinned(uint32_t slot) const;
  // True when no slot is pinned: every local may live in a register.
  bool canPromoteAll() const { return pinnedSlots.empty(); }
};

IrLocalEscapeInfo analyzeIrLocalEscape(const IrFunction &function);

} // namespace primec
