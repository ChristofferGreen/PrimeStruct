#include "primec/runtime/VmHost.h"

#include <utility>

namespace primec {

const char *irHostValueKindName(IrHostValueKind kind) {
  switch (kind) {
  case IrHostValueKind::Void:
    return "void";
  case IrHostValueKind::I32:
    return "i32";
  case IrHostValueKind::I64:
    return "i64";
  case IrHostValueKind::U64:
    return "u64";
  case IrHostValueKind::F32:
    return "f32";
  case IrHostValueKind::F64:
    return "f64";
  case IrHostValueKind::Bool:
    return "bool";
  case IrHostValueKind::String:
    return "string";
  }
  return "unknown";
}

namespace {
std::string describeSignature(const std::vector<IrHostValueKind> &parameters, IrHostValueKind returnKind) {
  std::string text = "(";
  for (size_t i = 0; i < parameters.size(); ++i) {
    if (i != 0) {
      text += ", ";
    }
    text += irHostValueKindName(parameters[i]);
  }
  text += ") -> ";
  text += irHostValueKindName(returnKind);
  return text;
}
} // namespace

void VmHostFunctions::bind(std::string name, VmHostBinding binding) {
  bindings_[std::move(name)] = std::move(binding);
}

const VmHostBinding *VmHostFunctions::find(std::string_view name) const {
  const auto it = bindings_.find(name);
  return it == bindings_.end() ? nullptr : &it->second;
}

bool VmHostFunctions::verify(const IrModule &module, std::string &error) const {
  for (const IrHostImport &import : module.hostImports) {
    const VmHostBinding *binding = find(import.name);
    if (binding == nullptr || (!binding->invoke && !binding->invokeString)) {
      error = "unbound host function: " + import.name + " " + describeSignature(import.parameters, import.returnKind);
      return false;
    }
    if (binding->parameters != import.parameters || binding->returnKind != import.returnKind) {
      error = "host function signature mismatch for " + import.name + ": script declares " +
              describeSignature(import.parameters, import.returnKind) + " but the host bound " +
              describeSignature(binding->parameters, binding->returnKind);
      return false;
    }
  }
  return true;
}

} // namespace primec
