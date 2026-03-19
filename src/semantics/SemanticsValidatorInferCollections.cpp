#include "SemanticsValidator.h"

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
    return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesDirectMapConstructorPath("/std/collections/map/map") ||
         matchesDirectMapConstructorPath("/std/collections/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/mapOct") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapOct");
}

} // namespace

std::string SemanticsValidator::normalizeCollectionTypePath(const std::string &typePath) const {
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
  if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
    return "/map";
  }
  if (typePath == "/string" || typePath == "string") {
    return "/string";
  }
  return "";
}

bool SemanticsValidator::hasImportedDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__t");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  if (canonicalPath.rfind("/File/", 0) == 0 || canonicalPath.rfind("/FileError/", 0) == 0) {
    canonicalPath.insert(0, "/std/file");
  }
  for (const auto &importPath : program_.imports) {
    if (importPath == canonicalPath) {
      return true;
    }
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      const std::string prefix = importPath.substr(0, importPath.size() - 2);
      if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut) {
  auto findDefParamBinding = [&](const std::vector<ParameterInfo> &defParams,
                                 const std::string &name) -> const BindingInfo * {
    for (const auto &param : defParams) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };
  auto parseTypeText = [&](const std::string &typeText, BindingInfo &parsedOut) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
      parsedOut.typeName = base;
      parsedOut.typeTemplateArg = argText;
      return true;
    }
    parsedOut.typeName = normalized;
    parsedOut.typeTemplateArg.clear();
    return true;
  };

  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      break;
    }
    return parseTypeText(returnType, bindingOut);
  }

  ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));

  std::vector<ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    std::optional<std::string> restrictType;
    std::string parseError;
    (void)parseBindingInfo(
        paramExpr, def.namespacePrefix, structNames_, importAliases_, paramInfo.binding, restrictType, parseError);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  std::unordered_map<std::string, BindingInfo> defLocals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, parseError)) {
        defLocals[stmt.name] = binding;
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt)) {
        defLocals[stmt.name] = binding;
      }
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  if (valueExpr->kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findDefParamBinding(defParams, valueExpr->name)) {
      bindingOut = *paramBinding;
      return true;
    }
    auto localIt = defLocals.find(valueExpr->name);
    if (localIt != defLocals.end()) {
      bindingOut = localIt->second;
      return true;
    }
  }
  return inferBindingTypeFromInitializer(*valueExpr, defParams, defLocals, bindingOut);
}

