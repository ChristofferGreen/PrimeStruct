#include "IrLowererCountAccessHelpers.h"

#include <limits>
#include <string_view>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

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

bool isExplicitVectorCompatibilityName(const Expr &expr, std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "vector/" + std::string(helperName);
}

bool isVectorCountTarget(const Expr &target, const LocalMap &localsIn) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           (it->second.kind == LocalInfo::Kind::Vector ||
            (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
            it->second.isSoaVector);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    return (collection == "vector" || collection == "soa_vector") && target.templateArgs.size() == 1;
  }
  return false;
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
        (kind == LocalInfo::Kind::Reference && info.referenceToVector) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToVector);
    const bool isMapTarget =
        (kind == LocalInfo::Kind::Reference && info.referenceToMap) ||
        (kind == LocalInfo::Kind::Pointer && info.pointerToMap);
    if (!isArrayTarget && !isVectorTarget && !isMapTarget) {
      return false;
    }
    if (isVectorTarget && info.isSoaVector && isExplicitVectorCompatibilityName(countExpr, "count")) {
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
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(vectorPrefix.size());
    if (helperNameOut == "count" || helperNameOut == "capacity") {
      return true;
    }
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdVectorPrefix.size());
    return true;
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
  if (normalized.rfind(mapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(mapPrefix.size());
    return true;
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdMapPrefix.size());
    return true;
  }
  return false;
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == name;
}

} // namespace

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  hasEntryArgsOut = false;
  entryArgsNameOut.clear();
  if (entryDef.parameters.empty()) {
    return true;
  }
  if (entryDef.parameters.size() != 1) {
    error = "native backend only supports a single array<string> entry parameter";
    return false;
  }
  const Expr &param = entryDef.parameters.front();
  if (!isEntryArgsParam(param)) {
    error = "native backend entry parameter must be array<string>";
    return false;
  }
  if (!param.args.empty()) {
    error = "native backend does not allow entry parameter defaults";
    return false;
  }
  hasEntryArgsOut = true;
  entryArgsNameOut = param.name;
  return true;
}

bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error) {
  out = EntryCountAccessSetup{};
  if (!resolveEntryArgsParameter(entryDef, out.hasEntryArgs, out.entryArgsName, error)) {
    return false;
  }
  out.classifiers = makeCountAccessClassifiers(out.hasEntryArgs, out.entryArgsName);
  return true;
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName) {
  CountAccessClassifiers classifiers;
  classifiers.isEntryArgsName = makeIsEntryArgsName(hasEntryArgs, entryArgsName);
  classifiers.isArrayCountCall = makeIsArrayCountCall(hasEntryArgs, entryArgsName);
  classifiers.isVectorCapacityCall = makeIsVectorCapacityCall();
  classifiers.isStringCountCall = makeIsStringCountCall();
  return classifiers;
}

IsEntryArgsNameFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName) {
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isEntryArgsName(expr, localsIn, hasEntryArgs, entryArgsName);
  };
}

IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName) {
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isArrayCountCall(expr, localsIn, hasEntryArgs, entryArgsName);
  };
}

IsVectorCapacityCallFn makeIsVectorCapacityCall() {
  return [](const Expr &expr, const LocalMap &localsIn) {
    return isVectorCapacityCall(expr, localsIn);
  };
}

IsStringCountCallFn makeIsStringCountCall() {
  return [](const Expr &expr, const LocalMap &localsIn) {
    return isStringCountCall(expr, localsIn);
  };
}

bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!hasEntryArgs || expr.kind != Expr::Kind::Name || expr.name != entryArgsName) {
    return false;
  }
  return localsIn.count(entryArgsName) == 0;
}

bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!(isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isExplicitArrayCountName(expr) && isVectorCountTarget(target, localsIn)) {
    return false;
  }
  if (isEntryArgsName(target, localsIn, hasEntryArgs, entryArgsName)) {
    return true;
  }
  if (isDereferencedCollectionCountTarget(expr, target, localsIn)) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.isArgsPack) {
      const LocalInfo &info = it->second;
      if (info.isSoaVector && isExplicitVectorCompatibilityName(expr, "count")) {
        return false;
      }
      if (info.argsPackElementKind == LocalInfo::Kind::Array ||
          info.argsPackElementKind == LocalInfo::Kind::Vector ||
          info.argsPackElementKind == LocalInfo::Kind::Buffer ||
          info.argsPackElementKind == LocalInfo::Kind::Map ||
          (info.argsPackElementKind == LocalInfo::Kind::Reference &&
           (info.referenceToArray || info.referenceToVector || info.referenceToBuffer || info.referenceToMap)) ||
          (info.argsPackElementKind == LocalInfo::Kind::Pointer &&
           (info.pointerToArray || info.pointerToVector || info.pointerToBuffer || info.pointerToMap)) ||
          info.isSoaVector) {
        return true;
      }
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray || it->second.referenceToVector ||
             it->second.referenceToBuffer || it->second.referenceToMap;
    }
    if (it->second.kind == LocalInfo::Kind::Pointer) {
      return it->second.pointerToArray || it->second.pointerToVector ||
             it->second.pointerToBuffer || it->second.pointerToMap;
    }
    if (it->second.isSoaVector && isExplicitVectorCompatibilityName(expr, "count")) {
      return false;
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Buffer || it->second.isSoaVector ||
           it->second.kind == LocalInfo::Kind::Map;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
      if (localIt != localsIn.end() && localIt->second.isArgsPack) {
        const LocalInfo &info = localIt->second;
        if (info.argsPackElementKind == LocalInfo::Kind::Array ||
            info.argsPackElementKind == LocalInfo::Kind::Vector ||
            info.argsPackElementKind == LocalInfo::Kind::Buffer ||
            info.argsPackElementKind == LocalInfo::Kind::Map ||
            (info.argsPackElementKind == LocalInfo::Kind::Reference &&
             (info.referenceToArray || info.referenceToVector || info.referenceToBuffer || info.referenceToMap)) ||
            (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToArray) ||
            (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToVector) ||
            (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToBuffer) ||
            (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToMap) ||
            info.isSoaVector) {
          return true;
        }
      }
    }
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    if (collection == "soa_vector" && isExplicitVectorCompatibilityName(expr, "count")) {
      return false;
    }
    if (collection == "array" || collection == "vector" || collection == "soa_vector") {
      return target.templateArgs.size() == 1;
    }
    if (collection == "map") {
      return target.templateArgs.size() == 2;
    }
  }
  return false;
}

bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn) {
  if (!isVectorBuiltinName(expr, "capacity") || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  auto isSupportedVectorTarget = [&](const LocalInfo &info, bool fromArgsPack) {
    const LocalInfo::Kind kind = fromArgsPack ? info.argsPackElementKind : info.kind;
    if (info.isSoaVector) {
      return false;
    }
    return kind == LocalInfo::Kind::Vector ||
           (kind == LocalInfo::Kind::Reference && info.referenceToVector) ||
           (kind == LocalInfo::Kind::Pointer && info.pointerToVector);
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && isSupportedVectorTarget(it->second, false);
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        return it != localsIn.end() && isSupportedVectorTarget(it->second, false);
      }

      std::string accessName;
      if (getBuiltinArrayAccessName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(derefTarget.args.front().name);
        return localIt != localsIn.end() && localIt->second.isArgsPack &&
               isSupportedVectorTarget(localIt->second, true);
      }
      return false;
    }

    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
      return localIt != localsIn.end() && localIt->second.isArgsPack &&
             isSupportedVectorTarget(localIt->second, true);
    }

    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    return collection == "vector" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isStringCountCall(const Expr &expr, const LocalMap &localsIn) {
  if (!(isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) || expr.args.size() != 1) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String &&
           it->second.stringSource == LocalInfo::StringSource::TableIndex;
  }
  return false;
}

