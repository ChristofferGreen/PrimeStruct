#include "IrLowererCallHelpers.h"

#include <sstream>
#include <utility>

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

std::string inferExperimentalSoaVectorStructPathFromTypeName(
    const std::string &typeName) {
  const size_t first = typeName.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = typeName.find_last_not_of(" \t\r\n");
  std::string normalizedArg = typeName.substr(first, last - first + 1);
  if (normalizedArg.empty()) {
    return "";
  }
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
}

bool hasInferredTypedWrappedMap(const LocalInfo &localInfo, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         localInfo.mapKeyKind != LocalInfo::ValueKind::Unknown &&
         localInfo.mapValueKind != LocalInfo::ValueKind::Unknown;
}

bool isExperimentalMapStructPath(const std::string &structPath) {
  return structPath == "/std/collections/experimental_map/Map" ||
         structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

std::string mapKindTypeName(LocalInfo::ValueKind kind) {
  switch (kind) {
  case LocalInfo::ValueKind::Int32:
    return "i32";
  case LocalInfo::ValueKind::Int64:
    return "i64";
  case LocalInfo::ValueKind::UInt64:
    return "u64";
  case LocalInfo::ValueKind::Bool:
    return "bool";
  case LocalInfo::ValueKind::Float32:
    return "f32";
  case LocalInfo::ValueKind::Float64:
    return "f64";
  case LocalInfo::ValueKind::String:
    return "string";
  default:
    return "";
  }
}

std::string inferExperimentalMapStructPathFromKinds(LocalInfo::ValueKind keyKind,
                                                    LocalInfo::ValueKind valueKind) {
  const std::string keyType = mapKindTypeName(keyKind);
  const std::string valueType = mapKindTypeName(valueKind);
  if (keyType.empty() || valueType.empty()) {
    return "";
  }

  const std::string canonicalArgs = keyType + "," + valueType;
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  return specializedPath.str();
}

const Expr *findForwardedReturnValueExpr(const Definition &definition) {
  if (definition.returnExpr.has_value()) {
    return &*definition.returnExpr;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    valueExpr = &stmt;
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      return &stmt.args.front();
    }
  }
  return valueExpr;
}

bool resolveForwardedReturnParameterName(const Definition &definition,
                                         std::string &parameterNameOut) {
  parameterNameOut.clear();
  const Expr *valueExpr = findForwardedReturnValueExpr(definition);
  if (valueExpr == nullptr) {
    return false;
  }
  if (isReturnCall(*valueExpr) && valueExpr->args.size() == 1) {
    valueExpr = &valueExpr->args.front();
  }
  if (valueExpr->kind != Expr::Kind::Name) {
    return false;
  }
  for (const auto &parameter : definition.parameters) {
    if (parameter.name == valueExpr->name) {
      parameterNameOut = valueExpr->name;
      return true;
    }
  }
  return false;
}

const Expr *resolveCallArgumentForParameter(const Expr &target,
                                            const Definition &callee,
                                            const std::string &parameterName) {
  for (size_t index = 0; index < target.args.size(); ++index) {
    if (index < target.argNames.size() &&
        target.argNames[index].has_value() &&
        *target.argNames[index] == parameterName) {
      return &target.args[index];
    }
  }
  for (size_t index = 0; index < callee.parameters.size(); ++index) {
    if (callee.parameters[index].name != parameterName) {
      continue;
    }
    return index < target.args.size() ? &target.args[index] : nullptr;
  }
  return nullptr;
}

bool isForwardedMapNewConstructor(const Expr &expr) {
  std::string constructorName;
  return resolvePublishedStdlibSurfaceConstructorExprMemberName(
             expr,
             primec::StdlibSurfaceId::CollectionsMapConstructors,
             constructorName) &&
         constructorName == "mapNew";
}

