#include "primec/embed/Script.h"

#include "ScriptModule.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include <string_view>
#include <utility>

namespace primec::embed {

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
  if (!vm.execute(module_->ir, value, error, views)) {
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
