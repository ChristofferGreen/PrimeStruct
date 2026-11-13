#include "primec/support/CallNames.h"

namespace primec {

std::string_view trimLeadingSlash(std::string_view path) {
  if (!path.empty() && path.front() == '/') {
    path.remove_prefix(1);
  }
  return path;
}

bool readSingleSegmentName(std::string_view spelling, std::string &out) {
  out.clear();
  const std::string_view rest = trimLeadingSlash(spelling);
  if (rest.find('/') != std::string_view::npos) {
    return false;
  }
  out.assign(rest);
  return true;
}

bool isSingleSegmentName(std::string_view spelling, std::string_view target) {
  const std::string_view rest = trimLeadingSlash(spelling);
  return rest.find('/') == std::string_view::npos && rest == target;
}

bool matchSimpleCallName(std::string_view spelling,
                         std::string_view namespacePrefix,
                         std::string_view target,
                         const SimpleCallPolicy &policy) {
  std::string name(spelling);
  if (policy.joinNamespacePrefix && !namespacePrefix.empty() &&
      name.find('/') == std::string::npos) {
    std::string scoped(namespacePrefix);
    if (scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    name = scoped + "/" + name;
  }
  name = std::string(trimLeadingSlash(name));
  if (policy.stripGpuPrefix && name.rfind("std/gpu/", 0) == 0) {
    name.erase(0, 8);
  }
  if (policy.decide != nullptr) {
    bool matches = false;
    if (policy.decide(name, target, matches)) {
      return matches;
    }
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (policy.stripTemplateSuffix) {
    const size_t suffix = name.find("__t");
    if (suffix != std::string::npos) {
      name.erase(suffix);
    }
  }
  return name == target;
}

ScalarTypeKind scalarTypeKindForName(std::string_view typeName) {
  if (typeName == "int" || typeName == "i32") {
    return ScalarTypeKind::Int32;
  }
  if (typeName == "i64") {
    return ScalarTypeKind::Int64;
  }
  if (typeName == "u64") {
    return ScalarTypeKind::UInt64;
  }
  if (typeName == "u8") {
    return ScalarTypeKind::UInt8;
  }
  if (typeName == "bool") {
    return ScalarTypeKind::Bool;
  }
  if (typeName == "string") {
    return ScalarTypeKind::String;
  }
  if (typeName == "float" || typeName == "f32") {
    return ScalarTypeKind::Float32;
  }
  if (typeName == "f64") {
    return ScalarTypeKind::Float64;
  }
  if (typeName == "void") {
    return ScalarTypeKind::Void;
  }
  return ScalarTypeKind::None;
}

} // namespace primec