bool inferDirectMapConstructorTargetInfo(const Expr &target, MapAccessTargetInfo &info) {
  info = {};
  if (target.kind != Expr::Kind::Call || target.isBinding || target.isMethodCall) {
    return false;
  }

  std::string normalizedName = target.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  auto matchesPath = [&](const std::string &basePath) {
    return normalizedName == basePath || normalizedName.rfind(basePath + "__", 0) == 0;
  };
  auto isDirectMapConstructor = [&]() {
    return matchesPath("std/collections/map/map") ||
           isPublishedStdlibSurfaceConstructorExpr(
               target,
               primec::StdlibSurfaceId::CollectionsMapConstructors);
  };
  auto inferLiteralKind = [&](const Expr &valueExpr, LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    if (valueExpr.kind == Expr::Kind::Literal) {
      kindOut = valueExpr.isUnsigned ? LocalInfo::ValueKind::UInt64
                                     : (valueExpr.intWidth == 64 ? LocalInfo::ValueKind::Int64
                                                                 : LocalInfo::ValueKind::Int32);
      return true;
    }
    if (valueExpr.kind == Expr::Kind::BoolLiteral) {
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::FloatLiteral) {
      kindOut = valueExpr.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::StringLiteral) {
      kindOut = LocalInfo::ValueKind::String;
      return true;
    }
    return false;
  };

  if (!isDirectMapConstructor()) {
    return false;
  }

  if (target.templateArgs.size() == 2) {
    info.isMapTarget = true;
    info.mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
    info.mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
    info.structTypeName =
        inferExperimentalMapStructPathFromKinds(info.mapKeyKind, info.mapValueKind);
    return true;
  }

  if (target.args.empty() || (target.args.size() % 2) != 0) {
    return false;
  }

  info.isMapTarget = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  for (size_t i = 0; i < target.args.size(); i += 2) {
    LocalInfo::ValueKind currentKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind currentValueKind = LocalInfo::ValueKind::Unknown;
    if (!inferLiteralKind(target.args[i], currentKeyKind) ||
        !inferLiteralKind(target.args[i + 1], currentValueKind)) {
      return true;
    }
    if (keyKind == LocalInfo::ValueKind::Unknown) {
      keyKind = currentKeyKind;
    } else if (keyKind != currentKeyKind) {
      return false;
    }
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      valueKind = currentValueKind;
    } else if (valueKind != currentValueKind) {
      return false;
    }
  }

  info.mapKeyKind = keyKind;
  info.mapValueKind = valueKind;
  info.structTypeName =
      inferExperimentalMapStructPathFromKinds(info.mapKeyKind, info.mapValueKind);
  return true;
}

} // namespace

