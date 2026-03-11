#include "IrLowererCountAccessHelpers.h"

#include <limits>

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

bool isVectorCountTarget(const Expr &target, const LocalMap &localsIn) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Vector;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    return collection == "vector" && target.templateArgs.size() == 1;
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
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (helperNameOut == "count" || helperNameOut == "at" || helperNameOut == "at_unsafe") {
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
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray;
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Map;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    if (collection == "array" || collection == "vector") {
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
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Vector;
  }
  if (target.kind == Expr::Kind::Call) {
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
    return it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String;
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
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCapacityTargetFn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
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
      {},
      {},
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

} // namespace primec::ir_lowerer
