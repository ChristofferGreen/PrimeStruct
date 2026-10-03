// Key/value insert method rewrites, split out of SemanticsValidate.cpp (TODO-5384).
#include <cstdio>
#include "primec/support/CompileArena.h"
#include "primec/semantics/Semantics.h"
#include "primec/semantics/SemanticsBenchmark.h"
#include "primec/semantics/SemanticValidationPlan.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateBuiltinSoaMetadata.h"
#include "SemanticsValidateBuiltinSoaRewrites.h"
#include "SemanticsValidateCompileTimeIf.h"
#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateExperimentalSoaFieldViewRewrites.h"
#include "SemanticsValidateExperimentalSoaMethodRewrites.h"
#include "SemanticsValidateOmittedStructInitializers.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateSoaBindingExtraction.h"
#include "SemanticsValidateTransforms.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "RequirementPredicateFacts.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidationBenchmarkOrchestration.h"
#include "SemanticsValidationPublicationOrchestration.h"
#include "SemanticsValidator.h"
#include "TypeResolutionGraphPreparation.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

#include "SemanticsValidateKeyValueRewrites.h"

namespace primec {

namespace {

void rewriteBorrowedExperimentalKeyValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBorrowedExperimentalKeyValueMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBorrowedExperimentalKeyValueMethodExpr(
        stmt, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBorrowedExperimentalKeyValueMethodStatements(
          stmt.bodyArguments, bodyBindings, borrowedReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractBorrowedExperimentalKeyValueBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteBorrowedExperimentalKeyValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &borrowedReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBorrowedExperimentalKeyValueMethodExpr(
        arg, bindings, borrowedReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = borrowedExperimentalKeyValueHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if ((helperName == collection_helpers::kAtRef || helperName == collection_helpers::kAtUnsafeRef) &&
      receiver.kind == Expr::Kind::Call) {
    return;
  }
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedExperimentalKeyValueBinding(bindingIt->second)) {
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
      auto returnIt = borrowedReturnDefinitions.find(candidatePath);
      if (returnIt != borrowedReturnDefinitions.end() &&
          isBorrowedExperimentalKeyValueBinding(returnIt->second)) {
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
      !semantics::extractKeyValueCollectionTypesFromTypeText(
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

} // namespace

bool rewriteBorrowedExperimentalKeyValueMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> borrowedReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBorrowedExperimentalKeyValueReturnBinding(def); binding.has_value()) {
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
      if (auto binding = extractBorrowedExperimentalKeyValueBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBorrowedExperimentalKeyValueMethodStatements(
        def.statements, bindings, borrowedReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBorrowedExperimentalKeyValueMethodExpr(
          *def.returnExpr, bindings, borrowedReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

namespace {

void rewriteExperimentalKeyValueValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteExperimentalKeyValueValueMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalKeyValueValueMethodExpr(stmt, bindings, valueReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalKeyValueValueMethodStatements(
          stmt.bodyArguments, bodyBindings, valueReturnDefinitions, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractExperimentalKeyValueValueBinding(stmt); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalKeyValueValueMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &valueReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalKeyValueValueMethodExpr(arg, bindings, valueReturnDefinitions, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  const std::string helperName = experimentalKeyValueValueHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  std::optional<semantics::BindingInfo> receiverBinding;
  const Expr &receiver = expr.args.front();
  if ((helperName == "at" || helperName == "at_unsafe") &&
      receiver.kind == Expr::Kind::Call) {
    return;
  }
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalKeyValueValueBinding(bindingIt->second)) {
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
      auto returnIt = valueReturnDefinitions.find(candidatePath);
      if (returnIt != valueReturnDefinitions.end() &&
          isExperimentalKeyValueValueBinding(returnIt->second)) {
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
      !semantics::extractKeyValueCollectionTypesFromTypeText(bindingTypeText(*receiverBinding), keyType, valueType)) {
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

} // namespace

bool rewriteExperimentalKeyValueValueMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> valueReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalKeyValueValueReturnBinding(def); binding.has_value()) {
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
      if (auto binding = extractExperimentalKeyValueValueBinding(param); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalKeyValueValueMethodStatements(
        def.statements, bindings, valueReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalKeyValueValueMethodExpr(
          *def.returnExpr, bindings, valueReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

namespace {

bool isBuiltinKeyValueMutationBinding(const semantics::BindingInfo &binding) {
  if (isExperimentalKeyValueTypeText(bindingTypeText(binding))) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  return semantics::extractKeyValueCollectionTypesFromTypeText(
      bindingTypeText(binding), keyType, valueType);
}

bool isBuiltinKeyValueReferenceBinding(const semantics::BindingInfo &binding) {
  if (!isBuiltinKeyValueMutationBinding(binding)) {
    return false;
  }
  std::string normalizedType =
      semantics::normalizeBindingTypeName(bindingTypeText(binding));
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    normalizedType = semantics::normalizeBindingTypeName(base);
  }
  return normalizedType == "Reference" || normalizedType == "Pointer";
}

semantics::BindingInfo bindingInfoFromTypeText(const std::string &typeText) {
  semantics::BindingInfo binding;
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalizedType, base, argText)) {
    binding.typeName = semantics::normalizeBindingTypeName(base);
    binding.typeTemplateArg = argText;
  } else {
    binding.typeName = normalizedType;
  }
  return binding;
}

std::optional<semantics::BindingInfo> extractDefinitionReturnBinding(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    return bindingInfoFromTypeText(transform.templateArgs.front());
  }
  return std::nullopt;
}

std::string_view resolveBuiltinKeyValueInsertSurfaceMemberName(std::string_view name) {
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const std::string_view memberName = resolveStdlibSurfaceMemberName(*metadata, name);
  if (!collection_helpers::isInsertHelperName(memberName)) {
    return {};
  }
  if (name.find('/') == std::string_view::npos) {
    if (collection_helpers::isInsertHelperName(name)) {
      return memberName;
    }
    return {};
  }
  if (stdlibSurfaceMatchesSpelling(*metadata, name) ||
      findStdlibSurfaceMetadataByResolvedPath(name) == metadata) {
    return memberName;
  }
  return {};
}

std::string canonicalBuiltinKeyValueInsertSurfacePath(bool receiverIsReference) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(
      metadata->id,
      receiverIsReference ? collection_helpers::kInsertRef : "insert");
}

std::string resolveBuiltinKeyValueReadSurfaceMemberName(std::string_view name) {
  std::string normalizedName(name);
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  const size_t generatedSuffix = normalizedName.find("__t");
  if (generatedSuffix != std::string::npos) {
    normalizedName.erase(generatedSuffix);
  }
  if (normalizedName == "size") {
    return "count";
  }
  const std::string memberName =
      metadataBackedKeyValueHelperMethodName(normalizedName);
  if (collection_helpers::isCountHelperName(memberName) ||
      collection_helpers::isContainsHelperName(memberName) ||
      collection_helpers::isTryAtHelperName(memberName) ||
      collection_helpers::isAtHelperName(memberName) ||
      collection_helpers::isAtUnsafeHelperName(memberName)) {
    return memberName;
  }
  return {};
}

bool isBuiltinKeyValueReadHelperName(std::string_view name) {
  return !resolveBuiltinKeyValueReadSurfaceMemberName(name).empty();
}

bool isCanonicalBuiltinKeyValueReadHelperName(std::string_view name) {
  return collection_helpers::isCountHelperName(name) ||
         collection_helpers::isContainsHelperName(name) ||
         collection_helpers::isTryAtHelperName(name);
}

bool isBuiltinKeyValueInsertValueHelperName(std::string_view name) {
  return resolveBuiltinKeyValueInsertSurfaceMemberName(name) == "insert";
}

bool isBuiltinKeyValueInsertReferenceHelperName(std::string_view name) {
  return resolveBuiltinKeyValueInsertSurfaceMemberName(name) == collection_helpers::kInsertRef;
}

bool isBuiltinKeyValueInsertHelperName(std::string_view name) {
  return isBuiltinKeyValueInsertValueHelperName(name) ||
         isBuiltinKeyValueInsertReferenceHelperName(name);
}

bool isBuiltinCanonicalKeyValueConstructorExpr(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::string &definitionNamespace) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  auto isCanonicalConstructorPath = [](std::string path) {
    const size_t specializationSuffix = path.find("__");
    if (specializationSuffix != std::string::npos) {
      path.erase(specializationSuffix);
    }
    return isResolvedKeyValueConstructorPath(path);
  };
  if (expr.name == "map" || isCanonicalConstructorPath(expr.name)) {
    return true;
  }
  const std::string namespacedConstructorPath =
      !expr.namespacePrefix.empty() && expr.name.find('/') == std::string::npos
          ? expr.namespacePrefix + "/" + expr.name
          : std::string{};
  if (!namespacedConstructorPath.empty() &&
      isCanonicalConstructorPath(namespacedConstructorPath)) {
    return true;
  }
  for (const std::string &candidatePath :
       candidateDefinitionPaths(expr, definitionNamespace)) {
    if (isCanonicalConstructorPath(candidatePath)) {
      return true;
    }
    auto defIt = definitionMap.find(candidatePath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      continue;
    }
    if (isCanonicalConstructorPath(defIt->second->fullPath)) {
      return true;
    }
  }
  return false;
}

std::optional<semantics::BindingInfo> resolveBuiltinKeyValueInsertReceiverBinding(
    const Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  if (expr.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(expr.name);
    if (bindingIt == bindings.end()) {
      return std::nullopt;
    }
    return bindingIt->second;
  }

  if (expr.isFieldAccess && expr.args.size() == 1) {
    auto receiverBinding = resolveBuiltinKeyValueInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!receiverBinding.has_value()) {
      return std::nullopt;
    }
    const std::string receiverNamespace =
        !expr.args.front().namespacePrefix.empty() ? expr.args.front().namespacePrefix : definitionNamespace;
    const std::string receiverStructPath = resolveStructReceiverPathFromBinding(
        *receiverBinding, receiverNamespace, structPaths);
    if (receiverStructPath.empty()) {
      return std::nullopt;
    }
    auto defIt = definitionMap.find(receiverStructPath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      return std::nullopt;
    }
    for (const Expr &fieldExpr : defIt->second->statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != expr.name) {
        continue;
      }
      return extractParsedBindingInfo(fieldExpr, &structPaths);
    }
    return std::nullopt;
  }

  if (semantics::isSimpleCallName(expr, "location") && expr.args.size() == 1) {
    auto pointeeBinding = resolveBuiltinKeyValueInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!pointeeBinding.has_value()) {
      return std::nullopt;
    }
    semantics::BindingInfo binding;
    binding.typeName = "Reference";
    binding.typeTemplateArg = bindingTypeText(*pointeeBinding);
    return binding;
  }

  if (semantics::isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    auto borrowedBinding = resolveBuiltinKeyValueInsertReceiverBinding(
        expr.args.front(), bindings, definitionMap, structPaths, definitionNamespace);
    if (!borrowedBinding.has_value()) {
      return std::nullopt;
    }
    const std::string normalizedType = semantics::normalizeBindingTypeName(borrowedBinding->typeName);
    if ((normalizedType != "Reference" && normalizedType != "Pointer") ||
        borrowedBinding->typeTemplateArg.empty()) {
      return std::nullopt;
    }
    return bindingInfoFromTypeText(borrowedBinding->typeTemplateArg);
  }

  std::string accessName;
  if (semantics::getBuiltinArrayAccessName(expr, accessName) &&
      expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto packIt = bindings.find(expr.args.front().name);
    if (packIt == bindings.end()) {
      return std::nullopt;
    }
    std::string elemType;
    if (!semantics::getArgsPackElementType(packIt->second, elemType)) {
      return std::nullopt;
    }
    return bindingInfoFromTypeText(elemType);
  }

  if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
    for (const std::string &candidatePath :
         candidateDefinitionPaths(expr, definitionNamespace)) {
      auto defIt = definitionMap.find(candidatePath);
      if (defIt == definitionMap.end() || defIt->second == nullptr) {
        continue;
      }
      if (auto returnBinding = extractDefinitionReturnBinding(*defIt->second);
          returnBinding.has_value()) {
        return *returnBinding;
      }
    }
  }

  return std::nullopt;
}

void rewriteBuiltinKeyValueInsertExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_set<std::string> &constructorBackedBuiltinKeyValueBindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinKeyValueInsertExpr(
        arg,
        bindings,
        constructorBackedBuiltinKeyValueBindings,
        definitionMap,
        structPaths,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return;
  }

  const bool matchesBuiltinReadMethod =
      expr.isMethodCall && isBuiltinKeyValueReadHelperName(expr.name);
  const std::string scopedExprName =
      !expr.namespacePrefix.empty() && expr.name.find('/') == std::string::npos
          ? expr.namespacePrefix + "/" + expr.name
          : expr.name;
  const std::string directReadHelper =
      !expr.isMethodCall ? resolveBuiltinKeyValueReadSurfaceMemberName(scopedExprName)
                         : std::string{};
  std::string builtinAccessHelper;
  const bool hasBuiltinIndexedAccess =
      !expr.isMethodCall && semantics::getBuiltinArrayAccessName(expr, builtinAccessHelper);
  // The `m.at(k)` / `m.at_unsafe(k)` method spellings take the same canonical
  // access rewrite as the bare call.
  const bool isPlainAccessMethodSpelling =
      expr.isMethodCall && expr.namespacePrefix.empty() &&
      (expr.name == "at" || expr.name == "at_unsafe") &&
      expr.args.size() == 2 && expr.templateArgs.empty() &&
      !expr.hasBodyArguments && expr.bodyArguments.empty() &&
      !semantics::hasNamedArguments(expr.argNames);
  const std::string methodReadHelper =
      isPlainAccessMethodSpelling ? expr.name : std::string{};
  const bool matchesBuiltinAccessCall =
      directReadHelper == "at" || directReadHelper == "at_unsafe" ||
      builtinAccessHelper == "at" || builtinAccessHelper == "at_unsafe" ||
      methodReadHelper == "at" || methodReadHelper == "at_unsafe";
  // Bare `contains(m, k)` takes the same canonical-helper rewrite as the
  // method spelling; a user-defined root `/contains` keeps the
  // call. Bare `count(r)` on a borrowed map takes it too: by-value
  // map counts keep their existing resolution.
  const bool isBareBorrowedCountCandidate =
      !expr.isMethodCall && expr.namespacePrefix.empty() && expr.name == "count" &&
      expr.args.size() == 1 && expr.templateArgs.empty() &&
      directReadHelper == "count" && definitionMap.count("/count") == 0;
  const bool matchesBuiltinBareReadCall =
      !expr.isMethodCall && expr.namespacePrefix.empty() &&
      expr.name.find('/') == std::string::npos &&
      ((expr.args.size() == 2 && expr.templateArgs.empty() &&
        collection_helpers::isContainsHelperName(directReadHelper) &&
        directReadHelper == expr.name && definitionMap.count("/" + expr.name) == 0) ||
       isBareBorrowedCountCandidate);
  auto isStdlibOwnedDefinitionNamespace = [](const std::string &path) {
    if (path.rfind("/std/", 0) == 0) {
      return true;
    }
    if (path.size() <= 1 || path.front() != '/') {
      return false;
    }
    const size_t nextSlash = path.find('/', 1);
    const std::string rootName =
        nextSlash == std::string::npos ? path.substr(1)
                                       : path.substr(1, nextSlash - 1);
    return semantics::isRootBuiltinName(rootName) || rootName == "string" ||
           rootName == "Result" || rootName == "Maybe" ||
           rootName == "Buffer" || rootName == "ImageError" ||
           rootName == "ContainerError" || rootName == "GfxError";
  };
  auto explicitRemovedKeyValueCompatibilityReadPath = [&]() -> std::string {
    const std::string helperName =
        metadataBackedKeyValueHelperRootAliasMethodName(scopedExprName);
    if (helperName.empty()) {
      return {};
    }
    if (helperName != "at" && helperName != "at_unsafe" &&
        helperName != collection_helpers::kAtRef && helperName != collection_helpers::kAtUnsafeRef) {
      return {};
    }
    const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
    if (metadata == nullptr) {
      return {};
    }
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (alias.empty() || alias.find('/') != std::string_view::npos) {
        continue;
      }
      return "/" + std::string(alias) + "/" + helperName;
    }
    return {};
  }();
  if (matchesBuiltinAccessCall &&
      !explicitRemovedKeyValueCompatibilityReadPath.empty() &&
      !isStdlibOwnedDefinitionNamespace(definitionNamespace) &&
      definitionMap.count(explicitRemovedKeyValueCompatibilityReadPath) == 0) {
    return;
  }
  if (matchesBuiltinReadMethod || matchesBuiltinBareReadCall || matchesBuiltinAccessCall) {
    const Expr &receiver = expr.args.front();
    auto receiverBinding = resolveBuiltinKeyValueInsertReceiverBinding(
        receiver, bindings, definitionMap, structPaths, definitionNamespace);
    if (!receiverBinding.has_value() ||
        !isBuiltinKeyValueMutationBinding(*receiverBinding)) {
      return;
    }
    const bool receiverIsReference =
        isBuiltinKeyValueReferenceBinding(*receiverBinding);
    if (isBareBorrowedCountCandidate && !receiverIsReference) {
      return;
    }
    std::string helperName(
        resolveBuiltinKeyValueReadSurfaceMemberName(scopedExprName));
    if (helperName.empty() && hasBuiltinIndexedAccess) {
      helperName = builtinAccessHelper;
    }
    if (helperName.empty()) {
      return;
    }
    const bool isCanonicalKeyValueReadHelper =
        isCanonicalBuiltinKeyValueReadHelperName(helperName);
    if (helperName == collection_helpers::kCountRef) {
      helperName = "count";
    } else if (helperName == collection_helpers::kContainsRef) {
      helperName = "contains";
    } else if (helperName == collection_helpers::kTryAtRef) {
      helperName = "tryAt";
    } else if (helperName == collection_helpers::kAtRef) {
      helperName = "at";
    } else if (helperName == collection_helpers::kAtUnsafeRef) {
      helperName = "at_unsafe";
    }
    std::string keyType;
    std::string valueType;
    if (!semantics::extractKeyValueCollectionTypesFromTypeText(
            bindingTypeText(*receiverBinding), keyType, valueType)) {
      return;
    }
    // Explicit template arguments that disagree with the receiver's key/value
    // types stay on the call so validation rejects the mismatch instead of
    // the rewrite silently replacing them with the receiver's types.
    if (matchesBuiltinAccessCall && expr.templateArgs.size() == 2 &&
        (semantics::normalizeBindingTypeName(expr.templateArgs[0]) !=
             semantics::normalizeBindingTypeName(keyType) ||
         semantics::normalizeBindingTypeName(expr.templateArgs[1]) !=
             semantics::normalizeBindingTypeName(valueType))) {
      return;
    }
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    if ((matchesBuiltinReadMethod || matchesBuiltinBareReadCall) &&
        isCanonicalKeyValueReadHelper && receiverIsReference) {
      helperName += "_ref";
    }
    if (matchesBuiltinAccessCall && receiverIsReference) {
      helperName += "_ref";
    }
    if ((matchesBuiltinReadMethod || matchesBuiltinBareReadCall) &&
        isCanonicalKeyValueReadHelper) {
      helperName = metadataBackedCanonicalKeyValueHelperPath(helperName);
    }
    if (matchesBuiltinAccessCall) {
      helperName = metadataBackedCanonicalKeyValueHelperPath(helperName);
    }
    if (helperName.empty()) {
      return;
    }
    expr.name = helperName;
    expr.namespacePrefix.clear();
    if (matchesBuiltinAccessCall) {
      expr.templateArgs.clear();
    }
    expr.argNames.clear();
    return;
  }

  const bool matchesBuiltinInsertMethod =
      expr.isMethodCall && isBuiltinKeyValueInsertHelperName(expr.name);
  const bool matchesBuiltinInsertCall =
      !expr.isMethodCall && isBuiltinKeyValueInsertHelperName(expr.name);
  if (!matchesBuiltinInsertMethod && !matchesBuiltinInsertCall) {
    return;
  }

  const Expr &receiver = expr.args.front();
  auto receiverBinding = resolveBuiltinKeyValueInsertReceiverBinding(
      receiver, bindings, definitionMap, structPaths, definitionNamespace);
  if (!receiverBinding.has_value() || !isBuiltinKeyValueMutationBinding(*receiverBinding)) {
    return;
  }
  const bool receiverIsReference = isBuiltinKeyValueReferenceBinding(*receiverBinding);
  const bool expectsReferenceSurface =
      !expr.isMethodCall && isBuiltinKeyValueInsertReferenceHelperName(expr.name);
  const bool expectsValueSurface =
      !expr.isMethodCall && isBuiltinKeyValueInsertValueHelperName(expr.name);
  if ((expectsReferenceSurface && !receiverIsReference) ||
      (expectsValueSurface && receiverIsReference)) {
    return;
  }
  std::string keyType;
  std::string valueType;
  if (expr.templateArgs.empty() &&
      !semantics::extractKeyValueCollectionTypesFromTypeText(
          bindingTypeText(*receiverBinding), keyType, valueType)) {
    return;
  }
  const std::string canonicalInsertPath =
      canonicalBuiltinKeyValueInsertSurfacePath(receiverIsReference);
  if (canonicalInsertPath.empty()) {
    return;
  }
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = canonicalInsertPath;
  expr.namespacePrefix.clear();
  if (expr.templateArgs.empty()) {
    expr.templateArgs = {keyType, valueType};
  }
  expr.argNames.clear();
}

void rewriteBuiltinKeyValueInsertStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    std::unordered_set<std::string> constructorBackedBuiltinKeyValueBindings,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinKeyValueInsertExpr(
        stmt,
        bindings,
        constructorBackedBuiltinKeyValueBindings,
        definitionMap,
        structPaths,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      auto bodyConstructorBackedBindings = constructorBackedBuiltinKeyValueBindings;
      rewriteBuiltinKeyValueInsertStatements(
          stmt.bodyArguments,
          bodyBindings,
          bodyConstructorBackedBindings,
          definitionMap,
          structPaths,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
        auto isConstructorBackedKeyValueInitializer = [&](const Expr &initializer) {
          if (isBuiltinCanonicalKeyValueConstructorExpr(
                  initializer,
                  definitionMap,
                  definitionNamespace)) {
            return true;
          }
          if (initializer.kind == Expr::Kind::Name) {
            return constructorBackedBuiltinKeyValueBindings.count(initializer.name) != 0;
          }
          if (semantics::isSimpleCallName(initializer, "location") &&
              initializer.args.size() == 1 &&
              initializer.args.front().kind == Expr::Kind::Name) {
            return constructorBackedBuiltinKeyValueBindings.count(initializer.args.front().name) != 0;
          }
          return false;
        };
        if (stmt.args.size() == 1 &&
            isBuiltinKeyValueMutationBinding(*binding) &&
            isConstructorBackedKeyValueInitializer(stmt.args.front())) {
          constructorBackedBuiltinKeyValueBindings.insert(stmt.name);
        } else {
          constructorBackedBuiltinKeyValueBindings.erase(stmt.name);
        }
      }
    }
  }
}

} // namespace

bool rewriteBuiltinKeyValueInsertMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, const Definition *> definitionMap;
  std::unordered_set<std::string> structPaths;
  for (const Definition &def : program.definitions) {
    definitionMap[def.fullPath] = &def;
    if (semantics::isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    std::unordered_set<std::string> constructorBackedBuiltinKeyValueBindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      } else {
        for (const auto &transform : param.transforms) {
          if (transform.name == "args" && transform.templateArgs.size() == 1) {
            semantics::BindingInfo argsBinding;
            argsBinding.typeName = "args";
            argsBinding.typeTemplateArg = transform.templateArgs.front();
            bindings[param.name] = std::move(argsBinding);
            break;
          }
        }
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteBuiltinKeyValueInsertStatements(
        def.statements,
        bindings,
        constructorBackedBuiltinKeyValueBindings,
        definitionMap,
        structPaths,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      auto returnConstructorBackedBindings = constructorBackedBuiltinKeyValueBindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
          if (stmt.args.size() == 1 &&
              isBuiltinKeyValueMutationBinding(*binding) &&
              isBuiltinCanonicalKeyValueConstructorExpr(
                  stmt.args.front(),
                  definitionMap,
                  definitionNamespace)) {
            returnConstructorBackedBindings.insert(stmt.name);
          } else {
            returnConstructorBackedBindings.erase(stmt.name);
          }
        }
      }
      rewriteBuiltinKeyValueInsertExpr(
          *def.returnExpr,
          returnBindings,
          returnConstructorBackedBindings,
          definitionMap,
          structPaths,
          definitionNamespace);
    }
  }
  return true;
}

} // namespace primec
