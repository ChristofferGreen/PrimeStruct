// soa-surface-audit: exempt
#include "SemanticsValidateBuiltinSoaMetadata.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateSoaBindingExtraction.h"
#include "StdlibCollectionSurfaceHelpers.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec {

bool localImportPathCoversTarget(const std::string &importPath, const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

bool hasVisiblePublicSoaHelperDefinition(const Program &program, std::string_view helperName) {
  const std::string canonicalPath = "/std/collections/soa/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if (def.fullPath == canonicalPath) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootSoaHelper(const Program &program, std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa/" + std::string(helperName);
  const std::string canonicalPath =
      "/std/collections/soa/" + std::string(helperName);
  auto matchesSoaReceiverType = [&](const Expr &parameter) {
    return extractBuiltinSoaVectorBinding(parameter).has_value() ||
           extractExperimentalSoaVectorBinding(parameter).has_value();
  };
  for (const Definition &def : program.definitions) {
    if ((def.fullPath == rootPath || def.fullPath == samePath ||
         def.fullPath == canonicalPath) &&
        !def.parameters.empty() &&
        matchesSoaReceiverType(def.parameters.front())) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, rootPath) ||
        localImportPathCoversTarget(importPath, samePath) ||
        localImportPathCoversTarget(importPath, canonicalPath)) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootSoaHelperForReceiverType(const Program &program,
                                            std::string_view helperName,
                                            std::string_view receiverTypeName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa/" + std::string(helperName);
  const std::string canonicalPath =
      "/std/collections/soa/" + std::string(helperName);
  auto matchesReceiverType = [&](const Expr &parameter) {
    if (receiverTypeName == semantics::internalSoaCollectionTypeName()) {
      return extractBuiltinSoaVectorBinding(parameter).has_value() ||
             extractExperimentalSoaVectorBinding(parameter).has_value();
    }
    if (receiverTypeName == "vector") {
      return extractBuiltinVectorBinding(parameter).has_value();
    }
    return false;
  };
  for (const Definition &def : program.definitions) {
    if ((def.fullPath == rootPath || def.fullPath == samePath ||
         def.fullPath == canonicalPath) &&
        !def.parameters.empty() &&
        matchesReceiverType(def.parameters.front())) {
      return true;
    }
  }
  return false;
}

bool hasVisibleExperimentalSoaSamePathHelper(const Program &program,
                                             std::string_view helperName) {
  // Only a genuine user /soa/<helper> shadow counts. Treating the
  // canonical /std/collections/soa/<helper> spelling (which every
  // `import /std/collections/soa/*` covers) as a same-path shadow made
  // the method-sugar rewrite below fabricate "/soa/<helper>" names with
  // no definition behind them, so soa<T>-returning call receivers died
  // with "unknown method: /std/collections/soa_vector/<helper>".
  const std::string samePath = "/soa/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if (def.fullPath != samePath || def.parameters.empty()) {
      continue;
    }
    if (extractExperimentalSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, samePath)) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootExperimentalSoaHelper(const Program &program,
                                         std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if (def.fullPath != rootPath || def.parameters.empty()) {
      continue;
    }
    if (extractExperimentalSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> candidateDefinitionPaths(const Expr &expr, const std::string &definitionNamespace) {
  std::vector<std::string> candidatePaths;
  if (!expr.name.empty() && expr.name.front() == '/') {
    candidatePaths.push_back(expr.name);
    return candidatePaths;
  }
  if (expr.isMethodCall) {
    if (!expr.namespacePrefix.empty()) {
      candidatePaths.push_back(expr.namespacePrefix + "/" + expr.name);
    }
    return candidatePaths;
  }
  if (!expr.namespacePrefix.empty()) {
    candidatePaths.push_back(expr.namespacePrefix + "/" + expr.name);
  }
  if (!definitionNamespace.empty()) {
    candidatePaths.push_back(definitionNamespace + "/" + expr.name);
  }
  candidatePaths.push_back("/" + expr.name);
  candidatePaths.push_back(expr.name);
  return candidatePaths;
}

bool isFieldOnlyStructDefinition(const Definition &def) {
  bool hasStructTransform = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return false;
    }
    if (semantics::isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
  }
  if (hasStructTransform) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement ||
      def.returnExpr.has_value()) {
    return false;
  }
  return std::all_of(def.statements.begin(), def.statements.end(), [](const Expr &stmt) {
    return stmt.isBinding;
  });
}

