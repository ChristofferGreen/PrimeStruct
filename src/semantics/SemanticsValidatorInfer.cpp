#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>

namespace primec::semantics {

bool SemanticsValidator::inferUnknownReturnKinds() {
  for (const auto &def : program_.definitions) {
    if (returnKinds_[def.fullPath] == ReturnKind::Unknown) {
      if (!inferDefinitionReturnKind(def)) {
        return false;
      }
    }
  }
  return true;
}

ReturnKind SemanticsValidator::inferExprReturnKind(const Expr &expr,
                                                   const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals) {
  auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
    if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
        right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void ||
        left == ReturnKind::Array || right == ReturnKind::Array) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
      return ReturnKind::Float64;
    }
    if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
      return ReturnKind::Float32;
    }
    if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
      return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
      if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
          (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int && right == ReturnKind::Int) {
      return ReturnKind::Int;
    }
    return ReturnKind::Unknown;
  };
  auto isStructTypeName = [&](const std::string &typeName, const std::string &namespacePrefix) -> bool {
    if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
      return false;
    }
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolved) > 0) {
      return true;
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end()) {
      return structNames_.count(importIt->second) > 0;
    }
    return false;
  };
  auto referenceTargetKind = [&](const std::string &templateArg, const std::string &namespacePrefix) -> ReturnKind {
    if (templateArg.empty()) {
      return ReturnKind::Unknown;
    }
    ReturnKind kind = returnKindForTypeName(templateArg);
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (isStructTypeName(templateArg, namespacePrefix)) {
      return ReturnKind::Array;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "map") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        return ReturnKind::Array;
      }
    }
    return ReturnKind::Unknown;
  };
  auto uninitializedTargetKind = [&](const std::string &templateArg, const std::string &namespacePrefix) -> ReturnKind {
    if (templateArg.empty()) {
      return ReturnKind::Unknown;
    }
    ReturnKind kind = returnKindForTypeName(templateArg);
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (isStructTypeName(templateArg, namespacePrefix)) {
      return ReturnKind::Array;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "map") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        return ReturnKind::Array;
      }
    }
    return ReturnKind::Unknown;
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
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return ReturnKind::Float64;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
        ReturnKind refKind = referenceTargetKind(paramBinding->typeTemplateArg, expr.namespacePrefix);
        if (refKind != ReturnKind::Unknown) {
          return refKind;
        }
      }
      const std::string normalizedTypeName = normalizeBindingTypeName(paramBinding->typeName);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           normalizedTypeName == "soa_vector" || normalizedTypeName == "map") &&
          !paramBinding->typeTemplateArg.empty()) {
        return ReturnKind::Array;
      }
      if (isStructTypeName(paramBinding->typeName, expr.namespacePrefix)) {
        return ReturnKind::Array;
      }
      return returnKindForTypeName(paramBinding->typeName);
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return ReturnKind::Unknown;
    }
    if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
      ReturnKind refKind = referenceTargetKind(it->second.typeTemplateArg, expr.namespacePrefix);
      if (refKind != ReturnKind::Unknown) {
        return refKind;
      }
    }
    const std::string normalizedTypeName = normalizeBindingTypeName(it->second.typeName);
    if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
         normalizedTypeName == "soa_vector" || normalizedTypeName == "map") &&
        !it->second.typeTemplateArg.empty()) {
      return ReturnKind::Array;
    }
    if (isStructTypeName(it->second.typeName, expr.namespacePrefix)) {
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
        } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
          structPath = inferStructReturnPath(receiver, params, locals);
        }
        if (structPath.empty()) {
          return ReturnKind::Unknown;
        }
        auto defIt = defMap_.find(structPath);
        if (defIt == defMap_.end() || !defIt->second) {
          return ReturnKind::Unknown;
        }
        for (const auto &stmt : defIt->second->statements) {
          if (!stmt.isBinding || isStaticField(stmt)) {
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
        ReturnKind kind = uninitializedTargetKind(binding.typeTemplateArg, storage.namespacePrefix);
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
    }
    auto preferVectorStdlibHelperPathForCall = [&](const std::string &path) -> std::string {
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
        const std::string stdlibAlias =
            "/std/collections/map/" + preferred.substr(std::string("/map/").size());
        if (defMap_.count(stdlibAlias) > 0) {
          preferred = stdlibAlias;
        }
      }
      if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string mapAlias =
            "/map/" + preferred.substr(std::string("/std/collections/map/").size());
        if (defMap_.count(mapAlias) > 0) {
          preferred = mapAlias;
        }
      }
      return preferred;
    };
    const std::string resolvedCalleePath = preferVectorStdlibHelperPathForCall(resolveCalleePath(expr));
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
    if (isMatchCall(expr)) {
      Expr expanded;
      if (!lowerMatchToIf(expr, expanded, error_)) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expanded, params, locals);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
          return false;
        }
        if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
          return false;
        }
        return true;
      };
      auto inferBlockEnvelopeValue = [&](const Expr &candidate,
                                         const std::unordered_map<std::string, BindingInfo> &localsIn,
                                         const Expr *&valueExprOut,
                                         std::unordered_map<std::string, BindingInfo> &localsOut) -> bool {
        valueExprOut = nullptr;
        localsOut = localsIn;
        if (!isIfBlockEnvelope(candidate)) {
          return false;
        }
        bool sawReturn = false;
        for (const auto &bodyExpr : candidate.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo binding;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr, candidate.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
              return false;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, localsOut, binding);
            }
            localsOut.emplace(bodyExpr.name, std::move(binding));
            continue;
          }
          if (isReturnCall(bodyExpr)) {
            if (bodyExpr.args.size() != 1) {
              return false;
            }
            valueExprOut = &bodyExpr.args.front();
            sawReturn = true;
            continue;
          }
          if (!sawReturn) {
            valueExprOut = &bodyExpr;
          }
        }
        return valueExprOut != nullptr;
      };

      const Expr *thenValueExpr = nullptr;
      const Expr *elseValueExpr = nullptr;
      std::unordered_map<std::string, BindingInfo> thenLocals;
      std::unordered_map<std::string, BindingInfo> elseLocals;
      if (!inferBlockEnvelopeValue(expr.args[1], locals, thenValueExpr, thenLocals) ||
          !inferBlockEnvelopeValue(expr.args[2], locals, elseValueExpr, elseLocals)) {
        return ReturnKind::Unknown;
      }

      ReturnKind thenKind = inferExprReturnKind(*thenValueExpr, params, thenLocals);
      ReturnKind elseKind = inferExprReturnKind(*elseValueExpr, params, elseLocals);
      if (thenKind == elseKind) {
        return thenKind;
      }
      return combineNumeric(thenKind, elseKind);
    }
    if (isBlockCall(expr) && expr.hasBodyArguments) {
      const std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) == defMap_.end() && expr.args.empty() && expr.templateArgs.empty() &&
          !hasNamedArguments(expr.argNames)) {
        if (expr.bodyArguments.empty()) {
          return ReturnKind::Unknown;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        ReturnKind result = ReturnKind::Unknown;
        bool sawReturn = false;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
              return ReturnKind::Unknown;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info);
            }
            if (restrictType.has_value()) {
              const bool hasTemplate = !info.typeTemplateArg.empty();
              if (!restrictMatchesBinding(*restrictType,
                                          info.typeName,
                                          info.typeTemplateArg,
                                          hasTemplate,
                                          bodyExpr.namespacePrefix)) {
                return ReturnKind::Unknown;
              }
            }
            blockLocals.emplace(bodyExpr.name, std::move(info));
            continue;
          }
          if (isReturnCall(bodyExpr)) {
            if (bodyExpr.args.size() == 1) {
              result = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
              sawReturn = true;
            }
            continue;
          }
          if (!sawReturn) {
            result = inferExprReturnKind(bodyExpr, params, blockLocals);
          }
        }
        return result;
      }
    }
    auto normalizeCollectionTypePath = [](const std::string &typePath) -> std::string {
      if (typePath == "/array" || typePath == "array") {
        return "/array";
      }
      if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
          typePath == "std/collections/vector") {
        return "/vector";
      }
      if (typePath == "/soa_vector" || typePath == "soa_vector") {
        return "/soa_vector";
      }
      if (typePath == "/map" || typePath == "map" || typePath == "/std/collections/map" ||
          typePath == "std/collections/map") {
        return "/map";
      }
      if (typePath == "/string" || typePath == "string") {
        return "/string";
      }
      return "";
    };
    auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {
      typePathOut.clear();
      auto inferCollectionTypePathFromType =
          [&](const std::string &typeName, auto &&inferCollectionTypePathFromTypeRef) -> std::string {
        const std::string normalizedType = normalizeBindingTypeName(typeName);
        std::string normalized = normalizeCollectionTypePath(normalizedType);
        if (!normalized.empty()) {
          return normalized;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(normalizedType, base, arg)) {
          return {};
        }
        base = normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return {};
          }
          return inferCollectionTypePathFromTypeRef(args.front(), inferCollectionTypePathFromTypeRef);
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args)) {
          return {};
        }
        if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
          return "/" + base;
        }
        if (base == "map" && args.size() == 2) {
          return "/map";
        }
        return {};
      };
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string collection;
      if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection)) {
        if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
            target.templateArgs.size() == 1) {
          typePathOut = "/" + collection;
          return true;
        }
        if (collection == "map" && target.templateArgs.size() == 2) {
          typePathOut = "/map";
          return true;
        }
      }
      auto defIt = defMap_.find(resolvedTarget);
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string inferred = inferCollectionTypePathFromType(transform.templateArgs.front(),
                                                                 inferCollectionTypePathFromType);
          if (!inferred.empty()) {
            typePathOut = std::move(inferred);
            return true;
          }
          break;
        }
      }
      auto structIt = returnStructs_.find(resolvedTarget);
      if (structIt != returnStructs_.end()) {
        std::string normalized = normalizeCollectionTypePath(structIt->second);
        if (!normalized.empty()) {
          typePathOut = normalized;
          return true;
        }
      }
      auto kindIt = returnKinds_.find(resolvedTarget);
      if (kindIt != returnKinds_.end()) {
        if (kindIt->second == ReturnKind::Array) {
          typePathOut = "/array";
          return true;
        }
        if (kindIt->second == ReturnKind::String) {
          typePathOut = "/string";
          return true;
        }
      }
      return false;
    };
    auto resolveCallCollectionTemplateArgs =
        [&](const Expr &target, const std::string &expectedBase, std::vector<std::string> &argsOut) -> bool {
      argsOut.clear();
      auto extractCollectionArgsFromType =
          [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
        std::string base;
        std::string arg;
        const std::string normalizedType = normalizeBindingTypeName(typeName);
        if (!splitTemplateTypeName(normalizedType, base, arg)) {
          return false;
        }
        if (base == expectedBase) {
          return splitTopLevelTemplateArgs(arg, argsOut);
        }
        std::vector<std::string> args;
        if ((base == "Reference" || base == "Pointer") && splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return extractCollectionArgsFromTypeRef(args.front(), extractCollectionArgsFromTypeRef);
        }
        return false;
      };
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string collection;
      if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection) &&
          collection == expectedBase) {
        argsOut = target.templateArgs;
        return true;
      }
      auto defIt = defMap_.find(resolvedTarget);
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return extractCollectionArgsFromType(transform.templateArgs.front(), extractCollectionArgsFromType);
      }
      return false;
    };
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        auto resolveReference = [&](const BindingInfo &binding) -> bool {
          if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
            return false;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
            return false;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          elemType = args.front();
          return true;
        };
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (resolveReference(*paramBinding)) {
            return true;
          }
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (resolveReference(it->second)) {
            return true;
          }
          if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
              it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) &&
            (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
          std::vector<std::string> args;
          const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
          if (resolveCallCollectionTemplateArgs(target, expectedBase, args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
      }
      return false;
    };
    auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/vector") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "vector", args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
        if (!target.isMethodCall && isSimpleCallName(target, "to_aos") && target.args.size() == 1) {
          std::string sourceElemType;
          const Expr &source = target.args.front();
          if (source.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, source.name)) {
              if (paramBinding->typeName != "soa_vector" || paramBinding->typeTemplateArg.empty()) {
                return false;
              }
              sourceElemType = paramBinding->typeTemplateArg;
            } else {
              auto it = locals.find(source.name);
              if (it == locals.end() || it->second.typeName != "soa_vector" || it->second.typeTemplateArg.empty()) {
                return false;
              }
              sourceElemType = it->second.typeTemplateArg;
            }
          } else if (source.kind == Expr::Kind::Call) {
            std::string sourceCollection;
            if (defMap_.find(resolveCalleePath(source)) == defMap_.end() &&
                getBuiltinCollectionName(source, sourceCollection) && sourceCollection == "soa_vector") {
              if (source.templateArgs.size() == 1) {
                sourceElemType = source.templateArgs.front();
              }
            } else if (!source.isMethodCall && isSimpleCallName(source, "to_soa") && source.args.size() == 1) {
              const Expr &vectorSource = source.args.front();
              if (vectorSource.kind == Expr::Kind::Name) {
                if (const BindingInfo *paramBinding = findParamBinding(params, vectorSource.name)) {
                  if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
                    return false;
                  }
                  sourceElemType = paramBinding->typeTemplateArg;
                } else {
                  auto sourceIt = locals.find(vectorSource.name);
                  if (sourceIt == locals.end() || sourceIt->second.typeName != "vector" ||
                      sourceIt->second.typeTemplateArg.empty()) {
                    return false;
                  }
                  sourceElemType = sourceIt->second.typeTemplateArg;
                }
              } else if (vectorSource.kind == Expr::Kind::Call) {
                std::string vectorCollectionTypePath;
                if (!resolveCallCollectionTypePath(vectorSource, vectorCollectionTypePath) ||
                    vectorCollectionTypePath != "/vector") {
                  return false;
                }
                std::vector<std::string> vectorArgs;
                if (resolveCallCollectionTemplateArgs(vectorSource, "vector", vectorArgs) && vectorArgs.size() == 1) {
                  sourceElemType = vectorArgs.front();
                }
              } else {
                return false;
              }
            } else {
              return false;
            }
          } else {
            return false;
          }
          elemType = sourceElemType;
          return true;
        }
      }
      return false;
    };
    auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "soa_vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "soa_vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/soa_vector") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "soa_vector", args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
        if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
          return resolveVectorTarget(target.args.front(), elemType);
        }
      }
      return false;
    };
    auto resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName == "Buffer" && !it->second.typeTemplateArg.empty()) {
            elemType = it->second.typeTemplateArg;
            return true;
          }
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        if (isSimpleCallName(target, "buffer") && target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        if (isSimpleCallName(target, "upload") && target.args.size() == 1) {
          return resolveArrayTarget(target.args.front(), elemType);
        }
      }
      return false;
    };
    auto isBuiltinStringReceiverExpr = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        return resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string";
      }
      return false;
    };
    auto isDirectCanonicalVectorAccessCallOnBuiltinReceiver = [&](const Expr &candidate,
                                                                  size_t &receiverIndexOut) -> bool {
      receiverIndexOut = 0;
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
        return false;
      }
      if (candidate.args.empty()) {
        return false;
      }
      if (hasNamedArguments(candidate.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            receiverIndexOut = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndexOut = 0;
        }
      }
      if (receiverIndexOut >= candidate.args.size()) {
        return false;
      }
      std::string elemType;
      return resolveVectorTarget(candidate.args[receiverIndexOut], elemType) ||
             resolveArrayTarget(candidate.args[receiverIndexOut], elemType) ||
             isBuiltinStringReceiverExpr(candidate.args[receiverIndexOut]);
    };
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    auto resolveBuiltinAccessReceiverExpr = [&](const Expr &accessExpr) -> const Expr * {
      if (accessExpr.kind != Expr::Kind::Call || accessExpr.args.size() != 2) {
        return nullptr;
      }
      if (accessExpr.isMethodCall) {
        return accessExpr.args.empty() ? nullptr : &accessExpr.args.front();
      }
      size_t receiverIndex = 0;
      if (hasNamedArguments(accessExpr.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < accessExpr.args.size(); ++i) {
          if (i < accessExpr.argNames.size() && accessExpr.argNames[i].has_value() &&
              *accessExpr.argNames[i] == "values") {
            receiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndex = 0;
        }
      }
      return receiverIndex < accessExpr.args.size() ? &accessExpr.args[receiverIndex] : nullptr;
    };
    std::function<bool(const Expr &)> resolveStringTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string") {
          return true;
        }
        std::string builtinName;
        if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
          if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
            std::string elemType;
            std::string keyType;
            std::string valueType;
            if (resolveArrayTarget(*accessReceiver, elemType) || resolveVectorTarget(*accessReceiver, elemType)) {
              return normalizeBindingTypeName(elemType) == "string";
            }
            if (resolveMapTarget(*accessReceiver, keyType, valueType)) {
              return normalizeBindingTypeName(valueType) == "string";
            }
            if (resolveStringTarget(*accessReceiver)) {
              return false;
            }
          }
          size_t receiverIndex = 0;
          if (isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
            std::string elemType;
            return resolveArrayTarget(target.args[receiverIndex], elemType) &&
                   normalizeBindingTypeName(elemType) == "string";
          }
          const std::string resolvedTarget = resolveCalleePath(target);
          auto defIt = defMap_.find(resolvedTarget);
          if (defIt != defMap_.end() && defIt->second != nullptr) {
            if (!inferDefinitionReturnKind(*defIt->second)) {
              return false;
            }
            auto kindIt = returnKinds_.find(resolvedTarget);
            if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
              return kindIt->second == ReturnKind::String;
            }
          }
          std::string elemType;
          if (resolveArrayTarget(target.args.front(), elemType) && elemType == "string") {
            return true;
          }
          std::string keyType;
          std::string valueType;
          if (resolveMapTarget(target.args.front(), keyType, valueType) &&
              normalizeBindingTypeName(valueType) == "string") {
            return true;
          }
        }
      }
      return false;
    };
    resolveMapTarget = [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
      keyTypeOut.clear();
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractMapKeyValueTypes(*paramBinding, keyTypeOut, valueTypeOut);
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        return extractMapKeyValueTypes(it->second, keyTypeOut, valueTypeOut);
      }
      if (target.kind == Expr::Kind::Call) {
        auto returnsMapCollection = [&](const std::string &typeName) {
          std::string normalizedType = normalizeBindingTypeName(typeName);
          while (true) {
            std::string base;
            std::string arg;
            if (!splitTemplateTypeName(normalizedType, base, arg)) {
              return normalizedType == "map";
            }
            if (base == "map") {
              std::vector<std::string> args;
              return splitTopLevelTemplateArgs(arg, args) && args.size() == 2;
            }
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            return false;
          }
        };
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/map") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
            keyTypeOut = args[0];
            valueTypeOut = args[1];
          }
          return true;
        }
        auto defIt = defMap_.find(resolveCalleePath(target));
        if (defIt == defMap_.end() || !defIt->second) {
          return false;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          if (!returnsMapCollection(transform.templateArgs.front())) {
            return false;
          }
          std::string normalizedType = normalizeBindingTypeName(transform.templateArgs.front());
          while (true) {
            std::string base;
            std::string arg;
            if (!splitTemplateTypeName(normalizedType, base, arg)) {
              return true;
            }
            if (base == "map") {
              std::vector<std::string> args;
              if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
                keyTypeOut = args[0];
                valueTypeOut = args[1];
              }
              return true;
            }
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            return false;
          }
        }
        return false;
      }
      return false;
    };
    std::function<ReturnKind(const Expr &)> pointerTargetKind = [&](const Expr &pointerExpr) -> ReturnKind {
      if (pointerExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            return referenceTargetKind(paramBinding->typeTemplateArg, pointerExpr.namespacePrefix);
          }
          return ReturnKind::Unknown;
        }
        auto it = locals.find(pointerExpr.name);
        if (it != locals.end()) {
          if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
              !it->second.typeTemplateArg.empty()) {
            return referenceTargetKind(it->second.typeTemplateArg, pointerExpr.namespacePrefix);
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
              return referenceTargetKind(paramBinding->typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(paramBinding->typeName);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
              return referenceTargetKind(it->second.typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(it->second.typeName);
          }
        }
        if (getBuiltinMemoryName(pointerExpr, pointerName)) {
          if (pointerName == "alloc" && pointerExpr.templateArgs.size() == 1 && pointerExpr.args.size() == 1) {
            return referenceTargetKind(pointerExpr.templateArgs.front(), pointerExpr.namespacePrefix);
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
      auto preferVectorStdlibHelperPathForCall = [&](const std::string &path) -> std::string {
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
          const std::string stdlibAlias =
              "/std/collections/map/" + preferred.substr(std::string("/map/").size());
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          }
        }
        if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string mapAlias =
              "/map/" + preferred.substr(std::string("/std/collections/map/").size());
          if (defMap_.count(mapAlias) > 0) {
            preferred = mapAlias;
          }
        }
        return preferred;
      };
      auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
          return "";
        }
        const std::string callPath = preferVectorStdlibHelperPathForCall(resolveCalleePath(receiverExpr));
        if (callPath.empty()) {
          return "";
        }
        auto defIt = defMap_.find(callPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return "";
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
            return "";
          }
          if (base == "Pointer") {
            return "Pointer";
          }
          if (base == "Reference") {
            return "Reference";
          }
          return "";
        }
        return "";
      };

      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
      }
      {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (normalizedMethodName == "count") {
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/vector/count");
            return true;
          }
          if (resolveSoaVectorTarget(receiver, elemType)) {
            resolvedOut = "/soa_vector/count";
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/count");
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/count";
            return true;
          }
        }
        if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
          resolvedOut = preferVectorStdlibHelperPathForCall("/vector/capacity");
          return true;
        }
        if (normalizedMethodName == "count" && resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferVectorStdlibHelperPathForCall("/std/collections/map/count");
          return true;
        }
        if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/vector/" + normalizedMethodName);
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/" + normalizedMethodName);
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/" + normalizedMethodName;
            return true;
          }
        }
        if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
            resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferVectorStdlibHelperPathForCall("/std/collections/map/" + normalizedMethodName);
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
      if (typeName.empty()) {
        return false;
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
    auto resolveBuiltinCollectionMethodReturnKind = [&](const std::string &resolvedPath,
                                                        const Expr &receiverExpr,
                                                        ReturnKind &kindOut) -> bool {
      if (resolvedPath == "/array/count" || resolvedPath == "/vector/count" || resolvedPath == "/string/count" ||
          resolvedPath == "/map/count" || resolvedPath == "/std/collections/map/count" ||
          resolvedPath == "/vector/capacity") {
        kindOut = ReturnKind::Int;
        return true;
      }
      if (resolvedPath == "/string/at" || resolvedPath == "/string/at_unsafe") {
        kindOut = ReturnKind::Int;
        return true;
      }
      if (resolvedPath == "/array/at" || resolvedPath == "/array/at_unsafe") {
        std::string elemType;
        if (resolveArrayTarget(receiverExpr, elemType)) {
          ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
          if (kind != ReturnKind::Unknown) {
            kindOut = kind;
            return true;
          }
        }
        return false;
      }
      if (resolvedPath == "/vector/at" || resolvedPath == "/vector/at_unsafe") {
        std::string elemType;
        if (resolveVectorTarget(receiverExpr, elemType)) {
          ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
          if (kind != ReturnKind::Unknown) {
            kindOut = kind;
            return true;
          }
        }
        return false;
      }
      if (resolvedPath == "/map/at" || resolvedPath == "/map/at_unsafe" ||
          resolvedPath == "/std/collections/map/at" || resolvedPath == "/std/collections/map/at_unsafe") {
        std::string keyType;
        std::string valueType;
        if (resolveMapTarget(receiverExpr, keyType, valueType)) {
          ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
          if (kind != ReturnKind::Unknown) {
            kindOut = kind;
            return true;
          }
        }
        return false;
      }
      return false;
    };
    auto resolveBuiltinCollectionAccessCallReturnKind = [&](const Expr &callExpr, ReturnKind &kindOut) -> bool {
      kindOut = ReturnKind::Unknown;
      if (callExpr.isMethodCall || callExpr.args.size() != 2) {
        return false;
      }
      std::string builtinName;
      if (!getBuiltinArrayAccessName(callExpr, builtinName)) {
        return false;
      }
      std::string elemType;
      if (resolveStringTarget(callExpr.args.front())) {
        kindOut = ReturnKind::Int;
        return true;
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(callExpr.args.front(), keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        if (kind != ReturnKind::Unknown) {
          kindOut = kind;
          return true;
        }
      }
      if (resolveArrayTarget(callExpr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(elemType);
        if (kind != ReturnKind::Unknown) {
          kindOut = kind;
          return true;
        }
      }
      return false;
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
      return defMap_.find("/std/collections/vector/" + namespacedHelper) != defMap_.end() ||
             defMap_.find("/vector/" + namespacedHelper) != defMap_.end();
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
      if (defMap_.find("/map/count") != defMap_.end()) {
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
    auto isUnnamespacedMapCountBuiltinFallbackCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsCount = (normalized == "count");
      const bool resolvesCount = (resolveCalleePath(candidate) == "/count");
      if (!spellsCount && !resolvesCount) {
        return false;
      }
      if (defMap_.find("/count") != defMap_.end()) {
        return false;
      }
      if (candidate.args.empty()) {
        return false;
      }
      size_t receiverIndex = 0;
      if (hasNamedArguments(candidate.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            receiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndex = 0;
        }
      }
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
    };
    auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string helperName;
      if (normalized == "map/at") {
        helperName = "at";
      } else if (normalized == "map/at_unsafe") {
        helperName = "at_unsafe";
      } else {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/map/at") {
          helperName = "at";
        } else if (resolvedPath == "/map/at_unsafe") {
          helperName = "at_unsafe";
        } else {
          return false;
        }
      }
      if (defMap_.find("/map/" + helperName) != defMap_.end()) {
        return false;
      }
      if (candidate.args.empty()) {
        return false;
      }
      const bool hasNamedArgs = hasNamedArguments(candidate.argNames);
      size_t receiverIndex = 0;
      if (hasNamedArgs) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            receiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndex = 0;
        }
      }
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
    };
    auto explicitRemovedCollectionMethodPath = [&](const Expr &candidate) -> std::string {
      if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
          candidate.args.empty()) {
        return "";
      }
      auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "capacity" || helperName == "at" ||
               helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
               helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
               helperName == "remove_swap";
      };
      auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
      };
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string_view helperName;
      bool isStdNamespacedVectorHelper = false;
      std::string compatibilityCollection;
      if (normalized.rfind("array/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("array/").size());
        compatibilityCollection = "array";
      } else if (normalized.rfind("vector/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("vector/").size());
        compatibilityCollection = "vector";
      } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("std/collections/vector/").size());
        isStdNamespacedVectorHelper = true;
        compatibilityCollection = "vector";
      } else if (normalized.rfind("map/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("map/").size());
        compatibilityCollection = "map";
      } else if (normalized.rfind("std/collections/map/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("std/collections/map/").size());
        compatibilityCollection = "map";
      }
      if (helperName.empty()) {
        return "";
      }
      if (compatibilityCollection == "map") {
        if (!isRemovedMapCompatibilityHelper(helperName)) {
          return "";
        }
        const std::string removedPath = "/map/" + std::string(helperName);
        if (defMap_.find(removedPath) != defMap_.end()) {
          return "";
        }
        std::string keyType;
        std::string valueType;
        if (!resolveMapTarget(candidate.args.front(), keyType, valueType)) {
          return "";
        }
        return removedPath;
      }
      if (!isRemovedVectorCompatibilityHelper(helperName)) {
        return "";
      }
      std::string elemType;
      if (!resolveVectorTarget(candidate.args.front(), elemType) &&
          !resolveArrayTarget(candidate.args.front(), elemType) &&
          !resolveSoaVectorTarget(candidate.args.front(), elemType)) {
        return "";
      }
      if (isStdNamespacedVectorHelper) {
        return "/vector/" + std::string(helperName);
      }
      return "/" + normalized;
    };
    auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto matchesHelper = [&](const char *helper) -> bool {
        return isVectorBuiltinName(candidate, helper);
      };
      if (matchesHelper("push")) {
        nameOut = "push";
        return true;
      }
      if (matchesHelper("pop")) {
        nameOut = "pop";
        return true;
      }
      if (matchesHelper("reserve")) {
        nameOut = "reserve";
        return true;
      }
      if (matchesHelper("clear")) {
        nameOut = "clear";
        return true;
      }
      if (matchesHelper("remove_at")) {
        nameOut = "remove_at";
        return true;
      }
      if (matchesHelper("remove_swap")) {
        nameOut = "remove_swap";
        return true;
      }
      return false;
    };
    auto resolveVectorHelperMethodTarget = [&](const Expr &receiver,
                                               const std::string &helperName,
                                               std::string &resolvedOut) -> bool {
      resolvedOut.clear();
      auto resolveReceiverTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (isPrimitiveBindingTypeName(typeName)) {
          return "/" + normalizeBindingTypeName(typeName);
        }
        std::string resolvedType = resolveTypePath(typeName, namespacePrefix);
        if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
          auto importIt = importAliases_.find(typeName);
          if (importIt != importAliases_.end()) {
            resolvedType = importIt->second;
          }
        }
        return resolvedType;
      };
      if (receiver.kind == Expr::Kind::Name) {
        std::string typeName;
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
          }
        }
        if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
          return false;
        }
        const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
        if (resolvedType.empty()) {
          return false;
        }
        resolvedOut = resolvedType + "/" + helperName;
        return true;
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolvedType = resolveCalleePath(receiver);
        if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
          resolvedType = inferStructReturnPath(receiver, params, locals);
        }
        if (resolvedType.empty()) {
          return false;
        }
        resolvedOut = resolvedType + "/" + helperName;
        return true;
      }
      return false;
    };
    auto inferResolvedPathReturnKind = [&](const std::string &resolvedPath, ReturnKind &kindOut) -> bool {
      auto methodIt = defMap_.find(resolvedPath);
      if (methodIt == defMap_.end()) {
        return false;
      }
      if (!inferDefinitionReturnKind(*methodIt->second)) {
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
        const std::string stdlibAlias =
            "/std/collections/map/" + preferred.substr(std::string("/map/").size());
        if (defMap_.count(stdlibAlias) > 0) {
          preferred = stdlibAlias;
        }
      }
      if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string mapAlias =
            "/map/" + preferred.substr(std::string("/std/collections/map/").size());
        if (defMap_.count(mapAlias) > 0) {
          preferred = mapAlias;
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
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
            suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
            suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/vector/" + suffix);
        }
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/map/", 0) == 0) {
        appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
      } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
        appendUnique("/map/" + normalizedPath.substr(std::string("/std/collections/map/").size()));
      }
      return candidates;
    };
    auto isTemplateCompatibleDefinition = [&](const Definition &def) -> bool {
      if (expr.templateArgs.empty()) {
        return true;
      }
      return !def.templateArgs.empty() && def.templateArgs.size() == expr.templateArgs.size();
    };
    const std::string resolvedCallee = resolveCalleePath(expr);
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
        expr.isMethodCall ? explicitRemovedCollectionMethodPath(expr) : "";
    if (!removedCollectionMethodCompatibilityPath.empty()) {
      if (removedCollectionMethodCompatibilityPath == "/vector/at" ||
          removedCollectionMethodCompatibilityPath == "/vector/at_unsafe") {
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionMethodReturnKind(
                removedCollectionMethodCompatibilityPath, expr.args.front(), builtinMethodKind)) {
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
        !expr.isMethodCall && resolved == "/vector/count" && defMap_.find(resolved) == defMap_.end() &&
        defMap_.find("/std/collections/vector/count") != defMap_.end();
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
        if (!inferDefinitionReturnKind(*candidateIt->second)) {
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
      const std::string removedCollectionMethodPath = explicitRemovedCollectionMethodPath(expr);
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
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveArrayTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveStringTarget(expr.args.front())) {
        if (defMap_.find("/string/count") == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = "/string/count";
        hasResolvedPath = true;
      } else if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/std/collections/map/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/capacity");
        if (defMap_.find(methodPath) == defMap_.end()) {
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
        if (methodResolved == "/file_error/why") {
          return ReturnKind::String;
        }
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(methodResolved, expr.args.front(), builtinMethodKind)) {
          return builtinMethodKind;
        }
        if (defMap_.find(methodResolved) == defMap_.end()) {
          error_ = "unknown method: " + methodResolved;
          return ReturnKind::Unknown;
        }
        resolved = methodResolved;
        hasResolvedPath = true;
      }
    }
    if (!expr.isMethodCall) {
      std::string vectorHelper;
      if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {
        std::string namespacedCollection;
        std::string namespacedHelper;
        const bool isNamespacedCollectionHelperCall =
            getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
        const bool isNamespacedVectorHelperCall =
            isNamespacedCollectionHelperCall && namespacedCollection == "vector";
        if (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorHelperCall) {
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
            if (!resolveVectorHelperMethodTarget(receiverCandidate, vectorHelper, methodResolved)) {
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
    const bool isNamespacedVectorCountCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "count" &&
        isVectorBuiltinName(expr, "count") && !isArrayNamespacedVectorCountCompatibilityCall(expr);
    const bool isNamespacedMapCountCall =
        !expr.isMethodCall && isNamespacedMapHelperCall && namespacedHelper == "count" &&
        !isMapNamespacedCountCompatibilityCall(expr);
    const bool isUnnamespacedMapCountFallbackCall =
        !expr.isMethodCall && isUnnamespacedMapCountBuiltinFallbackCall(expr);
    const bool isResolvedMapCountCall =
        !expr.isMethodCall && (resolved == "/map/count" || resolved == "/std/collections/map/count");
    const bool isNamespacedVectorCapacityCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "capacity" &&
        isVectorBuiltinName(expr, "capacity");
    std::string builtinAccessName;
    const bool hasBuiltinAccessSpelling = getBuiltinArrayAccessName(expr, builtinAccessName);
    const bool isStdNamespacedVectorAccessSpelling =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
    const bool shouldAllowStdAccessCompatibilityFallback =
        isStdNamespacedVectorAccessSpelling && !builtinAccessName.empty() &&
        defMap_.find("/vector/" + builtinAccessName) != defMap_.end();
    const bool isBuiltinAccess =
        hasBuiltinAccessSpelling &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback);
    const bool isNamespacedVectorAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedVectorHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe");
    const bool isNamespacedMapAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedMapHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe") &&
        !isMapNamespacedAccessCompatibilityCall(expr);
    auto resolveBuiltinCollectionCountCapacityReturnKind = [&](const Expr &callExpr, ReturnKind &kindOut) -> bool {
      kindOut = ReturnKind::Unknown;
      if (callExpr.isMethodCall || callExpr.args.empty()) {
        return false;
      }
      const bool isCountLike =
          (isVectorBuiltinName(callExpr, "count") || isNamespacedMapCountCall ||
           isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
          callExpr.args.size() == 1 && !isArrayNamespacedVectorCountCompatibilityCall(callExpr) &&
          !prefersCanonicalVectorCountAliasDefinition;
      const bool isCapacityLike = isVectorBuiltinName(callExpr, "capacity") && callExpr.args.size() == 1;
      if (!isCountLike && !isCapacityLike) {
        return false;
      }
      std::string methodResolved;
      if (!resolveMethodCallPath(isCountLike ? "count" : "capacity", methodResolved)) {
        return false;
      }
      methodResolved = preferVectorStdlibHelperPath(methodResolved);
      auto methodIt = defMap_.find(methodResolved);
      if (methodIt != defMap_.end()) {
        if (!inferDefinitionReturnKind(*methodIt->second)) {
          return false;
        }
        auto kindIt = returnKinds_.find(methodResolved);
        if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
          kindOut = kindIt->second;
          return true;
        }
      }
      return resolveBuiltinCollectionMethodReturnKind(methodResolved, callExpr.args.front(), kindOut);
    };
    auto defIt = hasResolvedPath ? defMap_.find(resolved) : defMap_.end();
    const bool hasResolvedDefinition = defIt != defMap_.end();
    std::string normalizedCallName = expr.name;
    if (!normalizedCallName.empty() && normalizedCallName.front() == '/') {
      normalizedCallName.erase(normalizedCallName.begin());
    }
    const bool isStdNamespacedMapAccessSpelling = normalizedCallName.rfind("std/collections/map/", 0) == 0;
    const bool shouldDeferNamespacedVectorAccessCall =
        isNamespacedVectorAccessCall && (!hasResolvedDefinition || isStdNamespacedVectorAccessSpelling);
    const bool shouldDeferNamespacedMapAccessCall =
        isNamespacedMapAccessCall && (!hasResolvedDefinition || isStdNamespacedMapAccessSpelling);
    const bool shouldUseResolvedStdNamespacedVectorCountDefinition =
        isStdNamespacedVectorCountCall && defMap_.find("/vector/count") == defMap_.end() &&
        defMap_.find("/std/collections/vector/count") != defMap_.end();
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
    if (defIt != defMap_.end() && !shouldDeferResolvedNamespacedCollectionHelperReturn) {
      if (expr.isMethodCall) {
        const auto &calleeParams = paramsByDef_[resolved];
        std::string orderedArgError;
        if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, orderedArgError)) {
          error_ = orderedArgError.find("argument count mismatch") != std::string::npos
                       ? "argument count mismatch for " + resolved
                       : orderedArgError;
          return ReturnKind::Unknown;
        }
        std::vector<const Expr *> orderedArgs;
        if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderedArgError)) {
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
      if (!inferDefinitionReturnKind(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(expr, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(expr, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall &&
        (isVectorBuiltinName(expr, "count") || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        expr.args.size() == 1 &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        !prefersCanonicalVectorCountAliasDefinition &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCountCall || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall)) {
      std::string methodResolved;
      if (resolveMethodCallPath("count", methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(methodResolved, expr.args.front(), builtinMethodKind)) {
          return builtinMethodKind;
        }
        auto methodIt = defMap_.find(methodResolved);
        if (methodIt != defMap_.end()) {
          if (!inferDefinitionReturnKind(*methodIt->second)) {
            return ReturnKind::Unknown;
          }
          auto kindIt = returnKinds_.find(methodResolved);
          if (kindIt != returnKinds_.end()) {
            return kindIt->second;
          }
        }
      }
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (!resolveVectorTarget(expr.args.front(), elemType) && !resolveArrayTarget(expr.args.front(), elemType) &&
          !resolveStringTarget(expr.args.front()) &&
          !resolveMapTarget(expr.args.front(), keyType, valueType)) {
        return ReturnKind::Unknown;
      }
      return ReturnKind::Int;
    }
    if (!expr.isMethodCall &&
        (isVectorBuiltinName(expr, "count") || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        !expr.args.empty() && expr.args.size() != 1 &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        !prefersCanonicalVectorCountAliasDefinition &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCountCall || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall)) {
      std::string methodResolved;
      if (resolveMethodCallPath("count", methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind helperReturnKind = ReturnKind::Unknown;
        if (inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
          return helperReturnKind;
        }
      }
    }
    if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && !expr.args.empty() && expr.args.size() != 1 &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCapacityCall)) {
      std::string methodResolved;
      if (resolveMethodCallPath("capacity", methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind helperReturnKind = ReturnKind::Unknown;
        if (inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
          return helperReturnKind;
        }
      }
    }
    if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCapacityCall)) {
      std::string methodResolved;
      if (resolveMethodCallPath("capacity", methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(methodResolved, expr.args.front(), builtinMethodKind)) {
          return builtinMethodKind;
        }
        auto methodIt = defMap_.find(methodResolved);
        if (methodIt != defMap_.end()) {
          if (!inferDefinitionReturnKind(*methodIt->second)) {
            return ReturnKind::Unknown;
          }
          auto kindIt = returnKinds_.find(methodResolved);
          if (kindIt != returnKinds_.end()) {
            return kindIt->second;
          }
        }
      }
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        return ReturnKind::Int;
      }
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
            resolveBuiltinCollectionMethodReturnKind(methodResolved, receiverCandidate, builtinMethodKind)) {
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
          if (!inferDefinitionReturnKind(*methodIt->second)) {
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
      if (!inferDefinitionReturnKind(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(expr, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(expr, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      return ReturnKind::Unknown;
    }
    std::string builtinName;
    if (defMap_.find(resolved) == defMap_.end() && getBuiltinArrayAccessName(expr, builtinName) &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback) &&
        expr.args.size() == 2) {
      std::string elemType;
      if (resolveStringTarget(expr.args.front())) {
        return ReturnKind::Int;
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (resolveArrayTarget(expr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(elemType);
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.isMethodCall &&
        isSimpleCallName(expr, "contains") && expr.args.size() == 2) {
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        return ReturnKind::Bool;
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinGpuName(expr, builtinName)) {
      return ReturnKind::Int;
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
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (builtinName == "pow" && expr.args.size() == 2) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
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
      ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
      ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
      return combineNumeric(left, right);
    }
    if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 3) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
      result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
      return result;
    }
    if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
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

std::string SemanticsValidator::inferStructReturnPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto normalizeCollectionPath = [&](std::string typeName, std::string typeTemplateArg) -> std::string {
    if ((typeName == "Reference" || typeName == "Pointer") && !typeTemplateArg.empty()) {
      typeName = normalizeBindingTypeName(typeTemplateArg);
      typeTemplateArg.clear();
    } else {
      typeName = normalizeBindingTypeName(typeName);
    }
    if (typeName == "string") {
      return "/string";
    }
    if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector" || typeName == "map") &&
        !typeTemplateArg.empty()) {
      return "/" + typeName;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args)) {
        if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
          return "/" + base;
        }
        if (base == "map" && args.size() == 2) {
          return "/map";
        }
      }
    }
    return "";
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structNames_.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
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
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };
  auto normalizeCollectionTypePath = [&](const std::string &typePath) {
    if (typePath == "/array" || typePath == "array") {
      return std::string("/array");
    }
    if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
        typePath == "std/collections/vector") {
      return std::string("/vector");
    }
    if (typePath == "/soa_vector" || typePath == "soa_vector") {
      return std::string("/soa_vector");
    }
    if (typePath == "/map" || typePath == "map" || typePath == "/std/collections/map" ||
        typePath == "std/collections/map") {
      return std::string("/map");
    }
    if (typePath == "/string" || typePath == "string") {
      return std::string("/string");
    }
    return std::string();
  };
  auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {
    typePathOut.clear();
    auto inferCollectionTypePathFromType =
        [&](const std::string &typeName, auto &&inferCollectionTypePathFromTypeRef) -> std::string {
      const std::string normalizedType = normalizeBindingTypeName(typeName);
      std::string normalized = normalizeCollectionTypePath(normalizedType);
      if (!normalized.empty()) {
        return normalized;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(normalizedType, base, arg)) {
        return {};
      }
      base = normalizeBindingTypeName(base);
      if (base == "Reference" || base == "Pointer") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return {};
        }
        return inferCollectionTypePathFromTypeRef(args.front(), inferCollectionTypePathFromTypeRef);
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args)) {
        return {};
      }
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        return "/" + base;
      }
      if (base == "map" && args.size() == 2) {
        return "/map";
      }
      return {};
    };
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedTarget = resolveCalleePath(target);
    std::string collection;
    if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          target.templateArgs.size() == 1) {
        typePathOut = "/" + collection;
        return true;
      }
      if (collection == "map" && target.templateArgs.size() == 2) {
        typePathOut = "/map";
        return true;
      }
    }
    auto defIt = defMap_.find(resolvedTarget);
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string inferred =
            inferCollectionTypePathFromType(transform.templateArgs.front(), inferCollectionTypePathFromType);
        if (!inferred.empty()) {
          typePathOut = std::move(inferred);
          return true;
        }
        break;
      }
    }
    auto structIt = returnStructs_.find(resolvedTarget);
    if (structIt != returnStructs_.end()) {
      std::string normalized = normalizeCollectionTypePath(structIt->second);
      if (!normalized.empty()) {
        typePathOut = normalized;
        return true;
      }
    }
    auto kindIt = returnKinds_.find(resolvedTarget);
    if (kindIt != returnKinds_.end()) {
      if (kindIt->second == ReturnKind::Array) {
        typePathOut = "/array";
        return true;
      }
      if (kindIt->second == ReturnKind::String) {
        typePathOut = "/string";
        return true;
      }
    }
    return false;
  };
  auto resolveCallCollectionTemplateArgs =
      [&](const Expr &target, const std::string &expectedBase, std::vector<std::string> &argsOut) -> bool {
    argsOut.clear();
    auto extractCollectionArgsFromType =
        [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
      std::string base;
      std::string arg;
      const std::string normalizedType = normalizeBindingTypeName(typeName);
      if (!splitTemplateTypeName(normalizedType, base, arg)) {
        return false;
      }
      if (base == expectedBase) {
        return splitTopLevelTemplateArgs(arg, argsOut);
      }
      std::vector<std::string> args;
      if ((base == "Reference" || base == "Pointer") && splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return extractCollectionArgsFromTypeRef(args.front(), extractCollectionArgsFromTypeRef);
      }
      return false;
    };
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedTarget = resolveCalleePath(target);
    std::string collection;
    if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection) &&
        collection == expectedBase) {
      argsOut = target.templateArgs;
      return true;
    }
    auto defIt = defMap_.find(resolvedTarget);
    if (defIt == defMap_.end() || !defIt->second) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      return extractCollectionArgsFromType(transform.templateArgs.front(), extractCollectionArgsFromType);
    }
    return false;
  };
  auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      auto resolveReference = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return false;
        }
        elemType = args.front();
        return true;
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (resolveReference(*paramBinding)) {
          return true;
        }
        if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
            paramBinding->typeTemplateArg.empty()) {
          return false;
        }
        elemType = paramBinding->typeTemplateArg;
        return true;
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        if (resolveReference(it->second)) {
          return true;
        }
        if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
            it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> args;
        const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
    }
    return false;
  };
  auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
          return false;
        }
        elemType = paramBinding->typeTemplateArg;
        return true;
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        if (it->second.typeName != "vector" || it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "vector", args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
    }
    return false;
  };
  std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
  auto resolveBuiltinAccessReceiverExpr = [&](const Expr &accessExpr) -> const Expr * {
    if (accessExpr.kind != Expr::Kind::Call || accessExpr.args.size() != 2) {
      return nullptr;
    }
    if (accessExpr.isMethodCall) {
      return accessExpr.args.empty() ? nullptr : &accessExpr.args.front();
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(accessExpr.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < accessExpr.args.size(); ++i) {
        if (i < accessExpr.argNames.size() && accessExpr.argNames[i].has_value() &&
            *accessExpr.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    return receiverIndex < accessExpr.args.size() ? &accessExpr.args[receiverIndex] : nullptr;
  };
  auto isBuiltinStringReceiverExpr = [&](const Expr &target) -> bool {
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = locals.find(target.name);
      return it != locals.end() && it->second.typeName == "string";
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      return resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string";
    }
    return false;
  };
  auto isDirectCanonicalVectorAccessCallOnBuiltinReceiver = [&](const Expr &candidate,
                                                                size_t &receiverIndexOut) -> bool {
    receiverIndexOut = 0;
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndexOut = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndexOut = 0;
      }
    }
    if (receiverIndexOut >= candidate.args.size()) {
      return false;
    }
    std::string elemType;
    return resolveVectorTarget(candidate.args[receiverIndexOut], elemType) ||
           resolveArrayTarget(candidate.args[receiverIndexOut], elemType) ||
           isBuiltinStringReceiverExpr(candidate.args[receiverIndexOut]);
  };
  std::function<bool(const Expr &)> resolveStringTarget = [&](const Expr &target) -> bool {
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = locals.find(target.name);
      return it != locals.end() && it->second.typeName == "string";
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string") {
        return true;
      }
      std::string builtinName;
      if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
        if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
          std::string elemType;
          std::string keyType;
          std::string valueType;
          if (resolveArrayTarget(*accessReceiver, elemType) || resolveVectorTarget(*accessReceiver, elemType)) {
            return normalizeBindingTypeName(elemType) == "string";
          }
          if (resolveMapTarget(*accessReceiver, keyType, valueType)) {
            return normalizeBindingTypeName(valueType) == "string";
          }
          if (resolveStringTarget(*accessReceiver)) {
            return false;
          }
        }
        size_t receiverIndex = 0;
        if (isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
          std::string elemType;
          return resolveArrayTarget(target.args[receiverIndex], elemType) &&
                 normalizeBindingTypeName(elemType) == "string";
        }
        const std::string resolvedTarget = resolveCalleePath(target);
        auto defIt = defMap_.find(resolvedTarget);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          if (!inferDefinitionReturnKind(*defIt->second)) {
            return false;
          }
          auto kindIt = returnKinds_.find(resolvedTarget);
          if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
            return kindIt->second == ReturnKind::String;
          }
        }
        std::string elemType;
        if (resolveArrayTarget(target.args.front(), elemType) && elemType == "string") {
          return true;
        }
        std::string keyType;
        std::string valueType;
        if (resolveMapTarget(target.args.front(), keyType, valueType) &&
            normalizeBindingTypeName(valueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };
  resolveMapTarget = [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractMapKeyValueTypes(*paramBinding, keyTypeOut, valueTypeOut);
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      return extractMapKeyValueTypes(it->second, keyTypeOut, valueTypeOut);
    }
    if (target.kind == Expr::Kind::Call) {
      auto returnsMapCollection = [&](const std::string &typeName) {
        std::string normalizedType = normalizeBindingTypeName(typeName);
        while (true) {
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(normalizedType, base, arg)) {
            return normalizedType == "map";
          }
          if (base == "map") {
            std::vector<std::string> args;
            return splitTopLevelTemplateArgs(arg, args) && args.size() == 2;
          }
          if (base == "Reference" || base == "Pointer") {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
              return false;
            }
            normalizedType = normalizeBindingTypeName(args.front());
            continue;
          }
          return false;
        }
      };
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/map") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
        }
        return true;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        if (!returnsMapCollection(transform.templateArgs.front())) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(normalizeBindingTypeName(transform.templateArgs.front()), base, arg)) {
          return false;
        }
        while (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          if (!splitTemplateTypeName(normalizeBindingTypeName(args.front()), base, arg)) {
            return false;
          }
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 2) {
          return false;
        }
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    return false;
  };
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBuiltinBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  auto collectionHelperPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
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
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/vector/" + suffix);
      }
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      appendUnique("/map/" + normalizedPath.substr(std::string("/std/collections/map/").size()));
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [&](const std::string &path,
                                                               std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneBuiltinVectorAccessStructReturnCandidates = [&](const Expr &candidate,
                                                            const std::string &path,
                                                            std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("vector/", 0) == 0 || normalizedPath.rfind("std/collections/vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    if (normalizedPath.rfind("/std/collections/vector/", 0) != 0) {
      return;
    }
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (suffix != "at" && suffix != "at_unsafe") {
      return;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
      return;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return;
    }
    std::string elemType;
    if (!resolveVectorTarget(candidate.args[receiverIndex], elemType) &&
        !resolveArrayTarget(candidate.args[receiverIndex], elemType) &&
        !resolveStringTarget(candidate.args[receiverIndex])) {
      return;
    }
    const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
    for (auto it = candidates.begin(); it != candidates.end();) {
      if (*it == canonicalCandidate) {
        it = candidates.erase(it);
      } else {
        ++it;
      }
    }
  };
  auto isExplicitMapAccessStructReturnCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    std::string helperName;
    if (normalized == "map/at") {
      helperName = "at";
    } else if (normalized == "map/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return false;
    }
    if (defMap_.find("/map/" + helperName) != defMap_.end()) {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
  };

  if (expr.isLambda) {
    return "";
  }

  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (std::string collectionPath =
              normalizeCollectionPath(paramBinding->typeName, paramBinding->typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (paramBinding->typeName != "Reference" && paramBinding->typeName != "Pointer") {
        return resolveStructTypePath(paramBinding->typeName, expr.namespacePrefix);
      }
      return "";
    }
    auto it = locals.find(expr.name);
    if (it != locals.end()) {
      if (std::string collectionPath = normalizeCollectionPath(it->second.typeName, it->second.typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (it->second.typeName != "Reference" && it->second.typeName != "Pointer") {
        return resolveStructTypePath(it->second.typeName, expr.namespacePrefix);
      }
    }
    return "";
  }

  if (expr.kind == Expr::Kind::Call) {
    if (expr.isFieldAccess && expr.args.size() == 1) {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      if (receiverStruct.empty()) {
        return "";
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || !defIt->second) {
        return "";
      }
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || isStaticField(stmt) || stmt.name != expr.name) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return "";
        }
        std::string fieldType = fieldBinding.typeName;
        if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
          fieldType = fieldBinding.typeTemplateArg;
        }
        return resolveStructTypePath(fieldType, expr.namespacePrefix);
      }
      return "";
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      if (receiverStruct.empty()) {
        return "";
      }
      std::string methodName = expr.name;
      std::string rawMethodName = expr.name;
      if (!rawMethodName.empty() && rawMethodName.front() == '/') {
        rawMethodName.erase(rawMethodName.begin());
      }
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
          !namespacedHelper.empty()) {
        methodName = namespacedHelper;
      }
      const bool blocksBuiltinVectorAccessStructReturnForwarding =
          methodName == "at" || methodName == "at_unsafe";
      const bool isExplicitRemovedMapAliasStructReturnMethod =
          rawMethodName == "map/at" || rawMethodName == "map/at_unsafe" ||
          rawMethodName == "std/collections/map/at" || rawMethodName == "std/collections/map/at_unsafe";
      std::vector<std::string> methodCandidates;
      if (receiverStruct == "/vector") {
        methodCandidates = {"/vector/" + methodName};
        if (!blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/std/collections/vector/" + methodName);
        }
        if (methodName != "count" && !blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/array/" + methodName);
        }
      } else if (receiverStruct == "/array") {
        methodCandidates = {"/array/" + methodName};
        if (methodName != "count") {
          methodCandidates.push_back("/vector/" + methodName);
          if (!blocksBuiltinVectorAccessStructReturnForwarding) {
            methodCandidates.push_back("/std/collections/vector/" + methodName);
          }
        }
      } else if (receiverStruct == "/map") {
        if (!isExplicitRemovedMapAliasStructReturnMethod) {
          methodCandidates = {"/std/collections/map/" + methodName, "/map/" + methodName};
        }
      } else {
        methodCandidates = {receiverStruct + "/" + methodName};
      }
      for (const auto &candidate : methodCandidates) {
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      for (const auto &candidate : methodCandidates) {
        auto defIt = defMap_.find(candidate);
        if (defIt == defMap_.end()) {
          continue;
        }
        if (!inferDefinitionReturnKind(*defIt->second)) {
          return "";
        }
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      return "";
    }

    if (isMatchCall(expr)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(expr, expanded, error)) {
        return "";
      }
      return inferStructReturnPath(expanded, params, locals);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
      return (thenPath == elsePath) ? thenPath : "";
    }

    if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferStructReturnPath(valueExpr->args.front(), params, locals);
      }
      return inferStructReturnPath(*valueExpr, params, locals);
    }

    const std::string resolvedCallee = resolveCalleePath(expr);
    const bool isExplicitMapAccessCompatibilityCall = isExplicitMapAccessStructReturnCompatibilityCall(expr);
    const std::string structReturnProbePath = isExplicitMapAccessCompatibilityCall ? expr.name : resolvedCallee;
    auto resolvedCandidates = collectionHelperPathCandidates(structReturnProbePath);
    pruneMapAccessStructReturnCompatibilityCandidates(structReturnProbePath, resolvedCandidates);
    pruneBuiltinVectorAccessStructReturnCandidates(expr, structReturnProbePath, resolvedCandidates);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
    }
    std::string resolved = resolvedCandidates.empty() ? std::string() : resolvedCandidates.front();
    bool hasDefinitionCandidate = false;
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end()) {
        continue;
      }
      hasDefinitionCandidate = true;
      if (!inferDefinitionReturnKind(*defIt->second)) {
        return "";
      }
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
      if (resolved.empty()) {
        resolved = candidate;
      }
    }
    std::string collection;
    if (!hasDefinitionCandidate && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        return "/" + collection;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return "/map";
      }
    }
    if (!resolved.empty() && structNames_.count(resolved) > 0) {
      return resolved;
    }
  }

  return "";
}

