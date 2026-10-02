#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

// A host function the VM can call through IrOpcode::CallHost. Arguments arrive
// as raw 64-bit VM slots in declaration order: i32 sign-extended to 64 bits,
// i64/u64 as-is, f32 as the float bit pattern, f64 as the double bit pattern,
// bool as 0 or 1, string as a `const std::string *` into the module's string
// table (valid for the duration of the call). The result uses the same encoding
// (ignored for void). A string result is returned as text through
// VmHostBinding::invokeString.
using VmHostInvoke = std::function<bool(const uint64_t *args, uint64_t &result, std::string &error)>;

// Like VmHostInvoke for a function whose return kind is `string`: the host
// returns the text and the VM stores it as a run-time string (TODO-5364).
using VmHostStringInvoke = std::function<bool(const uint64_t *args, std::string &result, std::string &error)>;

struct VmHostBinding {
  std::vector<IrHostValueKind> parameters;
  IrHostValueKind returnKind = IrHostValueKind::Void;
  // For string-returning functions, set `invokeString` instead to return the
  // text itself; `invoke` then stays empty.
  VmHostInvoke invoke;
  VmHostStringInvoke invokeString = nullptr;
};

class VmHostFunctions {
public:
  // Adds or replaces the binding for `name`.
  void bind(std::string name, VmHostBinding binding);
  const VmHostBinding *find(std::string_view name) const;
  size_t size() const { return bindings_.size(); }

  // Checks that every host import of `module` is bound with the same signature.
  // Run before executing so an incomplete embedding fails without running any
  // instruction.
  bool verify(const IrModule &module, std::string &error) const;

private:
  std::map<std::string, VmHostBinding, std::less<>> bindings_;
};

const char *irHostValueKindName(IrHostValueKind kind);

} // namespace primec
