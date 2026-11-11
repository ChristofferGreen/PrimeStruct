#pragma once

#include <cstdint>
#include <unordered_map>

namespace primec::ui {

// One answer of a script sent to a preview page: the window's count of unanswered scripts goes down by one. An answer
// that arrives after the window's entry was erased (its preview was dropped) changes nothing, so the count never goes
// below 0 and a later preview of that window starts from 0.
inline void finishPendingScript(std::unordered_map<uint64_t, int> &pending, uint64_t window) {
  const auto entry = pending.find(window);
  if (entry != pending.end() && entry->second > 0) {
    entry->second -= 1;
  }
}

} // namespace primec::ui
