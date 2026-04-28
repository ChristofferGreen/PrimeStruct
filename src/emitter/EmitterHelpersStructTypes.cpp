#include "EmitterHelpers.h"

#include <string_view>

namespace primec::emitter {

namespace {

std::string resolveStructTypeName(const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  const std::unordered_map<std::string, std::string> &importAliases,
                                  const std::unordered_map<std::string, std::string> &structTypeMap) {
  auto normalize_map_import_alias_path = [](const std::string &path) {
    if (path.empty() || path.front() == '/') {
      return path;
    }
    if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
      return "/" + path;
    }
    return path;
  };
  if (typeName.empty()) {
    return "";
  }
  if (typeName == "Self") {
    auto it = structTypeMap.find(namespacePrefix);
    if (it != structTypeMap.end()) {
      return it->second;
    }
  }
  auto resolveFromMap = [&](const std::string &candidate) -> std::string {
    auto it = structTypeMap.find(candidate);
    if (it != structTypeMap.end()) {
      return it->second;
    }
    return "";
  };
  if (!typeName.empty() && typeName[0] == '/') {
    return resolveFromMap(typeName);
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == typeName) {
      std::string direct = resolveFromMap(namespacePrefix);
      if (!direct.empty()) {
        return direct;
      }
    }
    std::string prefix = namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + typeName;
      std::string resolved = resolveFromMap(candidate);
      if (!resolved.empty()) {
        return resolved;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
  }
  std::string rootResolved = resolveFromMap("/" + typeName);
  if (!rootResolved.empty()) {
    return rootResolved;
  }
  auto importIt = importAliases.find(typeName);
  if (importIt != importAliases.end()) {
    return resolveFromMap(normalize_map_import_alias_path(importIt->second));
  }
  return "";
}

} // namespace

std::string bindingTypeToCpp(const std::string &typeName,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap) {
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(typeName, base, arg)) {
    base = normalizeBindingTypeName(base);
    if (base == "Result" || base == "/std/result/Result" || base == "std/result/Result") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args)) {
        return args.size() == 2 ? "uint64_t" : "uint32_t";
      }
      return "uint32_t";
    }
    if (base == "uninitialized") {
      return bindingTypeToCpp(arg, namespacePrefix, importAliases, structTypeMap);
    }
    if (base == "Pointer") {
      std::string elemType = bindingTypeToCpp(arg, namespacePrefix, importAliases, structTypeMap);
      if (elemType.empty()) {
        elemType = "void";
      }
      return elemType + " *";
    }
    if (base == "Reference") {
      std::string elemType = bindingTypeToCpp(arg, namespacePrefix, importAliases, structTypeMap);
      if (elemType.empty()) {
        elemType = "void";
      }
      return "std::reference_wrapper<" + elemType + ">";
    }
    if (base == "array" || base == "vector" || base == "args") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        std::string elemType = bindingTypeToCpp(args[0], namespacePrefix, importAliases, structTypeMap);
        return "std::vector<" + elemType + ">";
      }
      return "std::vector<int>";
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        std::string keyType = bindingTypeToCpp(args[0], namespacePrefix, importAliases, structTypeMap);
        std::string valueType = bindingTypeToCpp(args[1], namespacePrefix, importAliases, structTypeMap);
        return "std::unordered_map<" + keyType + ", " + valueType + ">";
      }
      return "std::unordered_map<int, int>";
    }
  }
  if (typeName == "i32" || typeName == "int") {
    return "int";
  }
  if (typeName == "i64") {
    return "int64_t";
  }
  if (typeName == "u64") {
    return "uint64_t";
  }
  if (typeName == "bool") {
    return "bool";
  }
  if (typeName == "float" || typeName == "f32") {
    return "float";
  }
  if (typeName == "f64") {
    return "double";
  }
  if (typeName == "string") {
    return "std::string_view";
  }
  if (typeName == "File") {
    return "ps_file_handle";
  }
  if (typeName == "FileError") {
    return "uint32_t";
  }
  std::string structType = resolveStructTypeName(typeName, namespacePrefix, importAliases, structTypeMap);
  if (!structType.empty()) {
    return structType;
  }
  return "int";
}

std::string bindingTypeToCpp(const BindingInfo &info,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap) {
  const std::string typeName = normalizeBindingTypeName(info.typeName);
  if (typeName == "array" || typeName == "vector" || typeName == "args") {
    std::string elemType =
        bindingTypeToCpp(info.typeTemplateArg, namespacePrefix, importAliases, structTypeMap);
    return "std::vector<" + elemType + ">";
  }
  if (typeName == "File") {
    return "ps_file_handle";
  }
  if (typeName == "FileError") {
    return "uint32_t";
  }
  if (typeName == "Result" || typeName == "/std/result/Result" || typeName == "std/result/Result") {
    std::vector<std::string> args;
    if (splitTopLevelTemplateArgs(info.typeTemplateArg, args)) {
      return (args.size() == 2) ? "uint64_t" : "uint32_t";
    }
    return "uint32_t";
  }
  if (typeName == "map") {
    std::vector<std::string> parts;
    if (splitTopLevelTemplateArgs(info.typeTemplateArg, parts) && parts.size() == 2) {
      std::string keyType = bindingTypeToCpp(parts[0], namespacePrefix, importAliases, structTypeMap);
      std::string valueType = bindingTypeToCpp(parts[1], namespacePrefix, importAliases, structTypeMap);
      return "std::unordered_map<" + keyType + ", " + valueType + ">";
    }
    return "std::unordered_map<int, int>";
  }
  if (typeName == "Pointer") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg, namespacePrefix, importAliases, structTypeMap);
    if (base.empty()) {
      base = "void";
    }
    return base + " *";
  }
  if (typeName == "Reference") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg, namespacePrefix, importAliases, structTypeMap);
    if (base.empty()) {
      base = "void";
    }
    return base + " &";
  }
  return bindingTypeToCpp(typeName, namespacePrefix, importAliases, structTypeMap);
}

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

std::string typeNameForReturnKind(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Array:
      return "array";
    default:
      return "";
  }
}

} // namespace primec::emitter
