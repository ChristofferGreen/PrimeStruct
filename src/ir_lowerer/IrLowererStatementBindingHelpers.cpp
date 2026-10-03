#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"

#include <algorithm>
#include <cctype>

#include "IrLowererStatementBindingInternal.h"

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererStatementBindingHelpersFileLocal.h"

namespace primec::ir_lowerer {
using namespace ir_lowerer_statement_binding_helpers_file_local;

bool resolveSpecializedExperimentalMapTypeKindsForBindingType(
    const std::string &typeText,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    LocalInfo::ValueKind &keyKindOut,
    LocalInfo::ValueKind &valueKindOut) {
  return resolveSpecializedExperimentalMapTypeKinds(
      typeText, resolveDefinitionCall, keyKindOut, valueKindOut);
}

bool resolveSpecializedKeyValueStorageStructPathForBindingType(
    const std::string &typeText,
    std::string &structPathOut) {
  return resolveSpecializedKeyValueStorageStructPathFromTypeText(typeText, structPathOut);
}

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const ExprPredicateFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const ExprLocalsValueKindFn &inferExprKind,
                                                       const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex) {
  const ResolveDefinitionCallForStatementFn safeResolveDefinitionCall =
      resolveDefinitionCall ? resolveDefinitionCall
                            : ResolveDefinitionCallForStatementFn([](const Expr &) { return nullptr; });
  StatementBindingTypeInfo info;
  info.kind = bindingKind(stmt);
  info.usesBuiltinCollectionLayout = exprUsesRawBuiltinSoaVectorLayout(stmt);
  const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  auto isRawStructBufferInit = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isBinding) {
      return false;
    }
    const std::string &scopedName = candidate.name;
    return scopedName == "Buffer" || scopedName == "/std/gfx/Buffer" ||
           scopedName == "/std/gfx/experimental/Buffer" ||
           scopedName.rfind("/std/gfx/Buffer__t", 0) == 0 ||
           scopedName.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
  };
  if (hasExplicitType && info.kind == LocalInfo::Kind::Buffer && isRawStructBufferInit(init)) {
    info.kind = LocalInfo::Kind::Value;
  }
  StatementBindingTypeInfo semanticInfo;
  const bool hasSemanticBindingInfo = populateBindingTypeInfoFromSemanticBindingFact(
      stmt, safeResolveDefinitionCall, semanticProgram, semanticIndex, semanticInfo);
  deferSurfaceStructTypeName(semanticInfo);
  StatementBindingTypeInfo semanticInitInfo;
  const bool hasSemanticInitBindingInfo = !hasExplicitType &&
      populateBindingTypeInfoFromSemanticBindingFact(
          init, safeResolveDefinitionCall, semanticProgram, semanticIndex, semanticInitInfo);
  deferSurfaceStructTypeName(semanticInitInfo);
  StatementBindingTypeInfo semanticTryInitInfo;
  const bool hasSemanticTryInitInfo =
      populateBindingTypeInfoFromSemanticTryFact(
          init, safeResolveDefinitionCall, semanticProgram, semanticIndex, semanticTryInitInfo);
  deferSurfaceStructTypeName(semanticTryInitInfo);
  if (!hasExplicitType && hasSemanticBindingInfo) {
    return semanticInfo;
  }
  LocalInfo::ValueKind inferredInitValueKind = LocalInfo::ValueKind::Unknown;
  if (!hasExplicitType && info.kind == LocalInfo::Kind::Value) {
    if (init.kind == Expr::Kind::Name) {
      if (hasSemanticInitBindingInfo) {
        info.kind = semanticInitInfo.kind;
      } else if (auto it = localsIn.find(init.name); it != localsIn.end()) {
        info.kind = it->second.kind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      if (hasSemanticTryInitInfo) {
        info.kind = semanticTryInitInfo.kind;
      }
      inferredInitValueKind = inferExprKind(init, localsIn);
      if (isPointerMemoryIntrinsicCall(init)) {
        info.kind = LocalInfo::Kind::Pointer;
      } else if (inferredInitValueKind == LocalInfo::ValueKind::Unknown) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection)) {
          if (collection == "array") {
            info.kind = LocalInfo::Kind::Array;
          } else if (collection == "vector") {
            info.kind = LocalInfo::Kind::Vector;
          } else if (collection == "map") {
            info.kind = LocalInfo::Kind::Value;
          }
        }
      }
    }
  }

  if (!hasExplicitType) {
    StatementBindingTypeInfo inferredExprInfo;
    if (inferExprBindingTypeInfo(
            init, localsIn, inferExprKind, safeResolveDefinitionCall, semanticProgram, semanticIndex, inferredExprInfo)) {
      deferSurfaceStructTypeName(inferredExprInfo);
      if (info.kind == LocalInfo::Kind::Value) {
        info.kind = inferredExprInfo.kind;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = inferredExprInfo.valueKind;
      }
      if (info.keyValueKeyKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueKeyKind = inferredExprInfo.keyValueKeyKind;
      }
      if (info.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueValueKind = inferredExprInfo.keyValueValueKind;
      }
      if (info.structTypeName.empty()) {
        info.structTypeName = inferredExprInfo.structTypeName;
      }
      info.usesBuiltinCollectionLayout =
          info.usesBuiltinCollectionLayout || inferredExprInfo.usesBuiltinCollectionLayout;
      mergeStatementBindingAuxTypeInfo(inferredExprInfo, info);
    }
  }

  if (info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
      info.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
    if (hasExplicitType) {
      for (const auto &transform : stmt.transforms) {
        const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
        if (normalizedName == "map" && transform.templateArgs.size() == 2) {
          info.keyValueKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
          info.keyValueValueKind = valueKindFromTypeName(transform.templateArgs[1]);
          if (info.structTypeName.empty()) {
            std::string declaredType = transform.name + "<" +
                                       trimTemplateTypeText(transform.templateArgs[0]) + ", " +
                                       trimTemplateTypeText(transform.templateArgs[1]) + ">";
            resolveSpecializedKeyValueStorageStructPathFromTypeText(
                declaredType, info.structTypeName);
          }
          break;
        }
        if (normalizedName == "map" && transform.templateArgs.empty() &&
            resolveSpecializedExperimentalMapTypeKinds(
                transform.name, safeResolveDefinitionCall, info.keyValueKeyKind, info.keyValueValueKind)) {
          if (info.structTypeName.empty()) {
            resolveSpecializedKeyValueStorageStructPathFromTypeText(
                transform.name, info.structTypeName);
          }
          break;
        }
      }
    } else if (init.kind == Expr::Kind::Name) {
      if (hasSemanticInitBindingInfo &&
          semanticInitInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
          semanticInitInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
        info.keyValueKeyKind = semanticInitInfo.keyValueKeyKind;
        info.keyValueValueKind = semanticInitInfo.keyValueValueKind;
        if (info.structTypeName.empty()) {
          info.structTypeName = semanticInitInfo.structTypeName;
        }
      } else if (auto it = localsIn.find(init.name);
                 it != localsIn.end() && hasKeyValueKinds(it->second)) {
        info.keyValueKeyKind = it->second.keyValueKeyKind;
        info.keyValueValueKind = it->second.keyValueValueKind;
        if (info.structTypeName.empty()) {
          info.structTypeName = it->second.structTypeName;
        }
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
        info.keyValueKeyKind = valueKindFromTypeName(init.templateArgs[0]);
        info.keyValueValueKind = valueKindFromTypeName(init.templateArgs[1]);
        if (info.structTypeName.empty()) {
          std::string initType = "map<" + trimTemplateTypeText(init.templateArgs[0]) + ", " +
                                 trimTemplateTypeText(init.templateArgs[1]) + ">";
          resolveSpecializedKeyValueStorageStructPathFromTypeText(
              initType, info.structTypeName);
        }
      }
    }
    info.valueKind = info.keyValueValueKind;
    return info;
  }

  if (hasExplicitType) {
    std::string explicitTypeName;
    std::vector<std::string> explicitTemplateArgs;
    if (extractFirstBindingTypeTransform(stmt, explicitTypeName, explicitTemplateArgs)) {
      // TODO-5251: Reference<T, Capability>/Pointer<T, Capability>/
      // Slice<T, Capability> carry an optional capability marker as a
      // second template argument that is not part of the underlying
      // type identity (see SemanticsHelpersCore.cpp's typeCapabilityArg
      // handling). Reconstructing "Reference<int, Read>" here and feeding
      // it whole to populateBindingTypeInfoFromTypeText below caused the
      // unsplit "int, Read" text to be treated as a single (bogus)
      // pointee/struct type - drop the capability argument before
      // rebuilding the text, matching every other reconstruction of this
      // text elsewhere in the compiler.
      const std::string normalizedExplicitTypeName =
          normalizeCollectionBindingTypeName(explicitTypeName);
      if ((normalizedExplicitTypeName == "Reference" || normalizedExplicitTypeName == "Pointer" ||
           normalizedExplicitTypeName == "array") &&
          explicitTemplateArgs.size() > 1) {
        // "array" here also catches Slice<T, Capability>, which
        // normalizes to "array" but keeps its raw AST transform name
        // "Slice" - normalizeCollectionBindingTypeName maps it before this
        // check runs.
        explicitTemplateArgs.resize(1);
      }
      std::string explicitTypeText = explicitTypeName;
      if (!explicitTemplateArgs.empty()) {
        explicitTypeText += "<" + joinTemplateArgsText(explicitTemplateArgs) + ">";
      }
      if (normalizeCollectionBindingTypeName(explicitTypeName) == "soa" &&
          info.structTypeName.empty()) {
        resolveSpecializedExperimentalSoaVectorStructPath(
            explicitTypeText, info.structTypeName);
      }
      StatementBindingTypeInfo explicitTypeInfo;
      if (populateBindingTypeInfoFromTypeText(
              explicitTypeText, safeResolveDefinitionCall, explicitTypeInfo)) {
        deferSurfaceStructTypeName(explicitTypeInfo);
        if (info.kind == LocalInfo::Kind::Value ||
            (explicitTypeInfo.kind == LocalInfo::Kind::Value &&
             !explicitTypeInfo.structTypeName.empty())) {
          info.kind = explicitTypeInfo.kind;
        }
        if (info.valueKind == LocalInfo::ValueKind::Unknown) {
          info.valueKind = explicitTypeInfo.valueKind;
        }
        if (info.keyValueKeyKind == LocalInfo::ValueKind::Unknown) {
          info.keyValueKeyKind = explicitTypeInfo.keyValueKeyKind;
        }
        if (info.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
          info.keyValueValueKind = explicitTypeInfo.keyValueValueKind;
        }
        if (info.structTypeName.empty()) {
          info.structTypeName = explicitTypeInfo.structTypeName;
        }
        mergeStatementBindingAuxTypeInfo(explicitTypeInfo, info);
      }
    }
    if (hasSemanticBindingInfo) {
      if (info.kind == LocalInfo::Kind::Value ||
          (semanticInfo.kind == LocalInfo::Kind::Value &&
           !semanticInfo.structTypeName.empty())) {
        info.kind = semanticInfo.kind;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = semanticInfo.valueKind;
      }
      if (info.keyValueKeyKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueKeyKind = semanticInfo.keyValueKeyKind;
      }
      if (info.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueValueKind = semanticInfo.keyValueValueKind;
      }
      if (info.structTypeName.empty()) {
        info.structTypeName = semanticInfo.structTypeName;
      }
      mergeStatementBindingAuxTypeInfo(semanticInfo, info);
    }
    if (hasSemanticTryInitInfo) {
      if (info.kind == LocalInfo::Kind::Value ||
          (semanticTryInitInfo.kind == LocalInfo::Kind::Value &&
           !semanticTryInitInfo.structTypeName.empty())) {
        info.kind = semanticTryInitInfo.kind;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = semanticTryInitInfo.valueKind;
      }
      if (info.keyValueKeyKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueKeyKind = semanticTryInitInfo.keyValueKeyKind;
      }
      if (info.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
        info.keyValueValueKind = semanticTryInitInfo.keyValueValueKind;
      }
      if (info.structTypeName.empty()) {
        info.structTypeName = semanticTryInitInfo.structTypeName;
      }
      mergeStatementBindingAuxTypeInfo(semanticTryInitInfo, info);
    }
    const LocalInfo::ValueKind declaredValueKind = bindingValueKind(stmt, info.kind);
    if (declaredValueKind != LocalInfo::ValueKind::Unknown) {
      info.valueKind = declaredValueKind;
    }
    if (info.valueKind == LocalInfo::ValueKind::Unknown &&
        info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
        info.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
      info.valueKind = info.keyValueValueKind;
    }
    return info;
  }

  auto applySemanticInitializerInfo = [&]() {
    const StatementBindingTypeInfo *initializerInfo = nullptr;
    if (hasSemanticInitBindingInfo && semanticInitInfo.kind == info.kind) {
      initializerInfo = &semanticInitInfo;
    } else if (hasSemanticTryInitInfo && semanticTryInitInfo.kind == info.kind) {
      initializerInfo = &semanticTryInitInfo;
    }
    if (initializerInfo == nullptr) {
      return false;
    }
    info.valueKind = initializerInfo->valueKind;
    info.keyValueKeyKind = initializerInfo->keyValueKeyKind;
    info.keyValueValueKind = initializerInfo->keyValueValueKind;
    info.structTypeName = initializerInfo->structTypeName;
    info.usesBuiltinCollectionLayout =
        info.usesBuiltinCollectionLayout || initializerInfo->usesBuiltinCollectionLayout;
    mergeStatementBindingAuxTypeInfo(*initializerInfo, info);
    return true;
  };

  if (info.kind == LocalInfo::Kind::Value) {
    if (applySemanticInitializerInfo()) {
      return info;
    }
    const LocalInfo::ValueKind specialInitValueKind = inferSpecialCallValueKind(init);
    info.valueKind = (specialInitValueKind != LocalInfo::ValueKind::Unknown)
                         ? specialInitValueKind
                         : ((inferredInitValueKind != LocalInfo::ValueKind::Unknown)
                                ? inferredInitValueKind
                                : inferExprKind(init, localsIn));
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string builtinComparison;
      if (getBuiltinComparisonName(init, builtinComparison)) {
        info.valueKind = LocalInfo::ValueKind::Bool;
      }
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) {
    if (applySemanticInitializerInfo()) {
      return info;
    }
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        info.valueKind = it->second.valueKind;
      }
      if (info.structTypeName.empty()) {
        info.structTypeName = (it != localsIn.end()) ? it->second.structTypeName : "";
      }
    } else if (info.kind == LocalInfo::Kind::Pointer && init.kind == Expr::Kind::Call &&
               isPointerMemoryIntrinsicCall(init)) {
      info.valueKind = inferPointerMemoryIntrinsicValueKind(init, localsIn, inferExprKind);
      info.structTypeName = inferPointerMemoryIntrinsicStructType(init, localsIn);
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector) {
    if (applySemanticInitializerInfo()) {
      return info;
    }
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
        info.valueKind = it->second.valueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
          init.templateArgs.size() == 1) {
        info.valueKind = valueKindFromTypeName(init.templateArgs.front());
      }
    }
  }

  return info;
}

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const ExprPredicateFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const ExprLocalsValueKindFn &inferExprKind,
                                                       const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
                                                       const SemanticProductTargetAdapter *semanticProductTargets) {
  return inferStatementBindingTypeInfo(
      stmt,
      init,
      localsIn,
      hasExplicitBindingTypeTransform,
      bindingKind,
      bindingValueKind,
      inferExprKind,
      resolveDefinitionCall,
      semanticProductTargets == nullptr ? nullptr : semanticProductTargets->semanticProgram,
      semanticProductTargets == nullptr ? nullptr : &semanticProductTargets->semanticIndex);
}

bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const ExprPredicateFn &isBindingMutable,
                                 const ExprPredicateFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const ExprLocalsValueKindFn &inferExprKind,
                                 const ExprPredicateFn &isFileErrorBinding,
                                 const ExprLocalInfoVisitorFn &setReferenceArrayInfo,
                                 const ExprLocalInfoVisitorFn &applyStructArrayInfo,
                                 const ExprLocalInfoVisitorFn &applyStructValueInfo,
                                 const ExprPredicateFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition,
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex) {
  const ExprPredicateFn noopIsBindingMutable = [](const Expr &) { return false; };
  const ExprPredicateFn noopHasExplicitBindingTypeTransform =
      [](const Expr &) { return false; };
  const BindingKindFn noopBindingKind = [](const Expr &) { return LocalInfo::Kind::Value; };
  const BindingValueKindFn noopBindingValueKind =
      [](const Expr &, LocalInfo::Kind) { return LocalInfo::ValueKind::Unknown; };
  const ExprLocalsValueKindFn noopInferExprKind =
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; };
  const ExprPredicateFn noopIsFileErrorBinding = [](const Expr &) { return false; };
  const ExprLocalInfoVisitorFn noopSetReferenceArrayInfo =
      [](const Expr &, LocalInfo &) {};
  const ExprLocalInfoVisitorFn noopApplyStructInfo = [](const Expr &, LocalInfo &) {};
  const ExprPredicateFn noopIsStringBinding = [](const Expr &) { return false; };
  const std::function<const Definition *(const Expr &, const LocalMap &)> noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const std::function<const Definition *(const Expr &)> noopResolveDefinitionCall =
      [](const Expr &) -> const Definition * { return nullptr; };
  const std::function<bool(const std::string &, ReturnInfo &)> noopGetReturnInfo =
      [](const std::string &, ReturnInfo &) { return false; };
  const ExprPredicateFn &isBindingMutableFn =
      isBindingMutable ? isBindingMutable : noopIsBindingMutable;
  const ExprPredicateFn &hasExplicitBindingTypeTransformFn =
      hasExplicitBindingTypeTransform ? hasExplicitBindingTypeTransform
                                      : noopHasExplicitBindingTypeTransform;
  const BindingKindFn &bindingKindFn = bindingKind ? bindingKind : noopBindingKind;
  const BindingValueKindFn &bindingValueKindFn =
      bindingValueKind ? bindingValueKind : noopBindingValueKind;
  const ExprLocalsValueKindFn &inferExprKindFn =
      inferExprKind ? inferExprKind : noopInferExprKind;
  const ExprPredicateFn &isFileErrorBindingFn =
      isFileErrorBinding ? isFileErrorBinding : noopIsFileErrorBinding;
  const ExprLocalInfoVisitorFn &setReferenceArrayInfoFn =
      setReferenceArrayInfo ? setReferenceArrayInfo : noopSetReferenceArrayInfo;
  const ExprLocalInfoVisitorFn &applyStructArrayInfoFn =
      applyStructArrayInfo ? applyStructArrayInfo : noopApplyStructInfo;
  const ExprLocalInfoVisitorFn &applyStructValueInfoFn =
      applyStructValueInfo ? applyStructValueInfo : noopApplyStructInfo;
  const ExprPredicateFn &isStringBindingFn =
      isStringBinding ? isStringBinding : noopIsStringBinding;
  const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinitionFn =
      resolveMethodCallDefinition ? resolveMethodCallDefinition : noopResolveMethodCall;
  const std::function<const Definition *(const Expr &)> &resolveDefinitionCallFn =
      resolveDefinitionCall ? resolveDefinitionCall : noopResolveDefinitionCall;
  const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoFn =
      getReturnInfo ? getReturnInfo : noopGetReturnInfo;

  infoOut.isMutable = isBindingMutableFn(param);
  infoOut.isSoaVector = hasSoaVectorTypeTransform(param);
  infoOut.usesBuiltinCollectionLayout = exprUsesRawBuiltinSoaVectorLayout(param);
  infoOut.isArgsPack = isArgsPackBinding(param);
  infoOut.kind = bindingKindFn(param);
  if (hasExplicitBindingTypeTransformFn(param)) {
    infoOut.valueKind = bindingValueKindFn(param, infoOut.kind);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value &&
             isPointerMemoryIntrinsicCall(param.args.front())) {
    infoOut.kind = LocalInfo::Kind::Pointer;
    infoOut.valueKind =
        inferPointerMemoryIntrinsicValueKind(param.args.front(), localsForKindInference, inferExprKindFn);
    infoOut.structTypeName = inferPointerMemoryIntrinsicStructType(param.args.front(), localsForKindInference);
    infoOut.targetsUninitializedStorage =
        inferPointerMemoryIntrinsicTargetsUninitializedStorage(param.args.front(), localsForKindInference);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value) {
    infoOut.valueKind = inferExprKindFn(param.args.front(), localsForKindInference);
    if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string builtinComparison;
      if (getBuiltinComparisonName(param.args.front(), builtinComparison)) {
        infoOut.valueKind = LocalInfo::ValueKind::Bool;
      } else {
        infoOut.valueKind = LocalInfo::ValueKind::Int32;
      }
    }
    ResultExprInfo inferredResultInfo;
    if (inferCallParameterDefaultResultInfo(
            param.args.front(),
            localsForKindInference,
            inferExprKindFn,
            resolveMethodCallDefinitionFn,
            resolveDefinitionCallFn,
            getReturnInfoFn,
            inferredResultInfo,
            semanticProgram,
            semanticIndex) &&
        inferredResultInfo.isResult) {
      infoOut.isResult = true;
      infoOut.resultHasValue = inferredResultInfo.hasValue;
      infoOut.resultValueKind = inferredResultInfo.valueKind;
      infoOut.resultValueCollectionKind = inferredResultInfo.valueCollectionKind;
      infoOut.resultValueMapKeyKind = inferredResultInfo.valueMapKeyKind;
      infoOut.resultValueIsFileHandle = inferredResultInfo.valueIsFileHandle;
      infoOut.resultValueStructType = inferredResultInfo.valueStructType;
      infoOut.resultErrorType = inferredResultInfo.errorType;
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    }
  } else {
    infoOut.valueKind = bindingValueKindFn(param, infoOut.kind);
  }

  StatementBindingTypeInfo semanticBindingTypeInfo;
  if (populateBindingTypeInfoFromSemanticBindingFact(
          param, resolveDefinitionCallFn, semanticProgram, semanticIndex, semanticBindingTypeInfo)) {
    infoOut.kind = semanticBindingTypeInfo.kind;
    infoOut.valueKind = semanticBindingTypeInfo.valueKind;
    infoOut.keyValueKeyKind = semanticBindingTypeInfo.keyValueKeyKind;
    infoOut.keyValueValueKind = semanticBindingTypeInfo.keyValueValueKind;
    infoOut.structTypeName = semanticBindingTypeInfo.structTypeName;
    infoOut.referenceToArray = semanticBindingTypeInfo.referenceToArray;
    infoOut.pointerToArray = semanticBindingTypeInfo.pointerToArray;
    infoOut.referenceToVector = semanticBindingTypeInfo.referenceToVector;
    infoOut.pointerToVector = semanticBindingTypeInfo.pointerToVector;
    infoOut.referenceToBuffer = semanticBindingTypeInfo.referenceToBuffer;
    infoOut.pointerToBuffer = semanticBindingTypeInfo.pointerToBuffer;
    infoOut.isSoaVector = semanticBindingTypeInfo.isSoaVector;
    infoOut.usesBuiltinCollectionLayout =
        semanticBindingTypeInfo.usesBuiltinCollectionLayout;
    LocalInfo::ValueKind declaredValueKind =
        bindingValueKindFromTransforms(param, infoOut.kind);
    if (declaredValueKind == LocalInfo::ValueKind::Unknown) {
      declaredValueKind = bindingValueKindFn(param, infoOut.kind);
    }
    if (hasExplicitBindingTypeTransformFn(param) &&
        declaredValueKind != LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = declaredValueKind;
    }
  }

  if (hasKeyValueKinds(infoOut)) {
    for (const auto &transform : param.transforms) {
      const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
      if (normalizedName == "map" && transform.templateArgs.size() == 2) {
        infoOut.keyValueKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
        infoOut.keyValueValueKind = valueKindFromTypeName(transform.templateArgs[1]);
        infoOut.valueKind = infoOut.keyValueValueKind;
        if (infoOut.structTypeName.empty()) {
          std::string declaredType = transform.name + "<" +
                                     trimTemplateTypeText(transform.templateArgs[0]) + ", " +
                                     trimTemplateTypeText(transform.templateArgs[1]) + ">";
          std::string specializedStructPath;
          if (resolveSpecializedKeyValueStorageStructPathFromTypeText(
                  declaredType, specializedStructPath)) {
            infoOut.structTypeName = std::move(specializedStructPath);
          }
        }
        break;
      }
    }
  }
  for (const auto &transform : param.transforms) {
    if (normalizeCollectionBindingTypeName(transform.name) == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
    } else if (isCollectionVectorSurfaceBase(transform.name) &&
               transform.templateArgs.size() == 1) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      infoOut.structTypeName =
          specializedCollectionVectorRecordPathForElementType(
              trimTemplateTypeText(transform.templateArgs.front()));
    } else if (applyErrorTypeMetadata(transform.name, infoOut)) {
      continue;
    } else if (transform.name == "Result") {
      infoOut.isResult = true;
      infoOut.resultHasValue = (transform.templateArgs.size() == 2);
      infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
      infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueStructType.clear();
      if (infoOut.resultHasValue && !transform.templateArgs.empty()) {
        resolveSupportedResultCollectionType(
            transform.templateArgs.front(),
            infoOut.resultValueCollectionKind,
            infoOut.resultValueKind,
            &infoOut.resultValueMapKeyKind);
        if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value &&
            infoOut.resultValueKind == LocalInfo::ValueKind::Unknown &&
            resolveSpecializedExperimentalMapTypeKinds(
                transform.templateArgs.front(),
                resolveDefinitionCallFn,
                infoOut.resultValueMapKeyKind,
                infoOut.resultValueKind)) {
          infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
        } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          LocalInfo resultValueInfo;
          if (applyErrorTypeMetadata(transform.templateArgs.front(), resultValueInfo) &&
              !resultValueInfo.structTypeName.empty()) {
            infoOut.resultValueStructType = resultValueInfo.structTypeName;
            infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
          } else {
            infoOut.resultValueKind = valueKindFromTypeName(transform.templateArgs.front());
          }
        }
      }
      infoOut.resultValueIsFileHandle =
          infoOut.resultHasValue && !transform.templateArgs.empty() &&
          isFileHandleTypeText(transform.templateArgs.front());
      if (infoOut.resultValueIsFileHandle) {
        infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
      }
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      if (!transform.templateArgs.empty()) {
        infoOut.resultErrorType = transform.templateArgs.back();
      }
    } else if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
      const std::string originalTargetType = trimTemplateTypeText(transform.templateArgs.front());
      std::string targetType = originalTargetType;
      if (extractTopLevelUninitializedTypeText(originalTargetType, targetType)) {
        infoOut.targetsUninitializedStorage = true;
      }
      std::string wrappedBase;
      std::string wrappedArg;
      if (splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "File") {
        infoOut.isFileHandle = true;
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "array") {
        infoOut.pointerToArray = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "vector") {
        infoOut.pointerToVector = true;
        const std::string elementType = trimTemplateTypeText(wrappedArg);
        if (isCollectionVectorSurfaceBase(wrappedBase)) {
          infoOut.valueKind = LocalInfo::ValueKind::Int64;
          infoOut.structTypeName =
              specializedCollectionVectorRecordPathForElementType(elementType);
        } else {
          infoOut.valueKind = valueKindFromTypeName(elementType);
          if (infoOut.valueKind == LocalInfo::ValueKind::Unknown && infoOut.structTypeName.empty()) {
            infoOut.structTypeName =
                specializedCollectionVectorRecordPathForElementType(elementType);
          }
        }
      }
      if (transform.name == "Reference" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "vector" &&
          isCollectionVectorSurfaceBase(wrappedBase)) {
        infoOut.referenceToVector = true;
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
        infoOut.structTypeName =
            specializedCollectionVectorRecordPathForElementType(
                trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "soa") {
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        const std::string elementType = trimTemplateTypeText(wrappedArg);
        infoOut.valueKind = valueKindFromTypeName(elementType);
        if (infoOut.valueKind == LocalInfo::ValueKind::Unknown && infoOut.structTypeName.empty()) {
          infoOut.structTypeName =
              specializedExperimentalSoaVectorStructPathForElementType(
                  elementType);
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "map") {
        std::vector<std::string> args;
        if (splitTemplateArgs(wrappedArg, args) && args.size() == 2) {
          infoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
          infoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
          infoOut.valueKind = infoOut.keyValueValueKind;
          if (infoOut.structTypeName.empty()) {
            resolveSpecializedKeyValueStorageStructPathFromTypeText(targetType, infoOut.structTypeName);
          }
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "Buffer") {
        infoOut.pointerToBuffer = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      applyErrorTypeMetadata(targetType, infoOut);
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
        infoOut.isResult = true;
        infoOut.resultHasValue = resultHasValue;
        std::string resultValueType;
        infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
        infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
        infoOut.resultValueIsFileHandle =
            resultHasValue && extractResultValueTypeText(targetType, resultValueType) &&
            isFileHandleTypeText(resultValueType);
        if (resultHasValue && !resultValueType.empty()) {
          resolveSupportedResultCollectionType(
              resultValueType,
              infoOut.resultValueCollectionKind,
              infoOut.resultValueKind,
              &infoOut.resultValueMapKeyKind);
        }
        if (infoOut.resultValueIsFileHandle) {
          infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
        } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          infoOut.resultValueKind = resultValueKind;
        }
        infoOut.valueKind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = resultErrorType;
      }
    }
  }

  if (infoOut.isArgsPack) {
    std::string elementTypeText;
    bool hasElementTypeText = extractArgsPackElementTypeText(param, elementTypeText);
    if (!hasElementTypeText && semanticProgram != nullptr && semanticIndex != nullptr &&
        param.semanticNodeId != 0) {
      const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, param);
      const std::string bindingTypeText =
          bindingFact != nullptr
              ? resolveSemanticBindingTypeText(semanticProgram, *bindingFact)
              : std::string{};
      if (bindingFact == nullptr || bindingTypeText.empty()) {
        error = "missing semantic-product args-pack binding type: " +
                (param.name.empty() ? std::string("<unnamed>") : param.name);
        return false;
      }
      hasElementTypeText = extractArgsPackElementTypeTextFromTypeText(
          bindingTypeText, elementTypeText);
      if (!hasElementTypeText) {
        error = "incomplete semantic-product args-pack binding type: " +
                (param.name.empty() ? std::string("<unnamed>") : param.name);
        return false;
      }
    }
    if (hasElementTypeText) {
      applyArgsPackElementMetadata(elementTypeText, infoOut);
      applyArgsPackElementStructMetadata(
          param, elementTypeText, applyStructArrayInfoFn, applyStructValueInfoFn, infoOut);
    }
  }

  if (infoOut.errorTypeName == "GfxError" && infoOut.errorHelperNamespacePath.empty() && param.args.size() == 1) {
    const Expr &initExpr = param.args.front();
    const Definition *initDef =
        initExpr.isMethodCall ? resolveMethodCallDefinitionFn(initExpr, localsForKindInference)
                              : resolveDefinitionCallFn(initExpr);
    if (initDef != nullptr) {
      if (initDef->fullPath.rfind("/std/gfx/experimental/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/experimental/GfxError";
      } else if (initDef->fullPath.rfind("/std/gfx/", 0) == 0 ||
                 initDef->fullPath.rfind("/GfxError/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/GfxError";
      }
    }
  }

  infoOut.isFileError = infoOut.isFileError || isFileErrorBindingFn(param);
  auto applySpecializedWrappedMapBindingInfo = [&](const Expr &bindingExpr, LocalInfo &bindingInfo) {
    if ((bindingInfo.kind != LocalInfo::Kind::Reference &&
         bindingInfo.kind != LocalInfo::Kind::Pointer) ||
        hasKeyValueKinds(bindingInfo)) {
      return;
    }
    for (const auto &transform : bindingExpr.transforms) {
      if ((bindingInfo.kind == LocalInfo::Kind::Reference && transform.name != "Reference") ||
          (bindingInfo.kind == LocalInfo::Kind::Pointer && transform.name != "Pointer") ||
          transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string targetType = unwrapTopLevelUninitializedTypeText(transform.templateArgs.front());
      LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      if (!resolveSpecializedExperimentalMapTypeKindsForBindingType(
              targetType, resolveDefinitionCallFn, keyKind, valueKind)) {
        continue;
      }
      bindingInfo.keyValueKeyKind = keyKind;
      bindingInfo.keyValueValueKind = valueKind;
      bindingInfo.valueKind = valueKind;
      if (bindingInfo.structTypeName.empty()) {
        resolveSpecializedKeyValueStorageStructPathForBindingType(
            targetType, bindingInfo.structTypeName);
      }
      return;
    }
  };
  setReferenceArrayInfoFn(param, infoOut);
  applySpecializedWrappedMapBindingInfo(param, infoOut);
  applyStructArrayInfoFn(param, infoOut);
  applyStructValueInfoFn(param, infoOut);
  if (infoOut.isFileHandle) {
    infoOut.structTypeName.clear();
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
  }
  if ((infoOut.kind == LocalInfo::Kind::Reference || infoOut.kind == LocalInfo::Kind::Pointer) &&
      (infoOut.referenceToVector || infoOut.pointerToVector)) {
    const bool preserveSpecializedCollectionStruct =
        (infoOut.isSoaVector &&
         isSpecializedExperimentalSoaVectorStructPathText(infoOut.structTypeName)) ||
        (!infoOut.isSoaVector &&
         isSpecializedExperimentalVectorTypeText(infoOut.structTypeName));
    if (!preserveSpecializedCollectionStruct) {
      infoOut.structTypeName = infoOut.isSoaVector ? collection_helpers::kRootedSoa : collection_helpers::kRootedVector;
    }
  }
  if (infoOut.kind == LocalInfo::Kind::Value && !infoOut.structTypeName.empty()) {
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
  }
  const bool isUnsupportedStringPointerReferenceArgsPack = [&param]() {
    for (const auto &transform : param.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities" ||
          isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.name != "args" || transform.templateArgs.size() != 1) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg)) {
        return false;
      }
      const std::string normalizedBase = normalizeCollectionBindingTypeName(base);
      return (normalizedBase == "Pointer" || normalizedBase == "Reference") &&
             trimTemplateTypeText(arg) == "string";
    }
    return false;
  }();
  if (isUnsupportedStringPointerReferenceArgsPack) {
    error = "variadic args<T> does not support string pointers or references";
    return false;
  }
  if (!isStringBindingFn(param)) {
    return true;
  }
  if (infoOut.kind != LocalInfo::Kind::Value) {
    error = "native backend does not support string pointers or references";
    return false;
  }
  infoOut.valueKind = LocalInfo::ValueKind::String;
  infoOut.stringSource = LocalInfo::StringSource::RuntimeIndex;
  infoOut.stringIndex = -1;
  infoOut.argvChecked = true;
  return true;
}

bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const ExprPredicateFn &isBindingMutable,
                                 const ExprPredicateFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const ExprLocalsValueKindFn &inferExprKind,
                                 const ExprPredicateFn &isFileErrorBinding,
                                 const ExprLocalInfoVisitorFn &setReferenceArrayInfo,
                                 const ExprLocalInfoVisitorFn &applyStructArrayInfo,
                                 const ExprLocalInfoVisitorFn &applyStructValueInfo,
                                 const ExprPredicateFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition,
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
                                 const SemanticProductTargetAdapter *semanticProductTargets) {
  return inferCallParameterLocalInfo(
      param,
      localsForKindInference,
      isBindingMutable,
      hasExplicitBindingTypeTransform,
      bindingKind,
      bindingValueKind,
      inferExprKind,
      isFileErrorBinding,
      setReferenceArrayInfo,
      applyStructArrayInfo,
      applyStructValueInfo,
      isStringBinding,
      infoOut,
      error,
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      getReturnInfo,
      semanticProductTargets == nullptr ? nullptr : semanticProductTargets->semanticProgram,
      semanticProductTargets == nullptr ? nullptr : &semanticProductTargets->semanticIndex);
}

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error) {
  zeroOp = IrOpcode::PushI32;
  zeroImm = 0;
  if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector ||
      kind == LocalInfo::Kind::Buffer) {
    zeroOp = IrOpcode::PushI64;
    return true;
  }

  switch (valueKind) {
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
      zeroOp = IrOpcode::PushI64;
      return true;
    case LocalInfo::ValueKind::Float32:
      zeroOp = IrOpcode::PushF32;
      return true;
    case LocalInfo::ValueKind::Float64:
      zeroOp = IrOpcode::PushF64;
      return true;
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Bool:
      zeroOp = IrOpcode::PushI32;
      return true;
    case LocalInfo::ValueKind::String:
      zeroOp = IrOpcode::PushI64;
      return true;
    default:
      error = "native backend requires a concrete uninitialized storage type on " + bindingName;
      return false;
  }
}

} // namespace primec::ir_lowerer
