#include "IrLowererCountAccessClassifiers.h"

#include <string_view>
#include <utility>

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

std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName) {
  return std::string(collectionName) + "/" + std::string(memberName);
}

std::string rootedCollectionMemberPath(std::string_view collectionName,
                                       std::string_view memberName) {
  return "/" + collectionMemberPath(collectionName, memberName);
}

std::string experimentalCollectionRoot(std::string_view collectionName) {
  return "/std/collections/experimental_" + std::string(collectionName);
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return experimentalCollectionRoot(collectionName) + "/" + std::string(typeName);
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName) {
  return experimentalCollectionRoot(collectionName) + "/";
}

std::string trimLeadingSlash(std::string text) {
  if (!text.empty() && text.front() == '/') {
    text.erase(text.begin());
  }
  return text;
}

std::string experimentalVectorWrapperAlias(std::string_view suffix) {
  return std::string("vector") + std::string(suffix);
}

bool isExperimentalCollectionStructPath(const std::string &structTypeName,
                                        std::string_view collectionName,
                                        std::string_view typeName) {
  const std::string basePath = experimentalCollectionTypePath(collectionName, typeName);
  return structTypeName == basePath ||
         structTypeName.rfind(basePath + "__", 0) == 0;
}

std::string resolveScopedCallPath(const Expr &expr) {
  std::string scopedPath = expr.name;
  if (scopedPath.find('/') == std::string::npos && !expr.namespacePrefix.empty()) {
    scopedPath = expr.namespacePrefix + "/" + scopedPath;
  }
  return scopedPath;
}

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string canonicalCollectionMemberPath(std::string_view collectionName,
                                          std::string_view memberName) {
  return stdCollectionsRoot().substr(1) + "/" +
         std::string(collectionName) + "/" + std::string(memberName);
}

std::string stripGeneratedHelperSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t generatedSuffix =
      path.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return path;
}

bool resolveExperimentalVectorReadHelperAliasName(const Expr &expr,
                                                  std::string &helperNameOut) {
  helperNameOut.clear();
  const std::string experimentalPrefix =
      trimLeadingSlash(experimentalCollectionMemberRoot("vector"));
  std::string scopedPath = trimLeadingSlash(resolveScopedCallPath(expr));
  if (scopedPath.rfind(experimentalPrefix, 0) != 0) {
    return false;
  }
  std::string leaf = scopedPath.substr(experimentalPrefix.size());
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix != std::string::npos) {
    leaf.erase(generatedSuffix);
  }
  auto matchesAlias = [&](std::string_view suffix, std::string_view memberName) {
    if (leaf != experimentalVectorWrapperAlias(suffix)) {
      return false;
    }
    helperNameOut = memberName;
    return true;
  };
  return matchesAlias("Count", "count") ||
         matchesAlias("Capacity", "capacity") ||
         matchesAlias("At", "at") ||
         matchesAlias("AtUnsafe", "at_unsafe");
}

bool isExperimentalVectorStructPath(const std::string &structTypeName) {
  return isExperimentalCollectionStructPath(structTypeName, "vector", "Vector");
}

bool isExplicitRemovedCountLikeAliasCall(const Expr &expr,
                                         std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string scopedPath = resolveScopedCallPath(expr);
  if (helperName == "count" &&
      (scopedPath == rootedCollectionMemberPath("vector", "count") ||
       scopedPath == collectionMemberPath("vector", "count"))) {
    return false;
  }
  auto matchesCollectionRoot = [&](std::string_view collectionName) {
    return scopedPath == rootedCollectionMemberPath(collectionName, helperName) ||
           scopedPath == collectionMemberPath(collectionName, helperName);
  };
  return matchesCollectionRoot("vector") ||
         matchesCollectionRoot("array") ||
         matchesCollectionRoot("soa_vector");
}

bool isExplicitCanonicalVectorReadHelperCall(const Expr &expr,
                                             std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (helperName != "count" && helperName != "capacity" &&
      helperName != "at" && helperName != "at_unsafe") {
    return false;
  }
  std::string scopedPath = resolveScopedCallPath(expr);
  if (!scopedPath.empty() && scopedPath.front() == '/') {
    scopedPath.erase(scopedPath.begin());
  }
  scopedPath = stripGeneratedHelperSuffix(std::move(scopedPath));
  return scopedPath == canonicalCollectionMemberPath("vector", helperName);
}

bool isSoaVectorTargetImpl(const Expr &target, const LocalMap &localsIn) {
  auto isWrappedSoaVectorLocal = [&](const Expr &candidate, bool fromArgsPack) {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end() || !it->second.isSoaVector) {
      return false;
    }
    const LocalInfo::Kind kind = fromArgsPack ? it->second.argsPackElementKind : it->second.kind;
    return (kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
           (kind == LocalInfo::Kind::Pointer && it->second.pointerToVector);
  };
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
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (isWrappedSoaVectorLocal(derefTarget, false)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          isWrappedSoaVectorLocal(derefTarget.args.front(), true)) {
        return true;
      }
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
    const bool isSoaVectorTarget =
        info.isSoaVector &&
        ((kind == LocalInfo::Kind::Reference && info.referenceToVector) ||
         (kind == LocalInfo::Kind::Pointer && info.pointerToVector));
    const bool isBufferTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToBuffer) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToBuffer);
    const bool isMapTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToMap) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToMap) ||
        hasInferredTypedWrappedMap(info, kind);
    if (!isArrayTarget && !isVectorTarget && !isSoaVectorTarget && !isBufferTarget && !isMapTarget) {
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
  if (expr.kind == Expr::Kind::Call && name != nullptr) {
    std::string scopedPath = resolveScopedCallPath(expr);
    if (!scopedPath.empty() && scopedPath.front() == '/') {
      scopedPath.erase(scopedPath.begin());
    }
    scopedPath = stripGeneratedHelperSuffix(std::move(scopedPath));
    if (scopedPath == canonicalCollectionMemberPath("vector", name)) {
      return false;
    }
  }
  if (isExplicitCanonicalVectorReadHelperCall(expr, name)) {
    return false;
  }
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  if ((std::string_view{name} == "count" || std::string_view{name} == "capacity") &&
      isExplicitRemovedCountLikeAliasCall(expr, name)) {
    return false;
  }
  std::string aliasName;
  if (resolveExperimentalVectorReadHelperAliasName(expr, aliasName) &&
      aliasName == name) {
    return true;
  }
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
