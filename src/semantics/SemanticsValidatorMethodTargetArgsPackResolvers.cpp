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

  // Step 1b differential-audit harness
  // (docs/ReceiverTargetResolutionConsolidation.md, PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT):
  // computes the shared classifier's verdict alongside this function's own
  // inline cascade below and compares the two, purely for observation - it
  // never influences this function's actual return value. Zero-cost when
  // the env var is unset (a single cached getenv check).
  const bool diffAuditEnabled = primec::isReceiverTargetDiffAuditEnabled();
  primec::ReceiverElementFamily classifierFamilyForAudit =
      primec::ReceiverElementFamily::StructOrUnknown;
  if (diffAuditEnabled) {
    std::string auditElemBase;
    std::string auditElemArgText;
    const bool auditIsTemplateShaped =
        splitTemplateTypeName(collectionElemType, auditElemBase, auditElemArgText);
    if (auditIsTemplateShaped) {
      auditElemBase = normalizeBindingTypeName(auditElemBase);
    }
    primec::ReceiverElementFamilyJointInput jointInput;
    jointInput.unwrappedElementType = collectionElemType;
    jointInput.rawElementBaseType = normalizedElemBaseType;
    jointInput.isTemplateShaped = auditIsTemplateShaped;
    jointInput.templateShapedBaseName = auditElemBase;
    jointInput.normalizedMethodName = normalizedMethodName;
    primec::ReceiverElementFamilyPredicates auditPredicates{
        [](std::string_view name) {
          return isInternalSoaCollectionTypeName(name);
        },
        [](std::string_view name) {
          return isKeyValueSurfaceTypeName(std::string(name));
        },
    };
    classifierFamilyForAudit =
        primec::classifyReceiverElementFamilyJoint(jointInput, auditPredicates).family;
  }
  auto auditFamily = [&](primec::ReceiverElementFamily productionFamily) {
    if (!diffAuditEnabled) {
      return;
    }
    if (productionFamily != classifierFamilyForAudit) {
      std::cerr << "[receiver-target-diff-audit] MISMATCH in "
                   "resolveArgsPackElementMethodTarget: elementTypeText=\""
                << elementTypeText << "\" methodName=\"" << normalizedMethodName
                << "\" production=" << primec::describeReceiverElementFamily(productionFamily)
                << " classifier=" << primec::describeReceiverElementFamily(classifierFamilyForAudit)
                << "\n";
    }
    assert(productionFamily == classifierFamilyForAudit &&
           "receiver-target diff audit: classifier/production family disagreement "
           "(PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT) - see "
           "docs/ReceiverTargetResolutionConsolidation.md Step 1b");
  };

  if (collectionElemType == "string" || normalizedElemBaseType == "string") {
    auditFamily(primec::ReceiverElementFamily::String);
    return setCollectionMethodTarget("/string/" + normalizedMethodName);
  }
  if (collectionElemType == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    auditFamily(primec::ReceiverElementFamily::FileError);
    return !resolvedOut.empty();
  }
  std::string elemBase;
  std::string elemArgText;
  if (splitTemplateTypeName(collectionElemType, elemBase, elemArgText)) {
    elemBase = normalizeBindingTypeName(elemBase);
    if (elemBase == "vector" || elemBase == "array" ||
        isInternalSoaCollectionTypeName(elemBase)) {
      auditFamily(elemBase == "vector" || elemBase == "array"
                       ? primec::ReceiverElementFamily::VectorLike
                       : primec::ReceiverElementFamily::Soa);
      return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
    }
    if (elemBase == "Buffer" &&
        (normalizedMethodName == "count" || normalizedMethodName == "empty" ||
         normalizedMethodName == "is_valid" || normalizedMethodName == "readback" ||
         normalizedMethodName == "load" || normalizedMethodName == "store")) {
      auditFamily(primec::ReceiverElementFamily::Buffer);
      return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
    }
    if (isKeyValueSurfaceTypeName(elemBase)) {
      auditFamily(primec::ReceiverElementFamily::KeyValue);
      return setPreferredKeyValueMethodTarget(receiverExpr, normalizedMethodName);
    }
    if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
      resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                             currentValidationState_.context.definitionPath);
      isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
      auditFamily(primec::ReceiverElementFamily::File);
      return true;
    }
  }
  if (isPrimitiveBindingTypeName(normalizedElemBaseType)) {
    resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
    auditFamily(primec::ReceiverElementFamily::Primitive);
    return true;
  }
  std::string resolvedElemType =
      resolveMethodTargetStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
  if (resolvedElemType.empty()) {
    resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
  }
  if (!resolvedElemType.empty()) {
    resolvedOut = resolvedElemType + "/" + normalizedMethodName;
    auditFamily(primec::ReceiverElementFamily::StructOrUnknown);
    return true;
  }
  auditFamily(primec::ReceiverElementFamily::StructOrUnknown);
  return false;
}

} // namespace primec::semantics
