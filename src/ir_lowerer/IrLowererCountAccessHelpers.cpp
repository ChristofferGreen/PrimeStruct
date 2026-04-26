#include "IrLowererCountAccessHelpers.h"
#include "IrLowererCountAccessClassifiers.h"

#include <limits>
#include <memory>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {
using count_access_detail::isDereferencedCollectionCountTarget;
using count_access_detail::isExplicitArrayCountName;
using count_access_detail::isMapBuiltinName;
using count_access_detail::resolveMapHelperAliasName;
using count_access_detail::resolveVectorHelperAliasName;
using count_access_detail::isVectorBuiltinName;
using count_access_detail::isVectorCountTarget;

namespace {

std::string_view resolveSemanticProductText(const SemanticProgram &semanticProgram,
                                            SymbolId id,
                                            const std::string &fallback) {
  if (id != InvalidSymbolId) {
    const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
    if (!resolved.empty()) {
      return resolved;
    }
  }
  return fallback;
}

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

bool isExplicitVectorCountMethodCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall) {
    return false;
  }
  const std::string scopedPath = resolveScopedCallPath(expr);
  return scopedPath == "/vector/count" ||
         scopedPath == "vector/count" ||
         scopedPath == "/std/collections/vector/count" ||
         scopedPath == "std/collections/vector/count";
}

bool isExplicitPublishedVectorCountCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string helperName;
  if (!resolveVectorHelperAliasName(expr, helperName) ||
      helperName != "count") {
    return false;
  }
  std::string scopedPath = resolveScopedCallPath(expr);
  if (!scopedPath.empty() && scopedPath.front() == '/') {
    scopedPath.erase(scopedPath.begin());
  }
  return scopedPath.rfind("std/collections/vector/", 0) == 0 ||
         scopedPath.rfind("std/collections/experimental_vector/", 0) == 0;
}

bool isExplicitRemovedCountLikeAliasCall(const Expr &expr,
                                         std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string scopedPath = resolveScopedCallPath(expr);
  return scopedPath == std::string("/vector/") + std::string(helperName) ||
         scopedPath == std::string("vector/") + std::string(helperName) ||
         scopedPath == std::string("/array/") + std::string(helperName) ||
         scopedPath == std::string("array/") + std::string(helperName) ||
         scopedPath == std::string("/soa_vector/") + std::string(helperName) ||
         scopedPath == std::string("soa_vector/") + std::string(helperName);
}

bool isNamedArgumentCollectionTemporary(const Expr &expr,
                                        std::string_view collectionName) {
  if (expr.kind != Expr::Kind::Call || !hasNamedArguments(expr.argNames)) {
    return false;
  }
  std::string collection;
  return getBuiltinCollectionName(expr, collection) && collection == collectionName;
}

bool hasInferredTypedWrappedMap(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         info.mapKeyKind != LocalInfo::ValueKind::Unknown &&
         info.mapValueKind != LocalInfo::ValueKind::Unknown;
}

bool resolveEntryArgsParameterFromSemanticProduct(const Definition &entryDef,
                                                  const SemanticProgram *semanticProgram,
                                                  bool &hasEntryArgsOut,
                                                  std::string &entryArgsNameOut,
                                                  std::string &error) {
  hasEntryArgsOut = false;
  entryArgsNameOut.clear();
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProgramBindingFact *entryParamFact = nullptr;
  std::size_t entryParamCount = 0;
  const auto bindingFacts = semanticProgramBindingFactView(*semanticProgram);
  for (const auto *entry : bindingFacts) {
    const std::string_view scopePath =
        resolveSemanticProductText(*semanticProgram, entry->scopePathId, entry->scopePath);
    const std::string_view siteKind =
        resolveSemanticProductText(*semanticProgram, entry->siteKindId, entry->siteKind);
    if (scopePath != entryDef.fullPath || siteKind != "parameter") {
      continue;
    }
    ++entryParamCount;
    if (entryParamFact == nullptr) {
      entryParamFact = entry;
    }
  }

  if (entryParamCount == 0) {
    if (!entryDef.parameters.empty()) {
      error = "missing semantic-product entry parameter fact: " + entryDef.fullPath;
      return false;
    }
    return true;
  }
  if (entryParamCount != 1) {
    error = "native backend only supports a single array<string> entry parameter";
    return false;
  }
  const std::string_view bindingTypeText = resolveSemanticProductText(
      *semanticProgram, entryParamFact->bindingTypeTextId, entryParamFact->bindingTypeText);
  if (bindingTypeText != "array<string>") {
    error = "native backend entry parameter must be array<string>";
    return false;
  }

  hasEntryArgsOut = true;
  entryArgsNameOut = std::string(resolveSemanticProductText(
      *semanticProgram, entryParamFact->nameId, entryParamFact->name));
  return true;
}

} // namespace