bool isReflectEnabledStructDefinition(const Definition &def) {
  return std::any_of(def.transforms.begin(), def.transforms.end(), [](const Transform &transform) {
    return transform.name == "reflect" || transform.name == "generate";
  });
}

bool validateBuiltinSoaHelperReturnMetadataExpr(
    const Expr &expr,
    const std::unordered_map<std::string, BuiltinSoaReturnInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &reflectedStructPaths,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::string &definitionNamespace,
    std::string &error);

bool validateBuiltinSoaHelperReturnMetadataStatements(
    const std::vector<Expr> &statements,
    const std::unordered_map<std::string, BuiltinSoaReturnInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &reflectedStructPaths,
    std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::string &definitionNamespace,
    std::string &error) {
  for (const Expr &stmt : statements) {
    if (!validateBuiltinSoaHelperReturnMetadataExpr(stmt,
                                                    soaCollectionReturnDefinitions,
                                                    structPaths,
                                                    reflectedStructPaths,
                                                    bindings,
                                                    definitionNamespace,
                                                    error)) {
      return false;
    }
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      if (!validateBuiltinSoaHelperReturnMetadataStatements(stmt.bodyArguments,
                                                            soaCollectionReturnDefinitions,
                                                            structPaths,
                                                            reflectedStructPaths,
                                                            bodyBindings,
                                                            definitionNamespace,
                                                            error)) {
        return false;
      }
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
  return true;
}

bool validateBuiltinSoaHelperReturnMetadataExpr(
    const Expr &expr,
    const std::unordered_map<std::string, BuiltinSoaReturnInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &reflectedStructPaths,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::string &definitionNamespace,
    std::string &error) {
  for (const Expr &arg : expr.args) {
    if (!validateBuiltinSoaHelperReturnMetadataExpr(arg,
                                                    soaCollectionReturnDefinitions,
                                                    structPaths,
                                                    reflectedStructPaths,
                                                    bindings,
                                                    definitionNamespace,
                                                    error)) {
      return false;
    }
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    if (!validateBuiltinSoaHelperReturnMetadataExpr(bodyArg,
                                                    soaCollectionReturnDefinitions,
                                                    structPaths,
                                                    reflectedStructPaths,
                                                    bindings,
                                                    definitionNamespace,
                                                    error)) {
      return false;
    }
  }
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return true;
  }
  const std::string helperName = builtinSoaCountHelperName(expr.name).empty()
                                     ? builtinSoaAccessHelperName(expr.name)
                                     : builtinSoaCountHelperName(expr.name);
  if (helperName.empty()) {
    return true;
  }
  if (helperName == "ref" || helperName == "ref_ref") {
    return true;
  }
  const bool helperArityMatches =
      (helperName == "count" || helperName == "count_ref") ? expr.args.size() == 1
                                                           : expr.args.size() == 2;
  if (!helperArityMatches || !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) || expr.hasBodyArguments) {
    return true;
  }
  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return true;
  }
  if (!receiver.isMethodCall) {
    return true;
  }
  auto appendMethodReceiverCandidatePath =
      [&](const Expr &callExpr, std::vector<std::string> &candidatePaths) {
        if (!callExpr.isMethodCall || callExpr.args.empty() ||
            callExpr.args.front().kind != Expr::Kind::Name) {
          return;
        }
        auto bindingIt = bindings.find(callExpr.args.front().name);
        if (bindingIt == bindings.end()) {
          return;
        }
        const std::string receiverTypePath =
            semantics::resolveStructTypePath(bindingIt->second.typeName,
                                             definitionNamespace,
                                             structPaths);
        if (receiverTypePath.empty()) {
          return;
        }
        candidatePaths.push_back(receiverTypePath + "/" + callExpr.name);
      };
  const BuiltinSoaReturnInfo *returnInfo = nullptr;
  std::vector<std::string> receiverCandidatePaths =
      candidateDefinitionPaths(receiver, definitionNamespace);
  appendMethodReceiverCandidatePath(receiver, receiverCandidatePaths);
  for (const std::string &candidatePath : receiverCandidatePaths) {
    auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
    if (returnIt != soaCollectionReturnDefinitions.end()) {
      returnInfo = &returnIt->second;
      break;
    }
  }
  if (returnInfo == nullptr || returnInfo->binding.typeTemplateArg.empty()) {
    return true;
  }
  const std::string elemType = semantics::normalizeBindingTypeName(returnInfo->binding.typeTemplateArg);
  if (returnInfo->templateParams.count(elemType) > 0) {
    return true;
  }
  const std::string structPath =
      semantics::resolveStructTypePath(elemType, returnInfo->namespacePrefix, structPaths);
  if (structPath.empty()) {
    if (elemType.find('<') == std::string::npos) {
      error = "meta.field_count requires struct type argument: " + elemType;
      return false;
    }
    return true;
  }
  if (reflectedStructPaths.count(structPath) == 0) {
    error = "meta.field_count requires reflect-enabled struct type argument: " + structPath;
    return false;
  }
  return true;
}

