#include "IrLowererCountAccessClassifiers.h"

#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "primec/AstCallPathHelpers.h"
#include "primec/SoaPathHelpers.h"

namespace primec::ir_lowerer::count_access_detail {

namespace {

bool hasInferredTypedWrappedKeyValue(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         info.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

bool isVectorTargetImpl(const Expr &target, const LocalMap &localsIn);

std::string experimentalCollectionRoot(std::string_view collectionName) {
  return "/std/collections/experimental_" + std::string(collectionName);
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return experimentalCollectionRoot(collectionName) + "/" + std::string(typeName);
}

bool isExperimentalCollectionStructPath(const std::string &structTypeName,
                                        std::string_view collectionName,
                                        std::string_view typeName) {
  const std::string basePath = experimentalCollectionTypePath(collectionName, typeName);
  return structTypeName == basePath ||
         structTypeName.rfind(basePath + "__", 0) == 0;
}

std::string unrootedSoaCollectionPath(std::string_view folderName) {
  std::string path = soa_paths::collectionPath(folderName);
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

std::string soaConversionHelperName() {
  return std::string("to") + "_aos";
}

bool isExperimentalVectorStructPath(const std::string &structTypeName) {
  return isExperimentalCollectionStructPath(structTypeName, "vector", "Vector");
}

bool getArrayVectorAccessClassifierName(const Expr &expr,
                                        std::string &accessNameOut) {
  if (getBuiltinArrayAccessName(expr, accessNameOut)) {
    return true;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      !expr.namespacePrefix.empty() || expr.name.find('/') != std::string::npos ||
      expr.args.size() != 2) {
    return false;
  }
  if (expr.name != "at" && expr.name != "at_unsafe") {
    return false;
  }
  accessNameOut = expr.name;
  return true;
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
    if (getBuiltinCollectionName(target, collection) &&
        collection == soa_paths::legacySoaFolder()) {
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
      if (getArrayVectorAccessClassifierName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          isWrappedSoaVectorLocal(derefTarget.args.front(), true)) {
        return true;
      }
    }
  }
  return false;
}

bool isPublicOrCompatibilitySoaToAosCall(const Expr &target) {
  const std::string helperName = soaConversionHelperName();
  return isCanonicalCollectionHelperCall(
             target, unrootedSoaCollectionPath(soa_paths::publicSoaFolder()), helperName, 1) ||
         isCanonicalCollectionHelperCall(
             target, unrootedSoaCollectionPath(soa_paths::legacySoaFolder()), helperName, 1);
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
    if (isPublicOrCompatibilitySoaToAosCall(target)) {
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
    const bool isKeyValueTarget = hasInferredTypedWrappedKeyValue(info, kind);
    if (!isArrayTarget && !isVectorTarget && !isSoaVectorTarget && !isBufferTarget && !isKeyValueTarget) {
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
  if (derefTarget.kind == Expr::Kind::Call &&
      getArrayVectorAccessClassifierName(derefTarget, accessName) &&
      derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(derefTarget.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && isSupportedDereferenceTarget(it->second, true);
  }

  return false;
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  return primec::ir_lowerer::resolveVectorHelperAliasName(expr, helperNameOut);
}

bool resolveKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  (void)expr;
  helperNameOut.clear();
  return false;
}

bool isUnqualifiedCollectionBuiltinName(const Expr &expr, const char *name) {
  if (expr.kind != Expr::Kind::Call || name == nullptr || expr.name != name) {
    return false;
  }
  if (!expr.namespacePrefix.empty() || expr.name.find('/') != std::string::npos) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer::count_access_detail
