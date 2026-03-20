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
      elemType.clear();
      auto resolveBinding = [&](const BindingInfo &binding) {
        return getArgsPackElementType(binding, elemType);
      };
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return resolveBinding(*paramBinding);
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          return resolveBinding(it->second);
        }
      }
      return false;
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
    auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,
                                                std::string &keyTypeOut,
                                                std::string &valueTypeOut) -> bool {
      auto extractFromTypeText = [&](std::string normalizedType) -> bool {
        while (true) {
          std::string base;
          std::string argText;
          if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
            base = normalizeBindingTypeName(base);
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            std::string normalizedBase = base;
            if (!normalizedBase.empty() && normalizedBase.front() == '/') {
              normalizedBase.erase(normalizedBase.begin());
            }
            if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
                return false;
              }
              keyTypeOut = args[0];
              valueTypeOut = args[1];
              return true;
            }
          }

          std::string resolvedPath = normalizedType;
          if (!resolvedPath.empty() && resolvedPath.front() != '/') {
            resolvedPath.insert(resolvedPath.begin(), '/');
          }
          std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
          if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
            normalizedResolvedPath.erase(normalizedResolvedPath.begin());
          }
          if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
            return false;
          }
          auto defIt = defMap_.find(resolvedPath);
          if (defIt == defMap_.end() || !defIt->second) {
            return false;
          }
          std::string keysElementType;
          std::string payloadsElementType;
          for (const auto &fieldExpr : defIt->second->statements) {
            if (!fieldExpr.isBinding) {
              continue;
            }
            BindingInfo fieldBinding;
            std::optional<std::string> restrictType;
            std::string parseError;
            if (!parseBindingInfo(fieldExpr,
                                  defIt->second->namespacePrefix,
                                  structNames_,
                                  importAliases_,
                                  fieldBinding,
                                  restrictType,
                                  parseError)) {
              continue;
            }
            if (normalizeBindingTypeName(fieldBinding.typeName) != "vector" || fieldBinding.typeTemplateArg.empty()) {
              continue;
            }
            std::vector<std::string> fieldArgs;
            if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, fieldArgs) || fieldArgs.size() != 1) {
              continue;
            }
            if (fieldExpr.name == "keys") {
              keysElementType = fieldArgs.front();
            } else if (fieldExpr.name == "payloads") {
              payloadsElementType = fieldArgs.front();
            }
          }
          if (keysElementType.empty() || payloadsElementType.empty()) {
            return false;
          }
          keyTypeOut = keysElementType;
          valueTypeOut = payloadsElementType;
          return true;
        }
      };

      keyTypeOut.clear();
      valueTypeOut.clear();
      if (binding.typeTemplateArg.empty()) {
        return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
      }
      return extractFromTypeText(normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
    };
    auto resolveExperimentalMapTarget = [&](const Expr &target,
                                            std::string &keyTypeOut,
                                            std::string &valueTypeOut) -> bool {
      keyTypeOut.clear();
      valueTypeOut.clear();
      auto isDirectMapConstructorCall = [&](const Expr &candidate) {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        return matchesPath("/std/collections/map/map") ||
               matchesPath("/std/collections/mapNew") ||
               matchesPath("/std/collections/mapSingle") ||
               matchesPath("/std/collections/mapDouble") ||
               matchesPath("/std/collections/mapPair") ||
               matchesPath("/std/collections/mapTriple") ||
               matchesPath("/std/collections/mapQuad") ||
               matchesPath("/std/collections/mapQuint") ||
               matchesPath("/std/collections/mapSext") ||
               matchesPath("/std/collections/mapSept") ||
               matchesPath("/std/collections/mapOct") ||
               matchesPath("/std/collections/experimental_map/mapNew") ||
               matchesPath("/std/collections/experimental_map/mapSingle") ||
               matchesPath("/std/collections/experimental_map/mapDouble") ||
               matchesPath("/std/collections/experimental_map/mapPair") ||
               matchesPath("/std/collections/experimental_map/mapTriple") ||
               matchesPath("/std/collections/experimental_map/mapQuad") ||
               matchesPath("/std/collections/experimental_map/mapQuint") ||
               matchesPath("/std/collections/experimental_map/mapSext") ||
               matchesPath("/std/collections/experimental_map/mapSept") ||
               matchesPath("/std/collections/experimental_map/mapOct");
      };
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractExperimentalMapFieldTypes(*paramBinding, keyTypeOut, valueTypeOut);
        }
        auto it = locals.find(target.name);
        return it != locals.end() &&
               extractExperimentalMapFieldTypes(it->second, keyTypeOut, valueTypeOut);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      if (isDirectMapConstructorCall(target)) {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferredReturn;
      return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
             extractExperimentalMapFieldTypes(inferredReturn, keyTypeOut, valueTypeOut);
    };
    auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
      if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
        return false;
      }
      std::string structPath;
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        const BindingInfo *receiverBinding = findParamBinding(params, receiver.name);
        if (!receiverBinding) {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            receiverBinding = &it->second;
          }
        }
        if (receiverBinding != nullptr) {
          std::string receiverType = receiverBinding->typeName;
          if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverBinding->typeTemplateArg.empty()) {
            receiverType = receiverBinding->typeTemplateArg;
          }
          structPath = resolveStructTypePath(receiverType, receiver.namespacePrefix, structNames_);
          if (structPath.empty()) {
            auto importIt = importAliases_.find(receiverType);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              structPath = importIt->second;
            }
          }
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
        std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
        if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
          structPath = inferredStruct;
        } else {
          const std::string resolvedType = resolveCalleePath(receiver);
          if (structNames_.count(resolvedType) > 0) {
            structPath = resolvedType;
          }
        }
      }
      if (structPath.empty()) {
        return false;
      }
      auto defIt = defMap_.find(structPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &fieldStmt : defIt->second->statements) {
        bool isStaticField = false;
        for (const auto &transform : fieldStmt.transforms) {
          if (transform.name == "static") {
            isStaticField = true;
            break;
          }
        }
        if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != target.name) {
          continue;
        }
        return resolveStructFieldBinding(*defIt->second, fieldStmt, bindingOut);
      }
      return false;
    };
    auto resolveExperimentalMapValueTarget = [&](const Expr &target,
                                                 std::string &keyTypeOut,
                                                 std::string &valueTypeOut) -> bool {
      auto extractValueBinding = [&](const BindingInfo &binding) {
        const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
        if (normalizedType == "Reference" || normalizedType == "Pointer") {
          return false;
        }
        return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
      };
      keyTypeOut.clear();
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractValueBinding(*paramBinding);
        }
        auto it = locals.find(target.name);
        return it != locals.end() && extractValueBinding(it->second);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return extractValueBinding(fieldBinding);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferredReturn;
      return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
             extractValueBinding(inferredReturn);
    };
    auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {
      if (helperName == "count") {
        return std::string("mapCount");
      }
      if (helperName == "contains") {
        return std::string("mapContains");
      }
      if (helperName == "tryAt") {
        return std::string("mapTryAt");
      }
      if (helperName == "at") {
        return std::string("mapAt");
      }
      if (helperName == "at_unsafe") {
        return std::string("mapAtUnsafe");
      }
      return std::string(helperName);
    };
    auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {
      return "/std/collections/experimental_map/" + preferredExperimentalMapHelperTarget(helperName);
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
      auto inferCollectionTypePathFromTypeText = [&](const std::string &typeText,
                                                     auto &&inferCollectionTypePathFromTypeTextRef)
          -> std::string {
        const std::string normalizedType = normalizeBindingTypeName(typeText);
        std::string normalizedCollectionType = normalizeCollectionTypePath(normalizedType);
        if (!normalizedCollectionType.empty()) {
          return normalizedCollectionType;
        }
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(normalizedType, base, argText)) {
          return {};
        }
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return {};
          }
          return inferCollectionTypePathFromTypeTextRef(args.front(), inferCollectionTypePathFromTypeTextRef);
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args)) {
          return {};
        }
        if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
          return "/" + base;
        }
        if (isMapCollectionTypeName(base) && args.size() == 2) {
          return "/map";
        }
        return {};
      };
      auto resolveStructTypePath = [&](const std::string &typeName,
                                       const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (!typeName.empty() && typeName[0] == '/') {
          return typeName;
        }
        std::string current = namespacePrefix;
        while (true) {
          if (!current.empty()) {
            std::string scoped = current + "/" + typeName;
            if (structNames_.count(scoped) > 0) {
              return scoped;
            }
            if (current.size() > typeName.size()) {
              const size_t start = current.size() - typeName.size();
              if (start > 0 && current[start - 1] == '/' &&
                  current.compare(start, typeName.size(), typeName) == 0 &&
                  structNames_.count(current) > 0) {
                return current;
              }
            }
          } else {
            std::string root = "/" + typeName;
            if (structNames_.count(root) > 0) {
              return root;
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
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        return "";
      };
      if (expr.args.empty()) {
        return false;
      }
      const Expr &receiver = expr.args.front();
      std::string typeName;
      std::string typeTemplateArg;
      std::string normalizedMethodName = methodName;
      if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
        normalizedMethodName.erase(normalizedMethodName.begin());
      }
      if (normalizedMethodName.rfind("vector/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
      } else if (normalizedMethodName.rfind("array/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
      } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
      } else if (normalizedMethodName.rfind("map/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
      } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
      }
      auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
          return "";
        }
        return this->inferPointerLikeCallReturnType(receiverExpr, params, locals);
      };
      auto preferredMapMethodTargetForCall = [&](const Expr &receiverExpr, const std::string &helperName) {
        std::string keyType;
        std::string valueType;
        if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
          const std::string canonical = "/std/collections/map/" + helperName;
          if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
            return canonical;
          }
          return preferredCanonicalExperimentalMapHelperTarget(helperName);
        }
        const std::string canonical = "/std/collections/map/" + helperName;
        const std::string alias = "/map/" + helperName;
        if (hasDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
          return canonical;
        }
        if (hasDefinitionPath(alias)) {
          return alias;
        }
        return canonical;
      };
      auto resolveCollectionMethodFromTypePath = [&](const std::string &collectionTypePath) -> bool {
        if (normalizedMethodName == "count") {
          if (collectionTypePath == "/array") {
            resolvedOut = preferVectorStdlibHelperPath("/array/count");
            return true;
          }
          if (collectionTypePath == "/vector") {
            resolvedOut = preferVectorStdlibHelperPath("/vector/count");
            return true;
          }
          if (collectionTypePath == "/soa_vector") {
            resolvedOut = "/soa_vector/count";
            return true;
          }
          if (collectionTypePath == "/string") {
            resolvedOut = "/string/count";
            return true;
          }
          if (collectionTypePath == "/map") {
            resolvedOut = preferredMapMethodTargetForCall(receiver, "count");
            return true;
          }
        }
        if (normalizedMethodName == "capacity" && collectionTypePath == "/vector") {
          resolvedOut = preferVectorStdlibHelperPath("/vector/capacity");
          return true;
        }
        if (normalizedMethodName == "contains" && collectionTypePath == "/map") {
          resolvedOut = preferredMapMethodTargetForCall(receiver, "contains");
          return true;
        }
        if (normalizedMethodName == "tryAt" && collectionTypePath == "/map") {
          resolvedOut = preferredMapMethodTargetForCall(receiver, "tryAt");
          return true;
        }
        if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
          if (collectionTypePath == "/array") {
            resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
            return true;
          }
          if (collectionTypePath == "/vector") {
            resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
            return true;
          }
          if (collectionTypePath == "/string") {
            resolvedOut = "/string/" + normalizedMethodName;
            return true;
          }
          if (collectionTypePath == "/map") {
            resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
            return true;
          }
        }
        if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
            collectionTypePath == "/soa_vector") {
          resolvedOut = "/soa_vector/" + normalizedMethodName;
          return true;
        }
        return false;
      };
      auto shouldPreferStructReturnMethodTargetForCall = [&](const Expr &receiverExpr) {
        return receiverExpr.kind == Expr::Kind::Call &&
               !receiverExpr.isBinding &&
               receiverExpr.isFieldAccess;
      };

      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
        std::string receiverTypeText;
        if (inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
            !receiverTypeText.empty()) {
          const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
          const std::string receiverCollectionTypePath =
              inferCollectionTypePathFromTypeText(normalizedReceiverType, inferCollectionTypePathFromTypeText);
          if (!receiverCollectionTypePath.empty() &&
              resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
            return true;
          }
          const std::string resolvedReceiverType =
              resolveStructTypePath(normalizedReceiverType, receiver.namespacePrefix);
          if (!resolvedReceiverType.empty()) {
            resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
            return true;
          }
          if (isPrimitiveBindingTypeName(normalizedReceiverType)) {
            resolvedOut = "/" + normalizedReceiverType + "/" + normalizedMethodName;
            return true;
          }
        }
        resolvedType = inferStructReturnPath(receiver, params, locals);
        if (!resolvedType.empty()) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
        const ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
        if (receiverKind == ReturnKind::Unknown || receiverKind == ReturnKind::Void) {
          return false;
        }
        if (receiverKind != ReturnKind::Array) {
          const std::string receiverType = typeNameForReturnKind(receiverKind);
          if (receiverType.empty()) {
            return false;
          }
          resolvedOut = "/" + receiverType + "/" + normalizedMethodName;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
          shouldPreferStructReturnMethodTargetForCall(receiver)) {
        std::string inferredStructPath = inferStructReturnPath(receiver, params, locals);
        if (!inferredStructPath.empty() && structNames_.count(inferredStructPath) > 0) {
          resolvedOut = inferredStructPath + "/" + normalizedMethodName;
          return true;
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
                 receiver.isMethodCall && !receiver.args.empty()) {
        std::string receiverHelperName = receiver.name;
        if (!receiverHelperName.empty() && receiverHelperName.front() == '/') {
          receiverHelperName.erase(receiverHelperName.begin());
        }
        if (receiverHelperName.rfind("map/", 0) == 0) {
          receiverHelperName.erase(0, std::string("map/").size());
        } else if (receiverHelperName.rfind("std/collections/map/", 0) == 0) {
          receiverHelperName.erase(0, std::string("std/collections/map/").size());
        }
        std::string keyType;
        std::string valueType;
        if ((receiverHelperName == "at" || receiverHelperName == "at_unsafe") &&
            resolveMapTarget(receiver.args.front(), keyType, valueType)) {
          const std::string resolvedReceiver =
              preferredMapMethodTargetForCall(receiver, receiverHelperName);
          auto defIt = defMap_.find(resolvedReceiver);
          if (defIt != defMap_.end() && defIt->second != nullptr) {
            BindingInfo inferredReceiverBinding;
            if (inferDefinitionReturnBinding(*defIt->second, inferredReceiverBinding)) {
              const std::string receiverTypeText =
                  inferredReceiverBinding.typeTemplateArg.empty()
                      ? inferredReceiverBinding.typeName
                      : inferredReceiverBinding.typeName + "<" + inferredReceiverBinding.typeTemplateArg + ">";
              const std::string resolvedReceiverType =
                  resolveStructTypePath(normalizeBindingTypeName(receiverTypeText), defIt->second->namespacePrefix);
              if (!resolvedReceiverType.empty()) {
                resolvedOut = resolvedReceiverType + "/" + normalizedMethodName;
                return true;
              }
            }
          }
        }
      }
      if (receiver.kind == Expr::Kind::Call) {
        std::string receiverCollectionTypePath;
        if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
            resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Name &&
          findParamBinding(params, receiver.name) == nullptr &&
          locals.find(receiver.name) == locals.end()) {
        std::string resolvedReceiverPath;
        const std::string rootReceiverPath = "/" + receiver.name;
        if (defMap_.find(rootReceiverPath) != defMap_.end()) {
          resolvedReceiverPath = rootReceiverPath;
        } else {
          auto importIt = importAliases_.find(receiver.name);
          if (importIt != importAliases_.end()) {
            resolvedReceiverPath = importIt->second;
          }
        }
        if (!resolvedReceiverPath.empty() &&
            (structNames_.count(resolvedReceiverPath) > 0 ||
             defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
          resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
          return true;
        }
        const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
        if (!resolvedType.empty()) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
      }
      {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (normalizedMethodName == "count") {
          if (resolveArgsPackCountTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/array/count");
            return true;
          }
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/vector/count");
            return true;
          }
          if (resolveSoaVectorTarget(receiver, elemType)) {
            resolvedOut = "/soa_vector/count";
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/array/count");
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/count";
            return true;
          }
        }
        if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
          resolvedOut = preferVectorStdlibHelperPath("/vector/capacity");
          return true;
        }
        if (normalizedMethodName == "count" && resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, "count");
          return true;
        }
        if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt") &&
            resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
          return true;
        }
        if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
          if (resolveArgsPackAccessTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
            return true;
          }
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/vector/" + normalizedMethodName);
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPath("/array/" + normalizedMethodName);
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/" + normalizedMethodName;
            return true;
          }
        }
        if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
            resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
          return true;
        }
        if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
            resolveSoaVectorTarget(receiver, elemType)) {
          resolvedOut = "/soa_vector/" + normalizedMethodName;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
          typeTemplateArg = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
            typeTemplateArg = it->second.typeTemplateArg;
          }
        }
      }
      if (typeName.empty()) {
        std::string receiverTypeText;
        if (receiver.kind == Expr::Kind::Call &&
            inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
            !receiverTypeText.empty()) {
          const std::string normalizedReceiverType = normalizeBindingTypeName(receiverTypeText);
          const std::string receiverCollectionTypePath =
              inferCollectionTypePathFromTypeText(normalizedReceiverType, inferCollectionTypePathFromTypeText);
          if (!receiverCollectionTypePath.empty() &&
              resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
            return true;
          }
          typeName = normalizedReceiverType;
        }
      }
      if (typeName.empty()) {
        typeName = inferPointerLikeCallReturnType(receiver);
      }
      if (typeName.empty()) {
        ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
        std::string inferred;
        if (inferredKind == ReturnKind::Array) {
          inferred = inferStructReturnPath(receiver, params, locals);
          if (inferred.empty()) {
            inferred = typeNameForReturnKind(inferredKind);
          }
        } else {
          inferred = typeNameForReturnKind(inferredKind);
        }
        if (!inferred.empty()) {
          typeName = inferred;
        }
      }
      if (typeName.empty() && receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        BindingInfo callBinding;
        std::vector<std::string> resolvedCandidates;
        auto appendResolvedCandidate = [&](const std::string &candidate) {
          if (candidate.empty()) {
            return;
          }
          for (const auto &existing : resolvedCandidates) {
            if (existing == candidate) {
              return;
            }
          }
          resolvedCandidates.push_back(candidate);
        };
        const std::string resolvedReceiverPath = resolveCalleePath(receiver);
        appendResolvedCandidate(resolvedReceiverPath);
        if (resolvedReceiverPath.rfind("/vector/", 0) == 0) {
          appendResolvedCandidate("/std/collections/vector/" +
                                  resolvedReceiverPath.substr(std::string("/vector/").size()));
        } else if (resolvedReceiverPath.rfind("/std/collections/vector/", 0) == 0) {
          appendResolvedCandidate("/vector/" +
                                  resolvedReceiverPath.substr(std::string("/std/collections/vector/").size()));
        } else if (resolvedReceiverPath.rfind("/map/", 0) == 0) {
          appendResolvedCandidate("/std/collections/map/" +
                                  resolvedReceiverPath.substr(std::string("/map/").size()));
        } else if (resolvedReceiverPath.rfind("/std/collections/map/", 0) == 0) {
          appendResolvedCandidate("/map/" +
                                  resolvedReceiverPath.substr(std::string("/std/collections/map/").size()));
        }
        for (const auto &candidate : resolvedCandidates) {
          auto defIt = defMap_.find(candidate);
          if (defIt == defMap_.end() || defIt->second == nullptr) {
            continue;
          }
          if (!inferDefinitionReturnBinding(*defIt->second, callBinding)) {
            continue;
          }
          typeName = callBinding.typeName;
          typeTemplateArg = callBinding.typeTemplateArg;
          break;
        }
      }
      if (typeName.empty()) {
        return false;
      }
      if (typeName == "FileError" && normalizedMethodName == "why") {
        resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
        return true;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          resolvedOut = "/" + typeName + "/" + normalizedMethodName;
          return true;
        }
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedType = resolveStructTypePath(typeName, expr.namespacePrefix);
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    };
    auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
      if (isSimpleCallName(candidate, helper)) {
        return true;
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
        return false;
      }
      if (!(namespacedCollection == "vector" && namespacedHelper == helper)) {
        return false;
      }
      if ((namespacedHelper != "count" && namespacedHelper != "capacity") ||
          resolveCalleePath(candidate) != "/std/collections/vector/" + namespacedHelper) {
        return true;
      }
      return hasDefinitionPath("/std/collections/vector/" + namespacedHelper) ||
             hasDefinitionPath("/vector/" + namespacedHelper);
    };
    auto isArrayNamespacedVectorCountCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsArrayCount = (normalized == "array/count");
      const bool resolvesArrayCount = (resolveCalleePath(candidate) == "/array/count");
      if (!spellsArrayCount && !resolvesArrayCount) {
        return false;
      }
      for (const Expr &arg : candidate.args) {
        std::string elemType;
        if (resolveVectorTarget(arg, elemType)) {
          return true;
        }
      }
      return false;
    };
    auto isMapNamespacedCountCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsMapCount = (normalized == "map/count");
      const bool resolvesMapCount = (resolveCalleePath(candidate) == "/map/count");
      if (!spellsMapCount && !resolvesMapCount) {
        return false;
      }
      if (hasDefinitionPath("/map/count")) {
        return false;
      }
      for (const Expr &arg : candidate.args) {
        std::string keyType;
        std::string valueType;
        if (resolveMapTarget(arg, keyType, valueType)) {
          return true;
        }
      }
      return false;
    };
    const bool shouldInferBuiltinBareMapCountCall = true;
    auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      if (normalized != "map/at" && normalized != "map/at_unsafe") {
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if (namespacePrefix == "map" &&
            (normalized == "at" || normalized == "at_unsafe")) {
          normalized = "map/" + normalized;
        }
      }
      if (normalized != "map/at" && normalized != "map/at_unsafe") {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/map/at") {
          normalized = "map/at";
        } else if (resolvedPath == "/map/at_unsafe") {
          normalized = "map/at_unsafe";
        } else {
          return false;
        }
      }
      return !hasDefinitionPath("/" + normalized);
    };
    auto resolveRemovedCollectionMethodCompatibilityPath = [&](const Expr &candidate) {
      return this->methodRemovedCollectionCompatibilityPath(candidate, params, locals);
    };
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
    auto explicitMapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {
      size_t receiverIndex = 0;
      if (hasNamedArguments(candidate.argNames)) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            return i;
          }
        }
      }
      return receiverIndex;
    };
    auto canonicalExperimentalMapHelperName = [&](const std::string &resolvedPath, std::string &helperNameOut) {
      auto matchHelper = [&](std::string_view canonicalPath,
                             std::string_view aliasPath,
                             std::string_view wrapperPath,
                             std::string_view helperName) {
        const std::string canonicalPrefix = std::string(canonicalPath) + "__t";
        const std::string aliasPrefix = std::string(aliasPath) + "__t";
        const std::string wrapperPrefix = std::string(wrapperPath) + "__t";
        if (resolvedPath == canonicalPath || resolvedPath.rfind(canonicalPrefix, 0) == 0 ||
            resolvedPath == aliasPath || resolvedPath.rfind(aliasPrefix, 0) == 0 ||
            resolvedPath == wrapperPath || resolvedPath.rfind(wrapperPrefix, 0) == 0) {
          helperNameOut = std::string(helperName);
          return true;
        }
        return false;
      };
      return matchHelper("/std/collections/map/count", "/map/count", "/std/collections/mapCount", "count") ||
             matchHelper("/std/collections/map/contains", "/map/contains", "/std/collections/mapContains", "contains") ||
             matchHelper("/std/collections/map/tryAt", "/map/tryAt", "/std/collections/mapTryAt", "tryAt") ||
             matchHelper("/std/collections/map/at", "/map/at", "/std/collections/mapAt", "at") ||
             matchHelper("/std/collections/map/at_unsafe", "/map/at_unsafe", "/std/collections/mapAtUnsafe", "at_unsafe");
    };
    auto canonicalizeExperimentalMapHelperResolvedPath = [&](const std::string &resolvedPath,
                                                             std::string &canonicalPathOut) {
      auto matchExperimentalHelper = [&](std::string_view experimentalPath, std::string_view canonicalPath) {
        const std::string experimentalPrefix = std::string(experimentalPath) + "__t";
        if (resolvedPath == experimentalPath || resolvedPath.rfind(experimentalPrefix, 0) == 0) {
          canonicalPathOut = std::string(canonicalPath);
          return true;
        }
        return false;
      };
      return matchExperimentalHelper("/std/collections/experimental_map/mapCount", "/std/collections/map/count") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapContains", "/std/collections/map/contains") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapTryAt", "/std/collections/map/tryAt") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapAt", "/std/collections/map/at") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapAtUnsafe",
                                     "/std/collections/map/at_unsafe");
    };
    auto tryRewriteExplicitCanonicalExperimentalMapHelperCall = [&](const Expr &candidate, Expr &rewrittenOut) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      const std::string resolvedPath = resolveCalleePath(candidate);
      std::string helperName;
      if (!canonicalExperimentalMapHelperName(resolvedPath, helperName)) {
        return false;
      }
      const size_t receiverIndex = explicitMapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      const Expr &receiverExpr = candidate.args[receiverIndex];
      if (!candidate.templateArgs.empty() &&
          receiverExpr.kind == Expr::Kind::Call &&
          !receiverExpr.isBinding &&
          !receiverExpr.isMethodCall) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      if (!resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType)) {
        return false;
      }
      rewrittenOut = candidate;
      rewrittenOut.name = preferredCanonicalExperimentalMapHelperTarget(helperName);
      rewrittenOut.namespacePrefix.clear();
      if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {keyType, valueType};
      }
      return true;
    };
    auto isExplicitCanonicalExperimentalMapBorrowedHelperCall = [&](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      std::string helperName;
      if (!canonicalExperimentalMapHelperName(resolveCalleePath(candidate), helperName)) {
        return false;
      }
      const size_t receiverIndex = explicitMapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
             !resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
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
    if (defMap_.count(resolvedCallee) == 0 &&
        canonicalizeExperimentalMapHelperResolvedPath(resolvedCallee, canonicalExperimentalMapHelperResolved)) {
      resolvedCallee = canonicalExperimentalMapHelperResolved;
    }
    Expr rewrittenCanonicalExperimentalMapHelperCall;
    if (!expr.isMethodCall &&
        tryRewriteExplicitCanonicalExperimentalMapHelperCall(expr, rewrittenCanonicalExperimentalMapHelperCall)) {
      return inferExprReturnKind(rewrittenCanonicalExperimentalMapHelperCall, params, locals);
    }
    if (!expr.isMethodCall && isExplicitCanonicalExperimentalMapBorrowedHelperCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isMapNamespacedCountCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isMapNamespacedAccessCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    const std::string removedCollectionMethodCompatibilityPath =
        expr.isMethodCall ? resolveRemovedCollectionMethodCompatibilityPath(expr) : "";
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
      const std::string removedCollectionMethodPath = resolveRemovedCollectionMethodCompatibilityPath(expr);
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
        !isMapNamespacedCountCompatibilityCall(expr) && !isStdNamespacedMapCountCall &&
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
        !isMapNamespacedCountCompatibilityCall(expr) &&
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
        !isMapNamespacedAccessCompatibilityCall(expr);
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
        !isMapNamespacedAccessCompatibilityCall(expr) &&
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
