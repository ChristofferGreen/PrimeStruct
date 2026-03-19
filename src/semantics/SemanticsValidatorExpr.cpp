#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex) {
  ExprContextScope exprScope(*this, expr);
  if (expr.isLambda) {
    return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      error_ = "entry argument strings are only supported in print calls or string bindings";
      return false;
    }
  }
  std::optional<EffectScope> effectScope;
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(expr, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  auto isMutableBinding = [&](const std::vector<ParameterInfo> &paramsIn,
                              const std::unordered_map<std::string, BindingInfo> &localsIn,
                              const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(paramsIn, name)) {
      return paramBinding->isMutable;
    }
    auto it = localsIn.find(name);
    return it != localsIn.end() && it->second.isMutable;
  };
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isIntegerExpr = [&](const Expr &arg,
                           const std::vector<ParameterInfo> &paramsIn,
                           const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::String || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral ||
          arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (isPointerExpr(arg, paramsIn, localsIn)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64;
        }
        auto it = localsIn.find(arg.name);
        if (it != localsIn.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
  };
  (void)isIntegerExpr;
  if (expr.kind == Expr::Kind::Literal) {
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(expr.stringValue, parsed, error_)) {
      return false;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error_ = "ascii string literal contains non-ASCII characters";
      return false;
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      if (movedBindings_.count(expr.name) > 0) {
        error_ = "use-after-move: " + expr.name;
        return false;
      }
      return true;
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      error_ = "math constant requires import /std/math/* or /std/math/<name>: " + expr.name;
      return false;
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    error_ = "unknown identifier: " + expr.name;
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isBinding) {
      error_ = "binding not allowed in expression context";
      return false;
    }
    std::optional<EffectScope> effectScope;
    if (!expr.transforms.empty()) {
      std::unordered_set<std::string> executionEffects;
      if (!resolveExecutionEffects(expr, executionEffects)) {
        return false;
      }
      effectScope.emplace(*this, std::move(executionEffects));
    }
    if (isMatchCall(expr)) {
      Expr expanded;
      if (!lowerMatchToIf(expr, expanded, error_)) {
        return false;
      }
      return validateExpr(params, locals, expanded);
    }
    if (isIfCall(expr)) {
      return validateIfExpr(params, locals, expr);
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "uninitialized")) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "uninitialized does not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "uninitialized does not accept arguments";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      return true;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "init") ||
                               isSimpleCallName(expr, "drop") ||
                               isSimpleCallName(expr, "take") ||
                               isSimpleCallName(expr, "borrow"))) {
      const std::string name = expr.name;
      auto isUninitializedStorage = [&](const Expr &arg) -> bool {
        BindingInfo binding;
        bool resolved = false;
        if (!resolveUninitializedStorageBinding(params, locals, arg, binding, resolved)) {
          return false;
        }
        if (!resolved || binding.typeName != "uninitialized" || binding.typeTemplateArg.empty()) {
          return false;
        }
        return true;
      };
      const bool treatAsUninitializedHelper =
          (name != "take") || (!expr.args.empty() && isUninitializedStorage(expr.args.front()));
      if (treatAsUninitializedHelper) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = name + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = name + " does not accept block arguments";
          return false;
        }
        const size_t expectedArgs = (name == "init") ? 2 : 1;
        if (expr.args.size() != expectedArgs) {
          error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
                   (expectedArgs == 1 ? "" : "s");
          return false;
        }
        if (name == "init" || name == "drop") {
          error_ = name + " is only supported as a statement";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if (!isUninitializedStorage(expr.args.front())) {
          error_ = name + " requires uninitialized<T> storage";
          return false;
        }
        return true;
      }
    }
    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      return validateBlockExpr(params, locals, expr);
    }
    if (isBuiltinBlockCall(expr)) {
      error_ = "block requires block arguments";
      return false;
    }
    if (isLoopCall(expr)) {
      error_ = "loop is only supported as a statement";
      return false;
    }
    if (isWhileCall(expr)) {
      error_ = "while is only supported as a statement";
      return false;
    }
    if (isForCall(expr)) {
      error_ = "for is only supported as a statement";
      return false;
    }
    if (isRepeatCall(expr)) {
      error_ = "repeat is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "dispatch")) {
      error_ = "dispatch is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      error_ = "buffer_store is only supported as a statement";
      return false;
    }
    bool hasVectorHelperCallResolution = false;
    std::string vectorHelperCallResolvedPath;
    size_t vectorHelperCallReceiverIndex = 0;
    if (!resolveExprVectorHelperCall(params,
                                     locals,
                                     expr,
                                     hasVectorHelperCallResolution,
                                     vectorHelperCallResolvedPath,
                                     vectorHelperCallReceiverIndex)) {
      return false;
    }
    if (isReturnCall(expr)) {
      error_ = "return not allowed in expression context";
      return false;
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
      if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
        return "/map";
      }
      if (typePath == "/string" || typePath == "string") {
        return "/string";
      }
      return "";
    };
    auto bindingTypeText = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    auto inferDefinitionReturnBinding = [&](const Definition &def, BindingInfo &bindingOut) -> bool {
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
        const Expr *innerValueExpr = nullptr;
        for (const auto &bodyExpr : candidate.bodyArguments) {
          if (bodyExpr.isBinding) {
            continue;
          }
          innerValueExpr = &bodyExpr;
        }
        return innerValueExpr;
      };
      auto extractDirectMapConstructorBinding = [&](const Expr &candidate, BindingInfo &directOut) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        const bool isDirectMapConstructor =
            matchesPath("/std/collections/map/map") ||
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
        if (!isDirectMapConstructor) {
          return false;
        }
        if (candidate.templateArgs.size() == 2) {
          directOut.typeName = "map";
          directOut.typeTemplateArg = candidate.templateArgs[0] + ", " + candidate.templateArgs[1];
          return true;
        }
        if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
          return false;
        }
        auto inferSimpleTypeText = [&](const Expr &value, std::string &typeTextOut) -> bool {
          typeTextOut.clear();
          if (value.kind == Expr::Kind::Literal) {
            typeTextOut = value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32");
            return true;
          }
          if (value.kind == Expr::Kind::BoolLiteral) {
            typeTextOut = "bool";
            return true;
          }
          if (value.kind == Expr::Kind::FloatLiteral) {
            typeTextOut = value.floatWidth == 64 ? "f64" : "f32";
            return true;
          }
          if (value.kind == Expr::Kind::StringLiteral) {
            typeTextOut = "string";
            return true;
          }
          if (value.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findDefParamBinding(defParams, value.name)) {
              typeTextOut = bindingTypeText(*paramBinding);
              return !typeTextOut.empty();
            }
            auto localIt = defLocals.find(value.name);
            if (localIt != defLocals.end()) {
              typeTextOut = bindingTypeText(localIt->second);
              return !typeTextOut.empty();
            }
          }
          return false;
        };
        std::string keyType;
        std::string valueType;
        for (size_t i = 0; i < candidate.args.size(); i += 2) {
          std::string currentKeyType;
          std::string currentValueType;
          if (!inferSimpleTypeText(candidate.args[i], currentKeyType) ||
              !inferSimpleTypeText(candidate.args[i + 1], currentValueType)) {
            return false;
          }
          if (keyType.empty()) {
            keyType = currentKeyType;
          } else if (normalizeBindingTypeName(keyType) != normalizeBindingTypeName(currentKeyType)) {
            return false;
          }
          if (valueType.empty()) {
            valueType = currentValueType;
          } else if (normalizeBindingTypeName(valueType) != normalizeBindingTypeName(currentValueType)) {
            return false;
          }
        }
        if (keyType.empty() || valueType.empty()) {
          return false;
        }
        directOut.typeName = "map";
        directOut.typeTemplateArg = keyType + ", " + valueType;
        return true;
      };
      std::function<bool(const Expr &, BindingInfo &)> inferValueBinding;
      inferValueBinding = [&](const Expr &candidate, BindingInfo &inferredOut) -> bool {
        if (candidate.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findDefParamBinding(defParams, candidate.name)) {
            inferredOut = *paramBinding;
            return true;
          }
          auto localIt = defLocals.find(candidate.name);
          if (localIt != defLocals.end()) {
            inferredOut = localIt->second;
            return true;
          }
          return false;
        }
        if (isIfCall(candidate) && candidate.args.size() == 3) {
          const Expr &thenArg = candidate.args[1];
          const Expr &elseArg = candidate.args[2];
          const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
          const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
          BindingInfo thenBinding;
          BindingInfo elseBinding;
          if (!inferValueBinding(thenValue ? *thenValue : thenArg, thenBinding) ||
              !inferValueBinding(elseValue ? *elseValue : elseArg, elseBinding)) {
            return false;
          }
          if (normalizeBindingTypeName(bindingTypeText(thenBinding)) !=
              normalizeBindingTypeName(bindingTypeText(elseBinding))) {
            return false;
          }
          inferredOut = thenBinding;
          return true;
        }
        if (const Expr *innerValueExpr = getEnvelopeValueExpr(candidate, false)) {
          if (isReturnCall(*innerValueExpr) && !innerValueExpr->args.empty()) {
            return inferValueBinding(innerValueExpr->args.front(), inferredOut);
          }
          return inferValueBinding(*innerValueExpr, inferredOut);
        }
        if (extractDirectMapConstructorBinding(candidate, inferredOut)) {
          return true;
        }
        return false;
      };
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
                     inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding)) {
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
      if (inferValueBinding(*valueExpr, bindingOut)) {
        return true;
      }
      return inferBindingTypeFromInitializer(*valueExpr, defParams, defLocals, bindingOut);
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
        if (isMapCollectionTypeName(base) && args.size() == 2) {
          return "/map";
        }
        return {};
      };
      auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {
        typeTextOut.clear();
        if (candidate.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
            typeTextOut = bindingTypeText(*paramBinding);
            return !typeTextOut.empty();
          }
          auto it = locals.find(candidate.name);
          if (it != locals.end()) {
            typeTextOut = bindingTypeText(it->second);
            return !typeTextOut.empty();
          }
          return false;
        }
        if (candidate.kind == Expr::Kind::Literal) {
          typeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (candidate.kind == Expr::Kind::BoolLiteral) {
          typeTextOut = "bool";
          return true;
        }
        if (candidate.kind == Expr::Kind::FloatLiteral) {
          typeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (candidate.kind == Expr::Kind::StringLiteral) {
          typeTextOut = "string";
          return true;
        }
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto defIt = defMap_.find(resolvedCandidate);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name == "return" && transform.templateArgs.size() == 1) {
              typeTextOut = transform.templateArgs.front();
              return true;
            }
          }
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
            typeTextOut = bindingTypeText(inferredReturn);
            return !typeTextOut.empty();
          }
        }
        return false;
      };
      auto inferDirectMapConstructorTemplateArgs = [&](const Expr &candidate,
                                                       std::vector<std::string> &argsOut) -> bool {
        argsOut.clear();
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        const bool isDirectMapConstructor =
            matchesDirectMapConstructorPath("/std/collections/map/map") ||
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
        if (!isDirectMapConstructor) {
          return false;
        }
        if (candidate.templateArgs.size() == 2) {
          argsOut = candidate.templateArgs;
          return true;
        }
        if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
          return false;
        }
        std::string keyType;
        std::string valueType;
        for (size_t i = 0; i < candidate.args.size(); i += 2) {
          std::string currentKeyType;
          std::string currentValueType;
          if (!inferTargetTypeText(candidate.args[i], currentKeyType) ||
              !inferTargetTypeText(candidate.args[i + 1], currentValueType)) {
            return false;
          }
          if (keyType.empty()) {
            keyType = currentKeyType;
          } else if (normalizeBindingTypeName(keyType) != normalizeBindingTypeName(currentKeyType)) {
            return false;
          }
          if (valueType.empty()) {
            valueType = currentValueType;
          } else if (normalizeBindingTypeName(valueType) != normalizeBindingTypeName(currentValueType)) {
            return false;
          }
        }
        if (keyType.empty() || valueType.empty()) {
          return false;
        }
        argsOut = {keyType, valueType};
        return true;
      };
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      std::vector<std::string> directMapArgs;
      if (inferDirectMapConstructorTemplateArgs(target, directMapArgs)) {
        typePathOut = "/map";
        return true;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string pointerBuiltin;
      if (getBuiltinPointerName(target, pointerBuiltin) && pointerBuiltin == "dereference" && target.args.size() == 1) {
        std::string derefTypeText;
        if (inferTargetTypeText(target.args.front(), derefTypeText)) {
          std::string inferred =
              inferCollectionTypePathFromType(derefTypeText, inferCollectionTypePathFromType);
          if (!inferred.empty()) {
            typePathOut = std::move(inferred);
            return true;
          }
        }
      }
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
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
          std::string inferred = inferCollectionTypePathFromType(
              bindingTypeText(inferredReturn), inferCollectionTypePathFromType);
          if (!inferred.empty()) {
            typePathOut = std::move(inferred);
            return true;
          }
        }
      }
      auto structIt = returnStructs_.find(resolvedTarget);
      if (structIt != returnStructs_.end()) {
        std::string normalized = normalizeCollectionTypePath(structIt->second);
        if (!normalized.empty()) {
          typePathOut = normalized;
          return true;
        }
        return false;
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
      auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {
        typeTextOut.clear();
        if (candidate.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
            typeTextOut = bindingTypeText(*paramBinding);
            return !typeTextOut.empty();
          }
          auto it = locals.find(candidate.name);
          if (it != locals.end()) {
            typeTextOut = bindingTypeText(it->second);
            return !typeTextOut.empty();
          }
          return false;
        }
        if (candidate.kind == Expr::Kind::Literal) {
          typeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (candidate.kind == Expr::Kind::BoolLiteral) {
          typeTextOut = "bool";
          return true;
        }
        if (candidate.kind == Expr::Kind::FloatLiteral) {
          typeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (candidate.kind == Expr::Kind::StringLiteral) {
          typeTextOut = "string";
          return true;
        }
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto defIt = defMap_.find(resolvedCandidate);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name == "return" && transform.templateArgs.size() == 1) {
              typeTextOut = transform.templateArgs.front();
              return true;
            }
          }
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
            typeTextOut = bindingTypeText(inferredReturn);
            return !typeTextOut.empty();
          }
        }
        return false;
      };
      auto inferDirectMapConstructorTemplateArgs = [&](const Expr &candidate) -> bool {
        if (expectedBase != "map" || candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        const bool isDirectMapConstructor =
            matchesDirectMapConstructorPath("/std/collections/map/map") ||
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
        if (!isDirectMapConstructor) {
          return false;
        }
        if (candidate.templateArgs.size() == 2) {
          argsOut = candidate.templateArgs;
          return true;
        }
        if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
          return false;
        }
        std::string keyType;
        std::string valueType;
        for (size_t i = 0; i < candidate.args.size(); i += 2) {
          std::string currentKeyType;
          std::string currentValueType;
          if (!inferTargetTypeText(candidate.args[i], currentKeyType) ||
              !inferTargetTypeText(candidate.args[i + 1], currentValueType)) {
            return false;
          }
          if (keyType.empty()) {
            keyType = currentKeyType;
          } else if (normalizeBindingTypeName(keyType) != normalizeBindingTypeName(currentKeyType)) {
            return false;
          }
          if (valueType.empty()) {
            valueType = currentValueType;
          } else if (normalizeBindingTypeName(valueType) != normalizeBindingTypeName(currentValueType)) {
            return false;
          }
        }
        if (keyType.empty() || valueType.empty()) {
          return false;
        }
        argsOut = {keyType, valueType};
        return true;
      };
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      if (inferDirectMapConstructorTemplateArgs(target)) {
        return true;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string pointerBuiltin;
      if (getBuiltinPointerName(target, pointerBuiltin) && pointerBuiltin == "dereference" && target.args.size() == 1) {
        std::string derefTypeText;
        if (inferTargetTypeText(target.args.front(), derefTypeText)) {
          return extractCollectionArgsFromType(derefTypeText, extractCollectionArgsFromType);
        }
      }
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
      BindingInfo inferredReturn;
      if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
        return extractCollectionArgsFromType(bindingTypeText(inferredReturn), extractCollectionArgsFromType);
      }
      return false;
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
    auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string accessName;
      if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
        return false;
      }
      const Expr *accessReceiver = resolveBuiltinAccessReceiverExprInline(target);
      if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
        return false;
      }
      auto resolveBinding = [&](const BindingInfo &binding) {
        return getArgsPackElementType(binding, elemTypeOut);
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(accessReceiver->name);
      return it != locals.end() && resolveBinding(it->second);
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
    auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
        return false;
      }
      std::string wrappedType;
      if (!resolveIndexedArgsPackElementType(target.args.front(), wrappedType)) {
        return false;
      }
      return extractWrappedPointeeType(wrappedType, elemTypeOut);
    };
    auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string wrappedType;
      if (!resolveIndexedArgsPackElementType(target, wrappedType)) {
        return false;
      }
      return extractWrappedPointeeType(wrappedType, elemTypeOut);
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
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        std::string base;
        std::string arg;
        if (fieldBinding.typeName == "Reference" && !fieldBinding.typeTemplateArg.empty() &&
            splitTemplateTypeName(fieldBinding.typeTemplateArg, base, arg) && base == "array") {
          std::vector<std::string> args;
          if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
            elemType = args.front();
            return true;
          }
        }
        if ((fieldBinding.typeName == "array" || fieldBinding.typeName == "vector") && !fieldBinding.typeTemplateArg.empty()) {
          elemType = fieldBinding.typeTemplateArg;
          return true;
        }
      }
      if (target.kind == Expr::Kind::Call) {
        std::string indexedElemType;
        if (resolveIndexedArgsPackElementType(target, indexedElemType) &&
            (extractCollectionElementType(indexedElemType, "array", elemType) ||
             extractCollectionElementType(indexedElemType, "vector", elemType))) {
          return true;
        }
        if (resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
            (extractCollectionElementType(indexedElemType, "array", elemType) ||
             extractCollectionElementType(indexedElemType, "vector", elemType))) {
          return true;
        }
        if (resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
            (extractCollectionElementType(indexedElemType, "array", elemType) ||
             extractCollectionElementType(indexedElemType, "vector", elemType))) {
          return true;
        }
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
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        if (fieldBinding.typeName != "vector" || fieldBinding.typeTemplateArg.empty()) {
          return false;
        }
        elemType = fieldBinding.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string indexedElemType;
        if (resolveIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "vector", elemType)) {
          return true;
        }
        if (resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "vector", elemType)) {
          return true;
        }
        if (resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "vector", elemType)) {
          return true;
        }
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/vector") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "vector", args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
        auto defIt = defMap_.find(resolveCalleePath(target));
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
              normalizeBindingTypeName(inferredReturn.typeName) == "vector" &&
              !inferredReturn.typeTemplateArg.empty()) {
            elemType = inferredReturn.typeTemplateArg;
            return true;
          }
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
    (void)resolveArgsPackCountTarget;
    auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {
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
    auto isNamedArgsPackMethodAccessCall = [&](const Expr &target) -> bool {
      if (!target.isMethodCall) {
        return false;
      }
      std::string accessName;
      if (!getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
        return false;
      }
      std::string elemType;
      if (!resolveArgsPackAccessTarget(target.args.front(), elemType)) {
        return false;
      }
      size_t namedCount = 0;
      for (const auto &argName : target.argNames) {
        if (!argName.has_value()) {
          continue;
        }
        ++namedCount;
        if (*argName != "index") {
          return false;
        }
      }
      return namedCount == 1;
    };
    auto isNamedArgsPackWrappedFileBuiltinAccessCall = [&](const Expr &target) -> bool {
      if (target.isMethodCall) {
        return false;
      }
      std::string accessName;
      if (!getBuiltinArrayAccessName(target, accessName) || accessName != "at" ||
          target.args.size() != 2) {
        return false;
      }
      if (target.argNames.size() != 2 || !target.argNames[0].has_value() ||
          !target.argNames[1].has_value() || *target.argNames[0] != "values" ||
          *target.argNames[1] != "index") {
        return false;
      }
      std::string elemType;
      if (!resolveArgsPackAccessTarget(target.args.front(), elemType)) {
        return false;
      }
      std::string pointeeType;
      if (!extractWrappedPointeeType(elemType, pointeeType)) {
        return false;
      }
      std::string base;
      std::string argText;
      return splitTemplateTypeName(normalizeBindingTypeName(pointeeType), base, argText) &&
             normalizeBindingTypeName(base) == "File" && !argText.empty();
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
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        if (fieldBinding.typeName != "soa_vector" || fieldBinding.typeTemplateArg.empty()) {
          return false;
        }
        elemType = fieldBinding.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string indexedElemType;
        if (resolveIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
        if (resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
        if (resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
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
    std::function<bool(const Expr &)> resolveMapTarget;
    auto resolveBuiltinAccessReceiverExpr = [&](const Expr &accessExpr) -> const Expr * {
      return resolveBuiltinAccessReceiverExprInline(accessExpr);
    };
    auto resolveMapValueTypeForStringTarget = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          std::string keyType;
          return extractMapKeyValueTypes(*paramBinding, keyType, valueTypeOut);
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        std::string keyType;
        return extractMapKeyValueTypes(it->second, keyType, valueTypeOut);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        std::string keyType;
        return extractMapKeyValueTypes(fieldBinding, keyType, valueTypeOut);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      std::string collectionTypePath;
      if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
        return false;
      }
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
        valueTypeOut = args[1];
      }
      return true;
    };
    std::function<bool(const Expr &)> resolveStringTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }

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
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return fieldBinding.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string") {
          return true;
        }
        if (target.isMethodCall && target.name == "why" && !target.args.empty()) {
          const Expr &receiver = target.args.front();
          if (receiver.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
              if (normalizeBindingTypeName(paramBinding->typeName) == "FileError") {
                return true;
              }
            } else if (auto it = locals.find(receiver.name); it != locals.end()) {
              if (normalizeBindingTypeName(it->second.typeName) == "FileError") {
                return true;
              }
            }
            if (receiver.name == "Result") {
              return true;
            }
          }
          std::string elemType;
          if ((resolveIndexedArgsPackElementType(receiver, elemType) ||
               resolveDereferencedIndexedArgsPackElementType(receiver, elemType)) &&
              normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType)) == "FileError") {
            return true;
          }
        }
        std::string builtinName;
        if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
          if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
            std::string elemType;
            std::string mapValueType;
            if (resolveArgsPackAccessTarget(*accessReceiver, elemType) || resolveArrayTarget(*accessReceiver, elemType) ||
                resolveVectorTarget(*accessReceiver, elemType)) {
              return normalizeBindingTypeName(elemType) == "string";
            }
            if (resolveMapValueTypeForStringTarget(*accessReceiver, mapValueType)) {
              return normalizeBindingTypeName(mapValueType) == "string";
            }
            if (resolveStringTarget(*accessReceiver)) {
              return false;
            }
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
          if ((resolveArgsPackAccessTarget(target.args.front(), elemType) ||
               resolveArrayTarget(target.args.front(), elemType)) &&
              elemType == "string") {
            return true;
          }
          if (resolveMapTarget(target.args.front())) {
            std::string keyType;
            std::string valueType;
            if (target.args.front().kind == Expr::Kind::Name) {
              if (const BindingInfo *paramBinding = findParamBinding(params, target.args.front().name)) {
                if (extractMapKeyValueTypes(*paramBinding, keyType, valueType) &&
                    normalizeBindingTypeName(valueType) == "string") {
                  return true;
                }
              } else {
                auto it = locals.find(target.args.front().name);
                if (it != locals.end() && extractMapKeyValueTypes(it->second, keyType, valueType) &&
                    normalizeBindingTypeName(valueType) == "string") {
                  return true;
                }
              }
            } else if (target.args.front().kind == Expr::Kind::Call) {
              std::string collectionTypePath;
              if (resolveCallCollectionTypePath(target.args.front(), collectionTypePath) &&
                  collectionTypePath == "/map") {
                std::vector<std::string> args;
                if (resolveCallCollectionTemplateArgs(target.args.front(), "map", args) && args.size() == 2 &&
                    normalizeBindingTypeName(args[1]) == "string") {
                  return true;
                }
              }
              BindingInfo mapFieldBinding;
              if (resolveFieldBindingTarget(target.args.front(), mapFieldBinding) &&
                  extractMapKeyValueTypes(mapFieldBinding, keyType, valueType) &&
                  normalizeBindingTypeName(valueType) == "string") {
                return true;
              }
            }
          }
        }
      }
      return false;
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
    auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,
                                          std::string &keyTypeOut,
                                          std::string &valueTypeOut) -> bool {
      return extractMapKeyValueTypes(binding, keyTypeOut, valueTypeOut) ||
             extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
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
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return extractExperimentalMapFieldTypes(fieldBinding, keyTypeOut, valueTypeOut);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      if (isDirectMapConstructorCall(target)) {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
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
    resolveMapTarget = [&](const Expr &target) -> bool {
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
          std::string keyType;
          std::string valueType;
          return extractAnyMapKeyValueTypes(*paramBinding, keyType, valueType);
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        std::string keyType;
        std::string valueType;
        return extractAnyMapKeyValueTypes(it->second, keyType, valueType);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        std::string keyType;
        std::string valueType;
        return extractAnyMapKeyValueTypes(fieldBinding, keyType, valueType);
      }
      if (target.kind == Expr::Kind::Call) {
        std::string accessName;
        if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
          if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
            std::string elemType;
            std::string keyType;
            std::string valueType;
            if (resolveArgsPackAccessTarget(*accessReceiver, elemType) &&
                extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
              return true;
            }
          }
        }
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (resolveDereferencedIndexedArgsPackElementType(target, elemType) &&
            extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
          return true;
        }
        if (resolveWrappedIndexedArgsPackElementType(target, elemType) &&
            extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
          return true;
        }
        if (isDirectMapConstructorCall(target)) {
          return true;
        }
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/map") {
          return true;
        }
        auto defIt = defMap_.find(resolveCalleePath(target));
        if (defIt == defMap_.end() || !defIt->second) {
          return false;
        }
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
            extractAnyMapKeyValueTypes(inferredReturn, keyType, valueType)) {
          return true;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          return returnsMapCollectionType(transform.templateArgs.front());
        }
        return false;
      }
      return false;
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
    auto definitionPathContains = [&](std::string_view needle) {
      return currentDefinitionPath_.find(std::string(needle)) != std::string::npos;
    };
    auto mapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {
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
    auto preferredBareMapHelperTarget = [&](std::string_view helperName) {
      const std::string canonical = "/std/collections/map/" + std::string(helperName);
      if (defMap_.find(canonical) != defMap_.end() || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      const std::string alias = "/map/" + std::string(helperName);
      if (defMap_.find(alias) != defMap_.end()) {
        return alias;
      }
      return canonical;
    };
    auto preferredBareVectorHelperTarget = [&](std::string_view helperName) {
      const std::string canonical = "/std/collections/vector/" + std::string(helperName);
      if (defMap_.find(canonical) != defMap_.end() || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      const std::string alias = "/vector/" + std::string(helperName);
      if (defMap_.find(alias) != defMap_.end()) {
        return alias;
      }
      return canonical;
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
    auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {
      if (helperName == "count") {
        return definitionPathContains("/mapCount");
      }
      if (helperName == "contains") {
        return definitionPathContains("/mapContains") ||
               definitionPathContains("/mapTryAt");
      }
      if (helperName == "tryAt") {
        return definitionPathContains("/mapTryAt");
      }
      if (helperName == "at" || helperName == "at_unsafe") {
        return definitionPathContains("/mapAt") ||
               definitionPathContains("/mapAtUnsafe") ||
               definitionPathContains("/mapTryAt") ||
               definitionPathContains("/mapAtRef");
      }
      return false;
    };
    (void)shouldBuiltinValidateCurrentMapWrapperHelper;
    auto tryRewriteBareMapHelperCall = [&](const Expr &candidate,
                                           std::string_view helperName,
                                           Expr &rewrittenOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      if (candidate.name == "/std/collections/map/" + std::string(helperName) ||
          candidate.name == "/map/" + std::string(helperName)) {
        return false;
      }
      const size_t receiverIndex = mapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size() || !resolveMapTarget(candidate.args[receiverIndex])) {
        return false;
      }
      rewrittenOut = candidate;
      std::string keyType;
      std::string valueType;
      if (resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
        rewrittenOut.name = preferredExperimentalMapHelperTarget(helperName);
      } else {
        rewrittenOut.name = preferredBareMapHelperTarget(helperName);
      }
      rewrittenOut.namespacePrefix.clear();
      return true;
    };
    auto tryRewriteBareVectorHelperCall = [&](const Expr &candidate,
                                              std::string_view helperName,
                                              Expr &rewrittenOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      if (candidate.name != helperName || candidate.name.find('/') != std::string::npos ||
          !candidate.namespacePrefix.empty()) {
        return false;
      }
      if (defMap_.find("/" + std::string(helperName)) != defMap_.end()) {
        return false;
      }
      const size_t receiverIndex = mapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string elemType;
      if (!resolveVectorTarget(candidate.args[receiverIndex], elemType)) {
        return false;
      }
      rewrittenOut = candidate;
      rewrittenOut.name = preferredBareVectorHelperTarget(helperName);
      rewrittenOut.namespacePrefix.clear();
      return true;
    };
    auto canonicalExperimentalMapHelperPath = [&](const std::string &resolvedPath, std::string &canonicalPathOut, std::string &helperNameOut) {
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
          canonicalPathOut = std::string(canonicalPath);
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
    auto tryRewriteCanonicalExperimentalMapHelperCall = [&](const Expr &candidate, Expr &rewrittenOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.args.empty()) {
        return false;
      }
      std::string canonicalPath;
      std::string helperName;
      Expr canonicalCandidate = candidate;
      if (candidate.isMethodCall) {
        std::string normalizedMethod = candidate.name;
        if (!normalizedMethod.empty() && normalizedMethod.front() == '/') {
          normalizedMethod.erase(normalizedMethod.begin());
        }
        if (normalizedMethod != "count" && normalizedMethod != "contains" && normalizedMethod != "tryAt" &&
            normalizedMethod != "at" && normalizedMethod != "at_unsafe") {
          return false;
        }
        helperName = normalizedMethod;
        canonicalPath = "/std/collections/map/" + normalizedMethod;
        canonicalCandidate.isMethodCall = false;
        canonicalCandidate.name = canonicalPath;
        canonicalCandidate.namespacePrefix.clear();
      } else {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (!canonicalExperimentalMapHelperPath(resolvedPath, canonicalPath, helperName)) {
          return false;
        }
      }
      const size_t receiverIndex = candidate.isMethodCall ? 0 : mapHelperReceiverIndex(canonicalCandidate);
      if (receiverIndex >= canonicalCandidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      if (!resolveExperimentalMapValueTarget(canonicalCandidate.args[receiverIndex], keyType, valueType) &&
          !resolveExperimentalMapTarget(canonicalCandidate.args[receiverIndex], keyType, valueType)) {
        return false;
      }
      rewrittenOut = canonicalCandidate;
      if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {keyType, valueType};
      }
      if (!candidate.isMethodCall) {
        rewrittenOut.name = preferredCanonicalExperimentalMapHelperTarget(helperName);
        rewrittenOut.namespacePrefix.clear();
      }
      return true;
    };
    auto explicitCanonicalExperimentalMapBorrowedHelperPath = [&](const Expr &candidate,
                                                                  std::string &resolvedPathOut) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      std::string helperName;
      if (!canonicalExperimentalMapHelperPath(resolveCalleePath(candidate), resolvedPathOut, helperName)) {
        return false;
      }
      const size_t receiverIndex = mapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
             !resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
    };
    auto hasResolvableMapHelperPath = [&](const std::string &path) {
      return defMap_.find(path) != defMap_.end() || hasImportedDefinitionPath(path);
    };
    const bool shouldBuiltinValidateBareMapCountCall = true;
    const bool shouldBuiltinValidateBareMapContainsCall = true;
    const bool shouldBuiltinValidateBareMapTryAtCall = true;
    const bool shouldBuiltinValidateBareMapAccessCall = true;
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
      if (!shouldBuiltinValidateBareMapCountCall) {
        return false;
      }
      return resolveMapTarget(candidate.args[receiverIndex]);
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
      if (defMap_.find("/map/count") != defMap_.end() || candidate.args.empty()) {
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
      return resolveMapTarget(candidate.args[receiverIndex]);
    };
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
      return defMap_.find("/" + normalized) == defMap_.end();
    };
    (void)isMapNamespacedAccessCompatibilityCall;
    auto getMapNamespacedMethodCompatibilityPath = [&](const Expr &candidate) -> std::string {
      if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
          candidate.args.empty()) {
        return "";
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      std::string helperName;
      if (normalized == "map/count") {
        helperName = "count";
      } else if (normalized == "map/at") {
        helperName = "at";
      } else if (normalized == "map/at_unsafe") {
        helperName = "at_unsafe";
      } else if (normalized == "std/collections/map/count") {
        helperName = "count";
      } else if (normalized == "std/collections/map/at") {
        helperName = "at";
      } else if (normalized == "std/collections/map/at_unsafe") {
        helperName = "at_unsafe";
      } else if (normalizedPrefix == "map" &&
                 (normalized == "count" || normalized == "at" || normalized == "at_unsafe")) {
        helperName = normalized;
      } else if (normalizedPrefix == "std/collections/map" &&
                 (normalized == "count" || normalized == "at" || normalized == "at_unsafe")) {
        helperName = normalized;
      } else {
        return "";
      }
      const std::string removedPath = "/map/" + helperName;
      if (defMap_.find(removedPath) != defMap_.end()) {
        return "";
      }
      if (!resolveMapTarget(candidate.args.front())) {
        return "";
      }
      return removedPath;
    };
    auto getDirectMapHelperCompatibilityPath = [&](const Expr &candidate) -> std::string {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
        return "";
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string helperName;
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      if (normalized == "map/count") {
        helperName = "count";
      } else if (normalized == "map/contains") {
        helperName = "contains";
      } else if (normalized == "map/tryAt") {
        helperName = "tryAt";
      } else if (normalized == "map/at") {
        helperName = "at";
      } else if (normalized == "map/at_unsafe") {
        helperName = "at_unsafe";
      } else if (normalizedPrefix == "map" &&
                 (normalized == "count" || normalized == "contains" || normalized == "tryAt" ||
                  normalized == "at" || normalized == "at_unsafe")) {
        helperName = normalized;
      } else {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/map/count") {
          helperName = "count";
        } else if (resolvedPath == "/map/contains") {
          helperName = "contains";
        } else if (resolvedPath == "/map/tryAt") {
          helperName = "tryAt";
        } else if (resolvedPath == "/map/at") {
          helperName = "at";
        } else if (resolvedPath == "/map/at_unsafe") {
          helperName = "at_unsafe";
        } else {
          return "";
        }
      }
      const std::string removedPath = "/map/" + helperName;
      if (defMap_.find(removedPath) != defMap_.end() || candidate.args.empty()) {
        return "";
      }
      if (helperName == "at" || helperName == "at_unsafe") {
        return removedPath;
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
      if (receiverIndex >= candidate.args.size() || !resolveMapTarget(candidate.args[receiverIndex])) {
        return "";
      }
      return removedPath;
    };
    auto resolveMapKeyType = [&](const Expr &target, std::string &keyTypeOut) -> bool {
      keyTypeOut.clear();
      std::string valueType;
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractAnyMapKeyValueTypes(*paramBinding, keyTypeOut, valueType);
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        return extractAnyMapKeyValueTypes(it->second, keyTypeOut, valueType);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return extractAnyMapKeyValueTypes(fieldBinding, keyTypeOut, valueType);
      }
      if (target.kind == Expr::Kind::Call) {
        std::string elemType;
        if ((resolveIndexedArgsPackElementType(target, elemType) ||
             resolveWrappedIndexedArgsPackElementType(target, elemType) ||
             resolveDereferencedIndexedArgsPackElementType(target, elemType)) &&
            extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueType)) {
          return true;
        }
        std::string collectionTypePath;
        if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
          return false;
        }
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
          keyTypeOut = args.front();
        }
        return true;
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      std::string keyType;
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractAnyMapKeyValueTypes(*paramBinding, keyType, valueTypeOut);
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        return extractAnyMapKeyValueTypes(it->second, keyType, valueTypeOut);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return extractAnyMapKeyValueTypes(fieldBinding, keyType, valueTypeOut);
      }
      if (target.kind == Expr::Kind::Call) {
        std::string elemType;
        if ((resolveIndexedArgsPackElementType(target, elemType) ||
             resolveWrappedIndexedArgsPackElementType(target, elemType) ||
             resolveDereferencedIndexedArgsPackElementType(target, elemType)) &&
            extractMapKeyValueTypesFromTypeText(elemType, keyType, valueTypeOut)) {
          return true;
        }
        std::string collectionTypePath;
        if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
          return false;
        }
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
          valueTypeOut = args[1];
        }
        return true;
      }
      return false;
    };
    auto isIndexedArgsPackMapReceiverTarget = [&](const Expr &receiverExpr) -> bool {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      return ((resolveIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveWrappedIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)) &&
              extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
    };
    auto isStringExpr = [&](const Expr &arg) -> bool {
      if (resolveStringTarget(arg)) {
        return true;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinArrayAccessName(arg, accessName) &&
            arg.args.size() == 2) {
          std::string mapValueType;
          if (resolveMapValueType(arg.args.front(), mapValueType) && normalizeBindingTypeName(mapValueType) == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto validateCollectionElementType = [&](const Expr &arg, const std::string &typeName,
                                              const std::string &errorPrefix) -> bool {
      const std::string normalizedType = normalizeBindingTypeName(typeName);
      if (normalizedType == "string") {
        if (!isStringExpr(arg)) {
          error_ = errorPrefix + typeName;
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(normalizedType);
      if (expectedKind == ReturnKind::Unknown) {
        return true;
      }
      if (isStringExpr(arg)) {
        error_ = errorPrefix + typeName;
        return false;
      }
      ReturnKind argKind = inferExprReturnKind(arg, params, locals);
      if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
        error_ = errorPrefix + typeName;
        return false;
      }
      return true;
    };
    auto isStaticHelperDefinition = [&](const Definition &def) -> bool {
      for (const auto &transform : def.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto hasDeclaredDefinitionPath = [&](const std::string &path) {
      for (const auto &def : program_.definitions) {
        if (def.fullPath == path) {
          return true;
        }
      }
      return false;
    };
    auto resolveMethodTarget =
        [&](const Expr &receiver, const std::string &methodName, std::string &resolvedOut, bool &isBuiltinOut) -> bool {
      isBuiltinOut = false;
      auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
               helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
               helperName == "remove_at" || helperName == "remove_swap";
      };
      auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
      };
      auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {
        std::string candidate = rawMethodName;
        if (!candidate.empty() && candidate.front() == '/') {
          candidate.erase(candidate.begin());
        }
        std::string normalizedPrefix = expr.namespacePrefix;
        if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
          normalizedPrefix.erase(normalizedPrefix.begin());
        }
        std::string_view helperName;
        bool isStdNamespacedVectorHelper = false;
        std::string compatibilityCollection;
        if (normalizedPrefix == "array") {
          helperName = candidate;
          compatibilityCollection = "array";
        } else if (normalizedPrefix == "vector") {
          helperName = candidate;
          compatibilityCollection = "vector";
        } else if (normalizedPrefix == "std/collections/vector") {
          helperName = candidate;
          isStdNamespacedVectorHelper = true;
          compatibilityCollection = "vector";
        } else if (normalizedPrefix == "map") {
          helperName = candidate;
          compatibilityCollection = "map";
        } else if (normalizedPrefix == "std/collections/map") {
          helperName = candidate;
          compatibilityCollection = "map";
        } else if (candidate.rfind("array/", 0) == 0) {
          helperName = std::string_view(candidate).substr(std::string_view("array/").size());
          compatibilityCollection = "array";
        } else if (candidate.rfind("vector/", 0) == 0) {
          helperName = std::string_view(candidate).substr(std::string_view("vector/").size());
          compatibilityCollection = "vector";
        } else if (candidate.rfind("std/collections/vector/", 0) == 0) {
          helperName = std::string_view(candidate).substr(std::string_view("std/collections/vector/").size());
          isStdNamespacedVectorHelper = true;
          compatibilityCollection = "vector";
        } else if (candidate.rfind("map/", 0) == 0) {
          helperName = std::string_view(candidate).substr(std::string_view("map/").size());
          compatibilityCollection = "map";
        } else if (candidate.rfind("std/collections/map/", 0) == 0) {
          helperName = std::string_view(candidate).substr(std::string_view("std/collections/map/").size());
          compatibilityCollection = "map";
        }
        if (helperName.empty()) {
          return "";
        }
        if (compatibilityCollection == "map") {
          if (!isRemovedMapCompatibilityHelper(helperName)) {
            return "";
          }
          return "/map/" + std::string(helperName);
        }
        if (!isRemovedVectorCompatibilityHelper(helperName)) {
          return "";
        }
        if (isStdNamespacedVectorHelper) {
          return "/vector/" + std::string(helperName);
        }
        return "/" + candidate;
      };
      const std::string explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName);
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
      auto isStaticBinding = [&](const Expr &bindingExpr) -> bool {
        for (const auto &transform : bindingExpr.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
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
      if (normalizedMethodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/ok";
        isBuiltinOut = true;
        return true;
      }
      if (normalizedMethodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/error";
        isBuiltinOut = true;
        return true;
      }
      if (normalizedMethodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/why";
        isBuiltinOut = true;
        return true;
      }
      if ((normalizedMethodName == "map" || normalizedMethodName == "and_then" || normalizedMethodName == "map2") &&
          receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/" + normalizedMethodName;
        isBuiltinOut = true;
        return true;
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
      std::string elemType;
      auto setCollectionMethodTarget = [&](const std::string &path) {
        if (!explicitRemovedMethodPath.empty() && path.rfind("/string/", 0) != 0) {
          resolvedOut = explicitRemovedMethodPath;
          isBuiltinOut = defMap_.count(resolvedOut) == 0;
          return true;
        }
        resolvedOut = preferVectorStdlibHelperPath(path);
        if ((resolvedOut == "/std/collections/map/count" ||
             resolvedOut == "/std/collections/map/contains" ||
             resolvedOut == "/std/collections/map/at" ||
             resolvedOut == "/std/collections/map/at_unsafe") &&
            (shouldBuiltinValidateCurrentMapWrapperHelper(
                 resolvedOut.substr(std::string("/std/collections/map/").size())) ||
             hasImportedDefinitionPath(resolvedOut))) {
          isBuiltinOut = true;
          return true;
        }
        isBuiltinOut = defMap_.count(resolvedOut) == 0 &&
                       !hasImportedDefinitionPath(resolvedOut);
        return true;
      };
      auto preferredMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
        std::string keyType;
        std::string valueType;
        if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
          const std::string canonical = "/std/collections/map/" + helperName;
          if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
            return canonical;
          }
          return preferredCanonicalExperimentalMapHelperTarget(helperName);
        }
        const std::string canonical = "/std/collections/map/" + helperName;
        const std::string alias = "/map/" + helperName;
        if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
          return canonical;
        }
        if (defMap_.count(alias) > 0) {
          return alias;
        }
        return canonical;
      };
      auto isFileMethodName = [&](const std::string &methodName) {
        return methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
               methodName == "read_byte" || methodName == "write_bytes" || methodName == "flush" ||
               methodName == "close";
      };
      auto preferredFileMethodTarget = [&](const std::string &methodName) {
        const std::string stdlibPath = "/File/" + methodName;
        if (defMap_.count(stdlibPath) > 0 || hasImportedDefinitionPath(stdlibPath)) {
          return stdlibPath;
        }
        return std::string("/file/") + methodName;
      };
      auto resolveArgsPackElementMethodTarget = [&](const std::string &elementTypeText,
                                                    const Expr &receiverExpr) -> bool {
        const std::string normalizedElemType = normalizeBindingTypeName(elementTypeText);
        std::string collectionElemType = normalizedElemType;
        std::string wrappedPointeeType;
        if (extractWrappedPointeeType(normalizedElemType, wrappedPointeeType)) {
          collectionElemType = normalizeBindingTypeName(wrappedPointeeType);
        }
        if (collectionElemType == "string") {
          return setCollectionMethodTarget("/string/" + normalizedMethodName);
        }
        if (collectionElemType == "FileError" && normalizedMethodName == "why") {
          resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
          isBuiltinOut = resolvedOut == "/file_error/why";
          return true;
        }
        std::string elemBase;
        std::string elemArgText;
        if (splitTemplateTypeName(collectionElemType, elemBase, elemArgText)) {
          elemBase = normalizeBindingTypeName(elemBase);
          if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
            return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
          }
          if (elemBase == "map") {
            return setCollectionMethodTarget(preferredMapMethodTarget(receiverExpr, normalizedMethodName));
          }
          if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
            resolvedOut = preferredFileMethodTarget(normalizedMethodName);
            isBuiltinOut = resolvedOut.rfind("/file/", 0) == 0;
            return true;
          }
        }
        if (isPrimitiveBindingTypeName(normalizedElemType)) {
          resolvedOut = "/" + normalizedElemType + "/" + normalizedMethodName;
          return true;
        }
        std::string resolvedElemType = resolveStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
        if (resolvedElemType.empty()) {
          resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
        }
        if (!resolvedElemType.empty()) {
          resolvedOut = resolvedElemType + "/" + normalizedMethodName;
          return true;
        }
        return false;
      };
      auto setIndexedArgsPackMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) -> bool {
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.args.size() != 2) {
          return false;
        }
        std::string accessName;
        if (!getBuiltinArrayAccessName(receiverExpr, accessName)) {
          return false;
        }
        const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(receiverExpr);
        if (accessReceiver == nullptr) {
          return false;
        }
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (!resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
            !extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
          return false;
        }
        resolvedOut = preferredMapMethodTarget(receiverExpr, helperName);
        isBuiltinOut = true;
        return true;
      };
      auto isDirectMapConstructorReceiverCall = [&](const Expr &receiverExpr) {
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
          return false;
        }
        const std::string resolvedReceiver = resolveCalleePath(receiverExpr);
        auto matchesPath = [&](std::string_view basePath) {
          return resolvedReceiver == basePath || resolvedReceiver.rfind(std::string(basePath) + "__t", 0) == 0;
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
      if ((normalizedMethodName == "count" || normalizedMethodName == "contains" ||
           normalizedMethodName == "tryAt" || normalizedMethodName == "at" ||
           normalizedMethodName == "at_unsafe") &&
          isDirectMapConstructorReceiverCall(receiver)) {
        return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
      }
      if (normalizedMethodName == "count") {
        if (resolveArgsPackCountTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/count");
        }
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/count");
        }
        if (resolveSoaVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/soa_vector/count");
        }
        if (resolveArrayTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/count");
        }
        if (resolveStringTarget(receiver)) {
          return setCollectionMethodTarget("/string/count");
        }
        if (setIndexedArgsPackMapMethodTarget(receiver, "count")) {
          return true;
        }
        if (resolveMapTarget(receiver)) {
          return setCollectionMethodTarget(preferredMapMethodTarget(receiver, "count"));
        }
      }
      if (normalizedMethodName == "contains" || normalizedMethodName == "tryAt") {
        if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
          return true;
        }
        if (resolveMapTarget(receiver)) {
          return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
        }
      }
      if (normalizedMethodName == "capacity") {
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/capacity");
        }
      }
      if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
        if (resolveArgsPackAccessTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/" + normalizedMethodName);
        }
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/" + normalizedMethodName);
        }
        if (resolveArrayTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/" + normalizedMethodName);
        }
        if (resolveStringTarget(receiver)) {
          return setCollectionMethodTarget("/string/" + normalizedMethodName);
        }
        if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
          return true;
        }
        if (resolveMapTarget(receiver)) {
          return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
        }
      }
      if (normalizedMethodName == "get" || normalizedMethodName == "ref") {
        if (resolveSoaVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/soa_vector/" + normalizedMethodName);
        }
      }
      if (resolveSoaVectorTarget(receiver, elemType)) {
        const std::string normalizedElemType = normalizeBindingTypeName(elemType);
        std::string currentNamespace;
        if (!currentDefinitionPath_.empty()) {
          const size_t slash = currentDefinitionPath_.find_last_of('/');
          if (slash != std::string::npos && slash > 0) {
            currentNamespace = currentDefinitionPath_.substr(0, slash);
          }
        }
        const std::string lookupNamespace =
            !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
        const std::string elementStructPath = resolveStructTypePath(normalizedElemType, lookupNamespace);
        if (!elementStructPath.empty()) {
          auto structIt = defMap_.find(elementStructPath);
          if (structIt != defMap_.end() && structIt->second != nullptr) {
            for (const auto &stmt : structIt->second->statements) {
              if (!stmt.isBinding || isStaticBinding(stmt) || stmt.name != normalizedMethodName) {
                continue;
              }
              const std::string helperPath = "/soa_vector/" + normalizedMethodName;
              if (defMap_.count(helperPath) > 0) {
                resolvedOut = helperPath;
                isBuiltinOut = false;
              } else {
                resolvedOut = "/soa_vector/field_view/" + normalizedMethodName;
                isBuiltinOut = true;
              }
              return true;
            }
          }
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
        std::string accessHelperName;
        if (getBuiltinArrayAccessName(receiver, accessHelperName) && !receiver.args.empty()) {
          const std::string removedMapCompatibilityPath = getDirectMapHelperCompatibilityPath(receiver);
          if (!removedMapCompatibilityPath.empty()) {
            error_ = "unknown call target: " + removedMapCompatibilityPath;
            return false;
          }
          size_t accessReceiverIndex = receiver.isMethodCall ? 0 : 0;
          if (!receiver.isMethodCall && hasNamedArguments(receiver.argNames)) {
            bool foundValues = false;
            for (size_t i = 0; i < receiver.args.size(); ++i) {
              if (i < receiver.argNames.size() && receiver.argNames[i].has_value() &&
                  *receiver.argNames[i] == "values") {
                accessReceiverIndex = i;
                foundValues = true;
                break;
              }
            }
            if (!foundValues) {
              accessReceiverIndex = 0;
            }
          }
          if (accessReceiverIndex < receiver.args.size()) {
            const Expr &accessReceiver = receiver.args[accessReceiverIndex];
            std::string accessElemType;
            std::string accessValueType;
            if (resolveArgsPackAccessTarget(accessReceiver, accessElemType) ||
                resolveVectorTarget(accessReceiver, accessElemType) ||
                resolveArrayTarget(accessReceiver, accessElemType)) {
              const std::string normalizedElemType =
                  normalizeBindingTypeName(unwrapReferencePointerTypeText(accessElemType));
              if (normalizedElemType == "string") {
                return setCollectionMethodTarget("/string/" + normalizedMethodName);
              }
              std::string elemBase;
              std::string elemArgText;
              if (splitTemplateTypeName(normalizedElemType, elemBase, elemArgText)) {
                elemBase = normalizeBindingTypeName(elemBase);
                if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
                  return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
                }
                if (elemBase == "map") {
                  if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
                    return true;
                  }
                  return setCollectionMethodTarget(preferredMapMethodTarget(receiver, normalizedMethodName));
                }
                if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
                  resolvedOut = preferredFileMethodTarget(normalizedMethodName);
                  isBuiltinOut = resolvedOut.rfind("/file/", 0) == 0;
                  return true;
                }
              }
              if (normalizedElemType == "FileError" && normalizedMethodName == "why") {
                resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
                isBuiltinOut = resolvedOut == "/file_error/why";
                return true;
              }
              if (isPrimitiveBindingTypeName(normalizedElemType)) {
                resolvedOut = "/" + normalizedElemType + "/" + normalizedMethodName;
                return true;
              }
              std::string resolvedElemType = resolveStructTypePath(normalizedElemType, receiver.namespacePrefix);
              if (resolvedElemType.empty()) {
                resolvedElemType = resolveTypePath(normalizedElemType, receiver.namespacePrefix);
              }
              if (!resolvedElemType.empty()) {
                resolvedOut = resolvedElemType + "/" + normalizedMethodName;
                return true;
              }
            }
            if (resolveStringTarget(accessReceiver)) {
              resolvedOut = "/i32/" + normalizedMethodName;
              return true;
            }
            if (resolveMapValueType(accessReceiver, accessValueType) &&
                resolveCalleePath(receiver).rfind("/map/", 0) == 0) {
              const std::string normalizedValueType = normalizeBindingTypeName(accessValueType);
              if (isPrimitiveBindingTypeName(normalizedValueType)) {
                resolvedOut = "/" + normalizedValueType + "/" + normalizedMethodName;
                return true;
              }
              std::string resolvedValueType = resolveStructTypePath(normalizedValueType, receiver.namespacePrefix);
              if (resolvedValueType.empty()) {
                resolvedValueType = resolveTypePath(normalizedValueType, receiver.namespacePrefix);
              }
              if (!resolvedValueType.empty()) {
                resolvedOut = resolvedValueType + "/" + normalizedMethodName;
                return true;
              }
            }
          }
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
        std::string dereferencedElemType;
        if (resolveDereferencedIndexedArgsPackElementType(receiver, dereferencedElemType) &&
            resolveArgsPackElementMethodTarget(dereferencedElemType, receiver)) {
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
      }
      std::string typeName;
      std::string typeTemplateArg;
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
        std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
        if (!inferredStruct.empty()) {
          std::string normalizedStruct = normalizeBindingTypeName(inferredStruct);
          if (!normalizedStruct.empty() && normalizedStruct.front() != '/') {
            normalizedStruct.insert(normalizedStruct.begin(), '/');
          }
          if (normalizedStruct == "/map" ||
              normalizedStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
            typeName = "/map";
          } else {
            typeName = inferredStruct;
          }
        }
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
      if (typeName == "File") {
        if (normalizedMethodName == "write" || normalizedMethodName == "write_line" ||
            normalizedMethodName == "write_byte" || normalizedMethodName == "read_byte" ||
            normalizedMethodName == "write_bytes" ||
            normalizedMethodName == "flush" || normalizedMethodName == "close") {
          resolvedOut = preferredFileMethodTarget(normalizedMethodName);
          isBuiltinOut = resolvedOut.rfind("/file/", 0) == 0;
          return true;
        }
      }
      if (typeName == "FileError" && normalizedMethodName == "why") {
        resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
        isBuiltinOut = resolvedOut == "/file_error/why";
        return true;
      }
      if (typeName == "string" &&
          (normalizedMethodName == "count" || normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
        return setCollectionMethodTarget("/string/" + normalizedMethodName);
      }
      if (typeName.empty()) {
        if (receiver.kind == Expr::Kind::Call) {
          if (!validateExpr(params, locals, receiver)) {
            return false;
          }
        }
        error_ = "unknown method target for " + normalizedMethodName;
        return false;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        error_ = "unknown method target for " + normalizedMethodName;
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedType = resolveStructTypePath(typeName, receiver.namespacePrefix);
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    };
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
    (void)isTypeNamespaceMethodCall;
    auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
      if (binding.typeName == "Reference") {
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
          std::vector<std::string> args;
          if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
            return ReturnKind::Array;
          }
        }
        return returnKindForTypeName(binding.typeTemplateArg);
      }
      return returnKindForTypeName(binding.typeName);
    };
    auto isConvertibleExpr = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
          kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool ||
          kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
        return true;
      }
      if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
        return false;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() &&
            getBuiltinCollectionName(arg, collection)) {
          return false;
        }
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Bool ||
                 paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 || localKind == ReturnKind::Bool ||
                 localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
        }
      }
      return true;
    };
    (void)isConvertibleExpr;

    std::string earlyPointerBuiltin;
    if (getBuiltinPointerName(expr, earlyPointerBuiltin)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "pointer helpers do not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "pointer helpers do not accept block arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "argument count mismatch for builtin " + earlyPointerBuiltin;
        return false;
      }
      if (earlyPointerBuiltin == "location") {
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name || isEntryArgsName(target.name) ||
            (locals.count(target.name) == 0 && !isParam(params, target.name))) {
          error_ = "location requires a local binding";
          return false;
        }
      }
      if (earlyPointerBuiltin == "dereference" && !isPointerLikeExpr(expr.args.front(), params, locals)) {
        error_ = "dereference requires a pointer or reference";
        return false;
      }
      return validateExpr(params, locals, expr.args.front());
    }

    std::string resolved = resolveCalleePath(expr);
    std::string canonicalExperimentalMapHelperResolved;
    if (!expr.isMethodCall && defMap_.count(resolved) == 0 &&
        canonicalizeExperimentalMapHelperResolvedPath(resolved, canonicalExperimentalMapHelperResolved)) {
      resolved = canonicalExperimentalMapHelperResolved;
    }
    Expr rewrittenCanonicalExperimentalMapHelperCall;
    if (tryRewriteCanonicalExperimentalMapHelperCall(expr, rewrittenCanonicalExperimentalMapHelperCall)) {
      return validateExpr(params, locals, rewrittenCanonicalExperimentalMapHelperCall);
    }
    std::string borrowedCanonicalExperimentalMapHelperPath;
    if (!expr.isMethodCall &&
        explicitCanonicalExperimentalMapBorrowedHelperPath(expr, borrowedCanonicalExperimentalMapHelperPath)) {
      error_ = "unknown call target: " + borrowedCanonicalExperimentalMapHelperPath;
      return false;
    }
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    bool hasMethodReceiverIndex = false;
    size_t methodReceiverIndex = 0;
    if (hasVectorHelperCallResolution) {
      resolved = vectorHelperCallResolvedPath;
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = vectorHelperCallReceiverIndex;
    }
    const std::string methodReflectionTarget = describeMethodReflectionTarget(params, locals, expr);
    if (!methodReflectionTarget.empty()) {
      if (methodReflectionTarget == "meta.object" || methodReflectionTarget == "meta.table") {
        error_ = "runtime reflection objects/tables are unsupported: " + methodReflectionTarget;
      } else if (isReflectionMetadataQueryName(expr.name)) {
        error_ = "reflection metadata queries are compile-time only and not yet implemented: " + methodReflectionTarget;
      } else {
        error_ = "unsupported reflection metadata query: " + methodReflectionTarget;
      }
      return false;
    }
    if (defMap_.count(resolved) == 0) {
      if (isReflectionMetadataQueryPath(resolved)) {
        error_ = "reflection metadata queries are compile-time only and not yet implemented: " + resolved;
        return false;
      }
      if (isRuntimeReflectionPath(resolved)) {
        error_ = "runtime reflection objects/tables are unsupported: " + resolved;
        return false;
      }
      if (resolved.rfind("/meta/", 0) == 0) {
        const std::string queryName = resolved.substr(6);
        if (!queryName.empty() && queryName.find('/') == std::string::npos) {
          error_ = "unsupported reflection metadata query: " + resolved;
          return false;
        }
      }
    }
    if (!expr.isMethodCall && isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      error_ = "unknown call target: /array/count";
      return false;
    }
    if (!expr.isMethodCall) {
      const std::string removedVectorCompatibilityPath = getDirectVectorHelperCompatibilityPath(expr);
      if (!removedVectorCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedVectorCompatibilityPath;
        return false;
      }
    }
    if (!expr.isMethodCall) {
      const std::string removedMapCompatibilityPath = getDirectMapHelperCompatibilityPath(expr);
      if (!removedMapCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedMapCompatibilityPath;
        return false;
      }
    }
    auto isKnownCollectionTarget = [&](const Expr &targetExpr) -> bool {
      std::string elemType;
      return resolveVectorTarget(targetExpr, elemType) || resolveArrayTarget(targetExpr, elemType) ||
             resolveStringTarget(targetExpr) || resolveMapTarget(targetExpr);
    };
    auto promoteCapacityToBuiltinValidation = [&](const Expr &targetExpr,
                                                  std::string &resolvedOut,
                                                  bool &isBuiltinMethodOut,
                                                  bool requireKnownCollection) {
      if (requireKnownCollection && !isKnownCollectionTarget(targetExpr)) {
        return;
      }
      // Route unresolved capacity() calls through builtin validation so
      // non-vector targets emit deterministic vector-target diagnostics.
      resolvedOut = "/vector/capacity";
      isBuiltinMethodOut = true;
    };
    auto isNonCollectionStructCapacityTarget = [&](const std::string &resolvedPath) -> bool {
      constexpr std::string_view suffix = "/capacity";
      if (resolvedPath.size() <= suffix.size() ||
          resolvedPath.compare(resolvedPath.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
      }
      const std::string receiverPath = resolvedPath.substr(0, resolvedPath.size() - suffix.size());
      if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
        return false;
      }
      return structNames_.count(receiverPath) > 0;
    };
    auto isNonCollectionStructAccessTarget = [&](const std::string &resolvedPath) -> bool {
      constexpr std::string_view atSuffix = "/at";
      constexpr std::string_view atUnsafeSuffix = "/at_unsafe";
      std::string receiverPath;
      if (resolvedPath.size() > atSuffix.size() &&
          resolvedPath.compare(resolvedPath.size() - atSuffix.size(), atSuffix.size(), atSuffix) == 0) {
        receiverPath = resolvedPath.substr(0, resolvedPath.size() - atSuffix.size());
      } else if (resolvedPath.size() > atUnsafeSuffix.size() &&
                 resolvedPath.compare(resolvedPath.size() - atUnsafeSuffix.size(), atUnsafeSuffix.size(),
                                      atUnsafeSuffix) == 0) {
        receiverPath = resolvedPath.substr(0, resolvedPath.size() - atUnsafeSuffix.size());
      } else {
        return false;
      }
      if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
        return false;
      }
      return structNames_.count(receiverPath) > 0;
    };
    auto experimentalGfxUnavailableConstructorDiagnostic = [&](const Expr &callExpr,
                                                               const std::string &resolvedPath) -> std::string {
      if (resolvedPath != "/std/gfx/experimental/Device" || callExpr.isMethodCall ||
          callExpr.kind != Expr::Kind::Call) {
        return "";
      }
      if (!callExpr.templateArgs.empty() || hasNamedArguments(callExpr.argNames) || !callExpr.bodyArguments.empty() ||
          callExpr.hasBodyArguments || !callExpr.args.empty()) {
        return "";
      }
      return "experimental gfx entry point not implemented yet: Device()";
    };
    auto experimentalGfxUnavailableMethodDiagnostic = [&](const std::string &resolvedPath) -> std::string {
      if (resolvedPath == "/std/gfx/experimental/Device/create_pipeline") {
        return "experimental gfx entry point not implemented yet: Device.create_pipeline([vertex_type] type, ...)";
      }
      return "";
    };
    if (expr.isFieldAccess) {
      if (expr.args.size() != 1) {
        error_ = "field access requires a receiver";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "field access does not accept template arguments";
        return false;
      }
      if (hasNamedArguments(expr.argNames)) {
        error_ = "field access does not accept named arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "field access does not accept block arguments";
        return false;
      }
      std::string typeReceiverPath;
      const bool typeNamespaceReceiver =
          isTypeNamespaceFieldReceiver(params, locals, expr.args.front(), typeReceiverPath);
      if (!typeNamespaceReceiver && !validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(params, locals, expr.args.front(), expr.name, fieldBinding)) {
        if (error_.empty()) {
          std::string receiverStructPath;
          if (!resolveStructFieldReceiverPath(params, locals, expr.args.front(), receiverStructPath)) {
            error_ = "field access requires struct receiver";
          } else {
            error_ = "unknown field: " + expr.name;
          }
        }
        return false;
      }
      return true;
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isNamespacedCollectionHelperCall =
        getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
    const bool isNamespacedVectorHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "vector";
    const bool isStdNamespacedVectorCountCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/count", 0) == 0;
    const bool hasStdNamespacedVectorCountDefinition =
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
        isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
    const bool isStdNamespacedMapCountCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/map/count", 0) == 0;
    const bool isNamespacedMapHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "map";
    const bool isNamespacedVectorCountCall =
        !expr.isMethodCall && !isStdNamespacedVectorCountCall &&
        isNamespacedVectorHelperCall && namespacedHelper == "count" &&
        isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        defMap_.find(resolved) == defMap_.end() &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr);
    const bool isNamespacedMapCountCall =
        !expr.isMethodCall && isNamespacedMapHelperCall && namespacedHelper == "count" &&
        !isStdNamespacedMapCountCall && !isMapNamespacedCountCompatibilityCall(expr) &&
        defMap_.find(resolved) == defMap_.end();
    auto isExplicitMapCountAliasCall = [&]() {
      std::string normalizedName = expr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      std::string normalizedPrefix = expr.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      return normalizedName == "map/count" || (normalizedPrefix == "map" && normalizedName == "count");
    };
    const bool isUnnamespacedMapCountFallbackCall =
        !expr.isMethodCall && isUnnamespacedMapCountBuiltinFallbackCall(expr);
    const bool isResolvedMapCountCall =
        !expr.isMethodCall && resolved == "/map/count" &&
        !isMapNamespacedCountCompatibilityCall(expr) &&
        !isUnnamespacedMapCountFallbackCall;
    const bool isStdNamespacedVectorCapacityCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/capacity", 0) == 0;
    const bool hasStdNamespacedVectorCapacityDefinition =
        hasImportedDefinitionPath("/std/collections/vector/capacity");
    const bool isNamespacedVectorCapacityCall =
        !expr.isMethodCall && !isStdNamespacedVectorCapacityCall &&
        isNamespacedVectorHelperCall && namespacedHelper == "capacity" &&
        isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        defMap_.find(resolved) == defMap_.end();
    const bool shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        isStdNamespacedVectorCapacityCall && hasStdNamespacedVectorCapacityDefinition;
    const bool isDirectStdNamespacedVectorCountWrapperMapTarget =
        !expr.isMethodCall && isStdNamespacedVectorCountCall && expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Call && resolveMapTarget(expr.args.front());
    const bool hasStdNamespacedVectorCountAliasDefinition =
        defMap_.find("/std/collections/vector/count") != defMap_.end() ||
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool prefersCanonicalVectorCountAliasDefinition =
        !expr.isMethodCall && resolved == "/vector/count" && defMap_.find(resolved) == defMap_.end() &&
        defMap_.find("/std/collections/vector/count") != defMap_.end();
    if (prefersCanonicalVectorCountAliasDefinition) {
      resolved = "/std/collections/vector/count";
    }
    if (expr.isMethodCall) {
      const std::string removedMapMethodPath = getMapNamespacedMethodCompatibilityPath(expr);
      if (!removedMapMethodPath.empty()) {
        error_ = "unknown method: " + removedMapMethodPath;
        return false;
      }
      if (!hasVectorHelperCallResolution) {
        if (expr.args.empty()) {
          error_ = "method call missing receiver";
          return false;
        }
        usedMethodTarget = true;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
        bool isBuiltinMethod = false;
        const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
        auto resolveBodyArgumentMethodTarget = [&](std::string &resolvedOut) -> bool {
          const Expr &receiver = expr.args.front();
          std::string methodName = expr.name;
          const bool isExplicitMethodPath = !methodName.empty() && methodName.front() == '/';
          if (!methodName.empty() && methodName.front() == '/') {
            methodName.erase(methodName.begin());
          }
          std::string namespacedCollection;
          std::string namespacedHelper;
          if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
              !namespacedHelper.empty()) {
            methodName = namespacedHelper;
          }
          auto preferredMapBodyArgumentTarget = [&](const std::string &helperName) {
            const std::string canonical = "/std/collections/map/" + helperName;
            const std::string alias = "/map/" + helperName;
            if (defMap_.count(canonical) > 0) {
              return canonical;
            }
            if (defMap_.count(alias) > 0) {
              return alias;
            }
            return canonical;
          };
          if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
            const std::string resolvedType = resolveCalleePath(receiver);
            if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
              resolvedOut = resolvedType + "/" + methodName;
              return true;
            }
            if (!isExplicitMethodPath &&
                (methodName == "count" || methodName == "at" || methodName == "at_unsafe") &&
                resolveMapTarget(receiver)) {
              resolvedOut = preferredMapBodyArgumentTarget(methodName);
              return true;
            }
          }
          std::string typeName;
          if (receiver.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
              typeName = paramBinding->typeName;
            } else if (auto it = locals.find(receiver.name); it != locals.end()) {
              typeName = it->second.typeName;
            }
          }
          if (typeName.empty()) {
            typeName = inferPointerLikeCallReturnType(receiver, params, locals);
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
            if (isPointerExpr(receiver, params, locals)) {
              typeName = "Pointer";
            } else if (isPointerLikeExpr(receiver, params, locals)) {
              typeName = "Reference";
            }
          }
          if (typeName == "Pointer" || typeName == "Reference") {
            if (!isExplicitMethodPath &&
                (methodName == "count" || methodName == "at" || methodName == "at_unsafe") &&
                resolveMapTarget(receiver)) {
              resolvedOut = preferredMapBodyArgumentTarget(methodName);
              return true;
            }
            resolvedOut = "/" + typeName + "/" + methodName;
            return true;
          }
          if (typeName.empty()) {
            return false;
          }
          if (isPrimitiveBindingTypeName(typeName)) {
            resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + methodName;
            return true;
          }
          std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
          if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
            auto importIt = importAliases_.find(typeName);
            if (importIt != importAliases_.end()) {
              resolvedType = importIt->second;
            }
          }
          if (resolvedType.empty()) {
            return false;
          }
          resolvedOut = resolvedType + "/" + methodName;
          return true;
        };
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
                                 isBuiltinMethod)) {
          if (hasBlockArgs &&
              resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
            error_.clear();
            isBuiltinMethod = false;
          } else {
            if (!hasBlockArgs || !resolveBodyArgumentMethodTarget(resolved)) {
              return false;
            }
            error_.clear();
            isBuiltinMethod = false;
          }
        } else if (hasBlockArgs) {
          std::string bodyArgumentResolved;
          if (resolveBodyArgumentMethodTarget(bodyArgumentResolved)) {
            resolved = bodyArgumentResolved;
            isBuiltinMethod = false;
          }
        }
        bool keepBuiltinIndexedArgsPackMapMethod = false;
        keepBuiltinIndexedArgsPackMapMethod = resolveMapTarget(expr.args.front());
        if (expr.args.front().kind == Expr::Kind::Call) {
          std::string accessName;
          if (getBuiltinArrayAccessName(expr.args.front(), accessName) && expr.args.front().args.size() == 2) {
            if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(expr.args.front())) {
              std::string elemType;
              std::string keyType;
              std::string valueType;
              keepBuiltinIndexedArgsPackMapMethod =
                  keepBuiltinIndexedArgsPackMapMethod ||
                  (resolveArgsPackAccessTarget(*accessReceiver, elemType) &&
                   extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
            }
          }
        }
        if (((resolved == "/std/collections/map/count" &&
              !hasDeclaredDefinitionPath("/map/count") &&
              !hasImportedDefinitionPath("/std/collections/map/count")) ||
             (resolved == "/std/collections/map/contains" &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains")) ||
             (resolved == "/std/collections/map/tryAt" &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
             (resolved == "/std/collections/map/at" &&
              !hasDeclaredDefinitionPath("/map/at") &&
              !hasImportedDefinitionPath("/std/collections/map/at")) ||
             (resolved == "/std/collections/map/at_unsafe" &&
              !hasDeclaredDefinitionPath("/map/at_unsafe") &&
              !hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) &&
            !hasDeclaredDefinitionPath(resolved) &&
            !keepBuiltinIndexedArgsPackMapMethod) {
          isBuiltinMethod = false;
        }
        const bool isCanonicalStdVectorMutatorMethodWithoutImportedHelper =
            (resolved == "/std/collections/vector/push" ||
             resolved == "/std/collections/vector/pop" ||
             resolved == "/std/collections/vector/reserve" ||
             resolved == "/std/collections/vector/clear" ||
             resolved == "/std/collections/vector/remove_at" ||
             resolved == "/std/collections/vector/remove_swap") &&
            !hasDeclaredDefinitionPath(resolved) &&
            !hasImportedDefinitionPath(resolved);
        if (isCanonicalStdVectorMutatorMethodWithoutImportedHelper) {
          error_ = "unknown method: " + resolved;
          return false;
        }
        if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
            isVectorBuiltinName(expr, "capacity") && !isStdNamespacedVectorCapacityCall) {
          promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod, true);
        }
        if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
            !hasImportedDefinitionPath(resolved) && !hasBlockArgs) {
          if (const std::string diagnostic = experimentalGfxUnavailableMethodDiagnostic(resolved);
              !diagnostic.empty()) {
            error_ = diagnostic;
          } else {
            error_ = "unknown method: " + resolved;
          }
          return false;
        }
        resolvedMethod = isBuiltinMethod;
      } else {
        resolvedMethod = false;
      }
    }
    ExprCollectionCountCapacityDispatchContext countCapacityContext;
    countCapacityContext.isNamespacedVectorHelperCall = isNamespacedVectorHelperCall;
    countCapacityContext.namespacedHelper = namespacedHelper;
    countCapacityContext.isStdNamespacedVectorCountCall = isStdNamespacedVectorCountCall;
    countCapacityContext.shouldBuiltinValidateStdNamespacedVectorCountCall =
        shouldBuiltinValidateStdNamespacedVectorCountCall;
    countCapacityContext.isStdNamespacedMapCountCall = isStdNamespacedMapCountCall;
    countCapacityContext.isNamespacedVectorCountCall = isNamespacedVectorCountCall;
    countCapacityContext.isNamespacedMapCountCall = isNamespacedMapCountCall;
    countCapacityContext.isUnnamespacedMapCountFallbackCall = isUnnamespacedMapCountFallbackCall;
    countCapacityContext.isResolvedMapCountCall = isResolvedMapCountCall;
    countCapacityContext.prefersCanonicalVectorCountAliasDefinition =
        prefersCanonicalVectorCountAliasDefinition;
    countCapacityContext.isStdNamespacedVectorCapacityCall = isStdNamespacedVectorCapacityCall;
    countCapacityContext.shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        shouldBuiltinValidateStdNamespacedVectorCapacityCall;
    countCapacityContext.isNamespacedVectorCapacityCall = isNamespacedVectorCapacityCall;
    countCapacityContext.isDirectStdNamespacedVectorCountWrapperMapTarget =
        isDirectStdNamespacedVectorCountWrapperMapTarget;
    countCapacityContext.hasStdNamespacedVectorCountAliasDefinition =
        hasStdNamespacedVectorCountAliasDefinition;
    countCapacityContext.shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateBareMapCountCall;
    countCapacityContext.resolveMapTarget = resolveMapTarget;
    countCapacityContext.isArrayNamespacedVectorCountCompatibilityCall =
        isArrayNamespacedVectorCountCompatibilityCall;
    countCapacityContext.tryRewriteBareVectorHelperCall =
        [&](const std::string &helperName, Expr &rewrittenOut) {
          return tryRewriteBareVectorHelperCall(expr, helperName, rewrittenOut);
        };
    countCapacityContext.promoteCapacityToBuiltinValidation =
        [&](const Expr &targetExpr, std::string &resolvedOut, bool &isBuiltinMethodOut,
            bool requireKnownCollection) {
          promoteCapacityToBuiltinValidation(targetExpr, resolvedOut, isBuiltinMethodOut,
                                             requireKnownCollection);
        };
    countCapacityContext.isNonCollectionStructCapacityTarget =
        isNonCollectionStructCapacityTarget;
    bool handledCountCapacity = false;
    if (!resolveExprCollectionCountCapacityTarget(params, locals, expr, countCapacityContext,
                                                  handledCountCapacity, resolved, resolvedMethod,
                                                  usedMethodTarget, hasMethodReceiverIndex,
                                                  methodReceiverIndex)) {
      return false;
    }
    (void)handledCountCapacity;
    Expr rewrittenVectorHelperCall;
    if (tryRewriteBareVectorHelperCall(expr, "at", rewrittenVectorHelperCall) ||
        tryRewriteBareVectorHelperCall(expr, "at_unsafe", rewrittenVectorHelperCall)) {
      return validateExpr(params, locals, rewrittenVectorHelperCall);
    }
    ExprCollectionAccessDispatchContext collectionAccessContext;
    collectionAccessContext.isNamespacedVectorHelperCall = isNamespacedVectorHelperCall;
    collectionAccessContext.isNamespacedMapHelperCall = isNamespacedMapHelperCall;
    collectionAccessContext.namespacedHelper = namespacedHelper;
    collectionAccessContext.shouldBuiltinValidateBareMapContainsCall =
        shouldBuiltinValidateBareMapContainsCall;
    collectionAccessContext.shouldBuiltinValidateBareMapAccessCall =
        shouldBuiltinValidateBareMapAccessCall;
    collectionAccessContext.resolveArrayTarget = resolveArrayTarget;
    collectionAccessContext.resolveVectorTarget = resolveVectorTarget;
    collectionAccessContext.resolveSoaVectorTarget = resolveSoaVectorTarget;
    collectionAccessContext.resolveStringTarget = resolveStringTarget;
    collectionAccessContext.resolveMapTarget = resolveMapTarget;
    collectionAccessContext.hasResolvableMapHelperPath = hasResolvableMapHelperPath;
    collectionAccessContext.isIndexedArgsPackMapReceiverTarget =
        isIndexedArgsPackMapReceiverTarget;
    bool handledCollectionAccess = false;
    if (!resolveExprCollectionAccessTarget(params, locals, expr, collectionAccessContext,
                                           handledCollectionAccess, resolved, resolvedMethod,
                                           usedMethodTarget, hasMethodReceiverIndex,
                                           methodReceiverIndex)) {
      return false;
    }
    (void)handledCollectionAccess;
    if (usedMethodTarget && !resolvedMethod) {
      auto defIt = defMap_.find(resolved);
      if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second)) {
        error_ = "static helper does not accept method-call syntax: " + resolved;
        return false;
      }
    }
    if (!validateExprBodyArguments(params, locals, expr, resolved, resolvedMethod, enclosingStatements,
                                   statementIndex)) {
      return false;
    }
    std::string gpuBuiltin;
    if (getBuiltinGpuName(expr, gpuBuiltin)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "gpu builtins do not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "gpu builtins do not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "gpu builtins do not accept arguments";
        return false;
      }
      if (!currentDefinitionIsCompute_) {
        error_ = "gpu builtins require a compute definition";
        return false;
      }
      return true;
    }
    PathSpaceBuiltin pathSpaceBuiltin;
    if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) && defMap_.find(resolved) == defMap_.end()) {
      error_ = pathSpaceBuiltin.name + " is statement-only";
      return false;
    }
    if (!resolvedMethod && resolved == "/file_error/why" && defMap_.find(resolved) == defMap_.end()) {
      resolvedMethod = true;
    }

    if (hasNamedArguments(expr.argNames)) {
      auto resolveVectorMutatorName = [&](const std::string &name, std::string &helperOut) -> bool {
        std::string normalized = name;
        if (!normalized.empty() && normalized.front() == '/') {
          normalized.erase(normalized.begin());
        }
        if (normalized.rfind("vector/", 0) == 0) {
          normalized = normalized.substr(std::string("vector/").size());
        } else if (normalized.rfind("array/", 0) == 0) {
          normalized = normalized.substr(std::string("array/").size());
        } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
          normalized = normalized.substr(std::string("std/collections/vector/").size());
        }
        if (normalized == "push" || normalized == "pop" || normalized == "reserve" || normalized == "clear" ||
            normalized == "remove_at" || normalized == "remove_swap") {
          helperOut = normalized;
          return true;
        }
        return false;
      };
      std::string vectorHelperName;
      if (resolveVectorMutatorName(expr.name, vectorHelperName) &&
          (resolvedMethod || defMap_.find(resolved) == defMap_.end())) {
        error_ = vectorHelperName + " is only supported as a statement";
        return false;
      }
    }

    if (!validateNamedArguments(expr.args, expr.argNames, resolved, error_)) {
      return false;
    }
    std::function<bool(const Expr &)> isUnsafeReferenceExpr;
    isUnsafeReferenceExpr = [&](const Expr &argExpr) -> bool {
        if (argExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, argExpr.name)) {
            return paramBinding->typeName == "Reference" && paramBinding->isUnsafeReference;
        }
        auto itLocal = locals.find(argExpr.name);
        return itLocal != locals.end() && itLocal->second.typeName == "Reference" && itLocal->second.isUnsafeReference;
        }
        if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
          return false;
        }
        auto hasUnsafeChildExpr = [&](const Expr &callExpr) -> bool {
          for (const auto &nestedArg : callExpr.args) {
            if (isUnsafeReferenceExpr(nestedArg)) {
              return true;
            }
          }
          for (const auto &bodyExpr : callExpr.bodyArguments) {
            if (isUnsafeReferenceExpr(bodyExpr)) {
              return true;
            }
          }
          return false;
        };
        if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
            isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
            isSimpleCallName(argExpr, "case")) {
          return hasUnsafeChildExpr(argExpr);
        }
        const std::string nestedResolved = resolveCalleePath(argExpr);
        if (nestedResolved.empty()) {
          return false;
        }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (isUnsafeReferenceExpr(*nestedArg)) {
          return true;
        }
      }
      return false;
    };
    std::function<bool(const Expr &, std::string &)> resolveEscapingReferenceRoot;
    resolveEscapingReferenceRoot = [&](const Expr &argExpr, std::string &rootOut) -> bool {
      rootOut.clear();
      if (argExpr.kind == Expr::Kind::Name) {
        if (findParamBinding(params, argExpr.name) != nullptr) {
          return false;
        }
        auto itLocal = locals.find(argExpr.name);
        if (itLocal == locals.end() || itLocal->second.typeName != "Reference") {
          return false;
        }
        std::string sourceRoot = itLocal->second.referenceRoot.empty() ? argExpr.name : itLocal->second.referenceRoot;
        if (const BindingInfo *rootParam = findParamBinding(params, sourceRoot)) {
          if (rootParam->typeName == "Reference") {
            return false;
          }
        }
        rootOut = sourceRoot;
        return true;
      }
      if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
        return false;
      }
      auto resolveChildRoot = [&](const Expr &callExpr) -> bool {
        for (const auto &nestedArg : callExpr.args) {
          if (resolveEscapingReferenceRoot(nestedArg, rootOut)) {
            return true;
          }
        }
        for (const auto &bodyExpr : callExpr.bodyArguments) {
          if (resolveEscapingReferenceRoot(bodyExpr, rootOut)) {
            return true;
          }
        }
        return false;
      };
      if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
          isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
          isSimpleCallName(argExpr, "case")) {
        return resolveChildRoot(argExpr);
      }
      const std::string nestedResolved = resolveCalleePath(argExpr);
      if (nestedResolved.empty()) {
        return false;
      }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (resolveEscapingReferenceRoot(*nestedArg, rootOut)) {
          return true;
        }
      }
      return false;
    };
    auto reportReferenceAssignmentEscape = [&](const std::string &sinkName, const Expr &rhsExpr) -> bool {
      std::string sourceRoot;
      if (!resolveEscapingReferenceRoot(rhsExpr, sourceRoot)) {
        return false;
      }
      if (sourceRoot.empty()) {
        sourceRoot = "<unknown>";
      }
      const std::string sink = sinkName.empty() ? "<unknown>" : sinkName;
      if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(rhsExpr)) {
        error_ = "unsafe reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      } else {
        error_ = "reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      }
      return true;
    };
    if (!expr.isMethodCall && resolvedMethod &&
        hasDeclaredDefinitionPath("/map/count") &&
        (!expr.templateArgs.empty() ||
         isExplicitMapCountAliasCall()) &&
        (resolved == "/map/count" || resolved == "/std/collections/map/count" ||
         resolveCalleePath(expr).rfind("/map/count__t", 0) == 0 ||
         resolveCalleePath(expr).rfind("/std/collections/map/count__t", 0) == 0)) {
      resolved = "/map/count";
      resolvedMethod = false;
    }
    auto canonicalResolvedLookupPath = [&](const std::string &path) {
      const size_t lastSlash = path.find_last_of('/');
      const size_t instantiationPos = path.find("__t", lastSlash == std::string::npos ? 0 : lastSlash + 1);
      if (instantiationPos == std::string::npos) {
        return path;
      }
      const std::string basePath = path.substr(0, instantiationPos);
      if (defMap_.find(basePath) != defMap_.end() || paramsByDef_.find(basePath) != paramsByDef_.end() ||
          hasDeclaredDefinitionPath(basePath)) {
        return basePath;
      }
      return path;
    };
    const std::string lookupResolved = canonicalResolvedLookupPath(resolved);
    const std::string diagnosticResolved =
        lookupResolved != resolved ? lookupResolved : diagnosticCallTargetPath(resolved);
    auto it = defMap_.find(lookupResolved);
    if (it == defMap_.end() && diagnosticResolved != lookupResolved) {
      it = defMap_.find(diagnosticResolved);
    }
    static const std::vector<ParameterInfo> kEmptyCalleeParams;
    auto calleeParamsIt = paramsByDef_.find(lookupResolved);
    if (calleeParamsIt == paramsByDef_.end() && diagnosticResolved != lookupResolved) {
      calleeParamsIt = paramsByDef_.find(diagnosticResolved);
    }
    const auto &calleeParams =
        calleeParamsIt != paramsByDef_.end() ? calleeParamsIt->second : kEmptyCalleeParams;
    if ((it == defMap_.end() && calleeParams.empty()) || resolvedMethod) {
      ExprTryBuiltinContext tryContext;
      tryContext.getDirectMapHelperCompatibilityPath =
          getDirectMapHelperCompatibilityPath;
      tryContext.isIndexedArgsPackMapReceiverTarget =
          isIndexedArgsPackMapReceiverTarget;
      bool handledTryBuiltin = false;
      if (!validateExprTryBuiltin(params, locals, expr, tryContext,
                                  handledTryBuiltin)) {
        return false;
      }
      if (handledTryBuiltin) {
        return true;
      }
      ExprResultFileBuiltinContext resultFileContext;
      resultFileContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          isNamedArgsPackWrappedFileBuiltinAccessCall;
      bool handledResultFileBuiltins = false;
      if (!validateExprResultFileBuiltins(params, locals, expr, resolved, resolvedMethod,
                                          resultFileContext, handledResultFileBuiltins)) {
        return false;
      }
      if (handledResultFileBuiltins) {
        return true;
      }
      ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
      namedArgumentBuiltinContext.isNamedArgsPackMethodAccessCall =
          isNamedArgsPackMethodAccessCall;
      namedArgumentBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          isNamedArgsPackWrappedFileBuiltinAccessCall;
      namedArgumentBuiltinContext.isArrayNamespacedVectorCountCompatibilityCall =
          isArrayNamespacedVectorCountCompatibilityCall;
      if (!validateExprNamedArgumentBuiltins(params, locals, expr, resolved,
                                            resolvedMethod,
                                            namedArgumentBuiltinContext)) {
        return false;
      }
      bool handledGpuBufferBuiltins = false;
      if (!validateExprGpuBufferBuiltins(params, locals, expr,
                                         handledGpuBufferBuiltins)) {
        return false;
      }
      if (handledGpuBufferBuiltins) {
        return true;
      }
      if (expr.args.size() < 4) {
        error_ = "dispatch requires kernel and three dimension arguments";
        return false;
      }
      if (expr.args.front().kind != Expr::Kind::Name) {
        error_ = "dispatch requires kernel name as first argument";
        return false;
      }
      const Expr &kernelExpr = expr.args.front();
      const std::string kernelPath = resolveCalleePath(kernelExpr);
      auto defIt = defMap_.find(kernelPath);
      if (defIt == defMap_.end()) {
        error_ = "unknown kernel: " + kernelPath;
        return false;
      }
      bool isCompute = false;
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name == "compute") {
          isCompute = true;
          break;
        }
      }
      if (!isCompute) {
        error_ = "dispatch requires compute definition: " + kernelPath;
        return false;
      }
      for (size_t i = 1; i <= 3; ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
        ReturnKind dimKind = inferExprReturnKind(expr.args[i], params, locals);
        if (!isIntegerKind(dimKind)) {
          error_ = "dispatch dimensions require integer expressions";
          return false;
        }
      }
      const auto &kernelParams = paramsByDef_[kernelPath];
      size_t trailingArgsPackIndex = kernelParams.size();
      const bool hasTrailingArgsPack =
          findTrailingArgsPackParameter(kernelParams, trailingArgsPackIndex, nullptr);
      const size_t minDispatchArgs = (hasTrailingArgsPack ? trailingArgsPackIndex : kernelParams.size()) + 4;
      if ((!hasTrailingArgsPack && expr.args.size() != minDispatchArgs) ||
          (hasTrailingArgsPack && expr.args.size() < minDispatchArgs)) {
        error_ = "dispatch argument count mismatch for " + kernelPath;
        return false;
      }
      for (size_t i = 4; i < expr.args.size(); ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer")) {
      if (currentDefinitionIsCompute_) {
        error_ = "buffer is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "buffer requires gpu_dispatch effect";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "buffer requires exactly one template argument";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "buffer requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      ReturnKind countKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (!isIntegerKind(countKind)) {
        error_ = "buffer size requires integer expression";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(expr.templateArgs.front());
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "upload")) {
      if (currentDefinitionIsCompute_) {
        error_ = "upload is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "upload requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "upload does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "upload requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "upload does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveArrayElemType(expr.args.front(), elemType)) {
        error_ = "upload requires array input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "upload requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "readback")) {
      if (currentDefinitionIsCompute_) {
        error_ = "readback is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "readback requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "readback does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "readback requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "readback does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args.front(), elemType)) {
        error_ = "readback requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "readback requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_load")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_load requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_load does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error_ = "buffer_load requires buffer and index arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_load does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_load requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_load requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_load requires integer index";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_store requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_store does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "buffer_store requires buffer, index, and value arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_store does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1]) ||
          !validateExpr(params, locals, expr.args[2])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_store requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_store requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_store requires integer index";
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(expr.args[2], params, locals);
      if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
        error_ = "buffer_store value type mismatch";
        return false;
      }
      return true;
    }
      if (expr.isMethodCall && resolved == "/std/collections/vector/count" &&
          !hasDeclaredDefinitionPath(resolved) &&
          !hasImportedDefinitionPath(resolved) &&
          expr.args.size() == 1) {
        std::string elemType;
        if (resolveMapTarget(expr.args.front()) || resolveArrayTarget(expr.args.front(), elemType) ||
            resolveStringTarget(expr.args.front())) {
          error_ = "unknown method: /std/collections/vector/count";
          return false;
        }
      }
      if (resolvedMethod && (resolved == "/array/count" || resolved == "/vector/count" ||
                             resolved == "/soa_vector/count" || resolved == "/string/count" ||
                             resolved == "/map/count" || resolved == "/std/collections/map/count")) {
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (resolved == "/map/count" || resolved == "/std/collections/map/count") {
          if (!resolveMapTarget(expr.args.front())) {
            if (!validateExpr(params, locals, expr.args.front())) {
              return false;
            }
            error_ = "count requires map target";
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && isVectorBuiltinName(expr, "count") &&
          !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
          (!shouldBuiltinValidateStdNamespacedVectorCountCall && !isStdNamespacedVectorCountCall) &&
          !isNamespacedMapCountCall && !isResolvedMapCountCall &&
          !isUnnamespacedMapCountBuiltinFallbackCall(expr) && it == defMap_.end()) {
        if (!shouldBuiltinValidateBareMapCountCall) {
          Expr rewrittenMapHelperCall;
          if (tryRewriteBareMapHelperCall(expr, "count", rewrittenMapHelperCall)) {
            return validateExpr(params, locals, rewrittenMapHelperCall);
          }
        }
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/vector/capacity") {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && isVectorBuiltinName(expr, "capacity") &&
          (!shouldBuiltinValidateStdNamespacedVectorCapacityCall && !isStdNamespacedVectorCapacityCall) &&
          it == defMap_.end()) {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      ExprMapSoaBuiltinContext mapSoaBuiltinContext;
      mapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =
          shouldBuiltinValidateBareMapContainsCall;
      mapSoaBuiltinContext.resolveMapKeyType = resolveMapKeyType;
      mapSoaBuiltinContext.resolveVectorTarget = resolveVectorTarget;
      mapSoaBuiltinContext.resolveSoaVectorTarget = resolveSoaVectorTarget;
      mapSoaBuiltinContext.resolveStringTarget = resolveStringTarget;
      mapSoaBuiltinContext.isNamedArgsPackMethodAccessCall =
          isNamedArgsPackMethodAccessCall;
      mapSoaBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          isNamedArgsPackWrappedFileBuiltinAccessCall;
      bool handledMapSoaBuiltins = false;
      if (!validateExprMapSoaBuiltins(params, locals, expr, resolved,
                                      resolvedMethod, mapSoaBuiltinContext,
                                      handledMapSoaBuiltins)) {
        return false;
      }
      if (handledMapSoaBuiltins) {
        return true;
      }
      std::string builtinName;
      bool handledNumericBuiltin = false;
      if (!validateNumericBuiltinExpr(params, locals, expr, handledNumericBuiltin)) {
        return false;
      }
      if (handledNumericBuiltin) {
        return true;
      }
      const bool hasBuiltinAccessSpelling =
          !expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName);
      const bool isStdNamespacedVectorAccessCall =
          hasBuiltinAccessSpelling && !expr.isMethodCall &&
          resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
      const bool hasStdNamespacedVectorAccessDefinition =
          isStdNamespacedVectorAccessCall &&
          hasImportedDefinitionPath(resolveCalleePath(expr));
      const bool shouldAllowStdAccessCompatibilityFallback = false;
      const bool isStdNamespacedMapAccessCall =
          hasBuiltinAccessSpelling && !expr.isMethodCall &&
          (resolveCalleePath(expr).rfind("/std/collections/map/at", 0) == 0 ||
           resolveCalleePath(expr).rfind("/std/collections/map/at_unsafe", 0) == 0);
      const bool hasStdNamespacedMapAccessDefinition =
          isStdNamespacedMapAccessCall &&
          hasImportedDefinitionPath(resolveCalleePath(expr));
      ExprCollectionAccessValidationContext collectionAccessValidationContext;
      collectionAccessValidationContext.isStdNamespacedVectorAccessCall =
          isStdNamespacedVectorAccessCall;
      collectionAccessValidationContext.shouldAllowStdAccessCompatibilityFallback =
          shouldAllowStdAccessCompatibilityFallback;
      collectionAccessValidationContext.hasStdNamespacedVectorAccessDefinition =
          hasStdNamespacedVectorAccessDefinition;
      collectionAccessValidationContext.isStdNamespacedMapAccessCall =
          isStdNamespacedMapAccessCall;
      collectionAccessValidationContext.hasStdNamespacedMapAccessDefinition =
          hasStdNamespacedMapAccessDefinition;
      collectionAccessValidationContext.shouldBuiltinValidateBareMapAccessCall =
          shouldBuiltinValidateBareMapAccessCall;
      collectionAccessValidationContext.resolveArgsPackAccessTarget =
          resolveArgsPackAccessTarget;
      collectionAccessValidationContext.resolveArrayTarget = resolveArrayTarget;
      collectionAccessValidationContext.resolveStringTarget = resolveStringTarget;
      collectionAccessValidationContext.resolveMapKeyType = resolveMapKeyType;
      collectionAccessValidationContext.isIndexedArgsPackMapReceiverTarget =
          isIndexedArgsPackMapReceiverTarget;
      collectionAccessValidationContext.isNamedArgsPackMethodAccessCall =
          isNamedArgsPackMethodAccessCall;
      collectionAccessValidationContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          isNamedArgsPackWrappedFileBuiltinAccessCall;
      collectionAccessValidationContext.isNonCollectionStructAccessTarget =
          isNonCollectionStructAccessTarget;
      collectionAccessValidationContext.tryRewriteBareMapHelperCall =
          [&](const Expr &candidate, const std::string &helperName,
              Expr &rewrittenOut) {
            return tryRewriteBareMapHelperCall(candidate, helperName, rewrittenOut);
          };
      bool handledCollectionAccessFallbacks = false;
      if (!validateExprCollectionAccessFallbacks(
              params, locals, expr, resolved, resolvedMethod,
              collectionAccessValidationContext,
              handledCollectionAccessFallbacks)) {
        return false;
      }
      if (handledCollectionAccessFallbacks) {
        return true;
      }
      bool handledScalarPointerMemoryBuiltins = false;
      if (!validateExprScalarPointerMemoryBuiltins(
              params, locals, expr, handledScalarPointerMemoryBuiltins)) {
        return false;
      }
      if (handledScalarPointerMemoryBuiltins) {
        return true;
      }
      bool handledCollectionLiteralBuiltins = false;
      if (!validateExprCollectionLiteralBuiltins(
              params, locals, expr, handledCollectionLiteralBuiltins)) {
        return false;
      }
      if (handledCollectionLiteralBuiltins) {
        return true;
      }
      if (resolveCalleePath(expr).rfind("/std/collections/map/count__t", 0) == 0) {
        if (!expr.templateArgs.empty()) {
          error_ = "template arguments are only supported on templated definitions: /std/collections/map/count";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for /std/collections/map/count";
          return false;
        }
        if (!resolveMapTarget(expr.args.front())) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          error_ = "count requires map target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front()) ||
            !validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        if (inferExprReturnKind(expr.args[1], params, locals) != ReturnKind::Bool) {
          error_ = "argument type mismatch for /std/collections/map/count";
          return false;
        }
        return true;
      }
      auto hasActiveBorrowForBinding = [&](const std::string &name,
                                           const std::string &ignoreBorrowName = std::string()) -> bool {
        if (currentDefinitionIsUnsafe_) {
          return false;
        }
        auto referenceRootForBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
          if (binding.typeName != "Reference") {
            return "";
          }
          if (!binding.referenceRoot.empty()) {
            return binding.referenceRoot;
          }
          return bindingName;
        };
        auto hasBorrowFrom = [&](const std::string &borrowName, const BindingInfo &binding) -> bool {
          if (borrowName == name) {
            return false;
          }
          if (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) {
            return false;
          }
          if (endedReferenceBorrows_.count(borrowName) > 0) {
            return false;
          }
          const std::string root = referenceRootForBinding(borrowName, binding);
          return !root.empty() && root == name;
        };
        for (const auto &param : params) {
          if (hasBorrowFrom(param.name, param.binding)) {
            return true;
          }
        }
        for (const auto &entry : locals) {
          if (hasBorrowFrom(entry.first, entry.second)) {
            return true;
          }
        }
        return false;
      };
      auto formatBorrowedBindingError = [&](const std::string &borrowRoot, const std::string &sinkName) {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        error_ = "borrowed binding: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + sink + ")";
      };
      auto findNamedBinding = [&](const std::string &name) -> const BindingInfo * {
        if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
          return paramBinding;
        }
        auto itBinding = locals.find(name);
        if (itBinding != locals.end()) {
          return &itBinding->second;
        }
        return nullptr;
      };
      auto resolveReferenceEscapeSink = [&](const std::string &targetName, std::string &sinkOut) -> bool {
        sinkOut.clear();
        if (const BindingInfo *targetParam = findParamBinding(params, targetName)) {
          if (targetParam->typeName == "Reference") {
            sinkOut = targetName;
            return true;
          }
          return false;
        }
        auto targetIt = locals.find(targetName);
        if (targetIt == locals.end() || targetIt->second.typeName != "Reference" ||
            targetIt->second.referenceRoot.empty()) {
          return false;
        }
        if (const BindingInfo *rootParam = findParamBinding(params, targetIt->second.referenceRoot)) {
          if (rootParam->typeName == "Reference") {
            sinkOut = targetIt->second.referenceRoot;
            return true;
          }
        }
        return false;
      };
      std::function<bool(const Expr &, std::string &)> resolveLocationRootBindingName;
      resolveLocationRootBindingName = [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          if (pointerExpr.args.front().kind != Expr::Kind::Name) {
            return false;
          }
          rootNameOut = pointerExpr.args.front().name;
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveLocationRootBindingName(pointerExpr.args.front(), rootNameOut);
      };
      std::function<bool(const Expr &, std::string &, std::string &)> resolveMutablePointerWriteTarget;
      resolveMutablePointerWriteTarget =
          [&](const Expr &pointerExpr, std::string &borrowRootOut, std::string &ignoreBorrowNameOut) -> bool {
        if (pointerExpr.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, pointerExpr.name)) {
            return false;
          }
          const BindingInfo *pointerBinding = findNamedBinding(pointerExpr.name);
          if (!pointerBinding || !isPointerLikeExpr(pointerExpr, params, locals)) {
            return false;
          }
          ignoreBorrowNameOut = pointerExpr.name;
          if (!pointerBinding->referenceRoot.empty()) {
            borrowRootOut = pointerBinding->referenceRoot;
          }
          return true;
        }
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          const Expr &locationTarget = pointerExpr.args.front();
          if (locationTarget.kind != Expr::Kind::Name || !isMutableBinding(params, locals, locationTarget.name)) {
            return false;
          }
          const BindingInfo *targetBinding = findNamedBinding(locationTarget.name);
          if (targetBinding == nullptr) {
            return false;
          }
          if (targetBinding->typeName == "Reference") {
            ignoreBorrowNameOut = locationTarget.name;
            if (!targetBinding->referenceRoot.empty()) {
              borrowRootOut = targetBinding->referenceRoot;
            } else {
              borrowRootOut = locationTarget.name;
            }
          } else {
            borrowRootOut = locationTarget.name;
          }
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveMutablePointerWriteTarget(pointerExpr.args.front(), borrowRootOut, ignoreBorrowNameOut);
      };
      if (isSimpleCallName(expr, "move")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (expr.isMethodCall) {
          error_ = "move does not support method-call syntax";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "move does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "move does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "move requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          error_ = "move requires a binding name";
          return false;
        }
        const BindingInfo *binding = findParamBinding(params, target.name);
        if (!binding) {
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            binding = &it->second;
          }
        }
        if (!binding) {
          error_ = "move requires a local binding or parameter: " + target.name;
          return false;
        }
        if (binding->typeName == "Reference") {
          error_ = "move does not support Reference bindings: " + target.name;
          return false;
        }
        if (hasActiveBorrowForBinding(target.name)) {
          formatBorrowedBindingError(target.name, target.name);
          return false;
        }
        if (movedBindings_.count(target.name) > 0) {
          error_ = "use-after-move: " + target.name;
          return false;
        }
        movedBindings_.insert(target.name);
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error_ = "assign requires exactly two arguments";
          return false;
        }
        const Expr &target = expr.args.front();
        const bool targetIsName = target.kind == Expr::Kind::Name;
        auto isLifecycleHelperPath = [](const std::string &fullPath) -> bool {
          static const std::array<std::string_view, 10> suffixes = {
              "/Create",       "/Destroy",       "/Copy",       "/Move",       "/CreateStack",
              "/DestroyStack", "/CreateHeap",    "/DestroyHeap","/CreateBuffer","/DestroyBuffer"};
          for (std::string_view suffix : suffixes) {
            if (fullPath.size() < suffix.size()) {
              continue;
            }
            if (fullPath.compare(fullPath.size() - suffix.size(), suffix.size(), suffix.data(), suffix.size()) == 0) {
              return true;
            }
          }
          return false;
        };
        auto isNamedFieldTarget = [&](const Expr &candidate, std::string_view rootName) -> bool {
          if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess || candidate.args.size() != 1) {
            return false;
          }
          const Expr *receiver = &candidate.args.front();
          while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess && receiver->args.size() == 1) {
            receiver = &receiver->args.front();
          }
          return receiver->kind == Expr::Kind::Name && receiver->name == rootName;
        };
        auto resolveFieldTargetRootName = [&](const Expr &candidate, std::string &rootNameOut) -> bool {
          if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess || candidate.args.size() != 1) {
            return false;
          }
          const Expr *receiver = &candidate.args.front();
          while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess && receiver->args.size() == 1) {
            receiver = &receiver->args.front();
          }
          if (receiver->kind != Expr::Kind::Name) {
            return false;
          }
          rootNameOut = receiver->name;
          return true;
        };
        auto validateMutableFieldAccessTarget = [&](const Expr &fieldTarget) -> bool {
          if (fieldTarget.kind != Expr::Kind::Call || !fieldTarget.isFieldAccess || fieldTarget.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          if (!validateExpr(params, locals, fieldTarget.args.front())) {
            return false;
          }
          BindingInfo fieldBinding;
          if (!resolveStructFieldBinding(params, locals, fieldTarget.args.front(), fieldTarget.name, fieldBinding)) {
            if (error_.empty()) {
              error_ = "assign target must be a mutable binding";
            }
            return false;
          }
          std::string fieldTargetRootName;
          const bool allowMutableReceiverFieldWrite =
              resolveFieldTargetRootName(fieldTarget, fieldTargetRootName) &&
              isMutableBinding(params, locals, fieldTargetRootName);
          const bool allowLifecycleFieldWrite =
              !fieldBinding.isMutable && isNamedFieldTarget(fieldTarget, "this") &&
              isLifecycleHelperPath(currentDefinitionPath_);
          if (!fieldBinding.isMutable && !allowLifecycleFieldWrite && !allowMutableReceiverFieldWrite) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          if (fieldTarget.args.front().kind == Expr::Kind::Name &&
              hasActiveBorrowForBinding(fieldTarget.args.front().name)) {
            formatBorrowedBindingError(fieldTarget.args.front().name, fieldTarget.args.front().name);
            return false;
          }
          return true;
        };
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = "assign target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
          if (!validateMutableFieldAccessTarget(target)) {
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string accessName;
          if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
            const Expr &collectionTarget = target.args.front();
            std::string elemType;
            if (!(resolveVectorTarget(collectionTarget, elemType) || resolveArrayTarget(collectionTarget, elemType))) {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            if (collectionTarget.kind == Expr::Kind::Name) {
              if (!isMutableBinding(params, locals, collectionTarget.name)) {
                error_ = "assign target must be a mutable binding: " + collectionTarget.name;
                return false;
              }
              if (hasActiveBorrowForBinding(collectionTarget.name)) {
                formatBorrowedBindingError(collectionTarget.name, collectionTarget.name);
                return false;
              }
            } else if (collectionTarget.kind == Expr::Kind::Call && collectionTarget.isFieldAccess) {
              if (!validateMutableFieldAccessTarget(collectionTarget)) {
                return false;
              }
            } else {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            if (!validateExpr(params, locals, target)) {
              return false;
            }
          } else {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = "assign target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = "assign target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(expr.args[1])) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink) {
              if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
                return false;
              }
            }
          }
          if (!currentDefinitionIsUnsafe_) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink && reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (currentDefinitionIsUnsafe_ && targetIsName && isUnsafeReferenceExpr(expr.args[1])) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink)) {
            if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
        }
        if (!currentDefinitionIsUnsafe_ && targetIsName) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink) &&
              reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        if (targetIsName) {
          movedBindings_.erase(target.name);
        }
        return true;
      }
      std::string mutateName;
      if (getBuiltinMutationName(expr, mutateName)) {
        if (expr.args.size() != 1) {
          error_ = mutateName + " requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = mutateName + " target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = mutateName + " target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = mutateName + " target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = mutateName + " target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = mutateName + " target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = mutateName + " target must be a mutable binding";
          return false;
        }
        if (!isNumericExpr(params, locals, target)) {
          error_ = mutateName + " requires numeric operand";
          return false;
        }
        return true;
      }
      if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos) {
        std::string builtinName;
        if (getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
            getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
            getBuiltinMathName(expr, builtinName, true)) {
          error_ = "math builtin requires import /std/math/* or /std/math/<name>: " + expr.name;
          return false;
        }
      }
      if (!expr.isMethodCall && expr.name.find('/') == std::string::npos) {
        const BindingInfo *callableBinding = findParamBinding(params, expr.name);
        if (callableBinding == nullptr) {
          auto localIt = locals.find(expr.name);
          if (localIt != locals.end()) {
            callableBinding = &localIt->second;
          }
        }
        if (callableBinding != nullptr && callableBinding->typeName == "lambda") {
          if (hasNamedArguments(expr.argNames)) {
            error_ = "named arguments not supported for lambda calls";
            return false;
          }
          if (!expr.templateArgs.empty()) {
            error_ = "lambda calls do not accept template arguments";
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error_ = "lambda calls do not accept block arguments";
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
      }
      if (!expr.isMethodCall &&
          (resolved == "/std/collections/vector/count" || resolved == "/vector/count") &&
          expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
        const bool isAliasCount = resolved == "/vector/count";
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType) &&
            !resolveArrayTarget(expr.args.front(), elemType) &&
            !resolveStringTarget(expr.args.front())) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (resolveMapTarget(expr.args.front())) {
            error_ = isAliasCount ? "unknown call target: /vector/count"
                                  : "unknown call target: /std/collections/vector/count";
            return false;
          }
          error_ = "count requires vector target";
          return false;
        }
      }
      if (!expr.isMethodCall &&
          (resolved == "/std/collections/vector/capacity" || resolved == "/vector/capacity") &&
          expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
        const bool isAliasCapacity = resolved == "/vector/capacity";
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (isAliasCapacity && resolveArrayTarget(expr.args.front(), elemType)) {
            error_ = "unknown call target: /vector/capacity";
            return false;
          }
          if (resolveMapTarget(expr.args.front())) {
            error_ = isAliasCapacity ? "unknown call target: /vector/capacity"
                                     : "unknown call target: /std/collections/vector/capacity";
            return false;
          }
          error_ = "capacity requires vector target";
          return false;
        }
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && !shouldBuiltinValidateBareMapContainsCall) {
        Expr rewrittenMapHelperCall;
        if (tryRewriteBareMapHelperCall(expr, "contains", rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") && !shouldBuiltinValidateBareMapTryAtCall) {
        Expr rewrittenMapHelperCall;
        if (tryRewriteBareMapHelperCall(expr, "tryAt", rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          !shouldBuiltinValidateBareMapAccessCall) {
        Expr rewrittenMapHelperCall;
        if (tryRewriteBareMapHelperCall(expr, builtinName, rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapContainsCall || isIndexedArgsPackMapReceiverTarget(expr.args.front()))) {
        std::string mapKeyType;
        if (!resolveMapKeyType(expr.args.front(), mapKeyType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          error_ = "contains requires map target";
          return false;
        }
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!isStringExpr(expr.args[1])) {
              error_ = "contains requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(expr.args[1])) {
                error_ = "contains requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind candidateKind = inferExprReturnKind(expr.args[1], params, locals);
              if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
                error_ = "contains requires map key type " + mapKeyType;
                return false;
              }
            }
          }
        }
        if (!validateExpr(params, locals, expr.args.front()) ||
            !validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        return true;
      }
      if (((expr.isMethodCall && resolved == "/std/collections/map/tryAt") ||
           (!expr.isMethodCall &&
            (isSimpleCallName(expr, "tryAt") || resolved == "/std/collections/map/tryAt"))) &&
          expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapTryAtCall || isIndexedArgsPackMapReceiverTarget(expr.args.front()))) {
        std::string mapKeyType;
        if (!resolveMapKeyType(expr.args.front(), mapKeyType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          error_ = "tryAt requires map target";
          return false;
        }
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!isStringExpr(expr.args[1])) {
              error_ = "tryAt requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(expr.args[1])) {
                error_ = "tryAt requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind candidateKind = inferExprReturnKind(expr.args[1], params, locals);
              if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
                error_ = "tryAt requires map key type " + mapKeyType;
                return false;
              }
            }
          }
        }
        if (!validateExpr(params, locals, expr.args.front()) ||
            !validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        return true;
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapAccessCall || isIndexedArgsPackMapReceiverTarget(expr.args.front()))) {
        std::string mapKeyType;
        if (resolveMapKeyType(expr.args.front(), mapKeyType)) {
          if (!mapKeyType.empty()) {
            if (normalizeBindingTypeName(mapKeyType) == "string") {
              if (!isStringExpr(expr.args[1])) {
                error_ = builtinName + " requires string map key";
                return false;
              }
            } else {
              ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
              if (keyKind != ReturnKind::Unknown) {
                if (resolveStringTarget(expr.args[1])) {
                  error_ = builtinName + " requires map key type " + mapKeyType;
                  return false;
                }
                ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
                if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                  error_ = builtinName + " requires map key type " + mapKeyType;
                  return false;
                }
              }
            }
          }
          if (!validateExpr(params, locals, expr.args.front()) ||
              !validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
      }
      error_ = "unknown call target: " + formatUnknownCallTarget(expr);
      return false;
    }
    auto argumentStructMismatchDiagnostic = [&](const std::string &paramName,
                                                const std::string &expectedTypePath,
                                                const std::string &actualTypePath) {
      const std::string prefix =
          "argument type mismatch for " + diagnosticResolved + " parameter " + paramName + ": ";
      if (isImplicitMatrixQuaternionConversion(expectedTypePath, actualTypePath)) {
        return prefix + implicitMatrixQuaternionConversionDiagnostic(expectedTypePath, actualTypePath);
      }
      return prefix + "expected " + expectedTypePath + " got " + actualTypePath;
    };
    auto expectedBindingTypeText = [](const BindingInfo &binding) {
      if (binding.typeName.empty()) {
        return std::string();
      }
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    auto isStringTypeName = [](const std::string &typeName) {
      return normalizeBindingTypeName(typeName) == "string";
    };
    auto isSoftwareNumericParamCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
      switch (expectedKind) {
        case ReturnKind::Integer:
          return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
                 actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
        case ReturnKind::Decimal:
          return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
                 actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
                 actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
                 actualKind == ReturnKind::Decimal;
        case ReturnKind::Complex:
          return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
                 actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
                 actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
                 actualKind == ReturnKind::Decimal || actualKind == ReturnKind::Complex;
        default:
          return false;
      }
    };
    auto isBuiltinCollectionLiteralExpr = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      if (defMap_.find(resolveCalleePath(candidate)) != defMap_.end()) {
        return false;
      }
      std::string collectionName;
      return getBuiltinCollectionName(candidate, collectionName);
    };
    auto validateArgumentTypeAgainstParam = [&](const Expr &arg,
                                                const ParameterInfo &param,
                                                const std::string &expectedTypeName,
                                                const std::string &expectedTypeText) -> bool {
      if (expectedTypeName.empty() || expectedTypeName == "auto") {
        return true;
      }
      if (isStringTypeName(expectedTypeName)) {
        if (!isStringExpr(arg)) {
          error_ =
              "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected string";
          return false;
        }
        return true;
      }
      if (isStringExpr(arg)) {
        error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected " +
                 expectedTypeText;
        return false;
      }
      std::string expectedBase;
      std::string expectedArgText;
      if (splitTemplateTypeName(expectedTypeText, expectedBase, expectedArgText)) {
        const std::string normalizedExpectedBase = normalizeBindingTypeName(expectedBase);
        std::vector<std::string> expectedTemplateArgs;
        if (splitTopLevelTemplateArgs(expectedArgText, expectedTemplateArgs)) {
          if (normalizedExpectedBase == "array" && expectedTemplateArgs.size() == 1) {
            std::string actualElemType;
            if (resolveArrayTarget(arg, actualElemType)) {
              if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                  normalizeBindingTypeName(actualElemType)) {
                return true;
              }
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got array<" + actualElemType + ">";
              return false;
            }
            if (resolveVectorTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got vector<" + actualElemType + ">";
              return false;
            }
            if (resolveSoaVectorTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got soa_vector<" + actualElemType + ">";
              return false;
            }
          } else if (normalizedExpectedBase == "vector" && expectedTemplateArgs.size() == 1) {
            std::string actualElemType;
            if (resolveVectorTarget(arg, actualElemType)) {
              if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                  normalizeBindingTypeName(actualElemType)) {
                return true;
              }
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got vector<" + actualElemType + ">";
              return false;
            }
            if (resolveSoaVectorTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got soa_vector<" + actualElemType + ">";
              return false;
            }
            if (resolveArrayTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got array<" + actualElemType + ">";
              return false;
            }
          } else if (normalizedExpectedBase == "soa_vector" && expectedTemplateArgs.size() == 1) {
            std::string actualElemType;
            if (resolveSoaVectorTarget(arg, actualElemType)) {
              if (normalizeBindingTypeName(expectedTemplateArgs.front()) ==
                  normalizeBindingTypeName(actualElemType)) {
                return true;
              }
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got soa_vector<" + actualElemType + ">";
              return false;
            }
            if (resolveVectorTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got vector<" + actualElemType + ">";
              return false;
            }
            if (resolveArrayTarget(arg, actualElemType)) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got array<" + actualElemType + ">";
              return false;
            }
          } else if (normalizedExpectedBase == "map" && expectedTemplateArgs.size() == 2) {
            std::string actualKeyType;
            std::string actualValueType;
            if (resolveMapKeyType(arg, actualKeyType) && resolveMapValueType(arg, actualValueType) &&
                (normalizeBindingTypeName(expectedTemplateArgs[0]) != normalizeBindingTypeName(actualKeyType) ||
                 normalizeBindingTypeName(expectedTemplateArgs[1]) != normalizeBindingTypeName(actualValueType))) {
              error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                       ": expected " +
                       expectedTypeText + " got map<" + actualKeyType + ", " + actualValueType + ">";
              return false;
            }
          }
        }
      }
      const ReturnKind expectedKind = returnKindForTypeName(normalizeBindingTypeName(expectedTypeName));
      if (expectedKind != ReturnKind::Unknown) {
        const ReturnKind actualKind = inferExprReturnKind(arg, params, locals);
        if (actualKind == ReturnKind::Array && isBuiltinCollectionLiteralExpr(arg)) {
          return true;
        }
        if (isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
          return true;
        }
        if (actualKind != ReturnKind::Unknown && actualKind != expectedKind) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name +
                   ": expected " +
                   typeNameForReturnKind(expectedKind) + " got " + typeNameForReturnKind(actualKind);
          return false;
        }
        return true;
      }
      std::string expectedStructPath =
          resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
      if (expectedStructPath.empty()) {
        const size_t calleeSlash = resolved.find_last_of('/');
        if (calleeSlash != std::string::npos && calleeSlash > 0) {
          expectedStructPath =
              resolveStructTypePath(expectedTypeName, resolved.substr(0, calleeSlash), structNames_);
        }
      }
      if (expectedStructPath.empty()) {
        return true;
      }
      const std::string actualStructPath = inferStructReturnPath(arg, params, locals);
      if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
        error_ = argumentStructMismatchDiagnostic(param.name, expectedStructPath, actualStructPath);
        return false;
      }
      if (actualStructPath.empty()) {
        const BindingInfo *actualBinding = nullptr;
        if (arg.kind == Expr::Kind::Name) {
          actualBinding = findParamBinding(params, arg.name);
          if (actualBinding == nullptr) {
            auto it = locals.find(arg.name);
            if (it != locals.end()) {
              actualBinding = &it->second;
            }
          }
        }
        if (actualBinding != nullptr &&
            (normalizeBindingTypeName(actualBinding->typeName) == "Reference" ||
             normalizeBindingTypeName(actualBinding->typeName) == "Pointer") &&
            !actualBinding->typeTemplateArg.empty()) {
          std::string actualTargetStructPath =
              resolveStructTypePath(actualBinding->typeTemplateArg, arg.namespacePrefix, structNames_);
          if (actualTargetStructPath.empty()) {
            const size_t calleeSlash = resolved.find_last_of('/');
            if (calleeSlash != std::string::npos && calleeSlash > 0) {
              actualTargetStructPath = resolveStructTypePath(
                  actualBinding->typeTemplateArg, resolved.substr(0, calleeSlash), structNames_);
            }
          }
          if (actualTargetStructPath == expectedStructPath) {
            return true;
          }
        }
        const ReturnKind actualKind = inferExprReturnKind(arg, params, locals);
        if (actualKind != ReturnKind::Unknown) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected " +
                   expectedStructPath + " got " + typeNameForReturnKind(actualKind);
          return false;
        }
      }
      return true;
    };
    if (it != defMap_.end() && calleeParams.empty() && structNames_.count(resolved) > 0) {
      if (expr.args.empty() && !hasNamedArguments(expr.argNames) && !expr.hasBodyArguments &&
          expr.bodyArguments.empty()) {
        if (const std::string diagnostic = experimentalGfxUnavailableConstructorDiagnostic(expr, resolved);
            !diagnostic.empty()) {
          error_ = diagnostic;
          return false;
        }
      }
      std::vector<ParameterInfo> fieldParams;
      fieldParams.reserve(it->second->statements.size());
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      bool hasMissingDefaults = false;
      for (const auto &stmt : it->second->statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + resolved;
          return false;
        }
        if (isStaticField(stmt)) {
          continue;
        }
        ParameterInfo field;
        field.name = stmt.name;
        if (!SemanticsValidator::resolveStructFieldBinding(*it->second, stmt, field.binding)) {
          return false;
        }
        if (stmt.args.size() == 1) {
          field.defaultExpr = &stmt.args.front();
        } else {
          hasMissingDefaults = true;
        }
        fieldParams.push_back(field);
      }
      if (hasMissingDefaults && expr.args.empty() && !hasNamedArguments(expr.argNames)) {
        if (hasStructZeroArgConstructor(resolved)) {
          return true;
        }
      }
      if (!validateNamedArgumentsAgainstParams(fieldParams, expr.argNames, error_)) {
        if (error_.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + diagnosticResolved;
        }
        return false;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(fieldParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + diagnosticResolved;
        } else {
          error_ = orderError;
        }
        return false;
      }
      std::unordered_set<const Expr *> explicitArgs;
      explicitArgs.reserve(expr.args.size());
      for (const auto &arg : expr.args) {
        explicitArgs.insert(&arg);
      }
      for (const auto *arg : orderedArgs) {
        if (!arg || explicitArgs.count(arg) == 0) {
          continue;
        }
        if (!validateExpr(params, locals, *arg)) {
          return false;
        }
      }
      for (size_t argIndex = 0; argIndex < orderedArgs.size() && argIndex < fieldParams.size(); ++argIndex) {
        const Expr *arg = orderedArgs[argIndex];
        if (arg == nullptr || explicitArgs.count(arg) == 0) {
          continue;
        }
        const ParameterInfo &fieldParam = fieldParams[argIndex];
        const std::string expectedTypeText = expectedBindingTypeText(fieldParam.binding);
        if (!validateArgumentTypeAgainstParam(
                *arg, fieldParam, fieldParam.binding.typeName, expectedTypeText)) {
          return false;
        }
      }
      return true;
    }
    Expr reorderedCallExpr;
    Expr trimmedTypeNamespaceCallExpr;
    const std::vector<Expr> *orderedCallArgs = &expr.args;
    const std::vector<std::optional<std::string>> *orderedCallArgNames = &expr.argNames;
    if (SemanticsValidator::isTypeNamespaceMethodCall(params, locals, expr, resolved)) {
      trimmedTypeNamespaceCallExpr = expr;
      trimmedTypeNamespaceCallExpr.args.erase(trimmedTypeNamespaceCallExpr.args.begin());
      if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
        trimmedTypeNamespaceCallExpr.argNames.erase(trimmedTypeNamespaceCallExpr.argNames.begin());
      }
      orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
      orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
    } else if (hasMethodReceiverIndex && methodReceiverIndex > 0 && methodReceiverIndex < expr.args.size()) {
      reorderedCallExpr = expr;
      std::swap(reorderedCallExpr.args[0], reorderedCallExpr.args[methodReceiverIndex]);
      if (reorderedCallExpr.argNames.size() < reorderedCallExpr.args.size()) {
        reorderedCallExpr.argNames.resize(reorderedCallExpr.args.size());
      }
      std::swap(reorderedCallExpr.argNames[0], reorderedCallExpr.argNames[methodReceiverIndex]);
      orderedCallArgs = &reorderedCallExpr.args;
      orderedCallArgNames = &reorderedCallExpr.argNames;
    }
    if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, error_)) {
      if (error_.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + diagnosticResolved;
      }
      return false;
    }
    std::vector<const Expr *> orderedArgs;
    std::vector<const Expr *> packedArgs;
    size_t packedParamIndex = calleeParams.size();
    std::string orderError;
    if (!buildOrderedArguments(calleeParams,
                               *orderedCallArgs,
                               *orderedCallArgNames,
                               orderedArgs,
                               packedArgs,
                               packedParamIndex,
                               orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + diagnosticResolved;
      } else {
        error_ = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
    for (const auto *arg : packedArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
    auto validateSpreadArgumentTypeAgainstParam = [&](const Expr &arg,
                                                      const ParameterInfo &param,
                                                      const std::string &expectedTypeName,
                                                      const std::string &expectedTypeText) -> bool {
      std::string actualElementTypeText;
      if (!resolveArgsPackElementTypeForExpr(arg, params, locals, actualElementTypeText)) {
        error_ = "spread argument requires args<T> value";
        return false;
      }
      const ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
      if (expectedKind != ReturnKind::Unknown) {
        const ReturnKind actualKind = returnKindForTypeName(actualElementTypeText);
        if (actualKind == ReturnKind::Unknown || actualKind == expectedKind ||
            isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
          return true;
        }
        error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected " +
                 typeNameForReturnKind(expectedKind) + " got " + typeNameForReturnKind(actualKind);
        return false;
      }
      const std::string expectedStructPath =
          resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
      if (!expectedStructPath.empty()) {
        const std::string actualStructPath =
            resolveStructTypePath(actualElementTypeText, arg.namespacePrefix, structNames_);
        if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
          error_ = argumentStructMismatchDiagnostic(param.name, expectedStructPath, actualStructPath);
          return false;
        }
        if (actualStructPath.empty() && normalizeBindingTypeName(actualElementTypeText) != expectedStructPath) {
          error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected " +
                   expectedStructPath + " got " + normalizeBindingTypeName(actualElementTypeText);
          return false;
        }
        return true;
      }
      if (normalizeBindingTypeName(actualElementTypeText) != normalizeBindingTypeName(expectedTypeText)) {
        error_ = "argument type mismatch for " + diagnosticResolved + " parameter " + param.name + ": expected " +
                 expectedTypeText + " got " + normalizeBindingTypeName(actualElementTypeText);
        return false;
      }
      return true;
    };
    auto validateArgumentsForParameter = [&](const ParameterInfo &param,
                                            const std::string &expectedTypeName,
                                            const std::string &expectedTypeText,
                                            const std::vector<const Expr *> &argsToValidate) -> bool {
      for (const Expr *arg : argsToValidate) {
        if (arg == nullptr) {
          continue;
        }
        if (arg->isSpread) {
          if (!validateSpreadArgumentTypeAgainstParam(*arg, param, expectedTypeName, expectedTypeText)) {
            return false;
          }
          continue;
        }
        if (!validateArgumentTypeAgainstParam(*arg, param, expectedTypeName, expectedTypeText)) {
          return false;
        }
      }
      return true;
    };
    for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
      const ParameterInfo &param = calleeParams[paramIndex];
      if (paramIndex == packedParamIndex) {
        std::string packElementTypeText;
        if (!getArgsPackElementType(param.binding, packElementTypeText)) {
          continue;
        }
        std::string packElementTypeName = packElementTypeText;
        std::string packBase;
        std::string packArgs;
        if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
          packElementTypeName = packBase;
        }
        if (!validateArgumentsForParameter(param, packElementTypeName, packElementTypeText, packedArgs)) {
          return false;
        }
        continue;
      }
      const Expr *arg = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
      if (arg == nullptr) {
        continue;
      }
      const std::string &expectedTypeName = param.binding.typeName;
      const std::string expectedTypeText =
          param.binding.typeTemplateArg.empty() ? expectedTypeName : expectedTypeName + "<" + param.binding.typeTemplateArg + ">";
      if (!validateArgumentTypeAgainstParam(*arg, param, expectedTypeName, expectedTypeText)) {
        return false;
      }
    }
    auto isReferenceTypeText = [](const std::string &typeName, const std::string &typeText) {
      if (normalizeBindingTypeName(typeName) == "Reference") {
        return true;
      }
      std::string base;
      std::string argText;
      return splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "Reference";
    };
    bool calleeIsUnsafe = false;
    if (it != defMap_.end() && it->second != nullptr) {
      for (const auto &transform : it->second->transforms) {
        if (transform.name == "unsafe") {
          calleeIsUnsafe = true;
          break;
        }
      }
    }
    if (currentDefinitionIsUnsafe_ && !calleeIsUnsafe) {
      for (size_t i = 0; i < calleeParams.size(); ++i) {
        const ParameterInfo &param = calleeParams[i];
        if (i == packedParamIndex) {
          std::string packElementTypeText;
          if (!getArgsPackElementType(param.binding, packElementTypeText)) {
            continue;
          }
          std::string packElementTypeName = packElementTypeText;
          std::string packBase;
          std::string packArgs;
          if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
            packElementTypeName = packBase;
          }
          if (!isReferenceTypeText(packElementTypeName, packElementTypeText)) {
            continue;
          }
          for (const Expr *arg : packedArgs) {
            if (!arg || !isUnsafeReferenceExpr(*arg)) {
              continue;
            }
            error_ = "unsafe reference escapes across safe boundary to " + resolved;
            return false;
          }
          continue;
        }
        const Expr *arg = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
        if (arg == nullptr) {
          continue;
        }
        const std::string expectedTypeText = param.binding.typeTemplateArg.empty()
                                                 ? param.binding.typeName
                                                 : param.binding.typeName + "<" + param.binding.typeTemplateArg + ">";
        if (!isReferenceTypeText(param.binding.typeName, expectedTypeText)) {
          continue;
        }
        if (!isUnsafeReferenceExpr(*arg)) {
          continue;
        }
        error_ = "unsafe reference escapes across safe boundary to " + resolved;
        return false;
      }
    }
    return true;
  }
  return false;
}

} // namespace primec::semantics
