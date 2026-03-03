#pragma once

#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

struct UninitializedTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct UninitializedFieldBindingInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

using ResolveStructTypePathFn =
    std::function<bool(const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut)>;
using FindUninitializedFieldTemplateArgFn =
    std::function<bool(const std::string &structPath, const std::string &fieldName, std::string &typeTemplateArgOut)>;

bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypePathFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error);
bool resolveUninitializedTypeInfoFromLocalStorage(const LocalInfo &local, UninitializedTypeInfo &out);
bool resolveUninitializedLocalStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const LocalInfo *&localOut,
                                               UninitializedTypeInfo &typeInfoOut,
                                               bool &resolvedOut);
bool findUninitializedFieldTemplateArg(const std::vector<UninitializedFieldBindingInfo> &fields,
                                       const std::string &fieldName,
                                       std::string &typeTemplateArgOut);
bool resolveUninitializedFieldStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                               const LocalInfo *&receiverOut,
                                               std::string &structPathOut,
                                               std::string &typeTemplateArgOut);

} // namespace primec::ir_lowerer
