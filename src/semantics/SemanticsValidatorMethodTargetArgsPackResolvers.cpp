// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/ReceiverElementFamilyClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

bool SemanticsValidator::resolveCurrentDefinitionParamBinding(
    const std::string &name, BindingInfo &bindingOut) const {
  if (currentValidationState_.context.definitionPath.empty()) {
    return false;
  }
  if (auto paramsIt = paramsByDef_.find(currentValidationState_.context.definitionPath);
      paramsIt != paramsByDef_.end()) {
    if (const BindingInfo *binding = findParamBinding(paramsIt->second, name)) {
      bindingOut = *binding;
      return true;
    }
  }
  auto defIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const Expr &param : defIt->second->parameters) {
    if (param.name != name) {
      continue;
    }
    std::optional<std::string> restrictType;
    std::string parseError;
    return parseBindingInfo(param,
                            defIt->second->namespacePrefix,
                            structNames_,
                            importAliases_,
                            bindingOut,
                            restrictType,
                            parseError,
                            &sumNames_);
  }
  return false;
}

bool SemanticsValidator::resolveArgsPackCountTarget(
    const Expr &target, std::string &elemType,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  elemType.clear();
  auto resolveBinding = [&](const BindingInfo &binding) {
    return getArgsPackElementType(binding, elemType);
  };
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      if (resolveBinding(*paramBinding)) {
        return true;
      }
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      if (resolveBinding(it->second)) {
        return true;
      }
    }
    BindingInfo currentDefBinding;
    if (resolveCurrentDefinitionParamBinding(target.name, currentDefBinding)) {
      return resolveBinding(currentDefBinding);
    }
  }
  return false;
}

bool SemanticsValidator::resolveArgsPackAccessTarget(
    const Expr &target, std::string &elemType,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  if (resolveArgsPackElementTypeForExpr(target, params, locals, elemType)) {
    return true;
  }
  if (target.kind != Expr::Kind::Name) {
    return false;
  }
  BindingInfo currentDefBinding;
  return resolveCurrentDefinitionParamBinding(target.name, currentDefBinding) &&
         getArgsPackElementType(currentDefBinding, elemType);
}

bool SemanticsValidator::resolveIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  std::string accessName;
  if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) ||
      target.args.size() != 2) {
    return false;
  }
  const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target);
  return accessReceiver != nullptr && resolveArgsPackAccessTarget(*accessReceiver, elemTypeOut);
}

bool SemanticsValidator::resolveDereferencedIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
    return false;
  }
  std::string wrappedType;
  return resolveIndexedArgsPackElementType(target.args.front(), wrappedType,
                                           resolveArgsPackAccessTarget) &&
         extractWrappedPointeeType(wrappedType, elemTypeOut);
}

bool SemanticsValidator::resolveWrappedIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  std::string wrappedType;
  return resolveIndexedArgsPackElementType(target, wrappedType, resolveArgsPackAccessTarget) &&
         extractWrappedPointeeType(wrappedType, elemTypeOut);
}

bool SemanticsValidator::extractCollectionElementType(const std::string &typeText,
                                                       const std::string &expectedBase,
                                                       std::string &elemTypeOut) const {
  elemTypeOut.clear();
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if (base != expectedBase) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  elemTypeOut = args.front();
  return true;
}

bool SemanticsValidator::resolveArgsPackElementMethodTarget(
    const std::string &elementTypeText, const Expr &receiverExpr,
    const std::string &normalizedMethodName,
    const std::function<bool(const std::string &)> &setCollectionMethodTarget,
    const std::function<bool(const Expr &, const std::string &)>
        &setPreferredKeyValueMethodTarget,
    std::string &resolvedOut, bool &isBuiltinOut) {
  const std::string normalizedElemType = normalizeBindingTypeName(elementTypeText);
  std::string normalizedElemBaseType = normalizedElemType;
  if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
    normalizedElemBaseType.erase(normalizedElemBaseType.begin());
  }
  std::string collectionElemType = normalizedElemType;
  std::string wrappedPointeeType;
  if (extractWrappedPointeeType(normalizedElemType, wrappedPointeeType)) {
    collectionElemType = normalizeBindingTypeName(wrappedPointeeType);
  }

  // Step 2 of docs/ReceiverTargetResolutionConsolidation.md: this function's
  // own classification cascade (R1-R9 of the Step 0 Rule Table's Row
  // category A) has been replaced by a single call into the shared
  // classifier proven byte-faithful by Step 1b's diff-audit harness (see
  // that section for the derivation of each of these two text inputs and
  // the fall-through quirks R2b/R4b/R6b the classifier reproduces
  // verbatim). Only the *downstream* action per family below is unchanged
  // from the pre-migration inline cascade.
  std::string elemBase;
  std::string elemArgText;
  const bool isTemplateShaped =
      splitTemplateTypeName(collectionElemType, elemBase, elemArgText);
  if (isTemplateShaped) {
    elemBase = normalizeBindingTypeName(elemBase);
  }
  primec::ReceiverElementFamilyJointInput jointInput;
  jointInput.unwrappedElementType = collectionElemType;
  jointInput.rawElementBaseType = normalizedElemBaseType;
  jointInput.isTemplateShaped = isTemplateShaped;
  jointInput.templateShapedBaseName = elemBase;
  jointInput.normalizedMethodName = normalizedMethodName;
  primec::ReceiverElementFamilyPredicates predicates{
      [](std::string_view name) {
        return isInternalSoaCollectionTypeName(name);
      },
      [](std::string_view name) {
        return isKeyValueSurfaceTypeName(std::string(name));
      },
  };
  const primec::ReceiverElementFamilyResult classified =
      primec::classifyReceiverElementFamilyJoint(jointInput, predicates);

  switch (classified.family) {
    case primec::ReceiverElementFamily::String:
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    case primec::ReceiverElementFamily::FileError:
      resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
      isBuiltinOut = resolvedOut == "/file_error/why";
      return !resolvedOut.empty();
    case primec::ReceiverElementFamily::VectorLike:
    case primec::ReceiverElementFamily::Soa:
      // R3: vector/array/soa share one dispatch shape; classified.collectionBaseName
      // is the already-normalized elemBase the pre-migration cascade used here.
      return setCollectionMethodTarget("/" + classified.collectionBaseName + "/" +
                                        normalizedMethodName);
    case primec::ReceiverElementFamily::Buffer:
      return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
    case primec::ReceiverElementFamily::KeyValue:
      return setPreferredKeyValueMethodTarget(receiverExpr, normalizedMethodName);
    case primec::ReceiverElementFamily::File:
      resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                             currentValidationState_.context.definitionPath);
      isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
      return true;
    case primec::ReceiverElementFamily::Primitive:
      // R7: deliberately built from normalizedElemBaseType (the raw,
      // non-Reference/Pointer-unwrapped text), matching production's
      // documented wrapped-vs-unwrapped asymmetry - NOT from
      // classified.normalizedElementBaseType, which the classifier always
      // derives from the *unwrapped* text and would silently discard that
      // asymmetry here.
      resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
      return true;
    case primec::ReceiverElementFamily::StructOrUnknown:
    default:
      break;
  }
  std::string resolvedElemType =
      resolveMethodTargetStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
  if (resolvedElemType.empty()) {
    resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
  }
  if (!resolvedElemType.empty()) {
    resolvedOut = resolvedElemType + "/" + normalizedMethodName;
    return true;
  }
  return false;
}

} // namespace primec::semantics
