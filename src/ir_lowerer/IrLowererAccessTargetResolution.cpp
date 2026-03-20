#include "IrLowererCallHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool inferDirectMapConstructorTargetInfo(const Expr &target, MapAccessTargetInfo &info) {
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
           matchesPath("std/collections/mapNew") ||
           matchesPath("std/collections/mapSingle") ||
           matchesPath("std/collections/mapDouble") ||
           matchesPath("std/collections/mapPair") ||
           matchesPath("std/collections/mapTriple") ||
           matchesPath("std/collections/mapQuad") ||
           matchesPath("std/collections/mapQuint") ||
           matchesPath("std/collections/mapSext") ||
           matchesPath("std/collections/mapSept") ||
           matchesPath("std/collections/mapOct") ||
           matchesPath("std/collections/experimental_map/mapNew") ||
           matchesPath("std/collections/experimental_map/mapSingle") ||
           matchesPath("std/collections/experimental_map/mapDouble") ||
           matchesPath("std/collections/experimental_map/mapPair") ||
           matchesPath("std/collections/experimental_map/mapTriple") ||
           matchesPath("std/collections/experimental_map/mapQuad") ||
           matchesPath("std/collections/experimental_map/mapQuint") ||
           matchesPath("std/collections/experimental_map/mapSext") ||
           matchesPath("std/collections/experimental_map/mapSept") ||
           matchesPath("std/collections/experimental_map/mapOct") ||
           isSimpleCallName(target, "mapNew") ||
           isSimpleCallName(target, "mapSingle") ||
           isSimpleCallName(target, "mapDouble") ||
           isSimpleCallName(target, "mapPair") ||
           isSimpleCallName(target, "mapTriple") ||
           isSimpleCallName(target, "mapQuad") ||
           isSimpleCallName(target, "mapQuint") ||
           isSimpleCallName(target, "mapSext") ||
           isSimpleCallName(target, "mapSept") ||
           isSimpleCallName(target, "mapOct");
  };
  auto inferLiteralKind = [&](const Expr &valueExpr, LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    if (valueExpr.kind == Expr::Kind::Literal) {
      kindOut = LocalInfo::ValueKind::Int32;
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
    return true;
  }

  if (target.args.empty() || (target.args.size() % 2) != 0) {
    return false;
  }

  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  for (size_t i = 0; i < target.args.size(); i += 2) {
    LocalInfo::ValueKind currentKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind currentValueKind = LocalInfo::ValueKind::Unknown;
    if (!inferLiteralKind(target.args[i], currentKeyKind) ||
        !inferLiteralKind(target.args[i + 1], currentValueKind)) {
      return false;
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

  info.isMapTarget = true;
  info.mapKeyKind = keyKind;
  info.mapValueKind = valueKind;
  return true;
}

} // namespace

MapAccessTargetInfo resolveMapAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo) {
  MapAccessTargetInfo info;
  auto populateFromDirectLocal = [&](const LocalInfo &localInfo) {
    if (localInfo.kind != LocalInfo::Kind::Map &&
        !(localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToMap) &&
        !(localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToMap)) {
      return false;
    }
    info.isMapTarget = true;
    info.mapKeyKind = localInfo.mapKeyKind;
    info.mapValueKind = localInfo.mapValueKind;
    return true;
  };
  auto populateFromArgsPackLocal = [&](const LocalInfo &localInfo) {
    if (!localInfo.isArgsPack ||
        (localInfo.argsPackElementKind != LocalInfo::Kind::Map &&
         !(localInfo.argsPackElementKind == LocalInfo::Kind::Reference && localInfo.referenceToMap) &&
         !(localInfo.argsPackElementKind == LocalInfo::Kind::Pointer && localInfo.pointerToMap))) {
      return false;
    }
    info.isMapTarget = true;
    info.mapKeyKind = localInfo.mapKeyKind;
    info.mapValueKind = localInfo.mapValueKind;
    return true;
  };
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end()) {
      populateFromDirectLocal(it->second);
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        if (it != localsIn.end() && populateFromDirectLocal(it->second)) {
          return info;
        }
      }
      std::string derefAccessName;
      if (derefTarget.kind == Expr::Kind::Call && getBuiltinArrayAccessName(derefTarget, derefAccessName) &&
          derefTarget.args.size() == 2 && derefTarget.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(derefTarget.args.front().name);
        if (localIt != localsIn.end() && populateFromArgsPackLocal(localIt->second)) {
          return info;
        }
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && populateFromArgsPackLocal(localIt->second)) {
          return info;
        }
      }
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "map" &&
        target.templateArgs.size() == 2) {
      info.isMapTarget = true;
      info.mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
      info.mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
      return info;
    }
    if (inferDirectMapConstructorTargetInfo(target, info)) {
      return info;
    }
    if (resolveCallMapAccessTargetInfo) {
      MapAccessTargetInfo inferred;
      if (resolveCallMapAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
  }
  return info;
}

MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn) {
  return resolveMapAccessTargetInfo(target, localsIn, {});
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
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
        (it->second.referenceToArray || it->second.referenceToVector)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = it->second.referenceToVector;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToArray) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
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
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
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
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Array) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = true;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = localInfo.structSlotCount;
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = true;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = true;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = localInfo.structSlotCount;
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
          (localInfo.referenceToArray || localInfo.referenceToVector)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.referenceToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = true;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = localInfo.structSlotCount;
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
          (localInfo.pointerToArray || localInfo.pointerToVector)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.pointerToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = true;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = localInfo.structSlotCount;
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
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = localIt->second.structSlotCount;
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array ||
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
               (localIt->second.referenceToArray || localIt->second.referenceToVector))) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget =
                localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                localIt->second.referenceToVector;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = localIt->second.structSlotCount;
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer && localIt->second.pointerToArray) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = false;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = localIt->second.structSlotCount;
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
            info.elemSlotCount = localIt->second.structSlotCount;
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
    if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
        target.templateArgs.size() == 1) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = valueKindFromTypeName(target.templateArgs.front());
      info.isVectorTarget = (collection == "vector");
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
