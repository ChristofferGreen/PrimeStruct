#pragma once

namespace primec::ir_lowerer {

// A call to a definition declared `return<Reference<T>>` (T a scalar) leaves the address of its referent on the stack.
// A value read of the call (a value binding, an operand, an argument of a value parameter) loads through that address;
// a consumer that wants the address itself (a `Reference` binding or parameter, `location`, `dereference`, a returned
// `Reference`) opens a ReferenceCallAddressScope around the `emitExpr` of the call, and the inline-call lowering
// leaves the address in place for the one call it emits first. The flag is thread-local: lowering runs on one thread.
inline bool &referenceCallAddressWanted() {
  thread_local bool wanted = false;
  return wanted;
}

class ReferenceCallAddressScope {
public:
  ReferenceCallAddressScope() : previous_(referenceCallAddressWanted()) {
    referenceCallAddressWanted() = true;
  }
  ~ReferenceCallAddressScope() {
    referenceCallAddressWanted() = previous_;
  }
  ReferenceCallAddressScope(const ReferenceCallAddressScope &) = delete;
  ReferenceCallAddressScope &operator=(const ReferenceCallAddressScope &) = delete;

private:
  bool previous_;
};

// Reads and clears the flag: the first inline call to be emitted owns it.
inline bool takeReferenceCallAddressWanted() {
  const bool wanted = referenceCallAddressWanted();
  referenceCallAddressWanted() = false;
  return wanted;
}

} // namespace primec::ir_lowerer
