#include "primec/embed/Script.h"

#include "ScriptModule.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace primec::embed {

void HostBindings::bindRaw(std::string name, std::vector<HostType> parameters, HostType returnType, RawInvoke invoke) {
  Entry fresh;
  fresh.name = std::move(name);
  fresh.parameters = std::move(parameters);
  fresh.returnType = returnType;
  fresh.invoke = std::move(invoke);
  for (Entry &entry : entries_) {
    if (entry.name == fresh.name) {
      entry = std::move(fresh);
      return;
    }
  }
  entries_.push_back(std::move(fresh));
}

void HostBindings::bindRawString(std::string name, std::vector<HostType> parameters, RawStringInvoke invoke) {
  Entry fresh;
  fresh.name = std::move(name);
  fresh.parameters = std::move(parameters);
  fresh.returnType = HostType::String;
  fresh.invokeString = std::move(invoke);
  for (Entry &entry : entries_) {
    if (entry.name == fresh.name) {
      entry = std::move(fresh);
      return;
    }
  }
  entries_.push_back(std::move(fresh));
}

namespace detail {
const char *hostTypeSpelling(HostType type) {
  switch (type) {
  case HostType::Void:
    return "void";
  case HostType::I32:
    return "i32";
  case HostType::I64:
    return "i64";
  case HostType::U64:
    return "u64";
  case HostType::F32:
    return "f32";
  case HostType::F64:
    return "f64";
  case HostType::Bool:
    return "bool";
  case HostType::String:
    return "string";
  }
  return "unknown";
}
} // namespace detail

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
  case HostType::String:
    return IrHostValueKind::String;
  }
  return IrHostValueKind::Void;
}

bool isEngineGeneratedImport(const std::string &name) {
  return name.rfind(detail::ExportArgPrefix, 0) == 0 || name.rfind(detail::ExportResultPrefix, 0) == 0;
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
    if (entry.invokeString) {
      binding.invokeString = [invoke = entry.invokeString](const uint64_t *args, std::string &result, std::string &error) {
        return invoke(args, result, error);
      };
    }
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

std::string describeSignature(const std::string &name, const ExportSignature &signature) {
  std::string text = name + "(";
  for (size_t i = 0; i < signature.parameters.size(); ++i) {
    text += (i == 0 ? "" : ", ");
    text += detail::hostTypeSpelling(signature.parameters[i]);
  }
  return text + ") -> " + detail::hostTypeSpelling(signature.returnType);
}

std::string describeTypes(const std::vector<HostType> &types, HostType returnType) {
  std::string text = "(";
  for (size_t i = 0; i < types.size(); ++i) {
    text += (i == 0 ? "" : ", ");
    text += detail::hostTypeSpelling(types[i]);
  }
  return text + ") -> " + detail::hostTypeSpelling(returnType);
}

// User bindings plus self-matching stubs for engine-generated imports, so a
// module's remaining imports can be verified without a call in flight.
VmHostFunctions functionsForVerification(const HostBindings &bindings, const IrModule &module) {
  VmHostFunctions functions = toVmHostFunctions(bindings);
  for (const IrHostImport &import : module.hostImports) {
    if (isEngineGeneratedImport(import.name)) {
      VmHostBinding stub{import.parameters, import.returnKind,
                         [](const uint64_t *, uint64_t &, std::string &) { return true; }};
      if (import.returnKind == IrHostValueKind::String) {
        stub.invokeString = [](const uint64_t *, std::string &, std::string &) { return true; };
      }
      functions.bind(import.name, std::move(stub));
    }
  }
  return functions;
}
} // namespace

std::vector<std::string> Script::requiredHostFunctions() const {
  std::vector<std::string> names;
  auto addFrom = [&names](const IrModule &ir) {
    for (const IrHostImport &import : ir.hostImports) {
      if (isEngineGeneratedImport(import.name)) {
        continue;
      }
      const std::string text = describeImport(import);
      if (std::find(names.begin(), names.end(), text) == names.end()) {
        names.push_back(text);
      }
    }
  };
  if (module_ != nullptr) {
    addFrom(module_->ir);
  }
  if (exports_ != nullptr) {
    for (const auto &entry : exports_->entries) {
      addFrom(entry.module->ir);
    }
  }
  return names;
}

