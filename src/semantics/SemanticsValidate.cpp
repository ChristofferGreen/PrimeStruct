#include "primec/Semantics.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateTransforms.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidator.h"
#include "TypeResolutionGraphPreparation.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec {

namespace semantics {
bool monomorphizeTemplates(Program &program, const std::string &entryPath, std::string &error);

namespace {

std::string returnKindSnapshotName(ReturnKind kind) {
  switch (kind) {
    case ReturnKind::Unknown:
      return "unknown";
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Void:
      return "void";
    case ReturnKind::Array:
      return "array";
  }
  return "unknown";
}

std::string bindingTypeTextForSnapshot(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

template <typename CaptureFn>
bool runTypeResolutionSnapshot(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &semanticTransforms,
    CaptureFn &&capture) {
  error.clear();
  if (!semantics::prepareProgramForTypeResolutionAnalysis(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }

  const std::vector<std::string> defaults = {"io_out", "io_err"};
  semantics::SemanticsValidator validator(program, entryPath, error, defaults, defaults, nullptr, false);
  if (!validator.run()) {
    return false;
  }
  capture(validator);
  return true;
}

} // namespace
}

namespace {

bool isExperimentalMapTypeText(const std::string &typeText) {
  std::string keyType;
  std::string valueType;
  if (!semantics::extractMapKeyValueTypesFromTypeText(typeText, keyType, valueType)) {
    return false;
  }
  const std::string normalizedInner = semantics::normalizeBindingTypeName(typeText);
  return normalizedInner == "Map" ||
         normalizedInner.rfind("Map<", 0) == 0 ||
         normalizedInner.rfind("/std/collections/experimental_map/Map", 0) == 0 ||
         normalizedInner.rfind("std/collections/experimental_map/Map", 0) == 0;
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalMapBinding(const Expr &expr) {
  semantics::BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "Reference" &&
        transform.templateArgs.size() == 1 &&
        isExperimentalMapTypeText(transform.templateArgs.front())) {
      info.typeName = "Reference";
      info.typeTemplateArg = transform.templateArgs.front();
      return info;
    }
  }
  return std::nullopt;
}

std::string bindingTypeText(const semantics::BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isExperimentalMapValueBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalMapTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractExperimentalMapBinding(const Expr &expr) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  if (!isExperimentalMapValueBinding(binding)) {
    return std::nullopt;
  }
  return binding;
}

bool isBorrowedExperimentalMapBinding(const semantics::BindingInfo &binding) {
  return semantics::normalizeBindingTypeName(binding.typeName) == "Reference" &&
         isExperimentalMapTypeText(binding.typeTemplateArg);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalMapReturnBinding(
    const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(transform.templateArgs.front(), base, argText)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(base) != "Reference") {
      continue;
    }
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1 ||
        !isExperimentalMapTypeText(args.front())) {
      continue;
    }
    semantics::BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = args.front();
    return info;
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalMapReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    std::string base;
    std::string argText;
    semantics::BindingInfo binding;
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isExperimentalMapValueBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::string borrowedExperimentalMapHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "mapCountRef";
  }
  if (methodName == "contains") {
    return "mapContainsRef";
  }
  if (methodName == "tryAt") {
    return "mapTryAtRef";
  }
  if (methodName == "at") {
    return "mapAtRef";
  }
  if (methodName == "at_unsafe") {
    return "mapAtUnsafeRef";
  }
  if (methodName == "insert") {
    return "mapInsertRef";
  }
  return {};
}

std::string experimentalMapValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "mapCount";
  }
  if (methodName == "contains") {
    return "mapContains";
  }
  if (methodName == "tryAt") {
    return "mapTryAt";
  }
  if (methodName == "at") {
    return "mapAt";
  }
  if (methodName == "at_unsafe") {
    return "mapAtUnsafe";
  }
  if (methodName == "insert") {
    return "mapInsert";
  }
  return {};
}

bool isBuiltinVectorTypeText(const std::string &typeText) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return normalizedType == "vector" || normalizedType.rfind("vector<", 0) == 0;
}

