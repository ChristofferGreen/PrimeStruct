#include "IrToCppHostImports.h"

#include "primec/runtime/VmTextHeap.h"

#include <cctype>
#include <map>
#include <sstream>

namespace primec {
namespace {

bool isCIdentifier(const std::string &name) {
  if (name.empty() || (std::isalpha(static_cast<unsigned char>(name[0])) == 0 && name[0] != '_')) {
    return false;
  }
  for (const char c : name) {
    if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_') {
      return false;
    }
  }
  return true;
}

std::string signatureOf(const IrHostImport &import) {
  std::string signature = hostCType(import.returnKind, true) != nullptr ? hostCType(import.returnKind, true) : "?";
  signature += "(";
  for (size_t i = 0; i < import.parameters.size(); ++i) {
    const char *type = hostCType(import.parameters[i], false);
    signature += (i == 0 ? "" : ", ");
    signature += type != nullptr ? type : "?";
  }
  signature += ")";
  return signature;
}

} // namespace

const char *hostCType(IrHostValueKind kind, bool isResult) {
  switch (kind) {
  case IrHostValueKind::Void:
    return isResult ? "void" : nullptr;
  case IrHostValueKind::I32:
    return "int32_t";
  case IrHostValueKind::I64:
    return "int64_t";
  case IrHostValueKind::U64:
    return "uint64_t";
  case IrHostValueKind::U8:
    return "uint8_t";
  case IrHostValueKind::Text:
    return "const char *";
  case IrHostValueKind::F32:
    return "float";
  case IrHostValueKind::F64:
    return "double";
  case IrHostValueKind::Bool:
    return "bool";
  case IrHostValueKind::String:
    return isResult ? nullptr : "const char *";
  }
  return nullptr;
}

bool validateHostImportsForCpp(const IrModule &module, std::string &error) {
  std::map<std::string, std::string> seen;
  for (const IrHostImport &import : module.hostImports) {
    if (!textHostFunctionName(import.name).empty()) {
      continue;  // answered by the generated program's runtime
    }
    if (!isCIdentifier(import.name)) {
      error = "host function name is not a C identifier, so the C++ backends cannot link it: " + import.name;
      return false;
    }
    if (hostCType(import.returnKind, true) == nullptr) {
      error = "host function " + import.name +
              " returns a `string`, which the C++ backends cannot pass; return a `String` instead";
      return false;
    }
    for (const IrHostValueKind parameter : import.parameters) {
      if (hostCType(parameter, false) == nullptr) {
        error = "host function " + import.name + " has a parameter the C++ backends cannot pass";
        return false;
      }
    }
    const std::string signature = signatureOf(import);
    const auto [it, inserted] = seen.emplace(import.name, signature);
    if (!inserted && it->second != signature) {
      error = "host function " + import.name + " is imported with two signatures: " + it->second + " and " + signature;
      return false;
    }
  }
  return true;
}

std::string hostImportDeclarations(const std::vector<IrHostImport> &imports) {
  if (imports.empty()) {
    return "";
  }
  std::ostringstream out;
  out << "extern \"C\" {\n";
  std::map<std::string, bool> declared;
  for (const IrHostImport &import : imports) {
    if (!textHostFunctionName(import.name).empty()) {
      continue;
    }
    if (!declared.emplace(import.name, true).second) {
      continue;
    }
    out << hostCType(import.returnKind, true) << " " << import.name << "(";
    for (size_t i = 0; i < import.parameters.size(); ++i) {
      out << (i == 0 ? "" : ", ") << hostCType(import.parameters[i], false);
    }
    if (import.parameters.empty()) {
      out << "void";
    }
    out << ");\n";
  }
  out << "}\n\n";
  return out.str();
}

bool hostImportsUseKind(const std::vector<IrHostImport> &imports, IrHostValueKind kind) {
  for (const IrHostImport &import : imports) {
    if (import.returnKind == kind) {
      return true;
    }
    for (const IrHostValueKind parameter : import.parameters) {
      if (parameter == kind) {
        return true;
      }
    }
  }
  return false;
}

} // namespace primec
