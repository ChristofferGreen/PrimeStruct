#include "SemanticsValidator.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace primec::semantics {

void SemanticsValidator::populateBuiltinCollectionDispatchStringResolver(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> &state,
    const std::function<bool(const Expr &, BindingInfo &)> &resolveBindingTarget,
    const std::function<bool(const Expr &, size_t &)>
        &isDirectCanonicalVectorAccessCallOnBuiltinReceiver) {
  const std::weak_ptr<BuiltinCollectionDispatchResolvers> weakState = state;
  state->resolveStringTarget = [=, this](const Expr &target) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return normalizeBindingTypeName(binding.typeName) == "string";
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }

    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        collectionTypePath == "/string") {
      return true;
    }
    if (target.isMethodCall && target.name == "why" && !target.args.empty()) {
      const Expr &receiver = target.args.front();
      if (resolveBindingTarget(receiver, binding) &&
          normalizeBindingTypeName(binding.typeName) == "FileError") {
        return true;
      }
      if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        return true;
      }
      std::string elemType;
      if ((lockedState->resolveIndexedArgsPackElementType(receiver, elemType) ||
           lockedState->resolveDereferencedIndexedArgsPackElementType(receiver, elemType)) &&
          normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType)) == "FileError") {
        return true;
      }
    }

    std::string builtinName;
    if (!getBuiltinArrayAccessName(target, builtinName) || target.args.size() != 2) {
      return false;
    }
    if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (lockedState->resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
          lockedState->resolveArrayTarget(*accessReceiver, elemType) ||
          lockedState->resolveVectorTarget(*accessReceiver, elemType) ||
          lockedState->resolveExperimentalVectorTarget(*accessReceiver, elemType)) {
        return normalizeBindingTypeName(elemType) == "string";
      }
      if (lockedState->resolveMapTarget(*accessReceiver, keyType, valueType)) {
        return normalizeBindingTypeName(valueType) == "string";
      }
      if (lockedState->resolveStringTarget(*accessReceiver)) {
        return false;
      }
    }

    size_t receiverIndex = 0;
    if (isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
      std::string elemType;
      return (lockedState->resolveArgsPackAccessTarget(target.args[receiverIndex], elemType) ||
              lockedState->resolveArrayTarget(target.args[receiverIndex], elemType) ||
              lockedState->resolveVectorTarget(target.args[receiverIndex], elemType) ||
              lockedState->resolveExperimentalVectorTarget(target.args[receiverIndex], elemType)) &&
             normalizeBindingTypeName(elemType) == "string";
    }

    const std::string resolvedTarget = resolveCalleePath(target);
    auto defIt = defMap_.find(resolvedTarget);
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        return false;
      }
      auto kindIt = returnKinds_.find(resolvedTarget);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second == ReturnKind::String;
      }
    }

    std::string elemType;
    if ((lockedState->resolveArgsPackAccessTarget(target.args.front(), elemType) ||
         lockedState->resolveArrayTarget(target.args.front(), elemType) ||
         lockedState->resolveVectorTarget(target.args.front(), elemType) ||
         lockedState->resolveExperimentalVectorTarget(target.args.front(), elemType)) &&
        elemType == "string") {
      return true;
    }
    std::string keyType;
    std::string valueType;
    return lockedState->resolveMapTarget(target.args.front(), keyType, valueType) &&
           normalizeBindingTypeName(valueType) == "string";
  };
}

} // namespace primec::semantics