bool SemanticsValidator::inferQueryExprTypeText(const Expr &expr,
                                                const std::vector<ParameterInfo> &params,
                                                const std::unordered_map<std::string, BindingInfo> &locals,
                                                std::string &typeTextOut) {
  auto resolveBindingTypeText = [&](const std::string &name, std::string &resolvedTypeTextOut) -> bool {
    resolvedTypeTextOut.clear();
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      resolvedTypeTextOut = bindingTypeText(*paramBinding);
      return !resolvedTypeTextOut.empty();
    }
    auto localIt = locals.find(name);
    if (localIt == locals.end()) {
      return false;
    }
    resolvedTypeTextOut = bindingTypeText(localIt->second);
    return !resolvedTypeTextOut.empty();
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

  std::unordered_set<std::string> inferredDefinitionTypeStack;
  std::function<bool(const Expr &, std::string &)> inferExprTypeText;
  inferExprTypeText = [&](const Expr &candidate, std::string &currentTypeTextOut) -> bool {
    currentTypeTextOut.clear();
    if (candidate.kind == Expr::Kind::Name) {
      return resolveBindingTypeText(candidate.name, currentTypeTextOut);
    }
    if (candidate.kind == Expr::Kind::Literal) {
      currentTypeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
      return true;
    }
    if (candidate.kind == Expr::Kind::BoolLiteral) {
      currentTypeTextOut = "bool";
      return true;
    }
    if (candidate.kind == Expr::Kind::FloatLiteral) {
      currentTypeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
      return true;
    }
    if (candidate.kind == Expr::Kind::StringLiteral) {
      currentTypeTextOut = "string";
      return true;
    }
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenTypeText;
      std::string elseTypeText;
      if (!inferExprTypeText(thenValue ? *thenValue : thenArg, thenTypeText) ||
          !inferExprTypeText(elseValue ? *elseValue : elseArg, elseTypeText)) {
        return false;
      }
      if (normalizeBindingTypeName(thenTypeText) != normalizeBindingTypeName(elseTypeText)) {
        return false;
      }
      currentTypeTextOut = thenTypeText;
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferExprTypeText(valueExpr->args.front(), currentTypeTextOut);
      }
      return inferExprTypeText(*valueExpr, currentTypeTextOut);
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      std::string wrappedTypeText;
      if (!inferExprTypeText(candidate.args.front(), wrappedTypeText)) {
        return false;
      }
      currentTypeTextOut = unwrapReferencePointerTypeText(wrappedTypeText);
      return !currentTypeTextOut.empty();
    }

    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          candidate.templateArgs.size() == 1) {
        currentTypeTextOut = collection + "<" + candidate.templateArgs.front() + ">";
        return true;
      }
      if (collection == "map" && candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
    }

    const std::string resolvedCandidate = resolveCalleePath(candidate);
    if (isDirectMapConstructorPath(resolvedCandidate)) {
      if (candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
      if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
        return false;
      }
      std::string keyTypeText;
      std::string valueTypeText;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyTypeText;
        std::string currentValueTypeText;
        if (!inferExprTypeText(candidate.args[i], currentKeyTypeText) ||
            !inferExprTypeText(candidate.args[i + 1], currentValueTypeText)) {
          return false;
        }
        if (keyTypeText.empty()) {
          keyTypeText = currentKeyTypeText;
        } else if (normalizeBindingTypeName(keyTypeText) != normalizeBindingTypeName(currentKeyTypeText)) {
          return false;
        }
        if (valueTypeText.empty()) {
          valueTypeText = currentValueTypeText;
        } else if (normalizeBindingTypeName(valueTypeText) != normalizeBindingTypeName(currentValueTypeText)) {
          return false;
        }
      }
      if (keyTypeText.empty() || valueTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = "map<" + keyTypeText + ", " + valueTypeText + ">";
      return true;
    }

    auto defIt = defMap_.find(resolvedCandidate);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (transform.templateArgs.front() == "auto") {
        break;
      }
      currentTypeTextOut = transform.templateArgs.front();
      return !currentTypeTextOut.empty();
    }
    if (!inferredDefinitionTypeStack.insert(resolvedCandidate).second) {
      return false;
    }
    const auto stackIt = inferredDefinitionTypeStack.find(resolvedCandidate);
    struct ScopeGuard {
      std::unordered_set<std::string> &stack;
      std::unordered_set<std::string>::const_iterator it;
      ~ScopeGuard() {
        stack.erase(it);
      }
    } guard{inferredDefinitionTypeStack, stackIt};
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      return false;
    }
    currentTypeTextOut = bindingTypeText(inferredReturn);
    return !currentTypeTextOut.empty();
  };

  return inferExprTypeText(expr, typeTextOut);
}

