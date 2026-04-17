#include "IrLowererCountAccessClassifiers.h"

#include <string_view>

#include "IrLowererHelpers.h"
#include "primec/AstCallPathHelpers.h"

namespace primec::ir_lowerer::count_access_detail {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool hasInferredTypedWrappedMap(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         info.mapKeyKind != LocalInfo::ValueKind::Unknown &&
         info.mapValueKind != LocalInfo::ValueKind::Unknown;
}

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isVectorTargetImpl(const Expr &target, const LocalMap &localsIn);

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

bool resolveCollectionsMapWrapperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "count" || helperName == "count_ref" ||
      helperName == "Count" || helperName == "CountRef" ||
      helperName == "mapCount" || helperName == "mapCountRef") {
    helperNameOut = "count";
    return true;
  }
  if (helperName == "contains" || helperName == "contains_ref" ||
      helperName == "Contains" || helperName == "ContainsRef" ||
      helperName == "mapContains" || helperName == "mapContainsRef") {
    helperNameOut = "contains";
    return true;
  }
  if (helperName == "tryAt" || helperName == "tryAt_ref" ||
      helperName == "TryAt" || helperName == "TryAtRef" ||
      helperName == "mapTryAt" || helperName == "mapTryAtRef") {
    helperNameOut = "tryAt";
    return true;
  }
  if (helperName == "at" || helperName == "at_ref" ||
      helperName == "At" || helperName == "AtRef" ||
      helperName == "mapAt" || helperName == "mapAtRef") {
    helperNameOut = "at";
    return true;
  }
  if (helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
      helperName == "AtUnsafe" || helperName == "AtUnsafeRef" ||
      helperName == "mapAtUnsafe" || helperName == "mapAtUnsafeRef") {
    helperNameOut = "at_unsafe";
    return true;
  }
  if (helperName == "insert" || helperName == "insert_ref" ||
      helperName == "Insert" || helperName == "InsertRef" ||
      helperName == "mapInsert" || helperName == "mapInsertRef" ||
      helperName == "MapInsert" || helperName == "MapInsertRef") {
    helperNameOut = "insert";
    return true;
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

bool isDereferencedCollectionCountTarget(const Expr &countExpr, const Expr &target, const LocalMap &localsIn) {
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
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string stdSoaVectorPrefix = "std/collections/soa_vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  auto resolveExperimentalVectorAlias = [&](std::string helperName) {
    helperName = stripGeneratedHelperSuffix(std::move(helperName));
    if (helperName == "vectorCount") {
      helperNameOut = "count";
      return true;
    }
    if (helperName == "vectorCapacity") {
      helperNameOut = "capacity";
      return true;
    }
    if (helperName == "vectorAt") {
      helperNameOut = "at";
      return true;
    }
    if (helperName == "vectorAtUnsafe") {
      helperNameOut = "at_unsafe";
      return true;
    }
    return false;
  };
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size()));
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(stdSoaVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdSoaVectorPrefix.size()));
    return helperNameOut == "count";
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    return resolveExperimentalVectorAlias(normalized.substr(experimentalVectorPrefix.size()));
  }
  return false;
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  const std::string collectionsMapWrapperPrefix = "std/collections/map";
  const std::string collectionsMapPascalWrapperPrefix = "std/collections/Map";
  const std::string experimentalMapPrefix = "std/collections/experimental_map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(mapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(stdMapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(collectionsMapWrapperPrefix, 0) == 0 &&
      normalized.rfind(stdMapPrefix, 0) != 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(collectionsMapWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(collectionsMapPascalWrapperPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(collectionsMapPascalWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(experimentalMapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(experimentalMapPrefix.size()), helperNameOut);
  }
  return false;
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
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
