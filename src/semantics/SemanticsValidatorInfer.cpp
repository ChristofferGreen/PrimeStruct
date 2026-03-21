#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace primec::semantics {

ReturnKind SemanticsValidator::inferExprReturnKind(const Expr &expr,
                                                   const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals) {
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
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return ReturnKind::Float64;
    }
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
      return returnKindForTypeName(paramBinding->typeName);
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
    return returnKindForTypeName(it->second.typeName);
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
          if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
            return false;
          }
          if (!typeName.empty() && typeName[0] == '/') {
            if (structNames_.count(typeName) > 0) {
              structPathOut = typeName;
              return true;
            }
            return false;
          }
          std::string current = namespacePrefix;
          while (true) {
            if (!current.empty()) {
              std::string scoped = current + "/" + typeName;
              if (structNames_.count(scoped) > 0) {
                structPathOut = scoped;
                return true;
              }
              if (current.size() > typeName.size()) {
                const size_t start = current.size() - typeName.size();
                if (start > 0 && current[start - 1] == '/' &&
                    current.compare(start, typeName.size(), typeName) == 0 &&
                    structNames_.count(current) > 0) {
                  structPathOut = current;
                  return true;
                }
              }
            } else {
              std::string root = "/" + typeName;
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
          auto importIt = importAliases_.find(typeName);
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
        for (const auto &stmt : defIt->second->statements) {
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
          if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
            return ReturnKind::Unknown;
          }
          std::string typeName = fieldBinding.typeName;
          if ((typeName == "Reference" || typeName == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
            typeName = fieldBinding.typeTemplateArg;
          }
          ReturnKind kind = returnKindForTypeName(typeName);
          if (kind != ReturnKind::Unknown) {
            return kind;
          }
          std::string fieldStructPath;
          if (resolveStructPathFromType(typeName, stmt.namespacePrefix, fieldStructPath)) {
            return ReturnKind::Array;
          }
          return ReturnKind::Unknown;
        }
        return ReturnKind::Unknown;
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
    auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
      return resolveInferArgsPackCountTarget(params, locals, target, elemType);
    };
    const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
    const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
    const auto &resolveIndexedArgsPackElementType =
        builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;
    const auto &resolveDereferencedIndexedArgsPackElementType =
        builtinCollectionDispatchResolvers.resolveDereferencedIndexedArgsPackElementType;
    const auto &resolveWrappedIndexedArgsPackElementType =
        builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;
    const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
    const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
    const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
    const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
    const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;
    const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
    const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;
    auto resolveExperimentalMapTarget = [&](const Expr &target,
                                            std::string &keyTypeOut,
                                            std::string &valueTypeOut) -> bool {
      return resolveInferExperimentalMapTarget(params, locals, target, keyTypeOut, valueTypeOut);
    };
    std::function<ReturnKind(const Expr &)> pointerTargetKind = [&](const Expr &pointerExpr) -> ReturnKind {
      if (pointerExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            return inferReferenceTargetKind(paramBinding->typeTemplateArg, pointerExpr.namespacePrefix);
          }
          return ReturnKind::Unknown;
        }
        auto it = locals.find(pointerExpr.name);
        if (it != locals.end()) {
          if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
              !it->second.typeTemplateArg.empty()) {
            return inferReferenceTargetKind(it->second.typeTemplateArg, pointerExpr.namespacePrefix);
          }
        }
        return ReturnKind::Unknown;
      }
      if (pointerExpr.kind == Expr::Kind::Call) {
        std::string pointerName;
        if (getBuiltinPointerName(pointerExpr, pointerName) && pointerName == "location" && pointerExpr.args.size() == 1) {
          const Expr &target = pointerExpr.args.front();
          if (target.kind != Expr::Kind::Name) {
            return ReturnKind::Unknown;
          }
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
              return inferReferenceTargetKind(paramBinding->typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(paramBinding->typeName);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
              return inferReferenceTargetKind(it->second.typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(it->second.typeName);
          }
        }
        if (getBuiltinMemoryName(pointerExpr, pointerName)) {
          if (pointerName == "alloc" && pointerExpr.templateArgs.size() == 1 && pointerExpr.args.size() == 1) {
            return inferReferenceTargetKind(pointerExpr.templateArgs.front(), pointerExpr.namespacePrefix);
          }
          if (pointerName == "realloc" && pointerExpr.args.size() == 2) {
            return pointerTargetKind(pointerExpr.args.front());
          }
          if (pointerName == "at" && pointerExpr.templateArgs.empty() && pointerExpr.args.size() == 3) {
            return pointerTargetKind(pointerExpr.args.front());
          }
          if (pointerName == "at_unsafe" && pointerExpr.templateArgs.empty() && pointerExpr.args.size() == 2) {
            return pointerTargetKind(pointerExpr.args.front());
          }
        }
        std::string opName;
        if (getBuiltinOperatorName(pointerExpr, opName) && (opName == "plus" || opName == "minus") &&
            pointerExpr.args.size() == 2) {
          ReturnKind leftKind = pointerTargetKind(pointerExpr.args[0]);
          if (leftKind != ReturnKind::Unknown) {
            return leftKind;
          }
          return pointerTargetKind(pointerExpr.args[1]);
        }
      }
      return ReturnKind::Unknown;
    };
    auto resolveMethodCallPath = [&](const std::string &methodName, std::string &resolvedOut) -> bool {
      return resolveInferMethodCallPath(expr, params, locals, methodName, resolvedOut);
    };
    auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
      return this->isVectorBuiltinName(candidate, helper);
    };
    auto isArrayNamespacedVectorCountCompatibilityCall = [&](const Expr &candidate) -> bool {
      return this->isArrayNamespacedVectorCountCompatibilityCall(candidate, builtinCollectionDispatchResolvers);
    };
    const bool shouldInferBuiltinBareMapCountCall = true;
    auto preferVectorStdlibHelperPathForDispatch = [&](const std::string &path) {
      return this->preferVectorStdlibHelperPath(path);
    };
    auto hasDeclaredDefinitionPathForDispatch = [&](const std::string &path) {
      return this->hasDeclaredDefinitionPath(path);
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
    auto preferVectorStdlibHelperPath = [&](const std::string &path) -> std::string {
      auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      std::string preferred = path;
      if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/array/").size());
        if (allowsArrayVectorCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            return vectorAlias;
          }
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            return stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            preferred = vectorAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          const std::string stdlibAlias = "/std/collections/map/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
        if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
            suffix != "at" && suffix != "at_unsafe") {
          const std::string mapAlias = "/map/" + suffix;
          if (defMap_.count(mapAlias) > 0) {
            preferred = mapAlias;
          }
        }
      }
      return preferred;
    };
    auto collectionHelperPathCandidates = [&](const std::string &path) {
      std::vector<std::string> candidates;
      auto appendUnique = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : candidates) {
          if (existing == candidate) {
            return;
          }
        }
        candidates.push_back(candidate);
      };

      std::string normalizedPath = path;
      if (!normalizedPath.empty() && normalizedPath.front() != '/') {
        if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
            normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
            normalizedPath.rfind("std/collections/map/", 0) == 0) {
          normalizedPath.insert(normalizedPath.begin(), '/');
        }
      }

      appendUnique(path);
      appendUnique(normalizedPath);
      if (normalizedPath.rfind("/array/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/array/").size());
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/vector/" + suffix);
          appendUnique("/std/collections/vector/" + suffix);
        }
      } else if (normalizedPath.rfind("/vector/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
            suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
            suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/std/collections/vector/" + suffix);
        }
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
        appendUnique("/vector/" + suffix);
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          appendUnique("/std/collections/map/" + suffix);
        }
      } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
        if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
            suffix != "at" && suffix != "at_unsafe") {
          appendUnique("/map/" + suffix);
        }
      }
      return candidates;
    };
    auto isTemplateCompatibleDefinition = [&](const Definition &def) -> bool {
      if (expr.templateArgs.empty()) {
        return true;
      }
      return !def.templateArgs.empty() && def.templateArgs.size() == expr.templateArgs.size();
    };
    std::string earlyPointerBuiltin;
    if (getBuiltinPointerName(expr, earlyPointerBuiltin) && expr.args.size() == 1) {
      if (earlyPointerBuiltin == "dereference") {
        ReturnKind targetKind = pointerTargetKind(expr.args.front());
        if (targetKind != ReturnKind::Unknown) {
          return targetKind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result") {
      if (expr.name == "error" && expr.args.size() == 2) {
        return ReturnKind::Bool;
      }
      if (expr.name == "why" && expr.args.size() == 2) {
        return ReturnKind::String;
      }
    }

    std::string resolvedCallee = resolveCalleePath(expr);
    std::string canonicalExperimentalMapHelperResolved;
    if (!expr.isMethodCall && defMap_.count(resolvedCallee) == 0 &&
        this->canonicalizeExperimentalMapHelperResolvedPath(resolvedCallee, canonicalExperimentalMapHelperResolved)) {
      resolvedCallee = canonicalExperimentalMapHelperResolved;
    }
    Expr rewrittenCanonicalExperimentalMapHelperCall;
    if (!expr.isMethodCall &&
        this->tryRewriteCanonicalExperimentalMapHelperCall(
            expr, builtinCollectionDispatchResolvers, rewrittenCanonicalExperimentalMapHelperCall)) {
      return inferExprReturnKind(rewrittenCanonicalExperimentalMapHelperCall, params, locals);
    }
    std::string borrowedExplicitCanonicalExperimentalMapHelperPath;
    if (!expr.isMethodCall &&
        this->explicitCanonicalExperimentalMapBorrowedHelperPath(
            expr, builtinCollectionDispatchResolvers, borrowedExplicitCanonicalExperimentalMapHelperPath)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    const std::string directRemovedMapCompatibilityPath =
        !expr.isMethodCall
            ? this->directMapHelperCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters)
            : std::string();
    const bool isMapNamespacedCountCompatibilityCall =
        directRemovedMapCompatibilityPath == "/map/count";
    const bool isMapNamespacedAccessCompatibilityCall =
        directRemovedMapCompatibilityPath == "/map/at" ||
        directRemovedMapCompatibilityPath == "/map/at_unsafe";
    if (!expr.isMethodCall && isMapNamespacedCountCompatibilityCall) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isMapNamespacedAccessCompatibilityCall) {
      return ReturnKind::Unknown;
    }
    const std::string removedCollectionMethodCompatibilityPath =
        expr.isMethodCall
            ? this->methodRemovedCollectionCompatibilityPath(
                  expr, params, locals, builtinCollectionDispatchResolverAdapters)
            : "";
    if (!removedCollectionMethodCompatibilityPath.empty()) {
      if (removedCollectionMethodCompatibilityPath == "/vector/at" ||
          removedCollectionMethodCompatibilityPath == "/vector/at_unsafe") {
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionMethodReturnKind(
                removedCollectionMethodCompatibilityPath,
                expr.args.front(),
                builtinCollectionDispatchResolvers,
                builtinMethodKind)) {
          return builtinMethodKind;
        }
      }
      return ReturnKind::Unknown;
    }
    const auto resolvedCandidates = collectionHelperPathCandidates(resolvedCallee);
    std::string resolved = resolvedCandidates.empty() ? preferVectorStdlibHelperPath(resolvedCallee)
                                                      : resolvedCandidates.front();
    bool hasResolvedPath = !expr.isMethodCall;
    const bool prefersCanonicalVectorCountAliasDefinition =
        !expr.isMethodCall && resolved == "/vector/count" && !hasDefinitionPath(resolved) &&
        hasDefinitionPath("/std/collections/vector/count");
    if (prefersCanonicalVectorCountAliasDefinition) {
      resolved = "/std/collections/vector/count";
    }
    if (!expr.isMethodCall && !resolvedCandidates.empty()) {
      std::string firstTemplateCompatibleCandidate;
      for (const auto &candidate : resolvedCandidates) {
        auto candidateIt = defMap_.find(candidate);
        if (candidateIt == defMap_.end() || !candidateIt->second) {
          continue;
        }
        if (!isTemplateCompatibleDefinition(*candidateIt->second)) {
          continue;
        }
        if (firstTemplateCompatibleCandidate.empty()) {
          firstTemplateCompatibleCandidate = candidate;
        }
        if (!ensureDefinitionReturnKindReady(*candidateIt->second)) {
          return ReturnKind::Unknown;
        }
        auto candidateKindIt = returnKinds_.find(candidate);
        const bool candidateReturnsStruct =
            candidateKindIt != returnKinds_.end() && candidateKindIt->second == ReturnKind::Array;
        if (candidateReturnsStruct && returnStructs_.find(candidate) != returnStructs_.end()) {
          resolved = candidate;
          hasResolvedPath = true;
          break;
        }
      }
      if (!firstTemplateCompatibleCandidate.empty() && !expr.templateArgs.empty() &&
          returnStructs_.find(resolved) == returnStructs_.end()) {
        resolved = firstTemplateCompatibleCandidate;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall && expr.name == "ok" && expr.args.size() >= 1) {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        return (expr.args.size() > 1) ? ReturnKind::Int64 : ReturnKind::Int;
      }
    }
    if (expr.isMethodCall && expr.name == "why" && expr.args.size() == 2) {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        return ReturnKind::String;
      }
    }
    if (expr.isMethodCall) {
      const std::string removedCollectionMethodPath =
          this->methodRemovedCollectionCompatibilityPath(
              expr, params, locals, builtinCollectionDispatchResolverAdapters);
      if (!removedCollectionMethodPath.empty()) {
        error_ = "unknown method: " + removedCollectionMethodPath;
        return ReturnKind::Unknown;
      }
    }
    if (expr.isMethodCall && isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (resolveArgsPackCountTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
        if (!hasDefinitionPath(methodPath)) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/count");
        if (!hasDefinitionPath(methodPath)) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveArrayTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
        if (!hasDefinitionPath(methodPath)) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveStringTarget(expr.args.front())) {
        if (!hasDefinitionPath("/string/count")) {
          return ReturnKind::Int;
        }
        resolved = "/string/count";
        hasResolvedPath = true;
      } else if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        const std::string methodPath =
            hasDefinitionPath("/std/collections/map/count")
                ? "/std/collections/map/count"
                : (hasDefinitionPath("/map/count") ? "/map/count" : "/std/collections/map/count");
        resolved = methodPath;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/capacity");
        if (!hasDefinitionPath(methodPath)) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall) {
      std::string methodResolved;
      if (resolveMethodCallPath(expr.name, methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        std::string logicalMethodResolved = methodResolved;
        std::string canonicalExperimentalMapHelperResolved;
        if (canonicalizeExperimentalMapHelperResolvedPath(
                methodResolved, canonicalExperimentalMapHelperResolved)) {
          logicalMethodResolved = canonicalExperimentalMapHelperResolved;
        }
        if (logicalMethodResolved == "/file_error/why" || logicalMethodResolved == "/FileError/why") {
          return ReturnKind::String;
        }
        if ((logicalMethodResolved == "/std/collections/map/count" &&
             hasImportedDefinitionPath("/std/collections/map/count")) ||
            (logicalMethodResolved == "/std/collections/map/contains" &&
             hasImportedDefinitionPath("/std/collections/map/contains")) ||
            (logicalMethodResolved == "/std/collections/map/tryAt" &&
             hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
            (logicalMethodResolved == "/std/collections/map/at" &&
             hasImportedDefinitionPath("/std/collections/map/at")) ||
            (logicalMethodResolved == "/std/collections/map/at_unsafe" &&
             hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) {
          ReturnKind builtinMethodKind = ReturnKind::Unknown;
          if (resolveBuiltinCollectionMethodReturnKind(
                  logicalMethodResolved, expr.args.front(), builtinCollectionDispatchResolvers, builtinMethodKind)) {
            return builtinMethodKind;
          }
        }
        std::string builtinMapKeyType;
        std::string builtinMapValueType;
        if (((logicalMethodResolved == "/std/collections/map/count" &&
              !hasDeclaredDefinitionPath("/map/count") &&
              !hasDefinitionPath("/map/count") &&
              !hasImportedDefinitionPath("/std/collections/map/count")) ||
             (logicalMethodResolved == "/std/collections/map/contains" &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains")) ||
             (logicalMethodResolved == "/std/collections/map/tryAt" &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
             (logicalMethodResolved == "/std/collections/map/at" &&
              !hasDeclaredDefinitionPath("/map/at") &&
              !hasDefinitionPath("/map/at") &&
              !hasImportedDefinitionPath("/std/collections/map/at")) ||
             (logicalMethodResolved == "/std/collections/map/at_unsafe" &&
              !hasDeclaredDefinitionPath("/map/at_unsafe") &&
              !hasDefinitionPath("/map/at_unsafe") &&
              !hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) &&
            !hasDeclaredDefinitionPath(logicalMethodResolved) &&
            !hasDefinitionPath(logicalMethodResolved) &&
            !resolveMapTarget(expr.args.front(), builtinMapKeyType, builtinMapValueType)) {
          error_ = "unknown method: " + methodResolved;
          return ReturnKind::Unknown;
        }
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (!hasDefinitionPath(logicalMethodResolved) &&
            resolveBuiltinCollectionMethodReturnKind(
                logicalMethodResolved, expr.args.front(), builtinCollectionDispatchResolvers, builtinMethodKind)) {
          return builtinMethodKind;
        }
        if (!hasDefinitionPath(methodResolved) && !hasImportedDefinitionPath(methodResolved)) {
          error_ = "unknown method: " + methodResolved;
          return ReturnKind::Unknown;
        }
        resolved = methodResolved;
        hasResolvedPath = true;
      }
    }
    if (!expr.isMethodCall) {
      std::string vectorHelper;
      if (this->getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {
        std::string namespacedCollection;
        std::string namespacedHelper;
        const bool isNamespacedCollectionHelperCall =
            getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
        const bool isNamespacedVectorHelperCall =
            isNamespacedCollectionHelperCall && namespacedCollection == "vector";
        const bool isStdNamespacedVectorCanonicalHelperCall =
            !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/", 0) == 0 &&
            (namespacedHelper == "push" || namespacedHelper == "pop" || namespacedHelper == "reserve" ||
             namespacedHelper == "clear" || namespacedHelper == "remove_at" ||
             namespacedHelper == "remove_swap");
        const bool shouldAllowStdNamespacedVectorHelperCompatibilityFallback =
            isStdNamespacedVectorCanonicalHelperCall && !namespacedHelper.empty() &&
            hasDefinitionPath("/vector/" + namespacedHelper);
        if ((!hasDefinitionPath(resolved) || isNamespacedVectorHelperCall) &&
            !(isStdNamespacedVectorCanonicalHelperCall && !hasDefinitionPath(resolved) &&
              !shouldAllowStdNamespacedVectorHelperCompatibilityFallback)) {
          auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
            if (candidate.kind != Expr::Kind::Name) {
              return false;
            }
            std::string typeName;
            if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
              typeName = normalizeBindingTypeName(paramBinding->typeName);
            } else {
              auto it = locals.find(candidate.name);
              if (it != locals.end()) {
                typeName = normalizeBindingTypeName(it->second.typeName);
              }
            }
            return typeName == "vector" || typeName == "soa_vector";
          };
          std::vector<size_t> receiverIndices;
          auto appendReceiverIndex = [&](size_t index) {
            if (index >= expr.args.size()) {
              return;
            }
            for (size_t existing : receiverIndices) {
              if (existing == index) {
                return;
              }
            }
            receiverIndices.push_back(index);
          };
          const bool hasNamedArgs = hasNamedArguments(expr.argNames);
          if (hasNamedArgs) {
            bool hasValuesNamedReceiver = false;
            for (size_t i = 0; i < expr.args.size(); ++i) {
              if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
                  *expr.argNames[i] == "values") {
                appendReceiverIndex(i);
                hasValuesNamedReceiver = true;
              }
            }
            if (!hasValuesNamedReceiver) {
              appendReceiverIndex(0);
              for (size_t i = 1; i < expr.args.size(); ++i) {
                appendReceiverIndex(i);
              }
            }
          } else {
            appendReceiverIndex(0);
          }
          const bool probePositionalReorderedReceiver =
              !hasNamedArgs && expr.args.size() > 1 &&
              (expr.args.front().kind == Expr::Kind::Literal ||
               expr.args.front().kind == Expr::Kind::BoolLiteral ||
               expr.args.front().kind == Expr::Kind::FloatLiteral ||
               expr.args.front().kind == Expr::Kind::StringLiteral ||
               (expr.args.front().kind == Expr::Kind::Name &&
                !isVectorHelperReceiverName(expr.args.front())));
          if (probePositionalReorderedReceiver) {
            for (size_t i = 1; i < expr.args.size(); ++i) {
              appendReceiverIndex(i);
            }
          }
          for (size_t receiverIndex : receiverIndices) {
            if (receiverIndex >= expr.args.size()) {
              continue;
            }
            const Expr &receiverCandidate = expr.args[receiverIndex];
            std::string methodResolved;
            if (!this->resolveVectorHelperMethodTarget(params, locals, receiverCandidate, vectorHelper, methodResolved)) {
              continue;
            }
            methodResolved = preferVectorStdlibHelperPath(methodResolved);
            ReturnKind helperReturnKind = ReturnKind::Unknown;
            if (inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
              return helperReturnKind;
            }
          }
        }
      }
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isNamespacedCollectionHelperCall =
        getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
    const bool isNamespacedVectorHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "vector";
    const bool isNamespacedMapHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "map";
    const bool isStdNamespacedVectorCountCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/count", 0) == 0;
    const bool hasStdNamespacedVectorCountDefinition =
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
        isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
    const bool isStdNamespacedMapCountCall =
        !expr.isMethodCall && resolveCalleePath(expr) == "/std/collections/map/count";
    const bool isNamespacedVectorCountCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "count" &&
        isVectorBuiltinName(expr, "count") && !isArrayNamespacedVectorCountCompatibilityCall(expr);
    const bool isNamespacedMapCountCall =
        !expr.isMethodCall && isNamespacedMapHelperCall && namespacedHelper == "count" &&
        !isMapNamespacedCountCompatibilityCall && !isStdNamespacedMapCountCall &&
        !hasDefinitionPath(resolved);
    const bool prefersExplicitDirectMapAccessAliasDefinition =
        !expr.isMethodCall &&
        (((isNamespacedMapHelperCall && (namespacedHelper == "at" || namespacedHelper == "at_unsafe")) ||
          (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
           (expr.name == "at" || expr.name == "at_unsafe")))) &&
        hasDefinitionPath("/map/" +
                          ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper));
    if (prefersExplicitDirectMapAccessAliasDefinition) {
      resolved = "/map/" + ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper);
      hasResolvedPath = true;
    }
    const BuiltinCollectionDispatchResolverAdapters mapCountDispatchResolverAdapters;
    const bool isUnnamespacedMapCountFallbackCall =
        !expr.isMethodCall &&
        this->isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, mapCountDispatchResolverAdapters);
    const bool isResolvedMapCountCall =
        !expr.isMethodCall && resolved == "/map/count" &&
        !isMapNamespacedCountCompatibilityCall &&
        !isUnnamespacedMapCountFallbackCall;
    const bool isNamespacedVectorCapacityCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "capacity" &&
        isVectorBuiltinName(expr, "capacity");
    const bool isStdNamespacedVectorCapacityCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/capacity", 0) == 0;
    const bool hasStdNamespacedVectorCapacityDefinition =
        hasImportedDefinitionPath("/std/collections/vector/capacity");
    const bool shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        isStdNamespacedVectorCapacityCall && hasStdNamespacedVectorCapacityDefinition;
    std::string builtinAccessName;
    const bool hasBuiltinAccessSpelling = getBuiltinArrayAccessName(expr, builtinAccessName);
    const bool isStdNamespacedVectorAccessSpelling =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
    const bool hasStdNamespacedVectorAccessDefinition =
        isStdNamespacedVectorAccessSpelling &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool isStdNamespacedMapAccessSpelling =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        (resolveCalleePath(expr) == "/std/collections/map/at" ||
         resolveCalleePath(expr) == "/std/collections/map/at_unsafe");
    const bool hasStdNamespacedMapAccessDefinition =
        isStdNamespacedMapAccessSpelling &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool isResolvedMapAccessCall =
        !expr.isMethodCall && (resolved == "/map/at" || resolved == "/map/at_unsafe") &&
        !isMapNamespacedAccessCompatibilityCall;
    const bool shouldAllowStdAccessCompatibilityFallback =
        isStdNamespacedVectorAccessSpelling && !builtinAccessName.empty() &&
        hasDefinitionPath("/vector/" + builtinAccessName);
    const bool isBuiltinAccess =
        hasBuiltinAccessSpelling &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback ||
         hasStdNamespacedVectorAccessDefinition) &&
        !isStdNamespacedMapAccessSpelling && !isResolvedMapAccessCall;
    const bool isNamespacedVectorAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedVectorHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe");
    const bool isNamespacedMapAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedMapHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe") &&
        !prefersExplicitDirectMapAccessAliasDefinition &&
        !isMapNamespacedAccessCompatibilityCall &&
        !hasDefinitionPath(resolved);
    const bool shouldInferBuiltinBareMapContainsCall = true;
    const bool shouldInferBuiltinBareMapTryAtCall = true;
    const bool shouldInferBuiltinBareMapAccessCall = true;
    auto isIndexedArgsPackMapReceiverTarget = [&](const Expr &receiverExpr) -> bool {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      return ((resolveIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveWrappedIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)) &&
              extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
    };
    BuiltinCollectionCountCapacityDispatchContext builtinCollectionCountCapacityDispatchContext;
    builtinCollectionCountCapacityDispatchContext.isCountLike =
        (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
         isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        expr.args.size() == 1 && !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
        !prefersCanonicalVectorCountAliasDefinition;
    builtinCollectionCountCapacityDispatchContext.isCapacityLike =
        isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall);
    builtinCollectionCountCapacityDispatchContext.isUnnamespacedMapCountFallbackCall =
        isUnnamespacedMapCountFallbackCall;
    builtinCollectionCountCapacityDispatchContext.shouldInferBuiltinBareMapCountCall =
        shouldInferBuiltinBareMapCountCall;
    builtinCollectionCountCapacityDispatchContext.resolveMethodCallPath = resolveMethodCallPath;
    builtinCollectionCountCapacityDispatchContext.preferVectorStdlibHelperPath =
        preferVectorStdlibHelperPathForDispatch;
    builtinCollectionCountCapacityDispatchContext.hasDeclaredDefinitionPath =
        hasDeclaredDefinitionPathForDispatch;
    builtinCollectionCountCapacityDispatchContext.resolveMapTarget = resolveMapTarget;
    builtinCollectionCountCapacityDispatchContext.dispatchResolvers = &builtinCollectionDispatchResolvers;
    auto defIt = hasResolvedPath ? defMap_.find(resolved) : defMap_.end();
    const bool hasResolvedDefinition = defIt != defMap_.end();
    std::string normalizedCallName = expr.name;
    if (!normalizedCallName.empty() && normalizedCallName.front() == '/') {
      normalizedCallName.erase(normalizedCallName.begin());
    }
    const bool isStdNamespacedMapAccessPath = normalizedCallName.rfind("std/collections/map/", 0) == 0;
    const bool shouldDeferNamespacedVectorAccessCall =
        isNamespacedVectorAccessCall && (!hasResolvedDefinition || isStdNamespacedVectorAccessSpelling);
    const bool shouldDeferNamespacedMapAccessCall =
        isNamespacedMapAccessCall && (!hasResolvedDefinition || isStdNamespacedMapAccessPath);
    const bool shouldUseResolvedStdNamespacedVectorCountDefinition =
        isStdNamespacedVectorCountCall && !hasDefinitionPath("/vector/count") &&
        hasDefinitionPath("/std/collections/vector/count");
    ReturnKind preferredBuiltinAccessKind = ReturnKind::Unknown;
    const bool hasPreferredBuiltinAccessKind =
        hasBuiltinAccessSpelling &&
        resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, preferredBuiltinAccessKind);
    bool shouldDeferResolvedNamespacedCollectionHelperReturn =
        isNamespacedVectorCountCall || isNamespacedMapCountCall || isResolvedMapCountCall ||
        isNamespacedVectorCapacityCall || shouldDeferNamespacedVectorAccessCall ||
        shouldDeferNamespacedMapAccessCall;
    if (prefersCanonicalVectorCountAliasDefinition) {
      shouldDeferResolvedNamespacedCollectionHelperReturn = false;
    }
    if (shouldUseResolvedStdNamespacedVectorCountDefinition) {
      shouldDeferResolvedNamespacedCollectionHelperReturn = false;
    }
    const bool isDirectBuiltinCountCapacityCountCall =
        !expr.isMethodCall &&
        (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
         isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        !expr.args.empty() &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
        !prefersCanonicalVectorCountAliasDefinition &&
        ((defMap_.find(resolved) == defMap_.end() && !isStdNamespacedMapCountCall) ||
         isNamespacedVectorCountCall || isStdNamespacedMapCountCall || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall);
    const bool isDirectBuiltinCountCapacityCapacityCall =
        !expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
        (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall) &&
        !expr.args.empty() &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCapacityCall);
    BuiltinCollectionDirectCountCapacityContext builtinCollectionDirectCountCapacityContext;
    builtinCollectionDirectCountCapacityContext.isDirectCountCall = isDirectBuiltinCountCapacityCountCall;
    builtinCollectionDirectCountCapacityContext.isDirectCountSingleArg =
        isDirectBuiltinCountCapacityCountCall && expr.args.size() == 1;
    builtinCollectionDirectCountCapacityContext.isDirectCapacityCall = isDirectBuiltinCountCapacityCapacityCall;
    builtinCollectionDirectCountCapacityContext.isDirectCapacitySingleArg =
        isDirectBuiltinCountCapacityCapacityCall && expr.args.size() == 1;
    builtinCollectionDirectCountCapacityContext.shouldInferBuiltinBareMapCountCall =
        shouldInferBuiltinBareMapCountCall;
    builtinCollectionDirectCountCapacityContext.resolveMethodCallPath = resolveMethodCallPath;
    builtinCollectionDirectCountCapacityContext.preferVectorStdlibHelperPath =
        preferVectorStdlibHelperPathForDispatch;
    builtinCollectionDirectCountCapacityContext.hasDeclaredDefinitionPath =
        hasDeclaredDefinitionPathForDispatch;
    builtinCollectionDirectCountCapacityContext.inferResolvedPathReturnKind =
        inferResolvedPathReturnKind;
    builtinCollectionDirectCountCapacityContext.tryRewriteBareMapHelperCall =
        [&](const Expr &candidate, Expr &rewrittenOut) {
          return this->tryRewriteBareMapHelperCall(
              candidate, "count", builtinCollectionDispatchResolvers, rewrittenOut);
        };
    builtinCollectionDirectCountCapacityContext.inferRewrittenExprReturnKind =
        [&](const Expr &rewrittenExpr) { return inferExprReturnKind(rewrittenExpr, params, locals); };
    builtinCollectionDirectCountCapacityContext.resolveArgsPackCountTarget = resolveArgsPackCountTarget;
    builtinCollectionDirectCountCapacityContext.resolveVectorTarget = resolveVectorTarget;
    builtinCollectionDirectCountCapacityContext.resolveArrayTarget = resolveArrayTarget;
    builtinCollectionDirectCountCapacityContext.resolveStringTarget = resolveStringTarget;
    builtinCollectionDirectCountCapacityContext.resolveMapTarget = resolveMapTarget;
    builtinCollectionDirectCountCapacityContext.dispatchResolvers =
        &builtinCollectionDispatchResolvers;
    auto isTypeNamespaceMethodCall = [&](const Expr &callExpr, const std::string &resolvedPath) -> bool {
      if (!callExpr.isMethodCall || callExpr.args.empty() || resolvedPath.empty()) {
        return false;
      }
      const Expr &receiver = callExpr.args.front();
      if (receiver.kind != Expr::Kind::Name) {
        return false;
      }
      if (findParamBinding(params, receiver.name) != nullptr || locals.find(receiver.name) != locals.end()) {
        return false;
      }
      const size_t methodSlash = resolvedPath.find_last_of('/');
      if (methodSlash == std::string::npos || methodSlash == 0) {
        return false;
      }
      const std::string receiverPath = resolvedPath.substr(0, methodSlash);
      const size_t receiverSlash = receiverPath.find_last_of('/');
      const std::string receiverTypeName =
          receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
      return receiverTypeName == receiver.name;
    };
    if (defIt != defMap_.end() && !shouldDeferResolvedNamespacedCollectionHelperReturn) {
      if (hasPreferredBuiltinAccessKind && expr.isMethodCall) {
        return preferredBuiltinAccessKind;
      }
      if (expr.isMethodCall) {
        const auto &calleeParams = paramsByDef_[resolved];
        Expr trimmedTypeNamespaceCallExpr;
        const std::vector<Expr> *orderedCallArgs = &expr.args;
        const std::vector<std::optional<std::string>> *orderedCallArgNames = &expr.argNames;
        if (isTypeNamespaceMethodCall(expr, resolved)) {
          trimmedTypeNamespaceCallExpr = expr;
          trimmedTypeNamespaceCallExpr.args.erase(trimmedTypeNamespaceCallExpr.args.begin());
          if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
            trimmedTypeNamespaceCallExpr.argNames.erase(trimmedTypeNamespaceCallExpr.argNames.begin());
          }
          orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
          orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
        }
        std::string orderedArgError;
        if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, orderedArgError)) {
          error_ = orderedArgError.find("argument count mismatch") != std::string::npos
                       ? "argument count mismatch for " + resolved
                       : orderedArgError;
          return ReturnKind::Unknown;
        }
        std::vector<const Expr *> orderedArgs;
        if (!buildOrderedArguments(calleeParams,
                                   *orderedCallArgs,
                                   *orderedCallArgNames,
                                   orderedArgs,
                                   orderedArgError)) {
          error_ = orderedArgError.find("argument count mismatch") != std::string::npos
                       ? "argument count mismatch for " + resolved
                       : orderedArgError;
          return ReturnKind::Unknown;
        }
        for (size_t paramIndex = 0; paramIndex < orderedArgs.size() && paramIndex < calleeParams.size(); ++paramIndex) {
          const Expr *arg = orderedArgs[paramIndex];
          if (arg == nullptr) {
            continue;
          }
          const ParameterInfo &param = calleeParams[paramIndex];
          if (param.binding.typeName.empty() || param.binding.typeName == "auto") {
            continue;
          }
          std::string expectedBase;
          std::string expectedArgText;
          if (splitTemplateTypeName(param.binding.typeName, expectedBase, expectedArgText) ||
              splitTemplateTypeName(param.binding.typeTemplateArg, expectedBase, expectedArgText)) {
            continue;
          }
          const ReturnKind expectedKind = returnKindForTypeName(normalizeBindingTypeName(param.binding.typeName));
          if (expectedKind == ReturnKind::Unknown) {
            continue;
          }
          const ReturnKind actualKind = inferExprReturnKind(*arg, params, locals);
          if (actualKind == ReturnKind::Unknown) {
            continue;
          }
          if (actualKind != expectedKind) {
            error_ = "argument type mismatch for " + resolved + " parameter " + param.name;
            return ReturnKind::Unknown;
          }
        }
      }
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(
                expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(
              expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      return ReturnKind::Unknown;
    }
    bool handledDirectBuiltinCountCapacity = false;
    ReturnKind directBuiltinCountCapacityKind =
        inferBuiltinCollectionDirectCountCapacityReturnKind(
            expr, resolved, builtinCollectionDirectCountCapacityContext, handledDirectBuiltinCountCapacity);
    if (handledDirectBuiltinCountCapacity) {
      return directBuiltinCountCapacityKind;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos")) &&
        expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      std::string elemType;
      if (isSimpleCallName(expr, "to_soa")) {
        if (resolveVectorTarget(expr.args.front(), elemType)) {
          return ReturnKind::Array;
        }
      } else {
        if (resolveSoaVectorTarget(expr.args.front(), elemType)) {
          return ReturnKind::Array;
        }
      }
    }
    const bool isBuiltinGet = isSimpleCallName(expr, "get");
    const bool isBuiltinRef = isSimpleCallName(expr, "ref");
    if (!expr.isMethodCall &&
        ((isBuiltinAccess && !expr.args.empty()) || ((isBuiltinGet || isBuiltinRef) && expr.args.size() == 2)) &&
        (defMap_.find(resolved) == defMap_.end() || shouldDeferNamespacedVectorAccessCall ||
         shouldDeferNamespacedMapAccessCall)) {
      const std::string helperName = isBuiltinAccess ? builtinAccessName : (isBuiltinGet ? "get" : "ref");
      std::vector<size_t> receiverIndices;
      auto appendReceiverIndex = [&](size_t index) {
        if (index >= expr.args.size()) {
          return;
        }
        for (size_t existing : receiverIndices) {
          if (existing == index) {
            return;
          }
        }
        receiverIndices.push_back(index);
      };
      const bool hasNamedArgs = hasNamedArguments(expr.argNames);
      if (hasNamedArgs) {
        bool hasValuesNamedReceiver = false;
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
            appendReceiverIndex(i);
            hasValuesNamedReceiver = true;
          }
        }
        if (!hasValuesNamedReceiver) {
          appendReceiverIndex(0);
          for (size_t i = 1; i < expr.args.size(); ++i) {
            appendReceiverIndex(i);
          }
        }
      } else {
        appendReceiverIndex(0);
      }
      auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        return resolveVectorTarget(candidate, elemType) || resolveArrayTarget(candidate, elemType) ||
               resolveSoaVectorTarget(candidate, elemType) || resolveStringTarget(candidate) ||
               resolveMapTarget(candidate, keyType, valueType);
      };
      const bool probePositionalReorderedReceiver =
          !hasNamedArgs && expr.args.size() > 1 &&
          (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
           expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
           (expr.args.front().kind == Expr::Kind::Name &&
            !isCollectionAccessReceiverExpr(expr.args.front())));
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
      const bool hasAlternativeCollectionReceiver = probePositionalReorderedReceiver &&
                                                    std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
                                                      if (index == 0 || index >= expr.args.size()) {
                                                        return false;
                                                      }
                                                      const Expr &candidate = expr.args[index];
                                                      std::string elemType;
                                                      std::string keyType;
                                                      std::string valueType;
                                                      return resolveVectorTarget(candidate, elemType) ||
                                                             resolveArrayTarget(candidate, elemType) ||
                                                             resolveSoaVectorTarget(candidate, elemType) ||
                                                             resolveStringTarget(candidate) ||
                                                             resolveMapTarget(candidate, keyType, valueType);
                                                    });
      for (size_t receiverIndex : receiverIndices) {
        if (receiverIndex >= expr.args.size()) {
          continue;
        }
        const Expr &receiverCandidate = expr.args[receiverIndex];
        std::string methodResolved;
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (resolveVectorTarget(receiverCandidate, elemType)) {
          methodResolved = "/vector/" + helperName;
        } else if (resolveArrayTarget(receiverCandidate, elemType)) {
          methodResolved = "/array/" + helperName;
        } else if ((helperName == "get" || helperName == "ref") &&
                   resolveSoaVectorTarget(receiverCandidate, elemType)) {
          methodResolved = "/soa_vector/" + helperName;
        } else if (resolveStringTarget(receiverCandidate)) {
          methodResolved = "/string/" + helperName;
        } else if (resolveMapTarget(receiverCandidate, keyType, valueType)) {
          methodResolved = "/map/" + helperName;
        } else {
          continue;
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(
                methodResolved, receiverCandidate, builtinCollectionDispatchResolvers, builtinMethodKind)) {
          if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
            continue;
          }
          return builtinMethodKind;
        }
        auto methodIt = defMap_.find(methodResolved);
        if (methodIt != defMap_.end()) {
          if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
            continue;
          }
          if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
            return ReturnKind::Unknown;
          }
          auto kindIt = returnKinds_.find(methodResolved);
          if (kindIt != returnKinds_.end()) {
            return kindIt->second;
          }
          return ReturnKind::Unknown;
        }
      }
    }
    if (defIt != defMap_.end() && shouldDeferResolvedNamespacedCollectionHelperReturn) {
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(
                expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(
              expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      return ReturnKind::Unknown;
    }
    if (hasPreferredBuiltinAccessKind) {
      return preferredBuiltinAccessKind;
    }
    std::string builtinName;
    if (defMap_.find(resolved) == defMap_.end() && getBuiltinArrayAccessName(expr, builtinName) &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback) &&
        (!isStdNamespacedMapAccessSpelling || hasStdNamespacedMapAccessDefinition) &&
        expr.args.size() == 2) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
          expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
      std::string elemType;
      if (resolveStringTarget(receiverExpr)) {
        return ReturnKind::Int;
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(receiverExpr, keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (resolveArgsPackAccessTarget(receiverExpr, elemType) ||
          resolveArrayTarget(receiverExpr, elemType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.isMethodCall &&
        isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
        shouldInferBuiltinBareMapContainsCall) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
          expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(receiverExpr, keyType, valueType)) {
        return ReturnKind::Bool;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && defMap_.find(resolved) == defMap_.end() && expr.args.size() == 2 &&
        (isSimpleCallName(expr, "contains") || isSimpleCallName(expr, "tryAt") ||
         getBuiltinArrayAccessName(expr, builtinAccessName))) {
      if ((isSimpleCallName(expr, "contains") && !shouldInferBuiltinBareMapContainsCall) ||
          (isSimpleCallName(expr, "tryAt") && !shouldInferBuiltinBareMapTryAtCall) ||
          ((expr.name == "at" || expr.name == "at_unsafe") && !shouldInferBuiltinBareMapAccessCall)) {
        Expr rewrittenMapHelperCall;
        if (this->tryRewriteBareMapHelperCall(expr,
                                              isSimpleCallName(expr, "contains")
                                                  ? "contains"
                                                  : (isSimpleCallName(expr, "tryAt") ? "tryAt" : builtinAccessName),
                                              builtinCollectionDispatchResolvers,
                                              rewrittenMapHelperCall)) {
          return inferExprReturnKind(rewrittenMapHelperCall, params, locals);
        }
      }
      std::string keyType;
      std::string valueType;
      const size_t mapReceiverIndex = this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers);
      const Expr &receiverExpr =
          mapReceiverIndex < expr.args.size() ? expr.args[mapReceiverIndex] : expr.args.front();
      if (resolveMapTarget(receiverExpr, keyType, valueType)) {
        std::string methodResolved;
        if (resolveMethodCallPath(expr.name, methodResolved)) {
          if (methodResolved == "/std/collections/map/contains" && !shouldInferBuiltinBareMapContainsCall &&
              !isIndexedArgsPackMapReceiverTarget(receiverExpr) &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains") &&
              !hasDeclaredDefinitionPath("/std/collections/map/contains")) {
            error_ = "unknown call target: /std/collections/map/contains";
            return ReturnKind::Unknown;
          }
          if (methodResolved == "/std/collections/map/tryAt" && !shouldInferBuiltinBareMapTryAtCall &&
              !isIndexedArgsPackMapReceiverTarget(receiverExpr) &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt") &&
              !hasDeclaredDefinitionPath("/std/collections/map/tryAt")) {
            error_ = "unknown call target: /std/collections/map/tryAt";
            return ReturnKind::Unknown;
          }
          if ((methodResolved == "/map/at" || methodResolved == "/map/at_unsafe" ||
               methodResolved == "/std/collections/map/at" || methodResolved == "/std/collections/map/at_unsafe") &&
              !shouldInferBuiltinBareMapAccessCall &&
              !isIndexedArgsPackMapReceiverTarget(receiverExpr) &&
              !hasDeclaredDefinitionPath("/map/" + builtinAccessName) &&
              !hasImportedDefinitionPath("/std/collections/map/" + builtinAccessName) &&
              !hasDeclaredDefinitionPath("/std/collections/map/" + builtinAccessName)) {
            error_ = "unknown call target: /std/collections/map/" + builtinAccessName;
            return ReturnKind::Unknown;
          }
          auto methodIt = defMap_.find(methodResolved);
          if (methodIt != defMap_.end()) {
            if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
              return ReturnKind::Unknown;
            }
            auto kindIt = returnKinds_.find(methodResolved);
            if (kindIt != returnKinds_.end()) {
              return kindIt->second;
            }
          }
          ReturnKind builtinMethodKind = ReturnKind::Unknown;
          if (resolveBuiltinCollectionMethodReturnKind(
                  methodResolved, receiverExpr, builtinCollectionDispatchResolvers, builtinMethodKind)) {
            return builtinMethodKind;
          }
        }
      }
    }
    if (getBuiltinGpuName(expr, builtinName)) {
      return ReturnKind::Int;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "try") && expr.args.size() == 1) {
      ResultTypeInfo argResult;
      if (resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) &&
          argResult.isResult && argResult.hasValue) {
        return returnKindForTypeName(normalizeBindingTypeName(argResult.valueType));
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "buffer_load") && expr.args.size() == 2) {
      std::string elemType;
      if (resolveBufferTarget(expr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(elemType);
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "buffer") || isSimpleCallName(expr, "upload") ||
                               isSimpleCallName(expr, "readback"))) {
      return ReturnKind::Array;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && shouldInferBuiltinBareMapContainsCall &&
        expr.args.size() == 2) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
          expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(receiverExpr, keyType, valueType)) {
        return ReturnKind::Bool;
      }
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") && expr.args.size() == 2 &&
        (shouldInferBuiltinBareMapTryAtCall ||
         isIndexedArgsPackMapReceiverTarget(
             expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)]))) {
      ResultTypeInfo argResult;
      if (resolveResultTypeForExpr(expr, params, locals, argResult) && argResult.isResult) {
        return ReturnKind::Array;
      }
    }
    if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinAccessName) &&
        expr.args.size() == 2 &&
        (shouldInferBuiltinBareMapAccessCall ||
         isIndexedArgsPackMapReceiverTarget(
             expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)]))) {
      size_t receiverIndex = 0;
      size_t keyIndex = 1;
      const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
          expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
      const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(receiverExpr, keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
    }
    if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinAccessName) &&
        expr.args.size() == 2) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      const Expr &receiver =
          expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)];
      const bool hasCollectionReceiver =
          resolveArgsPackAccessTarget(receiver, elemType) ||
          resolveVectorTarget(receiver, elemType) ||
          resolveArrayTarget(receiver, elemType) ||
          resolveStringTarget(receiver) ||
          resolveMapTarget(receiver, keyType, valueType) ||
          resolveExperimentalMapTarget(receiver, keyType, valueType);
      if (!hasCollectionReceiver) {
        std::string methodResolved;
        if (resolveMethodCallPath(builtinAccessName, methodResolved) && !methodResolved.empty()) {
          error_ = "unknown method: " + methodResolved;
        }
        return ReturnKind::Unknown;
      }
    }
    if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
      if (builtinName == "dereference") {
        ReturnKind targetKind = pointerTargetKind(expr.args.front());
        if (targetKind != ReturnKind::Unknown) {
          return targetKind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinMemoryName(expr, builtinName)) {
      if (builtinName == "free") {
        return ReturnKind::Void;
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinComparisonName(expr, builtinName)) {
      return ReturnKind::Bool;
    }
    if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
      if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
        return ReturnKind::Bool;
      }
      if (builtinName == "lerp" && expr.args.size() == 3) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (builtinName == "pow" && expr.args.size() == 2) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[1], params, locals));
        return result;
      }
      if (expr.args.empty()) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (argKind == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinOperatorName(expr, builtinName)) {
      if (builtinName == "negate") {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      if ((builtinName == "plus" || builtinName == "minus" || builtinName == "multiply" ||
           builtinName == "divide") &&
          !inferStructReturnPath(expr, params, locals).empty()) {
        return ReturnKind::Array;
      }
      ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
      ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
      return combineInferredNumericKinds(left, right);
    }
    if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 3) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[1], params, locals));
      result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[2], params, locals));
      return result;
    }
    if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineInferredNumericKinds(result, inferExprReturnKind(expr.args[1], params, locals));
      return result;
    }
    if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinConvertName(expr, builtinName)) {
      if (expr.templateArgs.size() != 1) {
        return ReturnKind::Unknown;
      }
      return returnKindForTypeName(expr.templateArgs.front());
    }
    if (getBuiltinMutationName(expr, builtinName)) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args.front(), params, locals);
    }
    if (isSimpleCallName(expr, "move")) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args.front(), params, locals);
    }
    if (isAssignCall(expr)) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args[1], params, locals);
    }
    return ReturnKind::Unknown;
  }
  return ReturnKind::Unknown;
}

} // namespace primec::semantics