MapAccessTargetInfo resolveMapAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo) {
  MapAccessTargetInfo info;
  const auto peelLocationWrappers = [&](const Expr &expr) {
    const Expr *current = &expr;
    while (current->kind == Expr::Kind::Call &&
           isSimpleCallName(*current, "location") &&
           current->args.size() == 1) {
      current = &current->args.front();
    }
    return current;
  };
  auto populateFromDirectLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    const bool inferredWrappedMap = hasInferredTypedWrappedMap(localInfo, localInfo.kind);
    if (localInfo.kind != LocalInfo::Kind::Map &&
        !(localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToMap) &&
        !(localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap) &&
        !inferredWrappedMap) {
      return false;
    }
    auto resolvedExperimentalMapStructPath = [&](const std::string &structTypeName) {
      if (structTypeName.empty()) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.mapKeyKind,
                                                    localInfo.mapValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      if (structTypeName == "Map" ||
          structTypeName == "std/collections/experimental_map/Map" ||
          structTypeName == "/std/collections/experimental_map/Map") {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.mapKeyKind,
                                                    localInfo.mapValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      return structTypeName;
    };
    info.isMapTarget = true;
    info.mapKeyKind = localInfo.mapKeyKind;
    info.mapValueKind = localInfo.mapValueKind;
    const bool isWrappedMap =
        (localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToMap) ||
        (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap) ||
        inferredWrappedMap;
    info.isWrappedMapTarget = isWrappedMap && !dereferenced;
    const bool isDirectMapStorage = localInfo.kind == LocalInfo::Kind::Map;
    const std::string resolvedStructTypeName =
        resolvedExperimentalMapStructPath(localInfo.structTypeName);
    const bool preserveDirectExperimentalMapStruct =
        isDirectMapStorage && isExperimentalMapStructPath(resolvedStructTypeName);
    if ((!info.isWrappedMapTarget || dereferenced) &&
        (!isDirectMapStorage || preserveDirectExperimentalMapStruct)) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  auto populateFromArgsPackElement = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }
    auto resolvedExperimentalMapStructPath = [&](const std::string &structTypeName) {
      if (structTypeName.empty()) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.mapKeyKind,
                                                    localInfo.mapValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      if (structTypeName == "Map" ||
          structTypeName == "std/collections/experimental_map/Map" ||
          structTypeName == "/std/collections/experimental_map/Map") {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.mapKeyKind,
                                                    localInfo.mapValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      return structTypeName;
    };
    const bool isDirectMap = localInfo.argsPackElementKind == LocalInfo::Kind::Map;
    const bool inferredWrappedMap =
        hasInferredTypedWrappedMap(localInfo, localInfo.argsPackElementKind);
    const bool isWrappedMap =
        (localInfo.argsPackElementKind == LocalInfo::Kind::Reference && localInfo.referenceToMap) ||
        (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer && localInfo.pointerToMap) ||
        inferredWrappedMap;
    if (!isDirectMap && !isWrappedMap) {
      return false;
    }
    info.isMapTarget = true;
    info.mapKeyKind = localInfo.mapKeyKind;
    info.mapValueKind = localInfo.mapValueKind;
    info.isWrappedMapTarget = isWrappedMap && !dereferenced;
    const bool isDirectMapStorage =
        localInfo.argsPackElementKind == LocalInfo::Kind::Map;
    const std::string resolvedStructTypeName =
        resolvedExperimentalMapStructPath(localInfo.structTypeName);
    const bool preserveDirectExperimentalMapStruct =
        isDirectMapStorage && isExperimentalMapStructPath(resolvedStructTypeName);
    if ((!info.isWrappedMapTarget || dereferenced) &&
        (!isDirectMapStorage || preserveDirectExperimentalMapStruct)) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end()) {
      populateFromDirectLocal(it->second, false);
    }
    if (!info.isMapTarget && resolveCallMapAccessTargetInfo) {
      MapAccessTargetInfo inferred;
      if (resolveCallMapAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        if (it != localsIn.end() && populateFromDirectLocal(it->second, true)) {
          return info;
        }
      }
      std::string derefAccessName;
      if (derefTarget.kind == Expr::Kind::Call && getBuiltinArrayAccessName(derefTarget, derefAccessName) &&
          derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.args.front().name);
        if (it != localsIn.end() && populateFromArgsPackElement(it->second, true)) {
          return info;
        }
      }
    }
    std::string accessName;
    std::string helperName;
    const std::string scopedTargetPath = resolveScopedCallPath(target);
    const bool isAliasMapArgsPackAccess =
        resolveMapHelperAliasName(target, helperName) &&
        (helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref");
    const bool isExplicitMapArgsPackAccess =
        !target.isMethodCall &&
        (scopedTargetPath == "/map/at" || scopedTargetPath == "/std/collections/map/at" ||
         scopedTargetPath == "/map/at_ref" || scopedTargetPath == "/std/collections/map/at_ref" ||
         scopedTargetPath == "/map/at_unsafe" || scopedTargetPath == "/std/collections/map/at_unsafe" ||
         scopedTargetPath == "/map/at_unsafe_ref" || scopedTargetPath == "/std/collections/map/at_unsafe_ref" ||
         isAliasMapArgsPackAccess) &&
        target.args.size() == 2;
    if ((getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) ||
        isExplicitMapArgsPackAccess) {
      const Expr *accessReceiver = peelLocationWrappers(target.args.front());
      bool receiverDereferenced = false;
      while (accessReceiver->kind == Expr::Kind::Call &&
             isSimpleCallName(*accessReceiver, "dereference") &&
             accessReceiver->args.size() == 1) {
        receiverDereferenced = true;
        accessReceiver = peelLocationWrappers(accessReceiver->args.front());
      }

      if (accessReceiver->kind == Expr::Kind::Name) {
        auto it = localsIn.find(accessReceiver->name);
        if (it != localsIn.end() && populateFromArgsPackElement(it->second, false)) {
          info.isWrappedMapTarget = info.isWrappedMapTarget && !receiverDereferenced;
          return info;
        }
      }
    }
    std::string collection;
    if (resolveCallMapAccessTargetInfo) {
      MapAccessTargetInfo inferred;
      if (resolveCallMapAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    MapAccessTargetInfo directConstructorInfo;
    const bool hasDirectConstructorInfo = inferDirectMapConstructorTargetInfo(target, directConstructorInfo);
    if (hasDirectConstructorInfo) {
      return directConstructorInfo;
    }
    if (getBuiltinCollectionName(target, collection) && collection == "map" &&
        target.templateArgs.size() == 2) {
      info.isMapTarget = true;
      info.mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
      info.mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
      return info;
    }
  }
  return info;
}

MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn) {
  return resolveMapAccessTargetInfo(target, localsIn, {});
}