bool resolveEntryArgsParameter(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  if (semanticProgram != nullptr) {
    return resolveEntryArgsParameterFromSemanticProduct(
        entryDef, semanticProgram, hasEntryArgsOut, entryArgsNameOut, error);
  }

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

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  return resolveEntryArgsParameter(entryDef, nullptr, hasEntryArgsOut, entryArgsNameOut, error);
}

bool buildEntryCountAccessSetup(const Definition &entryDef,
                                const SemanticProgram *semanticProgram,
                                EntryCountAccessSetup &out,
                                std::string &error) {
  out = {};
  if (!resolveEntryArgsParameter(entryDef, semanticProgram, out.hasEntryArgs, out.entryArgsName, error)) {
    return false;
  }
  out.classifiers = makeCountAccessClassifiers(out.hasEntryArgs, out.entryArgsName);
  return true;
}

bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error) {
  return buildEntryCountAccessSetup(entryDef, nullptr, out, error);
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName) {
  CountAccessClassifiers classifiers{};
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
  if (isExplicitVectorCountMethodCall(expr)) {
    return false;
  }
  if (isExplicitPublishedVectorCountCall(expr) &&
      isVectorCountTarget(expr.args.front(), localsIn)) {
    return false;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "count")) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isNamedArgumentCollectionTemporary(target, "vector")) {
    return false;
  }
  const std::string scopedExprPath = resolveScopedCallPath(expr);
  const bool isBareSimpleVectorCountCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      expr.namespacePrefix.empty() && isSimpleCallName(expr, "count");
  if (isBareSimpleVectorCountCall &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
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
      if (info.argsPackElementKind == LocalInfo::Kind::Array ||
          info.argsPackElementKind == LocalInfo::Kind::Vector ||
          info.argsPackElementKind == LocalInfo::Kind::Buffer ||
          info.argsPackElementKind == LocalInfo::Kind::Map ||
          (info.argsPackElementKind == LocalInfo::Kind::Reference &&
           (info.referenceToArray || info.referenceToVector || info.referenceToBuffer || info.referenceToMap ||
            hasInferredTypedWrappedMap(info, info.argsPackElementKind))) ||
          (info.argsPackElementKind == LocalInfo::Kind::Pointer &&
           (info.pointerToArray || info.pointerToVector || info.pointerToBuffer || info.pointerToMap ||
            hasInferredTypedWrappedMap(info, info.argsPackElementKind))) ||
          info.isSoaVector) {
        return true;
      }
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray || it->second.referenceToVector ||
             it->second.referenceToBuffer || it->second.referenceToMap ||
             hasInferredTypedWrappedMap(it->second, it->second.kind);
    }
    if (it->second.kind == LocalInfo::Kind::Pointer) {
      return it->second.pointerToArray || it->second.pointerToVector ||
             it->second.pointerToBuffer || it->second.pointerToMap ||
             hasInferredTypedWrappedMap(it->second, it->second.kind);
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Buffer || it->second.isSoaVector ||
           it->second.kind == LocalInfo::Kind::Map;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string mapHelperAlias;
    const bool isNamespacedMapAccessCall =
        resolveMapHelperAliasName(target, mapHelperAlias) &&
        (mapHelperAlias == "at" || mapHelperAlias == "at_unsafe") &&
        (target.name.find('/') != std::string::npos || !target.namespacePrefix.empty());
    if (isNamespacedMapAccessCall) {
      return false;
    }
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
               (info.referenceToArray || info.referenceToVector || info.referenceToBuffer || info.referenceToMap ||
                hasInferredTypedWrappedMap(info, info.argsPackElementKind))) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToArray) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToVector) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToBuffer) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer &&
               (info.pointerToMap || hasInferredTypedWrappedMap(info, info.argsPackElementKind))) ||
              info.isSoaVector) {
            return true;
          }
      }
    }
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
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
  if (isExplicitRemovedCountLikeAliasCall(expr, "capacity")) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isNamedArgumentCollectionTemporary(target, "vector")) {
    return false;
  }
  const std::string scopedExprPath = resolveScopedCallPath(expr);
  const bool isBareOrCanonicalVectorCapacityCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      (scopedExprPath == "capacity" || scopedExprPath == "/std/collections/vector/capacity" ||
       scopedExprPath == "std/collections/vector/capacity");
  if (isBareOrCanonicalVectorCapacityCall &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
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
      return isVectorCountTarget(target, localsIn);
    }
    return collection == "vector" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isStringCountCall(const Expr &expr, const LocalMap &localsIn) {
  if (!(isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) || expr.args.size() != 1) {
    return false;
  }
  if (isExplicitVectorCountMethodCall(expr)) {
    return false;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "count")) {
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
  const auto isCountLikeCall = [&]() {
    return (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) &&
           expr.args.size() == 1;
  };
  const auto isBareSimpleCountLikeCall = [&](std::string_view helperName) {
    return expr.kind == Expr::Kind::Call &&
           !expr.isMethodCall &&
           expr.namespacePrefix.empty() &&
           isSimpleCallName(expr, std::string(helperName).c_str()) &&
           expr.args.size() == 1;
  };
  const auto shouldDeferBareSimpleCountLikeCall = [&](std::string_view helperName) {
    if (!isBareSimpleCountLikeCall(helperName)) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (helperName == "count") {
      const bool isRuntimeStringTarget =
          inferExprKind &&
          inferExprKind(target, localsIn) == LocalInfo::ValueKind::String;
      if (isArrayCountCallFn(expr, localsIn) ||
          isRuntimeStringTarget ||
          (isStringCountCallFn != nullptr &&
           isStringCountCallFn(expr, localsIn)) ||
          (isDynamicCollectionCountTargetFn != nullptr &&
           isDynamicCollectionCountTargetFn(target, localsIn)) ||
          (isDynamicVectorCountTargetFn != nullptr &&
           isDynamicVectorCountTargetFn(target, localsIn))) {
        return false;
      }
      return true;
    }
    if ((isVectorCapacityCallFn != nullptr &&
         isVectorCapacityCallFn(expr, localsIn)) ||
        (isDynamicVectorCapacityTargetFn != nullptr &&
         isDynamicVectorCapacityTargetFn(target, localsIn))) {
      return false;
    }
    return true;
  };
  if (shouldDeferBareSimpleCountLikeCall("count") ||
      shouldDeferBareSimpleCountLikeCall("capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if (isExplicitVectorCountMethodCall(expr)) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "count") ||
      isExplicitRemovedCountLikeAliasCall(expr, "capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const bool namedArgVectorTemporaryCountTarget =
      (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) &&
      expr.args.size() == 1 &&
      isNamedArgumentCollectionTemporary(expr.args.front(), "vector");
  if (namedArgVectorTemporaryCountTarget) {
    error = "count requires array, vector, map, or string target";
    return CountAccessCallEmitResult::Error;
  }
  const bool isFieldAccessTarget =
      expr.kind == Expr::Kind::Call && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call && expr.args.front().isFieldAccess;
  const auto isStaticallyKnownStringCountTarget = [&]() {
    if (!isCountLikeCall()) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           it->second.valueKind == LocalInfo::ValueKind::String &&
           it->second.stringSource == LocalInfo::StringSource::TableIndex;
  };
  const auto isRuntimeStringCountTarget = [&]() {
    if (!isCountLikeCall() || isStaticallyKnownStringCountTarget()) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
        return true;
      }
    }
    return inferExprKind &&
           inferExprKind(target, localsIn) == LocalInfo::ValueKind::String;
  };
  if (isArrayCountCallFn(expr, localsIn)) {
    if ((isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) && expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack && it->second.argsPackElementCount >= 0) {
        emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(it->second.argsPackElementCount));
        return CountAccessCallEmitResult::Emitted;
      }
    }
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

  const bool blocksBareVectorCountCall =
      isBareSimpleCountLikeCall("count") && expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCountTargetFn && isDynamicVectorCountTargetFn(expr.args.front(), localsIn) &&
       !isVectorCountTarget(expr.args.front(), localsIn) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCountCall =
      isBareSimpleCountLikeCall("count") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicCollectionCountTargetFn &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn));
  const bool blocksBareVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn) &&
       !(isVectorCapacityCallFn && isVectorCapacityCallFn(expr, localsIn)) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicVectorCapacityTargetFn &&
        isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn));
  if (blocksBareVectorCountCall || blocksLocalVectorCountCall ||
      (isExplicitPublishedVectorCountCall(expr) &&
       expr.args.size() == 1 &&
       ((isDynamicVectorCountTargetFn &&
         isDynamicVectorCountTargetFn(expr.args.front(), localsIn)) ||
        isVectorCountTarget(expr.args.front(), localsIn))) ||
      blocksBareVectorCapacityCall || blocksLocalVectorCapacityCall) {
    return CountAccessCallEmitResult::NotHandled;
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
    if (inferExprKind &&
        inferExprKind(expr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
      // String receivers can be dynamic call results; defer to string count emission.
    } else {
      if (!emitExpr(expr.args.front(), localsIn)) {
        return CountAccessCallEmitResult::Error;
      }
      emitInstruction(IrOpcode::LoadIndirect, 0);
      return CountAccessCallEmitResult::Emitted;
    }
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

  if (isRuntimeStringCountTarget()) {
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
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
