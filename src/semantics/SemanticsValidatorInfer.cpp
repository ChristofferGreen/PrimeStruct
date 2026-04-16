#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace primec::semantics {

ReturnKind SemanticsValidator::inferExprReturnKind(const Expr &expr,
                                                   const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals) {
  const uint64_t localsRevision = currentLocalBindingMemoRevision(&locals);
  const ExprReturnKindMemoKey memoKey{
      &expr,
      currentDefinitionContext_,
      currentExecutionContext_,
      params.empty() ? nullptr : params.data(),
      params.size(),
      &locals,
      locals.size(),
  };
  if (const auto memoIt = inferExprReturnKindMemo_.find(memoKey);
      memoIt != inferExprReturnKindMemo_.end() &&
      memoIt->second.localsRevision == localsRevision) {
    return memoIt->second.kind;
  }
  const ReturnKind inferred = inferExprReturnKindImpl(expr, params, locals);
  inferExprReturnKindMemo_[memoKey] = ExprReturnKindMemoValue{inferred, localsRevision};
  return inferred;
}

ReturnKind SemanticsValidator::inferExprReturnKindImpl(const Expr &expr,
                                                       const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  if (expr.isLambda) {
    return ReturnKind::Unknown;
  }

  if (expr.kind == Expr::Kind::Literal) {
    if (expr.isUnsigned) {
      return ReturnKind::UInt64;
    }
    if (expr.intWidth == 64) {
      return ReturnKind::Int64;
    }
    return ReturnKind::Int;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return ReturnKind::Bool;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return ReturnKind::String;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
        ReturnKind refKind = inferReferenceTargetKind(paramBinding->typeTemplateArg, expr.namespacePrefix);
        if (refKind != ReturnKind::Unknown) {
          return refKind;
        }
      }
      const std::string normalizedTypeName = normalizeBindingTypeName(paramBinding->typeName);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           normalizedTypeName == "soa_vector" || isMapCollectionTypeName(normalizedTypeName)) &&
          !paramBinding->typeTemplateArg.empty()) {
        return ReturnKind::Array;
      }
      if (isInferStructTypeName(paramBinding->typeName, expr.namespacePrefix)) {
        return ReturnKind::Array;
      }
      return returnKindForTypeName(bindingTypeText(*paramBinding));
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return ReturnKind::Unknown;
    }
    if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
      ReturnKind refKind = inferReferenceTargetKind(it->second.typeTemplateArg, expr.namespacePrefix);
      if (refKind != ReturnKind::Unknown) {
        return refKind;
      }
    }
    const std::string normalizedTypeName = normalizeBindingTypeName(it->second.typeName);
    if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
         normalizedTypeName == "soa_vector" || isMapCollectionTypeName(normalizedTypeName)) &&
        !it->second.typeTemplateArg.empty()) {
      return ReturnKind::Array;
    }
    if (isInferStructTypeName(it->second.typeName, expr.namespacePrefix)) {
      return ReturnKind::Array;
    }
    return returnKindForTypeName(bindingTypeText(it->second));
  }
  if (expr.kind == Expr::Kind::Name &&
      isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
    return ReturnKind::Float64;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isFieldAccess) {
      auto resolveStructFieldKind = [&](const Expr &receiver, const std::string &fieldName) -> ReturnKind {
        auto isStaticField = [](const Expr &stmt) -> bool {
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "static") {
              return true;
            }
          }
          return false;
        };
        auto resolveStructPathFromType = [&](const std::string &typeName,
                                             const std::string &namespacePrefix,
                                             std::string &structPathOut) -> bool {
          std::string normalizedTypeName = normalizeBindingTypeName(typeName);
          if (normalizedTypeName.empty() || isPrimitiveBindingTypeName(normalizedTypeName)) {
            return false;
          }
          std::string lookupTypeName = normalizedTypeName;
          std::string baseTypeName;
          std::string templateArgText;
          if (splitTemplateTypeName(normalizedTypeName, baseTypeName, templateArgText) &&
              !baseTypeName.empty()) {
            lookupTypeName = normalizeBindingTypeName(baseTypeName);
          }
          if (!lookupTypeName.empty() && lookupTypeName[0] == '/') {
            if (structNames_.count(lookupTypeName) > 0) {
              structPathOut = lookupTypeName;
              return true;
            }
            return false;
          }
          std::string current = namespacePrefix;
          while (true) {
            if (!current.empty()) {
              std::string scoped = current + "/" + lookupTypeName;
              if (structNames_.count(scoped) > 0) {
                structPathOut = scoped;
                return true;
              }
              if (current.size() > lookupTypeName.size()) {
                const size_t start = current.size() - lookupTypeName.size();
                if (start > 0 && current[start - 1] == '/' &&
                    current.compare(start, lookupTypeName.size(), lookupTypeName) == 0 &&
                    structNames_.count(current) > 0) {
                  structPathOut = current;
                  return true;
                }
              }
            } else {
              std::string root = "/" + lookupTypeName;
              if (structNames_.count(root) > 0) {
                structPathOut = root;
                return true;
              }
            }
            if (current.empty()) {
              break;
            }
            const size_t slash = current.find_last_of('/');
            if (slash == std::string::npos || slash == 0) {
              current.clear();
            } else {
              current.erase(slash);
            }
          }
          auto importIt = importAliases_.find(lookupTypeName);
          if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
            structPathOut = importIt->second;
            return true;
          }
          return false;
        };
        std::string structPath;
        const bool isTypeReceiver =
            receiver.kind == Expr::Kind::Name &&
            findParamBinding(params, receiver.name) == nullptr &&
            locals.find(receiver.name) == locals.end() &&
            resolveStructPathFromType(receiver.name, receiver.namespacePrefix, structPath);
        if (receiver.kind == Expr::Kind::Name) {
          const BindingInfo *binding = findParamBinding(params, receiver.name);
          if (!binding) {
            auto it = locals.find(receiver.name);
            if (it != locals.end()) {
              binding = &it->second;
            }
          }
          if (binding) {
            std::string typeName = binding->typeName;
            if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
              typeName = binding->typeTemplateArg;
            }
            (void)resolveStructPathFromType(typeName, receiver.namespacePrefix, structPath);
          }
        } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
          structPath = inferStructReturnPath(receiver, params, locals);
        }
        if (structPath.empty()) {
          ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
          if (receiverKind != ReturnKind::Unknown && receiverKind != ReturnKind::Array) {
            return receiverKind;
          }
          return ReturnKind::Unknown;
        }
        auto defIt = defMap_.find(structPath);
        if (defIt == defMap_.end() || !defIt->second) {
          return ReturnKind::Unknown;
        }
        const Definition *structDefinition = defIt->second;
        StructFieldReturnKindMemoKey fieldMemoKey{
            structDefinition,
            fieldName,
            isTypeReceiver,
        };
        if (const auto memoIt = structFieldReturnKindMemo_.find(fieldMemoKey);
            memoIt != structFieldReturnKindMemo_.end()) {
          return memoIt->second;
        }

        ReturnKind inferredFieldKind = ReturnKind::Unknown;
        for (const auto &stmt : structDefinition->statements) {
          if (!stmt.isBinding) {
            continue;
          }
          if (isTypeReceiver ? !isStaticField(stmt) : isStaticField(stmt)) {
            continue;
          }
          if (stmt.name != fieldName) {
            continue;
          }
          BindingInfo fieldBinding;
          if (!resolveStructFieldBinding(*structDefinition, stmt, fieldBinding)) {
            inferredFieldKind = ReturnKind::Unknown;
            break;
          }
          std::string typeName = fieldBinding.typeName;
          if ((typeName == "Reference" || typeName == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
            typeName = fieldBinding.typeTemplateArg;
          }
          inferredFieldKind = returnKindForTypeName(typeName);
          if (inferredFieldKind != ReturnKind::Unknown) {
            break;
          }
          std::string fieldStructPath;
          if (resolveStructPathFromType(typeName, stmt.namespacePrefix, fieldStructPath)) {
            inferredFieldKind = ReturnKind::Array;
          }
          break;
        }

        structFieldReturnKindMemo_.insert_or_assign(std::move(fieldMemoKey), inferredFieldKind);
        return inferredFieldKind;
      };
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return resolveStructFieldKind(expr.args.front(), expr.name);
    }
    if (isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) || isRepeatCall(expr)) {
      return ReturnKind::Void;
    }
    if (!expr.isMethodCall &&
        (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) && expr.args.size() == 1) {
      const Expr &storage = expr.args.front();
      BindingInfo binding;
      bool resolved = false;
      if (!resolveUninitializedStorageBinding(params, locals, storage, binding, resolved)) {
        return ReturnKind::Unknown;
      }
      if (resolved && binding.typeName == "uninitialized" && !binding.typeTemplateArg.empty()) {
        ReturnKind kind = inferUninitializedTargetKind(binding.typeTemplateArg, storage.namespacePrefix);
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
    }
    const std::string resolvedCalleePath = preferVectorStdlibHelperPath(resolveCalleePath(expr));
    std::string collection;
    if (defMap_.find(resolvedCalleePath) == defMap_.end() && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          expr.templateArgs.size() == 1) {
        return ReturnKind::Array;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return ReturnKind::Array;
      }
    }
    bool handledControlFlow = false;
    ReturnKind controlFlowKind = inferControlFlowExprReturnKind(expr, params, locals, handledControlFlow);
    if (handledControlFlow) {
      return controlFlowKind;
    }
    const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
    const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
    auto resolveMethodCallPath = [&](const std::string &methodName, std::string &resolvedOut) -> bool {
      return resolveInferMethodCallPath(expr, params, locals, methodName, resolvedOut);
    };
    auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
      return resolveInferArgsPackCountTarget(params, locals, target, elemType);
    };
    auto inferResolvedPathReturnKind = [&](const std::string &resolvedPath, ReturnKind &kindOut) -> bool {
      auto methodIt = defMap_.find(resolvedPath);
      if (methodIt == defMap_.end()) {
        return false;
      }
      if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
        return false;
      }
      auto kindIt = returnKinds_.find(resolvedPath);
      if (kindIt == returnKinds_.end()) {
        return false;
      }
      kindOut = kindIt->second;
      return true;
    };
    InferPreDispatchCallContext preDispatchCallContext;
    preDispatchCallContext.dispatchResolverAdapters =
        &builtinCollectionDispatchResolverAdapters;
    preDispatchCallContext.dispatchResolvers =
        &builtinCollectionDispatchResolvers;
    bool handledPreDispatchCall = false;
    ReturnKind preDispatchCallKind = inferPreDispatchCallReturnKind(
        expr,
        params,
        locals,
        preDispatchCallContext,
        handledPreDispatchCall);
    if (handledPreDispatchCall) {
      return preDispatchCallKind;
    }
    std::string resolved = preDispatchCallContext.resolved;
    InferCollectionDispatchSetup inferCollectionDispatchSetup;
    prepareInferCollectionDispatchSetup(
        params,
        locals,
        expr,
        resolved,
        resolveMethodCallPath,
        resolveArgsPackCountTarget,
        inferResolvedPathReturnKind,
        builtinCollectionDispatchResolvers,
        inferCollectionDispatchSetup);
    InferResolvedCallContext resolvedCallContext;
    resolvedCallContext.resolved = resolved;
    resolvedCallContext.collectionDispatchSetup = &inferCollectionDispatchSetup;
    resolvedCallContext.dispatchResolvers = &builtinCollectionDispatchResolvers;
    bool handledResolvedCall = false;
    ReturnKind resolvedCallKind =
        inferResolvedCallReturnKind(expr, params, locals, resolvedCallContext, handledResolvedCall);
    if (handledResolvedCall) {
      return resolvedCallKind;
    }
    bool handledDirectBuiltinCountCapacity = false;
    ReturnKind directBuiltinCountCapacityKind =
        inferBuiltinCollectionDirectCountCapacityReturnKind(
            expr,
            resolved,
            inferCollectionDispatchSetup
                .builtinCollectionDirectCountCapacityContext,
            handledDirectBuiltinCountCapacity);
    if (handledDirectBuiltinCountCapacity) {
      return directBuiltinCountCapacityKind;
    }
    InferLateFallbackBuiltinContext lateFallbackContext;
    lateFallbackContext.resolved = resolved;
    lateFallbackContext.collectionDispatchSetup = &inferCollectionDispatchSetup;
    lateFallbackContext.dispatchResolvers = &builtinCollectionDispatchResolvers;
    lateFallbackContext.resolveMethodCallPath = resolveMethodCallPath;
    bool handledLateFallback = false;
    ReturnKind lateFallbackKind =
        inferLateFallbackReturnKind(expr, params, locals, lateFallbackContext, handledLateFallback);
    if (handledLateFallback) {
      return lateFallbackKind;
    }
    bool handledScalarBuiltin = false;
    ReturnKind scalarBuiltinKind =
        inferScalarBuiltinReturnKind(expr, params, locals, handledScalarBuiltin);
    if (handledScalarBuiltin) {
      return scalarBuiltinKind;
    }
    return ReturnKind::Unknown;
  }
  return ReturnKind::Unknown;
}

} // namespace primec::semantics
