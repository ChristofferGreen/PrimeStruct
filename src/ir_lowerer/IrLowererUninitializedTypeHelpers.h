#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

struct UninitializedTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

using ResolveStructTypePathFn =
    std::function<bool(const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut)>;

bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypePathFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error);

} // namespace primec::ir_lowerer
