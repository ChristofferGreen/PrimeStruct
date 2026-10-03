#include "primec/ir_lowerer/IrLowererCountAccessHelpers.h"
#include "IrLowererCountAccessClassifiers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererCountAccessInternal.h"

namespace primec::ir_lowerer {
using namespace count_access_internal;

bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!hasEntryArgs || expr.kind != Expr::Kind::Name || expr.name != entryArgsName) {
    return false;
  }
  return localsIn.count(entryArgsName) == 0;
}

bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  return isArrayCountCall(expr, localsIn, hasEntryArgs, entryArgsName, nullptr, nullptr);
}

bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram) {
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  return isArrayCountCall(expr,
                          localsIn,
                          hasEntryArgs,
                          entryArgsName,
                          semanticProgram,
                          semanticProgram == nullptr ? nullptr : &semanticIndex);
}

bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram,
                      const SemanticProductIndex *semanticIndex) {
  const bool isSemanticVectorCountBridge =
      isSemanticVectorCountBridgeCall(expr, semanticProgram);
  const bool isSemanticArrayCountMethod =
      isSemanticArrayCountMethodTarget(expr, semanticProgram);
  if (!(count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
        isSemanticVectorCountBridge || isSemanticArrayCountMethod) ||
      expr.args.size() != 1) {
    return false;
  }
  if (isExplicitVectorCountMethodCall(expr) && !isSemanticArrayCountMethod) {
    return false;
  }
  if (isExplicitPublishedVectorMetadataCall(expr, "count") &&
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
  SemanticCountTargetInfo semanticTargetInfo;
  const bool hasSemanticTargetInfo =
      semanticProgram != nullptr &&
      semanticIndex != nullptr && target.semanticNodeId != 0 &&
      classifySemanticCountTarget(target, semanticProgram, semanticIndex, semanticTargetInfo);
  const std::string scopedExprPath = resolveScopedCallPath(expr);
  const bool isBareSimpleVectorCountCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      expr.namespacePrefix.empty() && isSimpleCallName(expr, "count");
  if (!hasSemanticTargetInfo && isBareSimpleVectorCountCall &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
  if (!hasSemanticTargetInfo && isExplicitArrayCountName(expr) &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
  if (isEntryArgsName(target, localsIn, hasEntryArgs, entryArgsName)) {
    return true;
  }
  if (hasSemanticTargetInfo) {
    return semanticTargetInfo.isCollection;
  }
  const SemanticDereferencedCountTargetResolution dereferencedTargetResolution =
      classifySemanticDereferencedCountTarget(target, semanticProgram, semanticIndex);
  if (dereferencedTargetResolution ==
      SemanticDereferencedCountTargetResolution::Collection) {
    return true;
  }
  if (dereferencedTargetResolution ==
      SemanticDereferencedCountTargetResolution::NonCollection) {
    return false;
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
      return true;
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray || it->second.referenceToVector ||
             it->second.referenceToBuffer ||
             hasInferredTypedWrappedKeyValue(it->second, it->second.kind);
    }
    if (it->second.kind == LocalInfo::Kind::Pointer) {
      return it->second.pointerToArray || it->second.pointerToVector ||
             it->second.pointerToBuffer ||
             hasInferredTypedWrappedKeyValue(it->second, it->second.kind);
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Buffer || it->second.isSoaVector ||
           (it->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
            it->second.keyValueValueKind != LocalInfo::ValueKind::Unknown);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string keyValueHelperAlias;
    const bool isNamespacedKeyValueAccessCall =
        count_access_detail::resolveKeyValueHelperAliasName(target, keyValueHelperAlias) &&
        (keyValueHelperAlias == "at" || keyValueHelperAlias == "at_unsafe") &&
        (target.name.find('/') != std::string::npos || !target.namespacePrefix.empty());
    if (isNamespacedKeyValueAccessCall) {
      return false;
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      const bool hasSemanticIndexAvailable = semanticProgram != nullptr && semanticIndex != nullptr;
      auto localIt = localsIn.find(target.args.front().name);
        if (!hasSemanticIndexAvailable && localIt != localsIn.end() && localIt->second.isArgsPack) {
          const LocalInfo &info = localIt->second;
          if (info.argsPackElementKind == LocalInfo::Kind::Array ||
              info.argsPackElementKind == LocalInfo::Kind::Vector ||
              info.argsPackElementKind == LocalInfo::Kind::Buffer ||
              (info.argsPackElementKind == LocalInfo::Kind::Value &&
               info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
               info.keyValueValueKind != LocalInfo::ValueKind::Unknown) ||
              (info.argsPackElementKind == LocalInfo::Kind::Reference &&
               (info.referenceToArray || info.referenceToVector || info.referenceToBuffer ||
                hasInferredTypedWrappedKeyValue(info, info.argsPackElementKind))) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToArray) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToVector) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToBuffer) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer &&
               hasInferredTypedWrappedKeyValue(info, info.argsPackElementKind)) ||
              info.isSoaVector) {
            return true;
          }
      }
    }
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    if (collection == "array" || collection == "vector" ||
        collection == soa_paths::legacySoaFolder()) {
      return target.templateArgs.size() == 1;
    }
    if (collection == "map") {
      return target.templateArgs.size() == 2;
    }
  }
  return false;
}

bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn) {
  return isVectorCapacityCall(expr, localsIn, nullptr, nullptr);
}

bool isVectorCapacityCall(const Expr &expr,
                          const LocalMap &localsIn,
                          const SemanticProgram *semanticProgram,
                          const SemanticProductIndex *semanticIndex) {
  if (!count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") || expr.args.size() != 1) {
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
  const bool isBareVectorCapacityCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      scopedExprPath == "capacity";
  // A plain local-variable target (target.kind == Name) is handled below by
  // looking the local up directly and checking its LocalInfo::Kind - that is
  // the common, straightforward case (e.g. `capacity(values)`) and must not
  // be excluded here. This early-out only guards the remaining, more exotic
  // target shapes (e.g. a soa-to-aos-wrapped temporary), where
  // isVectorCountTarget's generic "is this vector-like" answer is too broad
  // for a capacity query specifically.
  if (isBareVectorCapacityCall && target.kind != Expr::Kind::Name &&
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
  const auto isSemanticVectorCapacityTarget = [&](const Expr &candidate) {
    SemanticCountTargetInfo semanticInfo;
    if (!classifySemanticCountTarget(candidate,
                                     semanticProgram,
                                     semanticIndex,
                                     semanticInfo)) {
      return false;
    }
    return semanticInfo.isVector;
  };
  const auto hasSemanticCapacityTargetFact = [&](const Expr &candidate) {
    SemanticCountTargetInfo semanticInfo;
    return classifySemanticCountTarget(candidate,
                                       semanticProgram,
                                       semanticIndex,
                                       semanticInfo);
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && !it->second.isArgsPack &&
           isSupportedVectorTarget(it->second, false);
  }
  if (target.kind == Expr::Kind::Call) {
    if (hasPublishedSemanticCountTargetFact(target, semanticIndex) &&
        hasSemanticCapacityTargetFact(target)) {
      return isSemanticVectorCapacityTarget(target);
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.semanticNodeId != 0 && hasSemanticCapacityTargetFact(derefTarget)) {
        return isSemanticVectorCapacityTarget(derefTarget);
      }
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        return it != localsIn.end() && isSupportedVectorTarget(it->second, false);
      }

      std::string accessName;
      if (getArrayVectorAccessClassifierName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(derefTarget.args.front().name);
        return localIt != localsIn.end() && localIt->second.isArgsPack &&
               isSupportedVectorTarget(localIt->second, true);
      }
      return false;
    }

    std::string accessName;
    if (getArrayVectorAccessClassifierName(target, accessName) && target.args.size() == 2 &&
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
  return isStringCountCall(expr, localsIn, nullptr, nullptr);
}

bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram) {
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  return isStringCountCall(expr,
                           localsIn,
                           semanticProgram,
                           semanticProgram == nullptr ? nullptr : &semanticIndex);
}

bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram,
                       const SemanticProductIndex *semanticIndex) {
  if (!count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
      expr.args.size() != 1) {
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
    if (semanticProgram != nullptr && semanticIndex != nullptr &&
        target.semanticNodeId != 0) {
      SemanticCountTargetInfo semanticInfo;
      if (classifySemanticCountTarget(target, semanticProgram, semanticIndex, semanticInfo)) {
        return semanticInfo.isString;
      }
    }
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
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
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
  const Expr &target = expr.args.front();
  if (inferExprKind) {
    const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
    if (targetKind != LocalInfo::ValueKind::Unknown &&
        targetKind != LocalInfo::ValueKind::String) {
      return StringCountCallEmitResult::NotHandled;
    }
  }
  int32_t stringIndex = -1;
  size_t length = 0;
  if (!resolveStringTableTarget(target, localsIn, stringIndex, length)) {
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

} // namespace primec::ir_lowerer
