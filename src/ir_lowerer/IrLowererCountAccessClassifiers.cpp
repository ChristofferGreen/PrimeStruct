#include "IrLowererCountAccessClassifiers.h"

#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "primec/AstCallPathHelpers.h"

namespace primec::ir_lowerer::count_access_detail {

namespace {

bool hasInferredTypedWrappedMap(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         info.mapKeyKind != LocalInfo::ValueKind::Unknown &&
         info.mapValueKind != LocalInfo::ValueKind::Unknown;
}

bool isVectorTargetImpl(const Expr &target, const LocalMap &localsIn);

bool isExperimentalVectorStructPath(const std::string &structTypeName) {
  return structTypeName == "/std/collections/experimental_vector/Vector" ||
         structTypeName.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool isExplicitRemovedCountLikeAliasCall(const Expr &expr,
                                         std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string scopedPath = expr.name;
  if (scopedPath.find('/') == std::string::npos && !expr.namespacePrefix.empty()) {
    scopedPath = expr.namespacePrefix + "/" + scopedPath;
  }
  if (helperName == "count" &&
      (scopedPath == "/vector/count" || scopedPath == "vector/count")) {
    return false;
  }
  return scopedPath == std::string("/vector/") + std::string(helperName) ||
         scopedPath == std::string("vector/") + std::string(helperName) ||
         scopedPath == std::string("/array/") + std::string(helperName) ||
         scopedPath == std::string("array/") + std::string(helperName) ||
         scopedPath == std::string("/soa_vector/") + std::string(helperName) ||
         scopedPath == std::string("soa_vector/") + std::string(helperName);
}

bool isSoaVectorTargetImpl(const Expr &target, const LocalMap &localsIn) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "soa_vector") {
      return target.templateArgs.size() == 1;
    }
    if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
      return isVectorTargetImpl(target.args.front(), localsIn);
    }
  }
  return false;
}

bool isVectorTargetImpl(const Expr &target, const LocalMap &localsIn) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && !it->second.isSoaVector &&
           (it->second.kind == LocalInfo::Kind::Vector ||
            (it->second.kind == LocalInfo::Kind::Value &&
             isExperimentalVectorStructPath(it->second.structTypeName)) ||
            (it->second.kind == LocalInfo::Kind::Reference &&
             it->second.referenceToVector) ||
            (it->second.kind == LocalInfo::Kind::Pointer &&
             it->second.pointerToVector));
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "vector") {
      return target.templateArgs.size() == 1;
    }
    if (isCanonicalCollectionHelperCall(target, "std/collections/soa_vector", "to_aos", 1)) {
      return isSoaVectorTargetImpl(target.args.front(), localsIn);
    }
  }
  return false;
}

} // namespace

bool isExplicitArrayCountName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "array/count";
}

bool isVectorCountTarget(const Expr &target, const LocalMap &localsIn) {
  return isVectorTargetImpl(target, localsIn);
}

bool isDereferencedCollectionCountTarget(const Expr &, const Expr &target, const LocalMap &localsIn) {
  if (!(target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1)) {
    return false;
  }

  auto isSupportedDereferenceTarget = [&](const LocalInfo &info, bool fromArgsPack) {
    const LocalInfo::Kind kind = fromArgsPack ? info.argsPackElementKind : info.kind;
    const bool isArrayTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToArray) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToArray);
    const bool isVectorTarget =
        !info.isSoaVector &&
        ((kind == LocalInfo::Kind::Reference && info.referenceToVector) ||
         (kind == LocalInfo::Kind::Pointer && info.pointerToVector));
    const bool isBufferTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToBuffer) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToBuffer);
    const bool isMapTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToMap) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToMap) ||
        hasInferredTypedWrappedMap(info, kind);
    if (!isArrayTarget && !isVectorTarget && !isBufferTarget && !isMapTarget) {
      return false;
    }
    return true;
  };

  const Expr &derefTarget = target.args.front();
  if (derefTarget.kind == Expr::Kind::Name) {
    auto it = localsIn.find(derefTarget.name);
    return it != localsIn.end() && isSupportedDereferenceTarget(it->second, false);
  }

  std::string accessName;
  if (derefTarget.kind == Expr::Kind::Call && getBuiltinArrayAccessName(derefTarget, accessName) &&
      derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(derefTarget.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && isSupportedDereferenceTarget(it->second, true);
  }

  return false;
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  return primec::ir_lowerer::resolveVectorHelperAliasName(expr, helperNameOut);
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  return primec::ir_lowerer::resolveMapHelperAliasName(expr, helperNameOut);
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  if ((std::string_view{name} == "count" || std::string_view{name} == "capacity") &&
      isExplicitRemovedCountLikeAliasCall(expr, name)) {
    return false;
  }
  std::string aliasName;
  if (resolveVectorHelperAliasName(expr, aliasName) && aliasName == name) {
    return true;
  }
  return false;
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == name;
}

} // namespace primec::ir_lowerer::count_access_detail