bool inferForwardedMapAccessTargetInfo(
    const Expr &target,
    const Definition &callee,
    const LocalMap &localsIn,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    MapAccessTargetInfo &targetInfoOut) {
  targetInfoOut = {};
  if (target.kind != Expr::Kind::Call || target.isMethodCall || target.isBinding) {
    return false;
  }

  std::string parameterName;
  if (!resolveForwardedReturnParameterName(callee, parameterName)) {
    return false;
  }
  const Expr *forwardedArg =
      resolveCallArgumentForParameter(target, callee, parameterName);
  if (forwardedArg == nullptr) {
    return false;
  }
  if (isForwardedMapNewConstructor(*forwardedArg)) {
    return false;
  }

  MapAccessTargetInfo forwardedInfo =
      resolveMapAccessTargetInfo(*forwardedArg, localsIn, resolveCallMapAccessTargetInfo);
  if (!forwardedInfo.isMapTarget) {
    return false;
  }
  targetInfoOut = std::move(forwardedInfo);
  return true;
}

bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error) {
  if (!targetInfo.isMapTarget) {
    return true;
  }
  if (targetInfo.mapKeyKind == LocalInfo::ValueKind::Unknown ||
      targetInfo.mapValueKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires typed map bindings for " + accessName;
    return false;
  }
  return true;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error) {
  if (targetExpr.kind == Expr::Kind::StringLiteral) {
    return NonLiteralStringAccessTargetResult::Stop;
  }
  if (targetExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(targetExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports indexing into string literals or string bindings";
      return NonLiteralStringAccessTargetResult::Error;
    }
  }
  if (isEntryArgsName(targetExpr, localsIn)) {
    error = "native backend only supports entry argument indexing in print calls or string bindings";
    return NonLiteralStringAccessTargetResult::Error;
  }
  return NonLiteralStringAccessTargetResult::Continue;
}

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error) {
  indexKindOut = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
  if (!isSupportedIndexKind(indexKindOut)) {
    error = "native backend requires integer indices for " + accessName;
    return false;
  }
  return true;
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo) {
  ArrayVectorAccessTargetInfo info;
  const auto elementSlotCountForLocal = [](const LocalInfo &localInfo) {
    if (localInfo.isArgsPack) {
      const bool isInlineStructPack =
          (localInfo.argsPackElementKind == LocalInfo::Kind::Value ||
           localInfo.argsPackElementKind == LocalInfo::Kind::Map) &&
          !localInfo.structTypeName.empty() &&
          localInfo.structSlotCount > 0;
      return isInlineStructPack ? localInfo.structSlotCount : 1;
    }
    return localInfo.structSlotCount;
  };
  const auto populateFromArgsPackLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }

    info.elemKind = localInfo.valueKind;
    info.isSoaVector = localInfo.isSoaVector;
    info.isArgsPackTarget = !dereferenced;
    info.argsPackElementKind = localInfo.argsPackElementKind;
    info.elemSlotCount = elementSlotCountForLocal(localInfo);
    info.structTypeName = localInfo.structTypeName;
    info.isMapTarget = false;
    info.isWrappedMapTarget = false;

    if (localInfo.argsPackElementKind == LocalInfo::Kind::Map) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      info.isMapTarget = true;
      return true;
    }
    if (!localInfo.structTypeName.empty() &&
        (localInfo.argsPackElementKind == LocalInfo::Kind::Value ||
         localInfo.argsPackElementKind == LocalInfo::Kind::Map)) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer ||
         localInfo.referenceToMap || !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.referenceToVector;
      info.isMapTarget = localInfo.referenceToMap && dereferenced;
      info.isWrappedMapTarget = localInfo.referenceToMap && !dereferenced;
      if (localInfo.referenceToMap && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer ||
         localInfo.pointerToMap || !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.pointerToVector;
      info.isMapTarget = localInfo.pointerToMap && dereferenced;
      info.isWrappedMapTarget = localInfo.pointerToMap && !dereferenced;
      if (localInfo.pointerToMap && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    return false;
  };
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackLocal(it->second, false)) {
      return info;
    }
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
         it->second.kind == LocalInfo::Kind::Buffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.isSoaVector && !it->second.structTypeName.empty()) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isSoaVector = true;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
        (it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = it->second.referenceToVector;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer &&
        (it->second.pointerToArray || it->second.pointerToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (!info.isArrayOrVectorTarget && resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    auto resolveDereferencedArgsPackTarget = [&](const Expr &derefTarget) {
      std::string derefAccessName;
      if (!(getBuiltinArrayAccessName(derefTarget, derefAccessName) && derefTarget.args.size() == 2)) {
        return false;
      }

      const Expr &accessReceiver = derefTarget.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }

      auto localIt = localsIn.find(accessReceiver.name);
      if (localIt == localsIn.end() || !localIt->second.isArgsPack) {
        return false;
      }

      const LocalInfo &localInfo = localIt->second;
      if (populateFromArgsPackLocal(localInfo, true)) {
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Array) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Buffer) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = true;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
          (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.referenceToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
          (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.pointerToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      return false;
    };

    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && populateFromArgsPackLocal(localIt->second, false)) {
          return info;
        }
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array ||
              localIt->second.argsPackElementKind == LocalInfo::Kind::Buffer ||
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
               (localIt->second.referenceToArray || localIt->second.referenceToVector ||
                localIt->second.referenceToBuffer))) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget =
                localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                localIt->second.referenceToVector;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              (localIt->second.pointerToArray || localIt->second.pointerToBuffer)) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = false;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer && localIt->second.pointerToVector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
        }
      }
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1 &&
        resolveDereferencedArgsPackTarget(target.args.front())) {
      return info;
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) &&
        (collection == "array" || collection == "vector" || collection == "Buffer" ||
         collection == "soa_vector") &&
        target.templateArgs.size() == 1) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = valueKindFromTypeName(target.templateArgs.front());
      info.isVectorTarget = (collection == "vector");
      info.isSoaVector = (collection == "soa_vector");
      if (info.isSoaVector) {
        info.structTypeName =
            inferExperimentalSoaVectorStructPathFromTypeName(target.templateArgs.front());
      }
      return info;
    }
    if (resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
  }
  return info;
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn) {
  return resolveArrayVectorAccessTargetInfo(target, localsIn, {});
}

} // namespace primec::ir_lowerer