bool validateBuiltinSoaHelperReturnMetadataRequirements(Program &program,
                                                        std::string &error) {
  error.clear();
  std::unordered_map<std::string, BuiltinSoaReturnInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> reflectedStructPaths;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      BuiltinSoaReturnInfo info;
      info.binding = *binding;
      info.namespacePrefix = def.namespacePrefix;
      info.templateParams.insert(def.templateArgs.begin(), def.templateArgs.end());
      soaCollectionReturnDefinitions[def.fullPath] = info;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = std::move(info);
      }
    }
    if (isFieldOnlyStructDefinition(def)) {
      structPaths.insert(def.fullPath);
      if (isReflectEnabledStructDefinition(def)) {
        reflectedStructPaths.insert(def.fullPath);
      }
    }
  }
  if (soaCollectionReturnDefinitions.empty()) {
    return true;
  }
  for (const Definition &def : program.definitions) {
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    if (!validateBuiltinSoaHelperReturnMetadataStatements(def.statements,
                                                          soaCollectionReturnDefinitions,
                                                          structPaths,
                                                          reflectedStructPaths,
                                                          bindings,
                                                          definitionNamespace,
                                                          error)) {
      return false;
    }
    if (def.returnExpr.has_value() &&
        !validateBuiltinSoaHelperReturnMetadataExpr(*def.returnExpr,
                                                    soaCollectionReturnDefinitions,
                                                    structPaths,
                                                    reflectedStructPaths,
                                                    bindings,
                                                    definitionNamespace,
                                                    error)) {
      return false;
    }
  }
  return true;
}

std::optional<semantics::BindingInfo> extractParsedBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr,
          expr.namespacePrefix,
          structTypes != nullptr ? *structTypes : emptyStructTypes,
          emptyImportAliases,
          binding,
          restrictType,
          parseError)) {
    return std::nullopt;
  }
  return binding;
}

std::optional<semantics::BindingInfo> extractParsedOrExperimentalSoaBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes) {
  if (auto parsed = extractParsedBindingInfo(expr, structTypes); parsed.has_value()) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(parsed->typeName);
    if (expr.isBinding && normalizedType == "auto" && expr.args.size() == 1) {
      const Expr &initializer = expr.args.front();
      if (initializer.kind == Expr::Kind::Call && !initializer.isBinding &&
          !initializer.templateArgs.empty()) {
        std::string initPath = initializer.name;
        if (!initPath.empty() && initPath.front() != '/') {
          initPath.insert(initPath.begin(), '/');
        }
        const size_t leafStart = initPath.find_last_of('/');
        const size_t generatedSuffix =
            initPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
        if (generatedSuffix != std::string::npos) {
          initPath.erase(generatedSuffix);
        }
        if (initPath == "/std/collections/soa/soa" ||
            initPath == "/std/collections/soa/single" ||
            initPath == "/std/collections/soa/from_aos" ||
            initPath == collection_paths::memberPath(collection_paths::kInternalSoaVectorFolder, "soaVectorNew") ||
            initPath == collection_paths::memberPath(collection_paths::kInternalSoaVectorFolder, "soaVectorSingle") ||
            initPath == collection_paths::memberPath(collection_paths::kInternalSoaVectorFolder, "soaVectorFromAos") ||
            initPath == collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorNew") ||
            initPath == collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorSingle") ||
            initPath == collection_paths::memberPath(collection_paths::kExperimentalSoaVectorFolder, "soaVectorFromAos")) {
          semantics::BindingInfo inferred = *parsed;
          inferred.typeName = "SoaVector";
          inferred.typeTemplateArg = initializer.templateArgs.front();
          return inferred;
        }
      }
    }
    return parsed;
  }
  if (!expr.isBinding) {
    return std::nullopt;
  }
  return extractExperimentalSoaVectorFieldViewReceiverBinding(expr);
}