bool isBuiltinSoaVectorTypeText(const std::string &typeText) {
  std::string rawType = typeText;
  if (!rawType.empty() && rawType.front() == '/') {
    rawType.erase(rawType.begin());
  }
  if (rawType == "SoaVector" || rawType.rfind("SoaVector<", 0) == 0 ||
      rawType.rfind("SoaVector__", 0) == 0 ||
      rawType == "std/collections/experimental_soa_vector/SoaVector" ||
      rawType.rfind("std/collections/experimental_soa_vector/SoaVector<", 0) == 0 ||
      rawType.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0) {
    return false;
  }
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return normalizedType == "soa_vector" || normalizedType.rfind("soa_vector<", 0) == 0;
}

bool isExperimentalSoaVectorBaseName(const std::string &typeName) {
  std::string normalizedType = typeName;
  if (!normalizedType.empty() && normalizedType.front() == '/') {
    normalizedType.erase(normalizedType.begin());
  }
  return normalizedType == "SoaVector" || normalizedType.rfind("SoaVector__", 0) == 0 ||
         normalizedType == "std/collections/experimental_soa_vector/SoaVector" ||
         normalizedType.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0;
}

bool isExperimentalSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalSoaVectorBaseName(binding.typeName);
}

bool isBuiltinVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinVectorTypeText(bindingTypeText(binding));
}

bool isBuiltinSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isBuiltinSoaVectorTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractBuiltinVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isExperimentalSoaVectorBinding(binding) ? std::optional<semantics::BindingInfo>(binding) : std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isBuiltinVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string normalizedReturnType =
        semantics::normalizeBindingTypeName(transform.templateArgs.front());
    if (semantics::splitTemplateTypeName(normalizedReturnType, base, argText)) {
      if (semantics::normalizeBindingTypeName(base) == "Reference" ||
          semantics::normalizeBindingTypeName(base) == "Pointer") {
        continue;
      }
      binding.typeName = semantics::normalizeBindingTypeName(base);
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if (isBuiltinSoaVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    semantics::BindingInfo binding;
    std::string base;
    std::string argText;
    const std::string returnType = transform.templateArgs.front();
    if (semantics::splitTemplateTypeName(returnType, base, argText)) {
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        continue;
      }
      binding.typeName = base;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = returnType;
    }
    if (isExperimentalSoaVectorBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

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

bool hasVisibleRootToAosHelper(const Program &program, std::string_view receiverTypeName) {
  for (const Definition &def : program.definitions) {
    if (def.fullPath != "/to_aos" || def.parameters.empty()) {
      continue;
    }
    if (receiverTypeName == "soa_vector" &&
        extractBuiltinSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
    if (receiverTypeName == "vector" &&
        extractBuiltinVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, "/to_aos")) {
      return true;
    }
  }
  return false;
}

bool hasVisibleRootSoaHelper(const Program &program, std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa_vector/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if ((def.fullPath == rootPath || def.fullPath == samePath) &&
        !def.parameters.empty() &&
        extractBuiltinSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, rootPath) ||
        localImportPathCoversTarget(importPath, samePath)) {
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

std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromTypeText(
    const std::string &typeText,
    std::string_view expectedBase) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(normalizedType, base, argText) ||
      semantics::normalizeBindingTypeName(base) != expectedBase) {
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

std::string builtinSoaConversionMethodName(std::string_view methodName) {
  std::string normalized(methodName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("vector/", 0) == 0) {
    normalized = normalized.substr(std::string("vector/").size());
  } else if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
    normalized = normalized.substr(std::string("std/collections/vector/").size());
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
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "get" || normalized == "ref") {
    return normalized;
  }
  return {};
}

std::string builtinSoaCountHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "count") {
    return normalized;
  }
  return {};
}

bool isOldExplicitSoaCountHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "soa_vector/count";
}

