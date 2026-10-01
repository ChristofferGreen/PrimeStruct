#include "primec/embed/Script.h"

#include "ScriptModule.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include <string_view>
#include <utility>

namespace primec::embed {

void HostBindings::bindRaw(std::string name, std::vector<HostType> parameters, HostType returnType, RawInvoke invoke) {
  for (Entry &entry : entries_) {
    if (entry.name == name) {
      entry = Entry{std::move(name), std::move(parameters), returnType, std::move(invoke)};
      return;
    }
  }
  entries_.push_back(Entry{std::move(name), std::move(parameters), returnType, std::move(invoke)});
}

namespace {
IrHostValueKind toIrKind(HostType type) {
  switch (type) {
  case HostType::Void:
    return IrHostValueKind::Void;
  case HostType::I32:
    return IrHostValueKind::I32;
  case HostType::I64:
    return IrHostValueKind::I64;
  case HostType::U64:
    return IrHostValueKind::U64;
  case HostType::F32:
    return IrHostValueKind::F32;
  case HostType::F64:
    return IrHostValueKind::F64;
  case HostType::Bool:
    return IrHostValueKind::Bool;
  }
  return IrHostValueKind::Void;
}

VmHostFunctions toVmHostFunctions(const HostBindings &bindings) {
  VmHostFunctions functions;
  for (const HostBindings::Entry &entry : bindings.entries()) {
    VmHostBinding binding;
    for (const HostType type : entry.parameters) {
      binding.parameters.push_back(toIrKind(type));
    }
    binding.returnKind = toIrKind(entry.returnType);
    binding.invoke = entry.invoke;
    functions.bind(entry.name, std::move(binding));
  }
  return functions;
}

std::string describeImport(const IrHostImport &import) {
  std::string text = import.name + "(";
  for (size_t i = 0; i < import.parameters.size(); ++i) {
    text += (i == 0 ? "" : ", ");
    text += irHostValueKindName(import.parameters[i]);
  }
  return text + ") -> " + irHostValueKindName(import.returnKind);
}
} // namespace

std::vector<std::string> Script::requiredHostFunctions() const {
  std::vector<std::string> names;
  if (valid()) {
    for (const IrHostImport &import : module_->ir.hostImports) {
      names.push_back(describeImport(import));
    }
  }
  return names;
}

bool Script::checkHostBindings(std::string &error) const {
  if (!valid()) {
    error = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return false;
  }
  return toVmHostFunctions(hostBindings_).verify(module_->ir, error);
}

ScriptResult Script::run(const std::vector<std::string> &args) const {
  ScriptResult result;
  if (!valid()) {
    result.diagnostics = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return result;
  }
  std::vector<std::string_view> views;
  views.reserve(args.size() + 1);
  views.push_back(name_);
  for (const auto &arg : args) {
    views.push_back(arg);
  }
  Vm vm;
  uint64_t value = 0;
  std::string error;
  const VmHostFunctions hostFunctions = toVmHostFunctions(hostBindings_);
  if (!vm.execute(module_->ir, value, error, views, hostFunctions)) {
    result.diagnostics = "VM error: " + error;
    return result;
  }
  result.ok = true;
  result.exitCode = static_cast<int>(static_cast<int32_t>(value));
  return result;
}

bool Script::saveBytecode(std::vector<uint8_t> &out, std::string &error) const {
  if (!valid()) {
    error = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return false;
  }
  return serializeIr(module_->ir, out, error);
}

Script Script::loadBytecode(const std::vector<uint8_t> &bytes, const std::string &name) {
  Script script;
  script.name_ = name;
  auto module = std::make_shared<Module>();
  std::string error;
  if (!deserializeIr(bytes, module->ir, error)) {
    script.diagnostics_ = "invalid bytecode: " + error;
    return script;
  }
  if (!validateIrModule(module->ir, IrValidationTarget::Vm, error)) {
    script.diagnostics_ = "bytecode failed VM validation: " + error;
    return script;
  }
  script.module_ = std::move(module);
  return script;
}

} // namespace primec::embed