SemanticsValidator::BuiltinCollectionDispatchResolvers SemanticsValidator::makeBuiltinCollectionDispatchResolvers(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {
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
  auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {
    pointeeTypeOut.clear();
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    pointeeTypeOut = args.front();
    return !pointeeTypeOut.empty();
  };
  auto extractCollectionElementType = [&](const std::string &typeText,
                                          const std::string &expectedBase,
                                          std::string &elemTypeOut) -> bool {
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
  };
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
  auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,
                                        std::string &keyTypeOut,
                                        std::string &valueTypeOut) -> bool {
    return extractMapKeyValueTypes(binding, keyTypeOut, valueTypeOut) ||
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto resolveBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        bindingOut = it->second;
        return true;
      }
    }
    return adapters.resolveBindingTarget != nullptr &&
           adapters.resolveBindingTarget(target, bindingOut);
  };
  auto inferCallBinding = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    return target.kind == Expr::Kind::Call &&
           adapters.inferCallBinding != nullptr &&
           adapters.inferCallBinding(target, bindingOut);
  };
  auto resolveArrayLikeBinding = [&](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    auto resolveReference = [&](const BindingInfo &candidate) -> bool {
      if (candidate.typeName != "Reference" || candidate.typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(candidate.typeTemplateArg, base, arg) ||
          normalizeBindingTypeName(base) != "array") {
        return false;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    };
    if (resolveReference(binding)) {
      return true;
    }
    if ((binding.typeName != "array" && binding.typeName != "vector") ||
        binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveVectorBinding = [&](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "vector" || binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveSoaVectorBinding = [&](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "soa_vector" || binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveStringBinding = [&](const BindingInfo &binding) -> bool {
    return normalizeBindingTypeName(binding.typeName) == "string";
  };
  auto resolveMapBinding = [&](const BindingInfo &binding,
                               std::string &keyTypeOut,
                               std::string &valueTypeOut) -> bool {
    return extractAnyMapKeyValueTypes(binding, keyTypeOut, valueTypeOut);
  };
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

  struct ReceiverResolverState {
    std::function<bool(const Expr &, std::string &)> resolveIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveDereferencedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveWrappedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveBufferTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapValueTarget;
    std::function<bool(const Expr &, size_t &)> isDirectCanonicalVectorAccessCallOnBuiltinReceiver;
  };
  auto state = std::make_shared<ReceiverResolverState>();

  state->resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {
    elemType.clear();
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemType); };
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
  state->resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string accessName;
    if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
      return false;
    }
    const Expr *accessReceiver = resolveBuiltinAccessReceiverExprInline(target);
    if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
      return false;
    }
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemTypeOut); };
    if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
      return resolveBinding(*paramBinding);
    }
    auto it = locals.find(accessReceiver->name);
    return it != locals.end() && resolveBinding(it->second);
  };
  state->resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
      return false;
    }
    std::string wrappedType;
    if (!state->resolveIndexedArgsPackElementType(target.args.front(), wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string wrappedType;
    if (!state->resolveIndexedArgsPackElementType(target, wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveArrayLikeBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> args;
        const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
    }
    return false;
  };
  state->resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) && args.size() == 1) {
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
              if (!resolveCallCollectionTypePath(vectorSource, params, locals, vectorCollectionTypePath) ||
                  vectorCollectionTypePath != "/vector") {
                return false;
              }
              std::vector<std::string> vectorArgs;
              if (resolveCallCollectionTemplateArgs(vectorSource, "vector", params, locals, vectorArgs) &&
                  vectorArgs.size() == 1) {
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
    if (inferCallBinding(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    return false;
  };
  state->resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveSoaVectorBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/soa_vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "soa_vector", params, locals, args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
      if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
        return state->resolveVectorTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  state->resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {
    auto resolveReferenceBufferType = [&](const std::string &typeName,
                                          const std::string &typeTemplateArg,
                                          std::string &elemTypeOut) -> bool {
      if ((typeName != "Reference" && typeName != "Pointer") || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) || normalizeBindingTypeName(base) != "Buffer" ||
          nestedArg.empty()) {
        return false;
      }
      elemTypeOut = nestedArg;
      return true;
    };
    auto resolveArgsPackReferenceBufferType = [&](const std::string &typeName,
                                                  const std::string &typeTemplateArg,
                                                  std::string &elemTypeOut) -> bool {
      if (typeName != "args" || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          (normalizeBindingTypeName(base) != "Reference" && normalizeBindingTypeName(base) != "Pointer") ||
          nestedArg.empty()) {
        return false;
      }
      return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackReferenceBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (resolveArgsPackReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemTypeOut)) {
          return true;
        }
      }
      auto it = locals.find(targetName);
      return it != locals.end() &&
             resolveArgsPackReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackValueBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      auto resolveBinding = [&](const BindingInfo &binding) {
        std::string packElemType;
        std::string base;
        return getArgsPackElementType(binding, packElemType) &&
               splitTemplateTypeName(normalizeBindingTypeName(packElemType), base, elemTypeOut) &&
               normalizeBindingTypeName(base) == "Buffer";
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(targetName);
      return it != locals.end() && resolveBinding(it->second);
    };
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
      if (resolveIndexedArgsPackValueBuffer(target, elemType)) {
        return true;
      }
      if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
        const Expr &innerTarget = target.args.front();
        if (innerTarget.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, innerTarget.name)) {
            if (resolveReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemType)) {
              return true;
            }
          }
          auto it = locals.find(innerTarget.name);
          if (it != locals.end() &&
              resolveReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemType)) {
            return true;
          }
        }
        if (resolveIndexedArgsPackReferenceBuffer(innerTarget, elemType)) {
          return true;
        }
      }
      if (isSimpleCallName(target, "buffer") && target.templateArgs.size() == 1) {
        elemType = target.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(target, "upload") && target.args.size() == 1) {
        return state->resolveArrayTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  state->resolveMapTarget = [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveMapBinding(binding, keyTypeOut, valueTypeOut);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string elemType;
      if (state->resolveIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
        const Expr *accessReceiver = resolveBuiltinAccessReceiverExprInline(target);
        if (accessReceiver != nullptr && accessReceiver->kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
            std::string packElemType;
            if (getArgsPackElementType(*paramBinding, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
          auto it = locals.find(accessReceiver->name);
          if (it != locals.end()) {
            std::string packElemType;
            if (getArgsPackElementType(it->second, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/map") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
        }
        return true;
      }
      if (inferCallBinding(target, binding) &&
          resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
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
        if (!returnsMapCollectionType(transform.templateArgs.front())) {
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
  state->resolveExperimentalMapTarget =
      [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
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
    return inferCallBinding(target, binding) &&
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  state->resolveExperimentalMapValueTarget =
      [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    auto extractValueBinding = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
    };
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractValueBinding(binding);
    }
    return target.kind == Expr::Kind::Call &&
           inferCallBinding(target, binding) &&
           extractValueBinding(binding);
  };
  state->isDirectCanonicalVectorAccessCallOnBuiltinReceiver =
      [&](const Expr &candidate, size_t &receiverIndexOut) -> bool {
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
    return state->resolveArgsPackAccessTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveVectorTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveArrayTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveStringTarget(candidate.args[receiverIndexOut]);
  };
  state->resolveStringTarget = [&](const Expr &target) -> bool {
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveStringBinding(binding);
    }
    if (target.kind == Expr::Kind::Call) {
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
      if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
        if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExprInline(target)) {
          std::string elemType;
          std::string keyType;
          std::string valueType;
          if (state->resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
              state->resolveArrayTarget(*accessReceiver, elemType) ||
              state->resolveVectorTarget(*accessReceiver, elemType)) {
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
        if (state->isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
          std::string elemType;
          return (state->resolveArgsPackAccessTarget(target.args[receiverIndex], elemType) ||
                  state->resolveArrayTarget(target.args[receiverIndex], elemType)) &&
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
             state->resolveArrayTarget(target.args.front(), elemType)) &&
            elemType == "string") {
          return true;
        }
        std::string keyType;
        std::string valueType;
        if (state->resolveMapTarget(target.args.front(), keyType, valueType) &&
            normalizeBindingTypeName(valueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };

  return {state->resolveIndexedArgsPackElementType,
          state->resolveDereferencedIndexedArgsPackElementType,
          state->resolveWrappedIndexedArgsPackElementType,
          state->resolveArgsPackAccessTarget,
          state->resolveArrayTarget,
          state->resolveVectorTarget,
          state->resolveSoaVectorTarget,
          state->resolveBufferTarget,
          state->resolveStringTarget,
          state->resolveMapTarget,
          state->resolveExperimentalMapTarget,
          state->resolveExperimentalMapValueTarget};
}

bool SemanticsValidator::resolveCallCollectionTypePath(const Expr &target,
                                                       const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       std::string &typePathOut) {
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
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    return {};
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  std::string targetTypeText;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText)) {
    const std::string inferred = inferCollectionTypePathFromType(targetTypeText, inferCollectionTypePathFromType);
    if (!inferred.empty()) {
      typePathOut = inferred;
      return true;
    }
  }

  const std::string resolvedTarget = resolveCalleePath(target);
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
}

bool SemanticsValidator::resolveCallCollectionTemplateArgs(const Expr &target,
                                                           const std::string &expectedBase,
                                                           const std::vector<ParameterInfo> &params,
                                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                                           std::vector<std::string> &argsOut) {
  argsOut.clear();
  auto extractCollectionArgsFromType =
      [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
    std::string base;
    std::string arg;
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == expectedBase || (expectedBase == "map" && isMapCollectionTypeName(base))) {
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

  std::string targetTypeText;
  return inferQueryExprTypeText(target, params, locals, targetTypeText) &&
         extractCollectionArgsFromType(targetTypeText, extractCollectionArgsFromType);
}

} // namespace primec::semantics
