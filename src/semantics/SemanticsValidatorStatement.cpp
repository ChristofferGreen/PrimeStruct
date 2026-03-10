#include "SemanticsValidator.h"
#include "SemanticsValidatorStatementLoopCountStep.h"

#include <cctype>
#include <functional>
#include <optional>

namespace primec::semantics {

bool SemanticsValidator::validateStatement(const std::vector<ParameterInfo> &params,
                                           std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &stmt,
                                           ReturnKind returnKind,
                                           bool allowReturn,
                                           bool allowBindings,
                                           bool *sawReturn,
                                           const std::string &namespacePrefix,
                                           const std::vector<Expr> *enclosingStatements,
                                           size_t statementIndex) {
  ExprContextScope statementScope(*this, stmt);
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
  auto isStringExpr = [&](const Expr &arg,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
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
          elemTypeOut = args.front();
          return true;
        };
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (resolveReference(*paramBinding)) {
            return true;
          }
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemTypeOut = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end()) {
          return false;
        }
        if (resolveReference(it->second)) {
          return true;
        }
        if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
            it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemTypeOut = it->second.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
          return false;
        }
        if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
            target.templateArgs.size() == 1) {
          elemTypeOut = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          valueTypeOut = parts[1];
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        valueTypeOut = parts[1];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
          return false;
        }
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    };
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(arg.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinArrayAccessName(arg, accessName) &&
          arg.args.size() == 2) {
        std::string elemType;
        if (resolveArrayTarget(arg.args.front(), elemType) && normalizeBindingTypeName(elemType) == "string") {
          return true;
        }
        std::string mapValueType;
        if (resolveMapValueType(arg.args.front(), mapValueType) &&
            normalizeBindingTypeName(mapValueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };
  auto isPrintableExpr = [&](const Expr &arg,
                             const std::vector<ParameterInfo> &paramsIn,
                             const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (isStringExpr(arg, paramsIn, localsIn)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::String) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Bool;
      }
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Bool;
      }
    }
    if (isPointerLikeExpr(arg, paramsIn, localsIn)) {
      return false;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string builtinName;
      if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinCollectionName(arg, builtinName)) {
        return false;
      }
    }
    return false;
  };
  auto isIntegerExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
  };
  auto resolveVectorBinding = [&](const Expr &target, const BindingInfo *&bindingOut) -> bool {
    bindingOut = nullptr;
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      bindingOut = paramBinding;
      return true;
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      bindingOut = &it->second;
      return true;
    }
    return false;
  };
  auto validateVectorElementType = [&](const Expr &arg, const std::string &typeName) -> bool {
    if (typeName.empty()) {
      error_ = "push requires vector element type";
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (normalizedType == "string") {
      if (!isStringExpr(arg, params, locals)) {
        error_ = "push requires element type " + typeName;
        return false;
      }
      return true;
    }
    ReturnKind expectedKind = returnKindForTypeName(normalizedType);
    if (expectedKind == ReturnKind::Unknown) {
      return true;
    }
    if (isStringExpr(arg, params, locals)) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    ReturnKind argKind = inferExprReturnKind(arg, params, locals);
    if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    return true;
  };
  auto validateVectorHelperTarget = [&](const Expr &target, const char *helperName, const BindingInfo *&bindingOut) -> bool {
    const std::string helper = helperName == nullptr ? "" : std::string(helperName);
    const bool allowSoaVectorTarget = helper == "push" || helper == "reserve";
    if (!resolveVectorBinding(target, bindingOut) || bindingOut == nullptr ||
        (bindingOut->typeName != "vector" && !(allowSoaVectorTarget && bindingOut->typeName == "soa_vector"))) {
      error_ = std::string(helperName) + " requires vector binding";
      return false;
    }
    if (!bindingOut->isMutable) {
      error_ = std::string(helperName) + " requires mutable vector binding";
      return false;
    }
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
    return namespacedCollection == "vector" && namespacedHelper == helper;
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
  auto preferVectorStdlibHelperPath = [&](const std::string &path) -> std::string {
    std::string preferred = path;
    if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/array/").size());
      const std::string vectorAlias = "/vector/" + suffix;
      if (defMap_.count(vectorAlias) > 0) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defMap_.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
    if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/vector/").size());
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defMap_.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      } else {
        const std::string arrayAlias = "/array/" + suffix;
        if (defMap_.count(arrayAlias) > 0) {
          preferred = arrayAlias;
        }
      }
    }
    if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
      const std::string vectorAlias = "/vector/" + suffix;
      if (defMap_.count(vectorAlias) > 0) {
        preferred = vectorAlias;
      } else {
        const std::string arrayAlias = "/array/" + suffix;
        if (defMap_.count(arrayAlias) > 0) {
          preferred = arrayAlias;
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
    return preferred;
  };
  auto resolveVectorHelperTargetPath = [&](const Expr &receiver,
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
  auto isNonCollectionStructVectorHelperTarget = [&](const std::string &resolvedPath) -> bool {
    const size_t slash = resolvedPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string receiverPath = resolvedPath.substr(0, slash);
    if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
      return false;
    }
    return structNames_.count(receiverPath) > 0;
  };
  auto validateVectorHelperReceiver = [&](const Expr &receiver, const std::string &helperName) -> bool {
    if (!validateExpr(params, locals, receiver)) {
      return false;
    }
    std::string resolvedMethod;
    if (!resolveVectorHelperTargetPath(receiver, helperName, resolvedMethod)) {
      return true;
    }
    if (defMap_.find(resolvedMethod) == defMap_.end() && isNonCollectionStructVectorHelperTarget(resolvedMethod)) {
      error_ = "unknown method: " + resolvedMethod;
      return false;
    }
    return true;
  };

  if (stmt.isBinding) {
    if (!allowBindings) {
      error_ = "binding not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "binding does not accept block arguments";
      return false;
    }
    if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
      error_ = "duplicate binding name: " + stmt.name;
      return false;
    }
    for (const auto &transform : stmt.transforms) {
      if (transform.name != "soa_vector" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (!isSoaVectorStructElementType(transform.templateArgs.front(), namespacePrefix, structNames_, importAliases_)) {
        break;
      }
      if (!validateSoaVectorElementFieldEnvelopes(transform.templateArgs.front(), namespacePrefix)) {
        return false;
      }
      break;
    }
    BindingInfo info;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(stmt, namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
      return false;
    }
    const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
    const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(info.typeName) == "auto";
    if (stmt.args.size() == 1 && stmt.args.front().isLambda && (!hasExplicitType || explicitAutoType)) {
      info.typeName = "lambda";
      info.typeTemplateArg.clear();
    }
    if (stmt.args.empty()) {
      if (structNames_.count(currentDefinitionPath_) > 0) {
        if (restrictType.has_value()) {
          const bool hasTemplate = !info.typeTemplateArg.empty();
          if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
            error_ = "restrict type does not match binding type";
            return false;
          }
        }
        locals.emplace(stmt.name, info);
        return true;
      }
      if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
        return false;
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (stmt.args.size() != 1) {
      error_ = "binding requires exactly one argument";
      return false;
    }
    const Expr &initializer = stmt.args.front();
    auto isEmptyBuiltinBlockInitializer = [&](const Expr &candidate) -> bool {
      if (!candidate.hasBodyArguments || !candidate.bodyArguments.empty()) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return false;
      }
      return isBuiltinBlockCall(candidate);
    };
    if (normalizeBindingTypeName(info.typeName) == "vector" && isEmptyBuiltinBlockInitializer(initializer)) {
      if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
        return false;
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    const bool entryArgInit = isEntryArgsAccess(initializer);
    const bool entryArgStringInit = isEntryArgStringBinding(locals, initializer);
    std::optional<EntryArgStringScope> entryArgScope;
    if (entryArgInit || entryArgStringInit) {
      entryArgScope.emplace(*this, true);
    }
    if (!validateExpr(params, locals, initializer)) {
      return false;
    }
    auto isStructConstructor = [&](const Expr &expr) -> bool {
      if (expr.kind != Expr::Kind::Call || expr.isBinding) {
        return false;
      }
      const std::string resolved = resolveCalleePath(expr);
      return structNames_.count(resolved) > 0;
    };
    auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return nullptr;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return nullptr;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return nullptr;
      }
      if (!allowAnyName && !isBuiltinBlockCall(candidate)) {
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
    std::function<bool(const Expr &)> isStructConstructorValueExpr;
    isStructConstructorValueExpr = [&](const Expr &candidate) -> bool {
      if (isStructConstructor(candidate)) {
        return true;
      }
      if (isMatchCall(candidate)) {
        Expr expanded;
        std::string error;
        if (!lowerMatchToIf(candidate, expanded, error)) {
          return false;
        }
        return isStructConstructorValueExpr(expanded);
      }
      if (isIfCall(candidate) && candidate.args.size() == 3) {
        const Expr &thenArg = candidate.args[1];
        const Expr &elseArg = candidate.args[2];
        const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
        const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
        const Expr &thenExpr = thenValue ? *thenValue : thenArg;
        const Expr &elseExpr = elseValue ? *elseValue : elseArg;
        return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
      }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      return isStructConstructorValueExpr(*valueExpr);
    }
    return false;
  };
    ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void &&
      !isStructConstructorValueExpr(initializer)) {
    error_ = "binding initializer requires a value";
    return false;
  }
    auto isSoftwareNumericBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
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
    auto isFloatBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
      if (expectedKind != ReturnKind::Float32 && expectedKind != ReturnKind::Float64) {
        return false;
      }
      return actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64;
    };
    if (!hasExplicitType || explicitAutoType) {
      (void)inferBindingTypeFromInitializer(initializer, params, locals, info);
    } else {
      const std::string expectedType = normalizeBindingTypeName(info.typeName);
      if (expectedType == "string") {
        if (!isStringExpr(initializer, params, locals)) {
          error_ = "binding initializer type mismatch";
          return false;
        }
      } else {
        const ReturnKind expectedKind = returnKindForTypeName(expectedType);
        if (expectedKind != ReturnKind::Unknown && initKind != ReturnKind::Unknown) {
          if (!isSoftwareNumericBindingCompatible(expectedKind, initKind) &&
              !isFloatBindingCompatible(expectedKind, initKind) &&
              initKind != expectedKind) {
            error_ = "binding initializer type mismatch";
            return false;
          }
        }
      }
    }
    if (info.typeName == "uninitialized") {
      if (info.typeTemplateArg.empty()) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
        error_ = "uninitialized bindings require uninitialized<T>() initializer";
        return false;
      }
      if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
        error_ = "uninitialized bindings require uninitialized<T>() initializer";
        return false;
      }
      if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() || !initializer.args.empty()) {
        error_ = "uninitialized does not accept arguments";
        return false;
      }
      if (initializer.templateArgs.size() != 1 ||
          !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
        error_ = "uninitialized initializer type mismatch";
        return false;
      }
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    if (entryArgInit || entryArgStringInit) {
      if (normalizeBindingTypeName(info.typeName) != "string") {
        error_ = "entry argument strings require string bindings";
        return false;
      }
      info.isEntryArgString = true;
    }
    auto referenceRootForBorrowBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
      if (binding.typeName == "Reference") {
        if (!binding.referenceRoot.empty()) {
          return binding.referenceRoot;
        }
        return bindingName;
      }
      return "";
    };
    auto pointerAliasRootForBinding = [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
      std::string referenceRoot = referenceRootForBorrowBinding(bindingName, binding);
      if (!referenceRoot.empty()) {
        return referenceRoot;
      }
      if (binding.typeName == "Pointer" && !binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return "";
    };
    auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        return paramBinding;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return nullptr;
      }
      return &it->second;
    };
    std::function<bool(const Expr &, std::string &)> resolvePointerRoot;
    resolvePointerRoot = [&](const Expr &expr, std::string &rootOut) -> bool {
      if (expr.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(expr.name);
        if (binding == nullptr) {
          return false;
        }
        rootOut = pointerAliasRootForBinding(expr.name, *binding);
        return !rootOut.empty();
      }
      if (expr.kind != Expr::Kind::Call) {
        return false;
      }
      std::string builtinName;
      if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          return false;
        }
        const BindingInfo *binding = resolveNamedBinding(target.name);
        if (binding != nullptr) {
          std::string root = pointerAliasRootForBinding(target.name, *binding);
          if (!root.empty()) {
            rootOut = std::move(root);
          } else {
            rootOut = target.name;
          }
          return true;
        }
        return false;
      }
      std::string opName;
      if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
        if (isPointerLikeExpr(expr.args[1], params, locals)) {
          return false;
        }
        return resolvePointerRoot(expr.args[0], rootOut);
      }
      return false;
    };
    if (info.typeName == "Pointer") {
      std::string pointerRoot;
      if (resolvePointerRoot(initializer, pointerRoot)) {
        info.referenceRoot = std::move(pointerRoot);
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (info.typeName == "Reference") {
      const Expr &init = initializer;
      auto formatBindingType = [](const BindingInfo &binding) -> std::string {
        if (binding.typeTemplateArg.empty()) {
          return binding.typeName;
        }
        return binding.typeName + "<" + binding.typeTemplateArg + ">";
      };
      std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
      resolvePointerTargetType = [&](const Expr &expr, std::string &targetOut) -> bool {
        if (expr.kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(expr.name);
          if (binding == nullptr) {
            return false;
          }
          if ((binding->typeName == "Pointer" || binding->typeName == "Reference") &&
              !binding->typeTemplateArg.empty()) {
            targetOut = binding->typeTemplateArg;
            return true;
          }
          return false;
        }
        if (expr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string builtinName;
        if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name) {
            return false;
          }
          const BindingInfo *binding = resolveNamedBinding(target.name);
          if (binding == nullptr) {
            return false;
          }
          if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
            targetOut = binding->typeTemplateArg;
          } else {
            targetOut = formatBindingType(*binding);
          }
          return true;
        }
        std::string opName;
        if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
          if (isPointerLikeExpr(expr.args[1], params, locals)) {
            return false;
          }
          return resolvePointerTargetType(expr.args[0], targetOut);
        }
        return false;
      };
      std::string pointerName;
      const bool initIsLocation =
          init.kind == Expr::Kind::Call && getBuiltinPointerName(init, pointerName) && pointerName == "location" &&
          init.args.size() == 1 && init.args.front().kind == Expr::Kind::Name;
      if (!initIsLocation && !currentDefinitionIsUnsafe_) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
      if (initIsLocation) {
        std::string safeTargetType;
        if (!resolvePointerTargetType(init, safeTargetType) ||
            !errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
          error_ = "Reference binding type mismatch";
          return false;
        }
      }
      if (!initIsLocation && currentDefinitionIsUnsafe_) {
        std::string pointerTargetType;
        if (!resolvePointerTargetType(init, pointerTargetType)) {
          error_ = "unsafe Reference bindings require pointer-like initializer";
          return false;
        }
        if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
          error_ = "unsafe Reference binding type mismatch";
          return false;
        }
        std::string borrowRoot;
        if (resolvePointerRoot(init, borrowRoot)) {
          info.referenceRoot = std::move(borrowRoot);
        }
        info.isUnsafeReference = true;
        locals.emplace(stmt.name, info);
        return true;
      }
      const Expr &target = init.args.front();
      auto resolveBorrowRoot = [&](const std::string &targetName, std::string &rootOut) -> bool {
        if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
          if (paramBinding->typeName == "Reference") {
            rootOut = referenceRootForBorrowBinding(targetName, *paramBinding);
          } else {
            rootOut = targetName;
          }
          return true;
        }
        auto it = locals.find(targetName);
        if (it == locals.end()) {
          return false;
        }
        if (it->second.typeName == "Reference") {
          rootOut = referenceRootForBorrowBinding(it->first, it->second);
        } else {
          rootOut = targetName;
        }
        return true;
      };
      std::string borrowRoot;
      if (!resolveBorrowRoot(target.name, borrowRoot) || borrowRoot.empty()) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
      bool sawMutableBorrow = false;
      bool sawImmutableBorrow = false;
      auto observeBorrow = [&](const std::string &bindingName, const BindingInfo &binding) {
        if (endedReferenceBorrows_.count(bindingName) > 0) {
          return;
        }
        const std::string root = referenceRootForBorrowBinding(bindingName, binding);
        if (root.empty() || root != borrowRoot) {
          return;
        }
        if (binding.isMutable) {
          sawMutableBorrow = true;
        } else {
          sawImmutableBorrow = true;
        }
      };
      for (const auto &param : params) {
        observeBorrow(param.name, param.binding);
      }
      for (const auto &entry : locals) {
        observeBorrow(entry.first, entry.second);
      }
      const bool conflict = info.isMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
      if (conflict && !currentDefinitionIsUnsafe_) {
        error_ = "borrow conflict: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + stmt.name + ")";
        return false;
      }
      info.referenceRoot = std::move(borrowRoot);
      info.isUnsafeReference = currentDefinitionIsUnsafe_;
    }
    locals.emplace(stmt.name, info);
    return true;
  }
  std::optional<EffectScope> effectScope;
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.transforms.empty() && !isBuiltinBlockCall(stmt)) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(stmt, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop"))) {
    const std::string name = stmt.name;
    auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
      if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
        return "";
      }
      if (!typeName.empty() && typeName[0] == '/') {
        return structNames_.count(typeName) > 0 ? typeName : "";
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
      if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
        return importIt->second;
      }
      return "";
    };
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = name + " does not accept block arguments";
      return false;
    }
    const size_t expectedArgs = (name == "init") ? 2 : 1;
    if (stmt.args.size() != expectedArgs) {
      error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
               (expectedArgs == 1 ? "" : "s");
      return false;
    }
    const Expr &storageArg = stmt.args.front();
    if (!validateExpr(params, locals, storageArg)) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolved = false;
    if (!resolveUninitializedStorageBinding(params, locals, storageArg, storageBinding, resolved)) {
      return false;
    }
    if (!resolved || storageBinding.typeName != "uninitialized" || storageBinding.typeTemplateArg.empty()) {
      error_ = name + " requires uninitialized<T> storage";
      return false;
    }
    if (name == "init") {
      if (!validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(stmt.args[1], params, locals);
      if (valueKind == ReturnKind::Void) {
        error_ = "init requires a value";
        return false;
      }
      auto trimType = [](const std::string &text) -> std::string {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
          ++start;
        }
        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
          --end;
        }
        return text.substr(start, end - start);
      };
      auto isBuiltinTemplateBase = [](const std::string &base) -> bool {
        return base == "array" || base == "vector" || base == "map" || base == "Result" || base == "Pointer" ||
               base == "Reference" || base == "Buffer" || base == "uninitialized";
      };
      std::function<bool(const std::string &, const std::string &)> typesMatch;
      typesMatch = [&](const std::string &expected, const std::string &actual) -> bool {
        std::string expectedTrim = trimType(expected);
        std::string actualTrim = trimType(actual);
        std::string expectedBase;
        std::string expectedArg;
        std::string actualBase;
        std::string actualArg;
        const bool expectedHasTemplate = splitTemplateTypeName(expectedTrim, expectedBase, expectedArg);
        const bool actualHasTemplate = splitTemplateTypeName(actualTrim, actualBase, actualArg);
        if (expectedHasTemplate != actualHasTemplate) {
          return false;
        }
        auto normalizeBase = [&](std::string base) -> std::string {
          base = trimType(base);
          if (!base.empty() && base[0] == '/') {
            base.erase(0, 1);
          }
          return base;
        };
        if (expectedHasTemplate) {
          std::string expectedNorm = normalizeBase(expectedBase);
          std::string actualNorm = normalizeBase(actualBase);
          if (isBuiltinTemplateBase(expectedNorm) || isBuiltinTemplateBase(actualNorm)) {
            if (expectedNorm != actualNorm) {
              return false;
            }
          } else if (!errorTypesMatch(expectedBase, actualBase, namespacePrefix)) {
            return false;
          }
          std::vector<std::string> expectedArgs;
          std::vector<std::string> actualArgs;
          if (!splitTopLevelTemplateArgs(expectedArg, expectedArgs) ||
              !splitTopLevelTemplateArgs(actualArg, actualArgs)) {
            return false;
          }
          if (expectedArgs.size() != actualArgs.size()) {
            return false;
          }
          for (size_t i = 0; i < expectedArgs.size(); ++i) {
            if (!typesMatch(expectedArgs[i], actualArgs[i])) {
              return false;
            }
          }
          return true;
        }
        return errorTypesMatch(expectedTrim, actualTrim, namespacePrefix);
      };
      auto inferValueTypeString = [&](const Expr &value, std::string &typeOut) -> bool {
        BindingInfo inferred;
        if (inferBindingTypeFromInitializer(value, params, locals, inferred)) {
          if (!inferred.typeTemplateArg.empty()) {
            typeOut = inferred.typeName + "<" + inferred.typeTemplateArg + ">";
            return true;
          }
          if (!inferred.typeName.empty() && inferred.typeName != "array") {
            typeOut = inferred.typeName;
            return true;
          }
        }
        if (inferExprReturnKind(value, params, locals) == ReturnKind::Array) {
          std::string structPath = inferStructReturnPath(value, params, locals);
          if (!structPath.empty()) {
            typeOut = structPath;
            return true;
          }
        }
        return false;
      };
      const std::string expectedType = storageBinding.typeTemplateArg;
      std::string actualType;
      if (inferValueTypeString(stmt.args[1], actualType)) {
        if (!typesMatch(expectedType, actualType)) {
          error_ = "init value type mismatch";
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind == ReturnKind::Array) {
        if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
          error_ = "init value type mismatch";
          return false;
        }
      } else if (expectedKind != ReturnKind::Unknown) {
        if (valueKind != ReturnKind::Unknown && valueKind != expectedKind) {
          error_ = "init value type mismatch";
          return false;
        }
      } else {
        std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix);
        if (!expectedStruct.empty()) {
          if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
            error_ = "init value type mismatch";
            return false;
          }
          if (valueKind == ReturnKind::Array) {
            std::string actualStruct = inferStructReturnPath(stmt.args[1], params, locals);
            if (!actualStruct.empty() && actualStruct != expectedStruct) {
              error_ = "init value type mismatch";
              return false;
            }
          }
        }
      }
    }
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "take") || isSimpleCallName(stmt, "borrow"))) {
    const std::string name = stmt.name;
    auto isUninitializedStorage = [&](const Expr &arg) -> bool {
      if (arg.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findBinding(params, locals, arg.name);
      return binding && binding->typeName == "uninitialized" && !binding->typeTemplateArg.empty();
    };
    const bool treatAsUninitializedHelper =
        (name != "take") || (!stmt.args.empty() && isUninitializedStorage(stmt.args.front()));
    if (treatAsUninitializedHelper) {
      if (hasNamedArguments(stmt.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error_ = name + " does not accept template arguments";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error_ = name + " does not accept block arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error_ = name + " requires exactly 1 argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (!isUninitializedStorage(stmt.args.front())) {
        error_ = name + " requires uninitialized<T> storage";
        return false;
      }
      return true;
    }
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      error_ = "execution body arguments must be calls";
      return false;
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!allowReturn) {
      error_ = "return not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "return does not accept block arguments";
      return false;
    }
    if (returnKind == ReturnKind::Void) {
      if (!stmt.args.empty()) {
        error_ = "return value not allowed for void definition";
        return false;
      }
    } else {
      if (stmt.args.size() != 1) {
        error_ = "return requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      auto declaredReferenceReturnTarget = [&]() -> std::optional<std::string> {
        auto defIt = defMap_.find(currentDefinitionPath_);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return std::nullopt;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "Reference") {
            continue;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return std::nullopt;
          }
          return args.front();
        }
        return std::nullopt;
      };
      if (std::optional<std::string> expectedReferenceTarget = declaredReferenceReturnTarget();
          expectedReferenceTarget.has_value()) {
        const Expr &returnExpr = stmt.args.front();
        if (returnExpr.kind != Expr::Kind::Name) {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        const BindingInfo *paramBinding = findParamBinding(params, returnExpr.name);
        if (paramBinding == nullptr || paramBinding->typeName != "Reference") {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        auto normalizeReferenceTarget = [&](const std::string &target) -> std::string {
          std::string trimmed = target;
          size_t start = 0;
          while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start])) != 0) {
            ++start;
          }
          size_t end = trimmed.size();
          while (end > start && std::isspace(static_cast<unsigned char>(trimmed[end - 1])) != 0) {
            --end;
          }
          trimmed = trimmed.substr(start, end - start);
          return normalizeBindingTypeName(trimmed);
        };
        if (normalizeReferenceTarget(paramBinding->typeTemplateArg) !=
            normalizeReferenceTarget(*expectedReferenceTarget)) {
          error_ = "reference return type mismatch";
          return false;
        }
      }
      if (returnKind != ReturnKind::Unknown) {
        ReturnKind exprKind = inferExprReturnKind(stmt.args.front(), params, locals);
        if (returnKind == ReturnKind::Array) {
          auto structIt = returnStructs_.find(currentDefinitionPath_);
          if (structIt != returnStructs_.end()) {
            std::string actualStruct = inferStructReturnPath(stmt.args.front(), params, locals);
            if (actualStruct.empty() || actualStruct != structIt->second) {
              std::string expectedType = structIt->second;
              if (expectedType == "/array" || expectedType == "/vector" || expectedType == "/map" ||
                  expectedType == "/string") {
                expectedType.erase(0, 1);
              }
              error_ = "return type mismatch: expected " + expectedType;
              return false;
            }
          } else if (exprKind != ReturnKind::Array) {
            error_ = "return type mismatch: expected array";
            return false;
          }
        } else if (exprKind != ReturnKind::Unknown && exprKind != returnKind) {
          error_ = "return type mismatch: expected " + typeNameForReturnKind(returnKind);
          return false;
        }
      }
    }
    if (sawReturn) {
      *sawReturn = true;
    }
    return true;
  }
  if (isMatchCall(stmt)) {
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error_)) {
      return false;
    }
    return validateStatement(params,
                             locals,
                             expanded,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             enclosingStatements,
                             statementIndex);
  }
  if (isIfCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "if does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "if requires condition, then, else";
      return false;
    }
    const Expr &cond = stmt.args[0];
    const Expr &thenArg = stmt.args[1];
    const Expr &elseArg = stmt.args[2];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = "if condition requires bool";
      return false;
    }
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
    std::vector<Expr> postMergeStatements;
    if (enclosingStatements != nullptr) {
      size_t start = statementIndex + 1;
      if (start > enclosingStatements->size()) {
        start = enclosingStatements->size();
      }
      postMergeStatements.insert(postMergeStatements.end(), enclosingStatements->begin() + start, enclosingStatements->end());
    }
    auto validateBranch = [&](const Expr &branch) -> bool {
      if (!isIfBlockEnvelope(branch)) {
        error_ = "if branches require block envelopes";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
      std::vector<Expr> livenessStatements = branch.bodyArguments;
      livenessStatements.insert(livenessStatements.end(), postMergeStatements.begin(), postMergeStatements.end());
      OnErrorScope onErrorScope(*this, std::nullopt);
      BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
      for (size_t bodyIndex = 0; bodyIndex < branch.bodyArguments.size(); ++bodyIndex) {
        const Expr &bodyExpr = branch.bodyArguments[bodyIndex];
        if (!validateStatement(params,
                               branchLocals,
                               bodyExpr,
                               returnKind,
                               allowReturn,
                               allowBindings,
                               sawReturn,
                               namespacePrefix,
                               &branch.bodyArguments,
                               bodyIndex)) {
          return false;
        }
        expireReferenceBorrowsForRemainder(params, branchLocals, livenessStatements, bodyIndex + 1);
      }
      return true;
    };
    if (!validateBranch(thenArg)) {
      return false;
    }
    if (!validateBranch(elseArg)) {
      return false;
    }
    return true;
  }

  auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto validateLoopBody = [&](const Expr &body,
                              const std::unordered_map<std::string, BindingInfo> &baseLocals,
                              const std::vector<Expr> &boundaryStatements,
                              bool includeNextIterationBody) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      error_ = "loop body requires a block envelope";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = baseLocals;
    std::vector<Expr> livenessStatements = body.bodyArguments;
    livenessStatements.insert(livenessStatements.end(), boundaryStatements.begin(), boundaryStatements.end());
    if (includeNextIterationBody) {
      livenessStatements.insert(livenessStatements.end(), body.bodyArguments.begin(), body.bodyArguments.end());
    }
    OnErrorScope onErrorScope(*this, std::nullopt);
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < body.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = body.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &body.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  };
  auto isBoolLiteralFalse = [](const Expr &expr) -> bool { return expr.kind == Expr::Kind::BoolLiteral && !expr.boolValue; };
  auto canIterateMoreThanOnce = [&](const Expr &countExpr, bool allowBoolCount) -> bool {
    return runSemanticsValidatorStatementCanIterateMoreThanOnceStep(countExpr, allowBoolCount);
  };
  if (isLoopCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "loop does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "loop does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "loop requires count and body";
      return false;
    }
    const Expr &count = stmt.args[0];
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64) {
      error_ = "loop count requires integer";
      return false;
    }
    if (runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(count)) {
      error_ = "loop count must be non-negative";
      return false;
    }
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, false);
    return validateLoopBody(stmt.args[1], locals, {}, includeNextIterationBody);
  }
  if (isWhileCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "while does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "while does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "while requires condition and body";
      return false;
    }
    const Expr &cond = stmt.args[0];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = "while condition requires bool";
      return false;
    }
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[1], locals, boundaryStatements, !conditionAlwaysFalse);
  }
  if (isForCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "for does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "for does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 4) {
      error_ = "for requires init, condition, step, and body";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> loopLocals = locals;
    if (!validateStatement(params, loopLocals, stmt.args[0], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    const Expr &cond = stmt.args[1];
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
    if (cond.isBinding) {
      if (!validateStatement(params, loopLocals, cond, returnKind, false, allowBindings, nullptr, namespacePrefix)) {
        return false;
      }
      auto it = loopLocals.find(cond.name);
      ReturnKind condKind = it == loopLocals.end() ? ReturnKind::Unknown : returnKindForBinding(it->second);
      if (condKind != ReturnKind::Bool) {
        error_ = "for condition requires bool";
        return false;
      }
    } else {
      if (!validateExpr(params, loopLocals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, loopLocals);
      if (condKind != ReturnKind::Bool) {
        error_ = "for condition requires bool";
        return false;
      }
    }
    if (!validateStatement(params, loopLocals, stmt.args[2], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = !cond.isBinding && isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(stmt.args[2]);
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[3], loopLocals, boundaryStatements, !conditionAlwaysFalse);
  }
  if (isRepeatCall(stmt)) {
    if (!stmt.hasBodyArguments) {
      error_ = "repeat requires block arguments";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "repeat does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "repeat requires exactly one argument";
      return false;
    }
    const Expr &count = stmt.args.front();
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64 &&
        countKind != ReturnKind::Bool) {
      error_ = "repeat count requires integer or bool";
      return false;
    }
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, true);
    Expr repeatBody;
    repeatBody.kind = Expr::Kind::Call;
    repeatBody.name = "repeat_body";
    repeatBody.bodyArguments = stmt.bodyArguments;
    repeatBody.hasBodyArguments = stmt.hasBodyArguments;
    return validateLoopBody(repeatBody, locals, {}, includeNextIterationBody);
  }
  if (isBuiltinBlockCall(stmt) && !stmt.hasBodyArguments) {
    error_ = "block requires block arguments";
    return false;
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    if (hasNamedArguments(stmt.argNames) || !stmt.args.empty() || !stmt.templateArgs.empty()) {
      error_ = "block does not accept arguments";
      return false;
    }
    std::optional<OnErrorHandler> blockOnError;
    if (!parseOnErrorTransform(stmt.transforms, stmt.namespacePrefix, "block", blockOnError)) {
      return false;
    }
    if (blockOnError.has_value()) {
      OnErrorScope onErrorArgsScope(*this, std::nullopt);
      for (const auto &arg : blockOnError->boundArgs) {
        if (!validateExpr(params, locals, arg)) {
          return false;
        }
      }
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    OnErrorScope onErrorScope(*this, blockOnError);
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, stmt.bodyArguments, bodyIndex + 1);
    }
    return true;
  }
  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(stmt, printBuiltin)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = printBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = printBuiltin.name + " requires exactly one argument";
      return false;
    }
    const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
    if (activeEffects_.count(effectName) == 0) {
      error_ = printBuiltin.name + " requires " + effectName + " effect";
      return false;
    }
    {
      EntryArgStringScope entryArgScope(*this, true);
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
    }
    if (!isPrintableExpr(stmt.args.front(), params, locals)) {
      error_ = printBuiltin.name + " requires an integer/bool or string literal/binding argument";
      return false;
    }
    return true;
  }
  auto isIntegerKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64;
  };
  auto isNumericOrBoolKind = [&](ReturnKind kind) -> bool {
    return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
           kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool;
  };
  auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "array" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "array" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string collection;
      if (defMap_.find(resolveCalleePath(arg)) != defMap_.end()) {
        return false;
      }
      if (getBuiltinCollectionName(arg, collection) && collection == "array" && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
    }
    return false;
  };
  auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
        if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto itLocal = locals.find(arg.name);
      if (itLocal != locals.end()) {
        if (itLocal->second.typeName == "Buffer" && !itLocal->second.typeTemplateArg.empty()) {
          elemType = itLocal->second.typeTemplateArg;
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (isSimpleCallName(arg, "buffer") && arg.templateArgs.size() == 1) {
        elemType = arg.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(arg, "upload") && arg.args.size() == 1) {
        return resolveArrayElemType(arg.args.front(), elemType);
      }
    }
    return false;
  };
  if (isSimpleCallName(stmt, "dispatch")) {
    if (currentDefinitionIsCompute_) {
      error_ = "dispatch is not allowed in compute definitions";
      return false;
    }
    if (activeEffects_.count("gpu_dispatch") == 0) {
      error_ = "dispatch requires gpu_dispatch effect";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "dispatch does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "dispatch does not accept block arguments";
      return false;
    }
    if (stmt.args.size() < 4) {
      error_ = "dispatch requires kernel and three dimension arguments";
      return false;
    }
    if (stmt.args.front().kind != Expr::Kind::Name) {
      error_ = "dispatch requires kernel name as first argument";
      return false;
    }
    const Expr &kernelExpr = stmt.args.front();
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
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
      ReturnKind dimKind = inferExprReturnKind(stmt.args[i], params, locals);
      if (!isIntegerKind(dimKind)) {
        error_ = "dispatch dimensions require integer expressions";
        return false;
      }
    }
    const auto &kernelParams = paramsByDef_[kernelPath];
    if (stmt.args.size() != kernelParams.size() + 4) {
      error_ = "dispatch argument count mismatch for " + kernelPath;
      return false;
    }
    for (size_t i = 4; i < stmt.args.size(); ++i) {
      if (!validateExpr(params, locals, stmt.args[i])) {
        return false;
      }
    }
    return true;
  }
  if (isSimpleCallName(stmt, "buffer_store")) {
    if (!currentDefinitionIsCompute_) {
      error_ = "buffer_store requires a compute definition";
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "buffer_store does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "buffer_store requires buffer, index, and value arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "buffer_store does not accept block arguments";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[0]) || !validateExpr(params, locals, stmt.args[1]) ||
        !validateExpr(params, locals, stmt.args[2])) {
      return false;
    }
    std::string elemType;
    if (!resolveBufferElemType(stmt.args[0], elemType)) {
      error_ = "buffer_store requires Buffer input";
      return false;
    }
    ReturnKind elemKind = returnKindForTypeName(elemType);
    if (!isNumericOrBoolKind(elemKind)) {
      error_ = "buffer_store requires numeric/bool element type";
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(stmt.args[1], params, locals);
    if (!isIntegerKind(indexKind)) {
      error_ = "buffer_store requires integer index";
      return false;
    }
    ReturnKind valueKind = inferExprReturnKind(stmt.args[2], params, locals);
    if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
      error_ = "buffer_store value type mismatch";
      return false;
    }
    return true;
  }
  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && defMap_.find(resolveCalleePath(stmt)) == defMap_.end()) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error_ = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
               " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return false;
    }
    if (activeEffects_.count(pathSpaceBuiltin.requiredEffect) == 0) {
      error_ = pathSpaceBuiltin.name + " requires " + pathSpaceBuiltin.requiredEffect + " effect";
      return false;
    }
    auto isStringExpr = [&](const Expr &candidate) -> bool {
      if (candidate.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      auto isStringTypeName = [&](const std::string &typeName) -> bool {
        return normalizeBindingTypeName(typeName) == "string";
      };
      auto isStringArrayBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "array" && binding.typeName != "vector") {
          return false;
        }
        return isStringTypeName(binding.typeTemplateArg);
      };
      auto isStringMapBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(binding.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        return isStringTypeName(parts[1]);
      };
      auto isStringCollectionTarget = [&](const Expr &target) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            return isStringArrayBinding(*paramBinding) || isStringMapBinding(*paramBinding);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            return isStringArrayBinding(it->second) || isStringMapBinding(it->second);
          }
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
            return false;
          }
          if (!getBuiltinCollectionName(target, collection)) {
            return false;
          }
          if ((collection == "array" || collection == "vector") && target.templateArgs.size() == 1) {
            return isStringTypeName(target.templateArgs.front());
          }
          if (collection == "map" && target.templateArgs.size() == 2) {
            return isStringTypeName(target.templateArgs[1]);
          }
        }
        return false;
      };
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          return isStringTypeName(paramBinding->typeName);
        }
        auto it = locals.find(candidate.name);
        return it != locals.end() && isStringTypeName(it->second.typeName);
      }
      if (candidate.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(candidate)) == defMap_.end() &&
            getBuiltinArrayAccessName(candidate, accessName) && candidate.args.size() == 2) {
          return isStringCollectionTarget(candidate.args.front());
        }
      }
      return false;
    };
    if (!isStringExpr(stmt.args.front())) {
      error_ = pathSpaceBuiltin.name + " requires string path argument";
      return false;
    }
    for (const auto &arg : stmt.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }
  std::string vectorHelper;
  if (getVectorStatementHelperName(stmt, vectorHelper)) {
    std::string vectorHelperResolved = resolveCalleePath(stmt);
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isNamespacedCollectionHelperCall =
        getNamespacedCollectionHelperName(stmt, namespacedCollection, namespacedHelper);
    const bool isNamespacedVectorHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "vector";
    const bool isUserMethodTarget =
        stmt.isMethodCall && defMap_.find(vectorHelperResolved) != defMap_.end() &&
        vectorHelperResolved.rfind("/vector/", 0) != 0 && vectorHelperResolved.rfind("/soa_vector/", 0) != 0;
    if (isUserMethodTarget) {
      return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
    }
    bool hasResolvedReceiverIndex = false;
    size_t resolvedReceiverIndex = 0;
    if (stmt.isMethodCall && !stmt.args.empty()) {
      hasResolvedReceiverIndex = true;
      resolvedReceiverIndex = 0;
    }
    if ((defMap_.find(vectorHelperResolved) == defMap_.end() || isNamespacedVectorHelperCall) &&
        !stmt.args.empty()) {
      auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Name) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!resolveVectorBinding(candidate, binding) || binding == nullptr) {
          return false;
        }
        if (binding->typeName == "soa_vector") {
          return true;
        }
        return binding->typeName == "vector" && binding->isMutable;
      };
      std::vector<size_t> receiverIndices;
      auto appendReceiverIndex = [&](size_t index) {
        if (index >= stmt.args.size()) {
          return;
        }
        for (size_t existing : receiverIndices) {
          if (existing == index) {
            return;
          }
        }
        receiverIndices.push_back(index);
      };
      const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
      if (hasNamedArgs) {
        bool hasValuesNamedReceiver = false;
        for (size_t i = 0; i < stmt.args.size(); ++i) {
          if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
            appendReceiverIndex(i);
            hasValuesNamedReceiver = true;
          }
        }
        if (!hasValuesNamedReceiver) {
          appendReceiverIndex(0);
          for (size_t i = 1; i < stmt.args.size(); ++i) {
            appendReceiverIndex(i);
          }
        }
      } else {
        appendReceiverIndex(0);
      }
      const bool probePositionalReorderedReceiver =
          !hasNamedArgs && stmt.args.size() > 1 &&
          (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
           stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
           (stmt.args.front().kind == Expr::Kind::Name &&
            !isVectorHelperReceiverName(stmt.args.front())));
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
      for (size_t receiverIndex : receiverIndices) {
        if (receiverIndex >= stmt.args.size()) {
          continue;
        }
        const Expr &receiverCandidate = stmt.args[receiverIndex];
        std::string methodTarget;
        if (resolveVectorHelperTargetPath(receiverCandidate, vectorHelper, methodTarget)) {
          methodTarget = preferVectorStdlibHelperPath(methodTarget);
        }
        if (defMap_.count(methodTarget) > 0) {
          vectorHelperResolved = methodTarget;
          hasResolvedReceiverIndex = true;
          resolvedReceiverIndex = receiverIndex;
          break;
        }
      }
    }
    if (defMap_.find(vectorHelperResolved) != defMap_.end()) {
      Expr helperCall = stmt;
      helperCall.name = vectorHelperResolved;
      helperCall.isMethodCall = false;
      if (hasResolvedReceiverIndex && resolvedReceiverIndex > 0 && resolvedReceiverIndex < helperCall.args.size()) {
        std::swap(helperCall.args[0], helperCall.args[resolvedReceiverIndex]);
        if (helperCall.argNames.size() < helperCall.args.size()) {
          helperCall.argNames.resize(helperCall.args.size());
        }
        std::swap(helperCall.argNames[0], helperCall.argNames[resolvedReceiverIndex]);
      }
      return validateExpr(params, locals, helperCall, enclosingStatements, statementIndex);
    }
    if (defMap_.find(vectorHelperResolved) == defMap_.end()) {
      auto validateBuiltinNamedReceiverShape = [&](const std::string &helperName) -> bool {
        if (!hasNamedArguments(stmt.argNames) || stmt.args.empty()) {
          return true;
        }
        size_t receiverIndex = 0;
        bool hasExplicitValuesReceiver = false;
        for (size_t i = 0; i < stmt.args.size(); ++i) {
          if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
              *stmt.argNames[i] == "values") {
            receiverIndex = i;
            hasExplicitValuesReceiver = true;
            break;
          }
        }
        if (!hasExplicitValuesReceiver) {
          for (size_t i = 0; i < stmt.args.size(); ++i) {
            const BindingInfo *candidateBinding = nullptr;
            if (!resolveVectorBinding(stmt.args[i], candidateBinding) || candidateBinding == nullptr) {
              continue;
            }
            const bool allowSoaVectorTarget = helperName == "push" || helperName == "reserve";
            if (candidateBinding->typeName == "vector" ||
                (allowSoaVectorTarget && candidateBinding->typeName == "soa_vector")) {
              receiverIndex = i;
              break;
            }
          }
        }
        if (receiverIndex >= stmt.args.size()) {
          receiverIndex = 0;
        }
        if (!validateVectorHelperReceiver(stmt.args[receiverIndex], helperName)) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!validateVectorHelperTarget(stmt.args[receiverIndex], helperName.c_str(), binding)) {
          return false;
        }
        return true;
      };
      if (hasNamedArguments(stmt.argNames)) {
        if (!validateBuiltinNamedReceiverShape(vectorHelper)) {
          return false;
        }
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error_ = vectorHelper + " does not accept template arguments";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error_ = vectorHelper + " does not accept block arguments";
        return false;
      }
      if (vectorHelper == "push") {
        if (stmt.args.size() != 2) {
          error_ = "push requires exactly two arguments";
          return false;
        }
        if (!validateVectorHelperReceiver(stmt.args.front(), "push")) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!validateVectorHelperTarget(stmt.args.front(), "push", binding)) {
          return false;
        }
        if (activeEffects_.count("heap_alloc") == 0) {
          error_ = "push requires heap_alloc effect";
          return false;
        }
        if (!validateExpr(params, locals, stmt.args[1])) {
          return false;
        }
        if (!validateVectorElementType(stmt.args[1], binding->typeTemplateArg)) {
          return false;
        }
        return true;
      }
      if (vectorHelper == "reserve") {
        if (stmt.args.size() != 2) {
          error_ = "reserve requires exactly two arguments";
          return false;
        }
        if (!validateVectorHelperReceiver(stmt.args.front(), "reserve")) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!validateVectorHelperTarget(stmt.args.front(), "reserve", binding)) {
          return false;
        }
        if (activeEffects_.count("heap_alloc") == 0) {
          error_ = "reserve requires heap_alloc effect";
          return false;
        }
        if (!validateExpr(params, locals, stmt.args[1])) {
          return false;
        }
        if (!isIntegerExpr(stmt.args[1])) {
          error_ = "reserve requires integer capacity";
          return false;
        }
        return true;
      }
      if (vectorHelper == "remove_at" || vectorHelper == "remove_swap") {
        if (stmt.args.size() != 2) {
          error_ = vectorHelper + " requires exactly two arguments";
          return false;
        }
        if (!validateVectorHelperReceiver(stmt.args.front(), vectorHelper)) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
          return false;
        }
        if (!validateExpr(params, locals, stmt.args[1])) {
          return false;
        }
        if (!isIntegerExpr(stmt.args[1])) {
          error_ = vectorHelper + " requires integer index";
          return false;
        }
        return true;
      }
      if (vectorHelper == "pop" || vectorHelper == "clear") {
        if (stmt.args.size() != 1) {
          error_ = vectorHelper + " requires exactly one argument";
          return false;
        }
        if (!validateVectorHelperReceiver(stmt.args.front(), vectorHelper)) {
          return false;
        }
        const BindingInfo *binding = nullptr;
        if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
          return false;
        }
        return true;
      }
    }
  }
  auto resolveBodyArgumentTarget = [&](const Expr &callExpr, std::string &resolvedOut) {
    if (!callExpr.isMethodCall) {
      resolvedOut = preferVectorStdlibHelperPath(resolveCalleePath(callExpr));
      return;
    }
    if (callExpr.args.empty()) {
      resolvedOut = preferVectorStdlibHelperPath(resolveCalleePath(callExpr));
      return;
    }
    const Expr &receiver = callExpr.args.front();
    std::string methodName = callExpr.name;
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    if (getNamespacedCollectionHelperName(callExpr, namespacedCollection, namespacedHelper) &&
        !namespacedHelper.empty()) {
      methodName = namespacedHelper;
    }
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
      const std::string resolvedType = resolveCalleePath(receiver);
      if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
        resolvedOut = preferVectorStdlibHelperPath(resolvedType + "/" + methodName);
        return;
      }
    }
    auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
      if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
        return "";
      }
      auto callPathCandidates = [&](const std::string &path) {
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
        appendUnique(path);
        if (path.rfind("/array/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/array/").size());
          appendUnique("/vector/" + suffix);
          appendUnique("/std/collections/vector/" + suffix);
        } else if (path.rfind("/vector/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/vector/").size());
          appendUnique("/std/collections/vector/" + suffix);
          appendUnique("/array/" + suffix);
        } else if (path.rfind("/std/collections/vector/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
          appendUnique("/vector/" + suffix);
          appendUnique("/array/" + suffix);
        } else if (path.rfind("/map/", 0) == 0) {
          appendUnique("/std/collections/map/" + path.substr(std::string("/map/").size()));
        } else if (path.rfind("/std/collections/map/", 0) == 0) {
          appendUnique("/map/" + path.substr(std::string("/std/collections/map/").size()));
        }
        return candidates;
      };
      for (const auto &callPath : callPathCandidates(resolveCalleePath(receiverExpr))) {
        auto defIt = defMap_.find(callPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          continue;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
            continue;
          }
          if (base == "Pointer") {
            return "Pointer";
          }
          if (base == "Reference") {
            return "Reference";
          }
        }
      }
      return "";
    };
    std::string typeName;
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        typeName = paramBinding->typeName;
      } else {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          typeName = it->second.typeName;
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
      if (isPointerExpr(receiver, params, locals)) {
        typeName = "Pointer";
      } else if (isPointerLikeExpr(receiver, params, locals)) {
        typeName = "Reference";
      }
    }
    if (typeName == "Pointer" || typeName == "Reference") {
      resolvedOut = "/" + typeName + "/" + methodName;
      return;
    }
    if (typeName.empty()) {
      resolvedOut = preferVectorStdlibHelperPath(resolveCalleePath(callExpr));
      return;
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      resolvedOut = preferVectorStdlibHelperPath("/" + normalizeBindingTypeName(typeName) + "/" + methodName);
      return;
    }
    std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    resolvedOut = preferVectorStdlibHelperPath(resolvedType + "/" + methodName);
  };

  if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBuiltinBlockCall(stmt) && !stmt.isLambda) {
    std::string collectionName;
    if (defMap_.find(resolveCalleePath(stmt)) == defMap_.end() && getBuiltinCollectionName(stmt, collectionName)) {
      error_ = collectionName + " literal does not accept block arguments";
      return false;
    }
    std::string resolved;
    resolveBodyArgumentTarget(stmt, resolved);
    if (defMap_.count(resolved) == 0) {
      error_ = "block arguments require a definition target: " + resolved;
      return false;
    }
    Expr call = stmt;
    call.bodyArguments.clear();
    call.hasBodyArguments = false;
    if (!validateExpr(params, locals, call)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    std::vector<Expr> livenessStatements = stmt.bodyArguments;
    if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
      for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
        livenessStatements.push_back((*enclosingStatements)[idx]);
      }
    }
    BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             false,
                             true,
                             nullptr,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  }
  return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
}

} // namespace primec::semantics