bool SemanticsValidator::inferDefinitionReturnKind(const Definition &def) {
  auto kindIt = returnKinds_.find(def.fullPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.front() == "auto") {
      hasReturnAuto = true;
    }
  }
  if (kindIt->second != ReturnKind::Unknown) {
    return true;
  }
  if (!inferenceStack_.insert(def.fullPath).second) {
    error_ = "return type inference requires explicit annotation on " + def.fullPath;
    return false;
  }
  ReturnKind inferred = ReturnKind::Unknown;
  std::string inferredStructPath;
  bool sawReturn = false;
  const auto &defParams = paramsByDef_[def.fullPath];
  std::unordered_map<std::string, BindingInfo> locals;
  auto recordInferredReturn = [&](const Expr *valueExpr,
                                  const std::unordered_map<std::string, BindingInfo> &activeLocals) -> bool {
    ReturnKind exprKind = ReturnKind::Void;
    std::string exprStructPath;
    if (valueExpr != nullptr) {
      exprKind = inferExprReturnKind(*valueExpr, defParams, activeLocals);
      if (exprKind == ReturnKind::Array) {
        exprStructPath = inferStructReturnPath(*valueExpr, defParams, activeLocals);
      }
    }
    if (exprKind == ReturnKind::Unknown) {
      if (error_.empty()) {
        error_ = "unable to infer return type on " + def.fullPath;
      }
      return false;
    }
    if (inferred == ReturnKind::Unknown) {
      inferred = exprKind;
      if (!exprStructPath.empty()) {
        inferredStructPath = exprStructPath;
      }
      return true;
    }
    if (inferred != exprKind) {
      if (error_.empty()) {
        error_ = "conflicting return types on " + def.fullPath;
      }
      return false;
    }
    if (inferred == ReturnKind::Array) {
      if (!exprStructPath.empty()) {
        if (inferredStructPath.empty()) {
          inferredStructPath = exprStructPath;
        } else if (inferredStructPath != exprStructPath) {
          if (error_.empty()) {
            error_ = "conflicting return types on " + def.fullPath;
          }
          return false;
        }
      } else if (!inferredStructPath.empty()) {
        if (error_.empty()) {
          error_ = "conflicting return types on " + def.fullPath;
        }
        return false;
      }
    }
    return true;
  };
  std::function<bool(const Expr &, std::unordered_map<std::string, BindingInfo> &)> inferStatement;
  inferStatement = [&](const Expr &stmt, std::unordered_map<std::string, BindingInfo> &activeLocals) -> bool {
    if (stmt.isBinding) {
      BindingInfo info;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
        return false;
      }
      if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
        (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, activeLocals, info);
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, def.namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      activeLocals.emplace(stmt.name, std::move(info));
      return true;
    }
    if (isReturnCall(stmt)) {
      sawReturn = true;
      const Expr *returnValue = stmt.args.empty() ? nullptr : &stmt.args.front();
      return recordInferredReturn(returnValue, activeLocals);
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      if (!lowerMatchToIf(stmt, expanded, error_)) {
        return false;
      }
      return inferStatement(expanded, activeLocals);
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      auto walkBlock = [&](const Expr &block) -> bool {
        std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
        for (const auto &bodyExpr : block.bodyArguments) {
          if (!inferStatement(bodyExpr, blockLocals)) {
            return false;
          }
        }
        return true;
      };
      if (!walkBlock(thenBlock)) {
        return false;
      }
      if (!walkBlock(elseBlock)) {
        return false;
      }
      return true;
    }
    if (isLoopCall(stmt) && stmt.args.size() == 2) {
      const Expr &body = stmt.args[1];
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isWhileCall(stmt) && stmt.args.size() == 2) {
      const Expr &body = stmt.args[1];
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isForCall(stmt) && stmt.args.size() == 4) {
      std::unordered_map<std::string, BindingInfo> loopLocals = activeLocals;
      if (!inferStatement(stmt.args[0], loopLocals)) {
        return false;
      }
      if (stmt.args[1].isBinding) {
        if (!inferStatement(stmt.args[1], loopLocals)) {
          return false;
        }
      }
      if (!inferStatement(stmt.args[2], loopLocals)) {
        return false;
      }
      const Expr &body = stmt.args[3];
      std::unordered_map<std::string, BindingInfo> blockLocals = loopLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isRepeatCall(stmt)) {
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isBlockCall(stmt) && stmt.hasBodyArguments) {
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    return true;
  };

  for (const auto &stmt : def.statements) {
    if (!inferStatement(stmt, locals)) {
      return false;
    }
  }
  if (def.returnExpr.has_value()) {
    sawReturn = true;
    if (!recordInferredReturn(&*def.returnExpr, locals)) {
      return false;
    }
  }
  if (!sawReturn) {
    if (hasReturnTransform && hasReturnAuto) {
      if (error_.empty()) {
        error_ = "unable to infer return type on " + def.fullPath;
      }
      return false;
    }
    kindIt->second = ReturnKind::Void;
  } else if (inferred == ReturnKind::Unknown) {
    if (error_.empty()) {
      error_ = "unable to infer return type on " + def.fullPath;
    }
    return false;
  } else {
    kindIt->second = inferred;
  }
  if (!inferredStructPath.empty()) {
    returnStructs_[def.fullPath] = inferredStructPath;
  }
  inferenceStack_.erase(def.fullPath);
  return true;
}

} // namespace primec::semantics