std::vector<std::string> Script::exportedFunctions() const {
  std::vector<std::string> names;
  if (exports_ != nullptr) {
    for (const auto &entry : exports_->entries) {
      names.push_back(describeSignature(entry.name, entry.signature));
    }
  }
  return names;
}

bool Script::checkHostBindings(std::string &error) const {
  if (!valid()) {
    error = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return false;
  }
  if (module_ != nullptr && !functionsForVerification(hostBindings_, module_->ir).verify(module_->ir, error)) {
    return false;
  }
  if (exports_ != nullptr) {
    for (const auto &entry : exports_->entries) {
      if (!functionsForVerification(hostBindings_, entry.module->ir).verify(entry.module->ir, error)) {
        return false;
      }
    }
  }
  return true;
}

ScriptResult Script::run(const std::vector<std::string> &args) const {
  ScriptResult result;
  if (module_ == nullptr) {
    result.diagnostics = diagnostics_.empty() ? (valid() ? "script has no main entry; use call()"
                                                          : "script was not compiled successfully")
                                              : diagnostics_;
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

bool Script::callRaw(std::string_view name,
                     const std::vector<HostType> &argumentTypes,
                     const std::vector<uint64_t> &argumentSlots,
                     const std::vector<std::string> &strings,
                     HostType returnType,
                     uint64_t &result,
                     std::string &stringResult,
                     std::string &error) const {
  if (!valid()) {
    error = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return false;
  }
  const ExportTable::Entry *entry = nullptr;
  if (exports_ != nullptr) {
    for (const auto &candidate : exports_->entries) {
      if (candidate.name == name) {
        entry = &candidate;
        break;
      }
    }
  }
  if (entry == nullptr) {
    error = "unknown exported function: " + std::string(name);
    return false;
  }
  if (entry->signature.parameters != argumentTypes || entry->signature.returnType != returnType) {
    error = "exported function " + describeSignature(entry->name, entry->signature) + " called as " +
            describeTypes(argumentTypes, returnType);
    return false;
  }

  // String arguments are served from `strings` through the reserved __psarg_string
  // host function, which hands the VM a run-time string per fetch; the compiled
  // module is shared and never copied per call.
  const std::vector<uint64_t> &arguments = argumentSlots;
  const IrModule *moduleToRun = &entry->module->ir;
  for (size_t i = 0; i < argumentTypes.size(); ++i) {
    if (argumentTypes[i] == HostType::String && arguments[i] >= strings.size()) {
      error = "internal error: string argument out of range";
      return false;
    }
  }

  VmHostFunctions functions = toVmHostFunctions(hostBindings_);
  uint64_t captured = 0;
  std::string capturedString;
  for (const HostType type : entry->signature.parameters) {
    const std::string importName = std::string(detail::ExportArgPrefix) + detail::hostTypeSpelling(type);
    if (type == HostType::String) {
      VmHostBinding binding;
      binding.parameters = {IrHostValueKind::I32};
      binding.returnKind = IrHostValueKind::String;
      binding.invokeString = [&arguments, &strings](const uint64_t *args, std::string &out, std::string &err) {
        const auto index = static_cast<uint64_t>(static_cast<uint32_t>(args[0]));
        if (index >= arguments.size() || arguments[static_cast<size_t>(index)] >= strings.size()) {
          err = "argument index out of range";
          return false;
        }
        out = strings[static_cast<size_t>(arguments[static_cast<size_t>(index)])];
        return true;
      };
      functions.bind(importName, std::move(binding));
      continue;
    }
    functions.bind(importName,
                   VmHostBinding{{IrHostValueKind::I32}, toIrKind(type),
                                 [&arguments](const uint64_t *args, uint64_t &out, std::string &err) {
                                   const auto index = static_cast<uint64_t>(static_cast<uint32_t>(args[0]));
                                   if (index >= arguments.size()) {
                                     err = "argument index out of range";
                                     return false;
                                   }
                                   out = arguments[static_cast<size_t>(index)];
                                   return true;
                                 }});
  }
  if (returnType != HostType::Void) {
    const std::string importName = std::string(detail::ExportResultPrefix) + detail::hostTypeSpelling(returnType);
    functions.bind(importName, VmHostBinding{{toIrKind(returnType)}, IrHostValueKind::Void,
                                              [&captured, &capturedString, returnType](const uint64_t *args, uint64_t &,
                                                                                       std::string &) {
                                                if (returnType == HostType::String) {
                                                  capturedString = *reinterpret_cast<const std::string *>(
                                                      static_cast<uintptr_t>(args[0]));
                                                } else {
                                                  captured = args[0];
                                                }
                                                return true;
                                              }});
  }
  const std::vector<std::string_view> views{name_};
  Vm vm;
  uint64_t exitValue = 0;
  std::string vmError;
  if (!vm.execute(*moduleToRun, exitValue, vmError, views, functions)) {
    error = "VM error: " + vmError;
    return false;
  }
  result = captured;
  stringResult = std::move(capturedString);
  return true;
}

namespace {
constexpr uint32_t BundleVersion = 1;
constexpr char BundleMagic[4] = {'P', 'S', 'B', 'N'};

void putU32(std::vector<uint8_t> &out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
  }
}

void putBlob(std::vector<uint8_t> &out, const std::vector<uint8_t> &blob) {
  putU32(out, static_cast<uint32_t>(blob.size()));
  out.insert(out.end(), blob.begin(), blob.end());
}

class BundleReader {
public:
  explicit BundleReader(const std::vector<uint8_t> &data) : data_(data) {}
  size_t remaining() const { return data_.size() - offset_; }
  bool u32(uint32_t &value) {
    if (remaining() < 4) {
      return false;
    }
    value = 0;
    for (int i = 0; i < 4; ++i) {
      value |= static_cast<uint32_t>(data_[offset_ + static_cast<size_t>(i)]) << (8 * i);
    }
    offset_ += 4;
    return true;
  }
  bool u8(uint8_t &value) {
    if (remaining() < 1) {
      return false;
    }
    value = data_[offset_++];
    return true;
  }
  bool blob(std::vector<uint8_t> &out) {
    uint32_t length = 0;
    if (!u32(length) || length > remaining()) {
      return false;
    }
    out.assign(data_.begin() + static_cast<std::ptrdiff_t>(offset_),
               data_.begin() + static_cast<std::ptrdiff_t>(offset_ + length));
    offset_ += length;
    return true;
  }
  bool text(std::string &out) {
    std::vector<uint8_t> bytes;
    if (!blob(bytes)) {
      return false;
    }
    out.assign(bytes.begin(), bytes.end());
    return true;
  }

private:
  const std::vector<uint8_t> &data_;
  size_t offset_ = 0;
};

std::shared_ptr<Script::Module> decodeModule(const std::vector<uint8_t> &bytes, std::string &error) {
  auto module = std::make_shared<Script::Module>();
  if (!deserializeIr(bytes, module->ir, error)) {
    error = "invalid bytecode: " + error;
    return nullptr;
  }
  if (!validateIrModule(module->ir, IrValidationTarget::Vm, error)) {
    error = "bytecode failed VM validation: " + error;
    return nullptr;
  }
  return module;
}
} // namespace

bool Script::saveBytecode(std::vector<uint8_t> &out, std::string &error) const {
  if (!valid()) {
    error = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return false;
  }
  if (exports_ == nullptr) {
    return serializeIr(module_->ir, out, error);
  }
  // Bundle: magic, version, optional main module, then each export module.
  out.assign(BundleMagic, BundleMagic + 4);
  putU32(out, BundleVersion);
  std::vector<uint8_t> blob;
  putU32(out, module_ != nullptr ? 1u : 0u);
  if (module_ != nullptr) {
    if (!serializeIr(module_->ir, blob, error)) {
      return false;
    }
    putBlob(out, blob);
  }
  putU32(out, static_cast<uint32_t>(exports_->entries.size()));
  for (const auto &entry : exports_->entries) {
    putBlob(out, std::vector<uint8_t>(entry.name.begin(), entry.name.end()));
    putU32(out, static_cast<uint32_t>(entry.signature.parameters.size()));
    for (const HostType type : entry.signature.parameters) {
      out.push_back(static_cast<uint8_t>(type));
    }
    out.push_back(static_cast<uint8_t>(entry.signature.returnType));
    if (!serializeIr(entry.module->ir, blob, error)) {
      return false;
    }
    putBlob(out, blob);
  }
  return true;
}

Script Script::loadBytecode(const std::vector<uint8_t> &bytes, const std::string &name) {
  Script script;
  script.name_ = name;
  std::string error;
  const bool isBundle = bytes.size() >= 4 && std::equal(BundleMagic, BundleMagic + 4, bytes.begin());
  if (!isBundle) {
    auto module = decodeModule(bytes, error);
    if (module == nullptr) {
      script.diagnostics_ = error;
      return script;
    }
    script.module_ = std::move(module);
    return script;
  }

  BundleReader reader(bytes);
  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t hasMain = 0;
  auto fail = [&](const std::string &detail) {
    // A partially decoded bundle must not leave a half-valid script behind.
    script.module_.reset();
    script.exports_.reset();
    script.diagnostics_ = "invalid bytecode bundle: " + detail;
    return script;
  };
  if (!reader.u32(magic) || !reader.u32(version) || version != BundleVersion) {
    return fail("unsupported version");
  }
  if (!reader.u32(hasMain) || hasMain > 1) {
    return fail("truncated header");
  }
  if (hasMain == 1) {
    std::vector<uint8_t> blob;
    if (!reader.blob(blob)) {
      return fail("truncated main module");
    }
    auto module = decodeModule(blob, error);
    if (module == nullptr) {
      return fail(error);
    }
    script.module_ = std::move(module);
  }
  uint32_t exportCount = 0;
  if (!reader.u32(exportCount) || exportCount > reader.remaining()) {
    return fail("truncated export count");
  }
  auto table = std::make_shared<ExportTable>();
  for (uint32_t i = 0; i < exportCount; ++i) {
    ExportTable::Entry entry;
    uint32_t parameterCount = 0;
    if (!reader.text(entry.name) || !reader.u32(parameterCount) || parameterCount > reader.remaining()) {
      return fail("truncated export signature");
    }
    for (uint32_t p = 0; p < parameterCount; ++p) {
      uint8_t type = 0;
      if (!reader.u8(type) || type > static_cast<uint8_t>(HostType::String)) {
        return fail("invalid export parameter type");
      }
      entry.signature.parameters.push_back(static_cast<HostType>(type));
    }
    uint8_t returnType = 0;
    if (!reader.u8(returnType) || returnType > static_cast<uint8_t>(HostType::String)) {
      return fail("invalid export return type");
    }
    entry.signature.returnType = static_cast<HostType>(returnType);
    std::vector<uint8_t> blob;
    if (!reader.blob(blob)) {
      return fail("truncated export module");
    }
    entry.module = decodeModule(blob, error);
    if (entry.module == nullptr) {
      return fail(error);
    }
    table->entries.push_back(std::move(entry));
  }
  if (reader.remaining() != 0) {
    return fail("trailing bytes");
  }
  script.exports_ = std::move(table);
  if (script.module_ == nullptr && script.exports_->entries.empty()) {
    return fail("bundle contains no modules");
  }
  return script;
}

} // namespace primec::embed
