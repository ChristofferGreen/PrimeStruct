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
  state->resolveStringTarget = [=, this](const Expr &target) -> bool {
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
      if ((state->resolveIndexedArgsPackElementType(receiver, elemType) ||
           state->resolveDereferencedIndexedArgsPackElementType(receiver, elemType)) &&
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
      if (state->resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
          state->resolveArrayTarget(*accessReceiver, elemType) ||
          state->resolveVectorTarget(*accessReceiver, elemType) ||
          state->resolveExperimentalVectorTarget(*accessReceiver, elemType)) {
        return normalizeBindingTypeName(elemType) == "string";
      }
      if (state->resolveMapTarget(*accessReceiver, keyType, valueType)) {
        return normalizeBindingTypeName(valueType) == "string";
      }
      if (state->resolveStringTarget(*accessReceiver)) {
        return false;
      }
    }

    size_t receiverIndex = 0;
    if (isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
      std::string elemType;
      return (state->resolveArgsPackAccessTarget(target.args[receiverIndex], elemType) ||
              state->resolveArrayTarget(target.args[receiverIndex], elemType) ||
              state->resolveVectorTarget(target.args[receiverIndex], elemType) ||
              state->resolveExperimentalVectorTarget(target.args[receiverIndex], elemType)) &&
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
    if ((state->resolveArgsPackAccessTarget(target.args.front(), elemType) ||
         state->resolveArrayTarget(target.args.front(), elemType) ||
         state->resolveVectorTarget(target.args.front(), elemType) ||
         state->resolveExperimentalVectorTarget(target.args.front(), elemType)) &&
        elemType == "string") {
      return true;
    }
    std::string keyType;
    std::string valueType;
    return state->resolveMapTarget(target.args.front(), keyType, valueType) &&
           normalizeBindingTypeName(valueType) == "string";
  };
}

} // namespace primec::semantics