std::string builtinSoaMutatorHelperName(std::string_view rawName) {
  std::string normalized(rawName);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized.rfind("soa_vector/", 0) == 0) {
    normalized = normalized.substr(std::string("soa_vector/").size());
  }
  if (normalized == "push" || normalized == "reserve") {
    return normalized;
  }
  return {};
}

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBuiltinSoaConversionMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaConversionMethodExpr(
        stmt, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaConversionMethodStatements(
          stmt.bodyArguments, bodyBindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaConversionMethodExpr(
        arg, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = builtinSoaConversionMethodName(expr.name);
  if (helperName.empty()) {
    return;
  }
  auto matchesBuiltinReceiverBinding = [&](const semantics::BindingInfo &binding) {
    if (helperName == "to_soa") {
      return isBuiltinVectorBinding(binding);
    }
    if (helperName == "to_aos") {
      return isBuiltinSoaVectorBinding(binding);
    }
    return false;
  };
  const auto &returnDefinitions =
      helperName == "to_soa" ? vectorReturnDefinitions : soaVectorReturnDefinitions;
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && matchesBuiltinReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = returnDefinitions.find(candidatePath);
      if (returnIt != returnDefinitions.end() &&
          matchesBuiltinReceiverBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
}

bool rewriteBuiltinSoaConversionMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaConversionMethodStatements(
        def.statements, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaConversionMethodExpr(
          *def.returnExpr, bindings, vectorReturnDefinitions, soaVectorReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper);

void rewriteBuiltinSoaToAosCallStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaToAosCallExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaToAosCallStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        builtinSoaConversionMethodName(candidate.name) == "to_soa" &&
        candidate.args.size() == 1) {
      auto vectorBinding = findBuiltinVectorValueBinding(candidate.args.front());
      if (!vectorBinding.has_value() || vectorBinding->typeTemplateArg.empty()) {
        return std::nullopt;
      }
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = vectorBinding->typeTemplateArg;
      return binding;
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaToAosCallExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
  }
  if (expr.kind != Expr::Kind::Call ||
      expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty() ||
      builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }
  if ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
      (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper)) {
    return;
  }

  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding = receiverBinding.has_value()
                                         ? std::optional<semantics::BindingInfo>{}
                                         : findBuiltinVectorValueBinding(expr.args.front());

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/to_aos";
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaToAosCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveVisibleRootSoaHelper =
      hasVisibleRootToAosHelper(program, "soa_vector");
  const bool preserveVisibleRootVectorHelper =
      hasVisibleRootToAosHelper(program, "vector");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaToAosCallStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaToAosCallExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveVisibleRootSoaHelper,
          preserveVisibleRootVectorHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveRefHelper);

void rewriteBuiltinSoaAccessStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveRefHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaAccessExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveRefHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaAccessStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveRefHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaAccessExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveRefHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaAccessExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveRefHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaAccessHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "get" && preserveGetHelper) ||
      (helperName == "ref" && preserveRefHelper)) {
    return;
  }
  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }

  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding = receiverBinding.has_value()
                                         ? std::optional<semantics::BindingInfo>{}
                                         : findBuiltinVectorValueBinding(expr.args.front());

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/" + helperName;
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaAccessCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveGetHelper = hasVisibleRootSoaHelper(program, "get");
  const bool preserveRefHelper = hasVisibleRootSoaHelper(program, "ref");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaAccessStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveRefHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaAccessExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveRefHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper);

void rewriteBuiltinSoaCountStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaCountExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaCountStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveCountHelper);
    }
    if (stmt.isBinding) {
      if (auto vectorBinding = extractBuiltinVectorBinding(stmt); vectorBinding.has_value()) {
        bindings[stmt.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper) {
  auto findBuiltinVectorValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = vectorReturnDefinitions.find(candidatePath);
      if (returnIt != vectorReturnDefinitions.end() && isBuiltinVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "vector");
        }
      }
    }
    return std::nullopt;
  };
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaCountExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaCountHelperName(expr.name);
  if (helperName.empty() || preserveCountHelper) {
    return;
  }

  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const bool explicitOldSoaCount = isOldExplicitSoaCountHelperName(expr.name);
  const auto fallbackVectorBinding =
      receiverBinding.has_value() || !explicitOldSoaCount
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/count";
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaCountCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveCountHelper = hasVisibleRootSoaHelper(program, "count");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto vectorBinding = extractBuiltinVectorBinding(param); vectorBinding.has_value()) {
        bindings[param.name] = *vectorBinding;
      } else if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaCountStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preserveCountHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaCountExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preserveCountHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper);

void rewriteBuiltinSoaMutatorStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaMutatorExpr(
        stmt,
        bindings,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaMutatorStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper);
    }
    if (stmt.isBinding) {
      if (auto soaBinding = extractBuiltinSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper) {
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate) -> std::optional<semantics::BindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end() && isBuiltinSoaVectorBinding(bindingIt->second)) {
        return bindingIt->second;
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "soa_vector" &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = "soa_vector";
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
        return returnIt->second;
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second), "soa_vector");
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaMutatorExpr(
        arg,
        bindings,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaMutatorHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "push" && preservePushHelper) ||
      (helperName == "reserve" && preserveReserveHelper)) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/soa_vector/" + helperName;
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (!receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaMutatorCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preservePushHelper = hasVisibleRootSoaHelper(program, "push");
  const bool preserveReserveHelper = hasVisibleRootSoaHelper(program, "reserve");
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto soaBinding = extractBuiltinSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinSoaMutatorStatements(
        def.statements,
        bindings,
        soaVectorReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaMutatorExpr(
          *def.returnExpr,
          bindings,
          soaVectorReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaToAosMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaToAosMethodExpr(stmt, bindings, soaVectorReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaToAosMethodStatements(
          stmt.bodyArguments, bodyBindings, soaVectorReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto soaBinding = extractExperimentalSoaVectorBinding(stmt); soaBinding.has_value()) {
        bindings[stmt.name] = *soaBinding;
      }
    }
  }
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaVectorReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaToAosMethodExpr(arg, bindings, soaVectorReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  if (builtinSoaConversionMethodName(expr.name) != "to_aos") {
    return;
  }

  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalSoaVectorBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = soaVectorReturnDefinitions.find(candidatePath);
      if (returnIt != soaVectorReturnDefinitions.end() &&
          isExperimentalSoaVectorBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = "/std/collections/experimental_soa_vector_conversions/soaVectorToAos";
  expr.namespacePrefix.clear();
}

bool rewriteExperimentalSoaToAosMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaVectorReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorReturnBinding(def); binding.has_value()) {
      soaVectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaVectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto soaBinding = extractExperimentalSoaVectorBinding(param); soaBinding.has_value()) {
        bindings[param.name] = *soaBinding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaToAosMethodStatements(
        def.statements, bindings, soaVectorReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaToAosMethodExpr(
          *def.returnExpr, bindings, soaVectorReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteBorrowedExperimentalMapMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBorrowedExperimentalMapMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBorrowedExperimentalMapMethodExpr(
        stmt, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBorrowedExperimentalMapMethodStatements(
          stmt.bodyArguments, bodyBindings, borrowedReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractBorrowedExperimentalMapBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteBorrowedExperimentalMapMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBorrowedExperimentalMapMethodExpr(
        arg, bindings, borrowedReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = borrowedExperimentalMapHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedExperimentalMapBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = borrowedReturnDefinitions.find(candidatePath);
      if (returnIt != borrowedReturnDefinitions.end() &&
          isBorrowedExperimentalMapBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(
          receiverBinding->typeTemplateArg, keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

bool rewriteBorrowedExperimentalMapMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> borrowedReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBorrowedExperimentalMapReturnBinding(def); binding.has_value()) {
      borrowedReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        borrowedReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractBorrowedExperimentalMapBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBorrowedExperimentalMapMethodStatements(
        def.statements, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBorrowedExperimentalMapMethodExpr(
          *def.returnExpr, bindings, borrowedReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalMapValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteExperimentalMapValueMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalMapValueMethodExpr(stmt, bindings, valueReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalMapValueMethodStatements(
          stmt.bodyArguments, bodyBindings, valueReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractExperimentalMapBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalMapValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalMapValueMethodExpr(arg, bindings, valueReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = experimentalMapValueHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalMapValueBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    std::vector<std::string> candidatePaths;
    if (!receiver.name.empty() && receiver.name.front() == '/') {
      candidatePaths.push_back(receiver.name);
    } else {
      if (!receiver.namespacePrefix.empty()) {
        candidatePaths.push_back(receiver.namespacePrefix + "/" + receiver.name);
      }
      if (!definitionNamespace.empty()) {
        candidatePaths.push_back(definitionNamespace + "/" + receiver.name);
      }
      candidatePaths.push_back("/" + receiver.name);
      candidatePaths.push_back(receiver.name);
    }
    for (const std::string &candidatePath : candidatePaths) {
      auto returnIt = valueReturnDefinitions.find(candidatePath);
      if (returnIt != valueReturnDefinitions.end() &&
          isExperimentalMapValueBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(bindingTypeText(*receiverBinding), keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = helperName;
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

bool rewriteExperimentalMapValueMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> valueReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalMapReturnBinding(def); binding.has_value()) {
      valueReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        valueReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractExperimentalMapBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalMapValueMethodStatements(
        def.statements, bindings, valueReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalMapValueMethodExpr(
          *def.returnExpr, bindings, valueReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

bool isBuiltinMapBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  if (isExperimentalMapTypeText(bindingTypeText(binding))) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return semantics::extractMapKeyValueTypesFromTypeText(
      bindingTypeText(binding), keyType, valueType);
}

std::optional<semantics::BindingInfo> extractBuiltinMapBinding(const Expr &expr) {
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  return isBuiltinMapBinding(binding) ? std::optional<semantics::BindingInfo>(binding)
                                      : std::nullopt;
}

constexpr std::string_view kBuiltinCanonicalMapInsertPath = "/std/collections/map/insert";
constexpr std::string_view kBuiltinCanonicalMapInsertPendingPath =
    "/std/collections/map/insert_builtin_pending";

void rewriteBuiltinMapInsertExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinMapInsertExpr(arg, bindings);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return;
  }

  const bool matchesBuiltinInsertMethod =
      expr.isMethodCall &&
      (expr.name == "insert" || expr.name == kBuiltinCanonicalMapInsertPath);
  const bool matchesBuiltinInsertCall =
      !expr.isMethodCall && expr.name == kBuiltinCanonicalMapInsertPath;
  if (!matchesBuiltinInsertMethod && !matchesBuiltinInsertCall) {
    return;
  }

  const Expr &receiver = expr.args.front();
  if (receiver.kind != Expr::Kind::Name) {
    return;
  }
  auto bindingIt = bindings.find(receiver.name);
  if (bindingIt == bindings.end() || !isBuiltinMapBinding(bindingIt->second)) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractMapKeyValueTypesFromTypeText(
          bindingTypeText(bindingIt->second), keyType, valueType)) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = std::string(kBuiltinCanonicalMapInsertPendingPath);
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

void rewriteBuiltinMapInsertStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings) {
  for (Expr &stmt : statements) {
    rewriteBuiltinMapInsertExpr(stmt, bindings);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinMapInsertStatements(stmt.bodyArguments, bodyBindings);
    }
    if (stmt.isBinding) {
      if (auto binding = extractBuiltinMapBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

bool rewriteBuiltinMapInsertMethods(Program &program, std::string &error) {
  error.clear();
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractBuiltinMapBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    rewriteBuiltinMapInsertStatements(def.statements, bindings);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinMapInsertExpr(*def.returnExpr, bindings);
    }
  }
  return true;
}

bool rewriteOmittedStructInitializers(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      fieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          fieldOnlyStruct = false;
          break;
        }
      }
    }
    if (hasStructTransform || fieldOnlyStruct) {
      structNames.insert(def.fullPath);
    }
  }

  std::unordered_set<std::string> publicDefinitions;
  publicDefinitions.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool sawPublic = false;
    bool sawPrivate = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "public") {
        sawPublic = true;
      } else if (transform.name == "private") {
        sawPrivate = true;
      }
    }
    if (sawPublic && !sawPrivate) {
      publicDefinitions.insert(def.fullPath);
    }
  }

  std::unordered_map<std::string, std::string> importAliases;
  auto isWildcardImport = [](const std::string &path, std::string &prefixOut) -> bool {
    if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
      prefixOut = path.substr(0, path.size() - 2);
      return true;
    }
    if (path.find('/', 1) == std::string::npos) {
      prefixOut = path;
      return true;
    }
    return false;
  };
  for (const auto &importPath : program.imports) {
    std::string prefix;
    if (isWildcardImport(importPath, prefix)) {
      std::string scopedPrefix = prefix;
      if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
        scopedPrefix += "/";
      }
      for (const auto &def : program.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions.count(def.fullPath) == 0) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (publicDefinitions.count(importPath) == 0) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }

  auto rewriteExpr = [&](auto &self, Expr &expr) -> bool {
    if (expr.isLambda) {
      for (auto &arg : expr.bodyArguments) {
        if (!self(self, arg)) {
          return false;
        }
      }
      return true;
    }
    for (auto &arg : expr.args) {
      if (!self(self, arg)) {
        return false;
      }
    }
    for (auto &arg : expr.bodyArguments) {
      if (!self(self, arg)) {
        return false;
      }
    }
    auto isEmptyBuiltinBlockInitializer = [&](const Expr &initializer) -> bool {
      if (!initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return false;
      }
      if (!initializer.args.empty() || !initializer.templateArgs.empty() ||
          semantics::hasNamedArguments(initializer.argNames)) {
        return false;
      }
      return initializer.kind == Expr::Kind::Call && !initializer.isBinding &&
             initializer.name == "block" && initializer.namespacePrefix.empty();
    };
    const bool omittedInitializer = expr.args.empty() ||
                                    (expr.args.size() == 1 && isEmptyBuiltinBlockInitializer(expr.args.front()));
    if (!omittedInitializer) {
      return true;
    }
    if (!expr.isBinding && expr.transforms.empty()) {
      return true;
    }
    semantics::BindingInfo info;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!semantics::parseBindingInfo(expr, expr.namespacePrefix, structNames, importAliases, info, restrictType, parseError)) {
      error = parseError;
      return false;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(info.typeName);
    if (!info.typeTemplateArg.empty()) {
      if (normalizedType != "vector" && normalizedType != "soa_vector") {
        error = "omitted initializer requires vector or soa_vector type: " + info.typeName;
        return false;
      }
      std::vector<std::string> templateArgs;
      if (!semantics::splitTopLevelTemplateArgs(info.typeTemplateArg, templateArgs) || templateArgs.size() != 1) {
        error = normalizedType + " requires exactly one template argument";
        return false;
      }
      Expr call;
      call.kind = Expr::Kind::Call;
      call.name = info.typeName;
      call.namespacePrefix = expr.namespacePrefix;
      call.templateArgs = std::move(templateArgs);
      expr.args.clear();
      expr.argNames.clear();
      expr.args.push_back(std::move(call));
      expr.argNames.push_back(std::nullopt);
      return true;
    }
    Expr call;
    call.kind = Expr::Kind::Call;
    call.name = info.typeName;
    call.namespacePrefix = expr.namespacePrefix;
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(call));
    expr.argNames.push_back(std::nullopt);
    return true;
  };

  for (auto &def : program.definitions) {
    for (auto &stmt : def.statements) {
      if (!rewriteExpr(rewriteExpr, stmt)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rewriteExpr(rewriteExpr, *def.returnExpr)) {
        return false;
      }
    }
  }
  for (auto &exec : program.executions) {
    for (auto &arg : exec.arguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
    for (auto &arg : exec.bodyArguments) {
      if (!rewriteExpr(rewriteExpr, arg)) {
        return false;
      }
    }
  }
  return true;
}
} // namespace

bool Semantics::validate(Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects,
                         const std::vector<std::string> &entryDefaultEffects,
                         const std::vector<std::string> &semanticTransforms,
                         SemanticDiagnosticInfo *diagnosticInfo,
                         bool collectDiagnostics) const {
  error.clear();
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool validationSucceeded = false;
  struct ValidationDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &validationSucceeded;

    ~ValidationDiagnosticScope() {
      if (!validationSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } validationDiagnosticScope{diagnosticSink, error, validationSucceeded};
  if (!semantics::applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!semantics::rewriteExperimentalGfxConstructors(program, error)) {
    return false;
  }
  if (!semantics::rewriteReflectionGeneratedHelpers(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaConversionMethods(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaToAosCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaAccessCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaCountCalls(program, error)) {
    return false;
  }
  if (!rewriteBuiltinSoaMutatorCalls(program, error)) {
    return false;
  }
  if (!rewriteExperimentalSoaToAosMethods(program, error)) {
    return false;
  }
  if (!rewriteBorrowedExperimentalMapMethods(program, error)) {
    return false;
  }
  if (!rewriteExperimentalMapValueMethods(program, error)) {
    return false;
  }
  if (!rewriteBuiltinMapInsertMethods(program, error)) {
    return false;
  }
  try {
    if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
      return false;
    }
  } catch (const std::exception &ex) {
    error = std::string("template monomorphization exception: ") + ex.what();
    return false;
  }
  if (!semantics::rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!semantics::rewriteConvertConstructors(program, error)) {
    return false;
  }
  semantics::SemanticsValidator validator(
      program, entryPath, error, defaultEffects, entryDefaultEffects, diagnosticInfo, collectDiagnostics);
  try {
    if (!validator.run()) {
      return false;
    }
  } catch (const std::exception &ex) {
    error = std::string("semantic validator exception: ") + ex.what();
    return false;
  }
  if (!rewriteOmittedStructInitializers(program, error)) {
    return false;
  }
  error.clear();
  validationSucceeded = true;
  return true;
}

bool semantics::computeTypeResolutionReturnSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionReturnSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.returnResolutionSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionReturnSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.kind),
          entry.structPath,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

std::string semantics::runSemanticsReturnKindNameStep(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &error) {
  return returnKindSnapshotName(getReturnKind(def, structNames, importAliases, error));
}

bool semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionLocalBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.localAutoBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionLocalBindingSnapshotEntry{
          entry.scopePath,
          entry.bindingName,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
          entry.initializerResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerBinding),
          bindingTypeTextForSnapshot(entry.initializerReceiverBinding),
          entry.initializerQueryTypeText,
          entry.initializerResultHasValue,
          entry.initializerResultValueType,
          entry.initializerResultErrorType,
          entry.initializerHasTry,
          entry.initializerTryOperandResolvedPath,
          bindingTypeTextForSnapshot(entry.initializerTryOperandBinding),
          bindingTypeTextForSnapshot(entry.initializerTryOperandReceiverBinding),
          entry.initializerTryOperandQueryTypeText,
          entry.initializerTryValueType,
          entry.initializerTryErrorType,
          entry.initializerHasTry ? returnKindSnapshotName(entry.initializerTryContextReturnKind) : std::string{},
          entry.initializerTryOnErrorHandlerPath,
          entry.initializerTryOnErrorErrorType,
          entry.initializerTryOnErrorBoundArgCount,
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryCallSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryCallSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryCallTypeSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionQueryCallSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.typeText,
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionQueryBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryResultTypeSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryResultTypeSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionQueryResultTypeSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.hasValue,
          entry.valueType,
          entry.errorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionTryValueSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionTryValueSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.tryValueSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionTryValueSnapshotEntry{
          entry.scopePath,
          entry.operandResolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.operandBinding),
          bindingTypeTextForSnapshot(entry.operandReceiverBinding),
          entry.operandQueryTypeText,
          entry.valueType,
          entry.errorType,
          returnKindSnapshotName(entry.contextReturnKind),
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

bool semantics::computeTypeResolutionCallBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionCallBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.callBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionCallBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.binding),
      });
    }
  });
}

bool semantics::computeTypeResolutionQueryReceiverBindingSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionQueryReceiverBindingSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out = {};
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.queryReceiverBindingSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionQueryReceiverBindingSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          bindingTypeTextForSnapshot(entry.receiverBinding),
      });
    }
  });
}

bool semantics::computeTypeResolutionOnErrorSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionOnErrorSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.onErrorSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionOnErrorSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.returnKind),
          entry.handlerPath,
          entry.errorType,
          entry.boundArgCount,
          entry.returnResultHasValue,
          entry.returnResultValueType,
          entry.returnResultErrorType,
      });
    }
  });
}

bool semantics::computeTypeResolutionValidationContextSnapshotForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    TypeResolutionValidationContextSnapshot &out,
    const std::vector<std::string> &semanticTransforms) {
  out.entries.clear();
  return runTypeResolutionSnapshot(program, entryPath, error, semanticTransforms, [&](auto &validator) {
    const auto entries = validator.validationContextSnapshotForTesting();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      out.entries.push_back(TypeResolutionValidationContextSnapshotEntry{
          entry.definitionPath,
          returnKindSnapshotName(entry.returnKind),
          entry.definitionIsCompute,
          entry.definitionIsUnsafe,
          entry.activeEffects,
          entry.hasResultType,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
          entry.hasOnError,
          entry.onErrorHandlerPath,
          entry.onErrorErrorType,
          entry.onErrorBoundArgCount,
      });
    }
  });
}

} // namespace primec