StringCountCallEmitResult tryEmitStringCountCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    std::string &error) {
  if (!isStringCountCallFn(expr, localsIn)) {
    return StringCountCallEmitResult::NotHandled;
  }
  if (expr.args.size() != 1) {
    error = "count requires exactly one argument";
    return StringCountCallEmitResult::Error;
  }
  int32_t stringIndex = -1;
  size_t length = 0;
  if (!resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
    error = "native backend only supports count() on string literals or string bindings";
    return StringCountCallEmitResult::Error;
  }
  if (length > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    error = "native backend string too large for count()";
    return StringCountCallEmitResult::Error;
  }
  emitPushI32(static_cast<int32_t>(length));
  return StringCountCallEmitResult::Emitted;
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicCollectionCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCapacityTargetFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  auto isExplicitStdVectorHelperCall = [&](std::string_view helperName) {
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name != helperName) {
      return false;
    }
    return expr.namespacePrefix == "/std/collections/vector" ||
           expr.namespacePrefix == "std/collections/vector";
  };
  const bool isExplicitStdVectorCountCall =
      isExplicitStdVectorHelperCall("count") &&
      expr.args.size() == 1 && isVectorCountTarget(expr.args.front(), localsIn);
  const bool isExplicitStdVectorCapacityCall =
      isExplicitStdVectorHelperCall("capacity") &&
      expr.args.size() == 1 &&
      ((expr.args.front().kind == Expr::Kind::Name &&
        [&]() {
          auto it = localsIn.find(expr.args.front().name);
          return it != localsIn.end() &&
                 (it->second.kind == LocalInfo::Kind::Vector ||
                  (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
                  (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector));
        }()) ||
       (expr.args.front().kind == Expr::Kind::Call &&
        [&]() {
          std::string collection;
          return getBuiltinCollectionName(expr.args.front(), collection) &&
                 collection == "vector" && expr.args.front().templateArgs.size() == 1;
        }()));

  const bool blocksBareVectorCountCall =
      expr.kind == Expr::Kind::Call && expr.name == "count" && expr.namespacePrefix.empty() && expr.args.size() == 1 &&
      (isDynamicVectorCountTargetFn && isDynamicVectorCountTargetFn(expr.args.front(), localsIn) &&
       !isVectorCountTarget(expr.args.front(), localsIn));
  const bool blocksBareVectorCapacityCall =
      expr.kind == Expr::Kind::Call && expr.name == "capacity" && expr.namespacePrefix.empty() && expr.args.size() == 1 &&
      (isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn) &&
       !(isVectorCapacityCallFn && isVectorCapacityCallFn(expr, localsIn)));
  if (blocksBareVectorCountCall || blocksBareVectorCapacityCall) {
    return CountAccessCallEmitResult::NotHandled;
  }

  if (isExplicitStdVectorCountCall) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  if (isExplicitStdVectorCapacityCall) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  if (isArrayCountCallFn(expr, localsIn)) {
    if (isEntryArgsNameFn(expr.args.front(), localsIn)) {
      emitInstruction(IrOpcode::PushArgc, 0);
      return CountAccessCallEmitResult::Emitted;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  if (isVectorCapacityCallFn(expr, localsIn)) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  if ((isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) && expr.args.size() == 1 &&
      isDynamicCollectionCountTargetFn && isDynamicCollectionCountTargetFn(expr.args.front(), localsIn)) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  if (isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
      isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn)) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  const auto stringCountResult = tryEmitStringCountCall(
      expr,
      localsIn,
      isStringCountCallFn,
      resolveStringTableTarget,
      [&](int32_t length) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(length)); },
      error);
  if (stringCountResult == StringCountCallEmitResult::Error) {
    return CountAccessCallEmitResult::Error;
  }
  if (stringCountResult == StringCountCallEmitResult::Emitted) {
    return CountAccessCallEmitResult::Emitted;
  }

  if ((isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    const Expr &target = expr.args.front();
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const Expr &accessTarget = target.args.front();
      bool stringMapAccess = false;
      if (accessTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(accessTarget.name);
        if (it != localsIn.end()) {
          const LocalInfo &info = it->second;
          stringMapAccess =
              ((info.kind == LocalInfo::Kind::Map) ||
               (info.kind == LocalInfo::Kind::Reference && info.referenceToMap) ||
               (info.kind == LocalInfo::Kind::Pointer && info.pointerToMap)) &&
              info.mapValueKind == LocalInfo::ValueKind::String;
        }
      } else if (accessTarget.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(accessTarget, collection) && collection == "map" &&
            accessTarget.templateArgs.size() == 2) {
          stringMapAccess =
              accessTarget.templateArgs[1] == "string" || accessTarget.templateArgs[1] == "/string";
        }
      }
      if (stringMapAccess) {
        if (!emitExpr(target, localsIn)) {
          return CountAccessCallEmitResult::Error;
        }
        emitInstruction(IrOpcode::LoadStringLength, 0);
        return CountAccessCallEmitResult::Emitted;
      }
    }
  }

  if ((isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) && expr.args.size() == 1 &&
      inferExprKind && inferExprKind(expr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  return CountAccessCallEmitResult::NotHandled;
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; },
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      {},
      {},
      {},
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

} // namespace primec::ir_lowerer