std::string resolveStructReceiverPathFromBinding(
    const semantics::BindingInfo &binding,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structPaths) {
  std::string typeText = semantics::normalizeBindingTypeName(
      binding.typeTemplateArg.empty() ? binding.typeName : binding.typeName + "<" + binding.typeTemplateArg + ">");
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = semantics::normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      typeText = base;
      break;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return {};
    }
    typeText = semantics::normalizeBindingTypeName(args.front());
  }
  return semantics::resolveStructTypePath(typeText, namespacePrefix, structPaths);
}

std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromTypeText(
    const std::string &typeText,
    std::string_view expectedBase) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    return std::nullopt;
  }
  const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
  const bool matchesSoaExpectedBase =
      expectedBase == semantics::internalSoaCollectionTypeName() &&
      semantics::isInternalOrExperimentalSoaStorageTypePath(normalizedBase);
  const bool matchesExpectedBase =
      normalizedBase == expectedBase ||
      matchesSoaExpectedBase;
  if (!matchesExpectedBase) {
    return std::nullopt;
  }
  semantics::BindingInfo binding;
  binding.typeName = std::string(expectedBase);
  binding.typeTemplateArg = argText;
  return binding;
}

std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromWrappedTypeText(
    const std::string &typeText,
    std::string_view expectedBase) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    return std::nullopt;
  }
  const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
  if (normalizedBase == "args") {
    return extractBuiltinCollectionBindingFromWrappedTypeText(argText, expectedBase);
  }
  if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
    return std::nullopt;
  }
  return extractBuiltinCollectionBindingFromTypeText(argText, expectedBase);
}

std::optional<BuiltinSoaReceiverBindingInfo> extractBuiltinSoaReceiverBinding(
    const semantics::BindingInfo &binding) {
  if (isBuiltinSoaVectorBinding(binding)) {
    return BuiltinSoaReceiverBindingInfo{binding, false};
  }
  if (auto wrapped = extractBuiltinCollectionBindingFromWrappedTypeText(
          bindingTypeText(binding), semantics::internalSoaCollectionTypeName());
      wrapped.has_value()) {
    return BuiltinSoaReceiverBindingInfo{*wrapped, true};
  }
  std::string elemType;
  if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType)) {
    semantics::BindingInfo canonicalBinding;
    canonicalBinding.typeName = semantics::internalSoaCollectionTypeName();
    canonicalBinding.typeTemplateArg = elemType;
    const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
    const bool borrowed = normalizedType == "Reference" || normalizedType == "Pointer";
    return BuiltinSoaReceiverBindingInfo{canonicalBinding, borrowed};
  }
  return std::nullopt;
}

std::string stripSoaSurfaceHelperPrefix(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  std::string helperName;
  if (semantics::splitSoaSurfaceHelperPath(normalized, &helperName, nullptr)) {
    return helperName;
  }
  return normalized;
}

std::string builtinSoaConversionMethodName(std::string_view methodName) {
  std::string normalized = stripSoaSurfaceHelperPrefix(methodName);
  if (normalized == methodName || (!methodName.empty() && methodName.front() == '/')) {
    const std::string vectorCollectionPrefix =
        std::string("std/collections/") + "vector" + "/";
    if (normalized.rfind(vectorCollectionPrefix, 0) == 0) {
      normalized = normalized.substr(vectorCollectionPrefix.size());
    }
  }
  if (normalized == "to_soa") {
    return "to_soa";
  }
  if (normalized == "to_aos") {
    return "to_aos";
  }
  return {};
}

std::string builtinSoaAccessHelperName(std::string_view rawName) {
  const std::string normalized = stripSoaSurfaceHelperPrefix(rawName);
  if (normalized == "get" || normalized == "get_ref" ||
      normalized == "ref" || normalized == "ref_ref") {
    return normalized;
  }
  return {};
}

std::string borrowedBuiltinSoaAccessHelperName(std::string_view helperName) {
  if (helperName == "get" || helperName == "get_ref") {
    return "get_ref";
  }
  if (helperName == "ref" || helperName == "ref_ref") {
    return "ref_ref";
  }
  return {};
}

std::string builtinSoaCountHelperName(std::string_view rawName) {
  const std::string normalized = stripSoaSurfaceHelperPrefix(rawName);
  if (normalized == "count" || normalized == "count_ref") {
    return normalized;
  }
  return {};
}

