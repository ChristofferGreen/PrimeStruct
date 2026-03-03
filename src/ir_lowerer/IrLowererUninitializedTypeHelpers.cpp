#include "IrLowererUninitializedTypeHelpers.h"

#include <vector>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypePathFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error) {
  out = UninitializedTypeInfo{};
  if (typeText.empty()) {
    return false;
  }
  auto isSupportedNumeric = [](LocalInfo::ValueKind kind) {
    return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
           kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Float32 ||
           kind == LocalInfo::ValueKind::Float64 || kind == LocalInfo::ValueKind::Bool;
  };

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(typeText, base, argText)) {
    if (base == "array" || base == "vector") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(argText, args) || args.size() != 1) {
        error = "native backend requires " + base + " to have exactly one template argument";
        return false;
      }
      LocalInfo::ValueKind elemKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
      if (!isSupportedNumeric(elemKind)) {
        error = "native backend only supports numeric/bool uninitialized " + base + " storage";
        return false;
      }
      out.kind = (base == "array") ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
      out.valueKind = elemKind;
      return true;
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(argText, args) || args.size() != 2) {
        error = "native backend requires map to have exactly two template arguments";
        return false;
      }
      LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
      LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(args.back()));
      if (keyKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Unknown ||
          valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool map values for uninitialized storage";
        return false;
      }
      out.kind = LocalInfo::Kind::Map;
      out.valueKind = valueKind;
      out.mapKeyKind = keyKind;
      out.mapValueKind = valueKind;
      return true;
    }
    if (base == "Pointer" || base == "Reference" || base == "Buffer") {
      out.kind = LocalInfo::Kind::Value;
      out.valueKind = LocalInfo::ValueKind::Int64;
      return true;
    }
    error = "native backend does not support uninitialized storage for type: " + typeText;
    return false;
  }

  LocalInfo::ValueKind kind = valueKindFromTypeName(typeText);
  if (isSupportedNumeric(kind)) {
    out.kind = LocalInfo::Kind::Value;
    out.valueKind = kind;
    return true;
  }
  std::string resolved;
  if (resolveStructTypePath(typeText, namespacePrefix, resolved)) {
    out.kind = LocalInfo::Kind::Value;
    out.structPath = resolved;
    return true;
  }
  if (kind == LocalInfo::ValueKind::String) {
    out.kind = LocalInfo::Kind::Value;
    out.valueKind = kind;
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