std::string borrowedBuiltinSoaCountHelperName(std::string_view helperName) {
  if (helperName == "count" || helperName == "count_ref") {
    return "count_ref";
  }
  return {};
}

bool isOldExplicitSoaCountHelperName(std::string_view rawName) {
  bool usesPublicSurface = false;
  std::string helperName;
  return semantics::splitSoaSurfaceHelperPath(
             rawName, &helperName, &usesPublicSurface) &&
         !usesPublicSurface &&
         (helperName == "count" || helperName == "count_ref");
}

std::string builtinSoaMutatorHelperName(std::string_view rawName) {
  const std::string normalized = stripSoaSurfaceHelperPrefix(rawName);
  if (normalized == "push" || normalized == "reserve") {
    return normalized;
  }
  return {};
}

std::string oldExplicitSoaMutatorHelperName(std::string_view rawName) {
  bool usesPublicSurface = false;
  std::string normalized;
  if (!semantics::splitSoaSurfaceHelperPath(
          rawName, &normalized, &usesPublicSurface) ||
      usesPublicSurface) {
    return {};
  }
  if (normalized == "push" || normalized == "reserve") {
    return normalized;
  }
  return {};
}

std::vector<std::string> candidatePathsForExprCall(
    const Expr &callExpr,
    const std::string &definitionNamespace,
    const std::unordered_map<std::string, semantics::BindingInfo> *bindings,
    const std::unordered_set<std::string> *structPaths) {
  std::vector<std::string> candidatePaths;
  if (callExpr.isMethodCall && !callExpr.args.empty() && bindings != nullptr && structPaths != nullptr &&
      callExpr.args.front().kind == Expr::Kind::Name) {
    const Expr &receiver = callExpr.args.front();
    auto bindingIt = bindings->find(receiver.name);
    if (bindingIt != bindings->end()) {
      const std::string receiverNamespace =
          !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : definitionNamespace;
      const std::string receiverStructPath = resolveStructReceiverPathFromBinding(
          bindingIt->second, receiverNamespace, *structPaths);
      if (!receiverStructPath.empty()) {
        candidatePaths.push_back(receiverStructPath + "/" + callExpr.name);
      }
    }
  }
  if (callExpr.isMethodCall && !callExpr.args.empty() && structPaths != nullptr &&
      callExpr.args.front().kind == Expr::Kind::Call) {
    // Struct-literal / constructor receivers (Holder{}.cloneValues()):
    // resolve the constructor name to a struct path so the member helper
    // definition is a candidate - otherwise chained soa helper methods on
    // such receivers never reach the method desugar and their canonical
    // targets stay unmaterialized in lowering.
    const Expr &receiver = callExpr.args.front();
    if (!receiver.name.empty()) {
      std::string receiverPath = receiver.name;
      if (receiverPath.front() != '/') {
        std::string prefix = !receiver.namespacePrefix.empty()
                                 ? receiver.namespacePrefix
                                 : definitionNamespace;
        if (!prefix.empty() && prefix.front() != '/') {
          prefix.insert(prefix.begin(), '/');
        }
        receiverPath = prefix.empty() ? "/" + receiverPath
                                      : prefix + "/" + receiverPath;
      }
      if (structPaths->count(receiverPath) > 0) {
        candidatePaths.push_back(receiverPath + "/" + callExpr.name);
      }
    }
  }
  if (!callExpr.name.empty() && callExpr.name.front() == '/') {
    candidatePaths.push_back(callExpr.name);
    return candidatePaths;
  }
  if (callExpr.isMethodCall) {
    if (!callExpr.namespacePrefix.empty()) {
      candidatePaths.push_back(callExpr.namespacePrefix + "/" + callExpr.name);
    }
    return candidatePaths;
  }
  if (!callExpr.namespacePrefix.empty()) {
    candidatePaths.push_back(callExpr.namespacePrefix + "/" + callExpr.name);
  }
  if (!definitionNamespace.empty()) {
    candidatePaths.push_back(definitionNamespace + "/" + callExpr.name);
  }
  candidatePaths.push_back("/" + callExpr.name);
  candidatePaths.push_back(callExpr.name);
  return candidatePaths;
}

Expr canonicalizeResolvedCallPath(const Expr &callExpr, const std::string &resolvedPath) {
  Expr rewritten = callExpr;
  rewritten.name = resolvedPath;
  rewritten.namespacePrefix.clear();
  rewritten.isMethodCall = false;
  rewritten.isFieldAccess = false;
  return rewritten;
}

} // namespace primec
