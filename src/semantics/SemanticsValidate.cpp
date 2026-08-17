// soa-surface-audit: exempt
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

uint64_t hashSemanticNodePath(const std::string &path) {
  constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
  constexpr uint64_t FnvPrime = 1099511628211ull;

  uint64_t hash = FnvOffsetBasis;
  for (unsigned char ch : path) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= FnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

std::string makeIndexedSemanticNodePath(const std::string &base, const char *segment, size_t index) {
  return base + "/" + segment + "[" + std::to_string(index) + "]";
}

void assignExprSemanticNodeIds(Expr &expr, const std::string &path) {
  expr.semanticNodeId = hashSemanticNodePath(path);
  for (size_t i = 0; i < expr.args.size(); ++i) {
    assignExprSemanticNodeIds(expr.args[i], makeIndexedSemanticNodePath(path, "arg", i));
  }
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(expr.bodyArguments[i], makeIndexedSemanticNodePath(path, "body", i));
  }
}

void assignDefinitionSemanticNodeIds(Definition &def) {
  const std::string basePath = "definition:" + def.fullPath;
  def.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    assignExprSemanticNodeIds(def.parameters[i], makeIndexedSemanticNodePath(basePath, "parameter", i));
  }
  for (size_t i = 0; i < def.statements.size(); ++i) {
    assignExprSemanticNodeIds(def.statements[i], makeIndexedSemanticNodePath(basePath, "statement", i));
  }
  if (def.returnExpr.has_value()) {
    assignExprSemanticNodeIds(*def.returnExpr, basePath + "/return");
  }
}

void assignExecutionSemanticNodeIds(Execution &exec) {
  const std::string basePath = "execution:" + exec.fullPath;
  exec.semanticNodeId = hashSemanticNodePath(basePath);
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.arguments[i], makeIndexedSemanticNodePath(basePath, "argument", i));
  }
  for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
    assignExprSemanticNodeIds(exec.bodyArguments[i], makeIndexedSemanticNodePath(basePath, "body", i));
  }
}

void assignSemanticNodeIds(Program &program) {
  for (auto &def : program.definitions) {
    assignDefinitionSemanticNodeIds(def);
  }
  for (auto &exec : program.executions) {
    assignExecutionSemanticNodeIds(exec);
  }
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
  if ((helperName == "at_ref" || helperName == "at_unsafe_ref") &&
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
  if (memberName != "insert" && memberName != "insert_ref") {
    return {};
  }
  if (name.find('/') == std::string_view::npos) {
    if (name == "insert" || name == "insert_ref") {
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
      receiverIsReference ? "insert_ref" : "insert");
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
  if (memberName == "count" || memberName == "count_ref" ||
      memberName == "contains" || memberName == "contains_ref" ||
      memberName == "tryAt" || memberName == "tryAt_ref" ||
      memberName == "at" || memberName == "at_ref" ||
      memberName == "at_unsafe" || memberName == "at_unsafe_ref") {
    return memberName;
  }
  return {};
}

bool isBuiltinKeyValueReadHelperName(std::string_view name) {
  return !resolveBuiltinKeyValueReadSurfaceMemberName(name).empty();
}

bool isCanonicalBuiltinKeyValueReadHelperName(std::string_view name) {
  return name == "count" || name == "count_ref" ||
         name == "contains" || name == "contains_ref" ||
         name == "tryAt" || name == "tryAt_ref";
}

bool isBuiltinKeyValueInsertValueHelperName(std::string_view name) {
  return resolveBuiltinKeyValueInsertSurfaceMemberName(name) == "insert";
}

bool isBuiltinKeyValueInsertReferenceHelperName(std::string_view name) {
  return resolveBuiltinKeyValueInsertSurfaceMemberName(name) == "insert_ref";
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
  const bool matchesBuiltinAccessCall =
      directReadHelper == "at" || directReadHelper == "at_unsafe" ||
      builtinAccessHelper == "at" || builtinAccessHelper == "at_unsafe";
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
        helperName != "at_ref" && helperName != "at_unsafe_ref") {
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
  if (matchesBuiltinReadMethod || matchesBuiltinAccessCall) {
    const Expr &receiver = expr.args.front();
    auto receiverBinding = resolveBuiltinKeyValueInsertReceiverBinding(
        receiver, bindings, definitionMap, structPaths, definitionNamespace);
    if (!receiverBinding.has_value() ||
        !isBuiltinKeyValueMutationBinding(*receiverBinding)) {
      return;
    }
    const bool receiverIsReference =
        isBuiltinKeyValueReferenceBinding(*receiverBinding);
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
    if (helperName == "count_ref") {
      helperName = "count";
    } else if (helperName == "contains_ref") {
      helperName = "contains";
    } else if (helperName == "tryAt_ref") {
      helperName = "tryAt";
    } else if (helperName == "at_ref") {
      helperName = "at";
    } else if (helperName == "at_unsafe_ref") {
      helperName = "at_unsafe";
    }
    std::string keyType;
    std::string valueType;
    if (!semantics::extractKeyValueCollectionTypesFromTypeText(
            bindingTypeText(*receiverBinding), keyType, valueType)) {
      return;
    }
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    if (matchesBuiltinReadMethod && isCanonicalKeyValueReadHelper &&
        receiverIsReference) {
      helperName += "_ref";
    }
    if (matchesBuiltinAccessCall && receiverIsReference) {
      helperName += "_ref";
    }
    if (matchesBuiltinReadMethod && isCanonicalKeyValueReadHelper) {
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

} // namespace

namespace {

struct SemanticValidationManifestExecutionState {
  Program &program;
  const std::string &entryPath;
  std::string &error;
  const std::vector<std::string> &defaultEffects;
  const std::vector<std::string> &entryDefaultEffects;
  const std::vector<std::string> &semanticTransforms;
  SemanticDiagnosticInfo *diagnosticInfo = nullptr;
  bool collectDiagnostics = false;
  SemanticProgram *semanticProgramOut = nullptr;
  const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr;
  const semantics::SemanticValidationBenchmarkRuntime &benchmarkRuntime;
  semantics::SemanticValidationBenchmarkPhase &validationBenchmark;
  semantics::SemanticValidatorLifetimeBenchmark &validatorLifetimeBenchmark;
  std::unique_ptr<semantics::SemanticsValidator> validator;
  semantics::SemanticsValidator::ValidationCounters validationCounters;
  bool validatorPassCompleted = false;
  bool semanticProductPublicationCompleted = false;
  const std::unordered_set<std::string> *lazyStdlibModuleKeys = nullptr;
};

bool validateSemanticValidationManifestExecutableShape(std::string &error) {
  const auto &manifest = semantics::semanticValidationPassManifest();
  if (manifest.empty()) {
    error = "semantic validation manifest is empty";
    return false;
  }

  std::vector<semantics::SemanticValidationPassId> ids;
  std::vector<std::string_view> names;
  bool sawValidator = false;
  bool sawPublication = false;
  for (const auto &pass : manifest) {
    if (pass.name.empty()) {
      error = "semantic validation manifest contains an unnamed pass";
      return false;
    }
    if (std::find(names.begin(), names.end(), pass.name) != names.end()) {
      error = "semantic validation manifest has duplicate pass name: " +
              std::string(pass.name);
      return false;
    }
    names.push_back(pass.name);
    if (std::find(ids.begin(), ids.end(), pass.id) != ids.end()) {
      error = "semantic validation manifest has duplicate executable pass id: " +
              std::string(pass.name);
      return false;
    }
    ids.push_back(pass.id);

    if (sawPublication) {
      error = "semantic validation manifest has pass after publication: " +
              std::string(pass.name);
      return false;
    }
    if (pass.kind == semantics::SemanticValidationPassKind::Publication &&
        !sawValidator) {
      error = "semantic validation manifest reached publication before validator: " +
              std::string(pass.name);
      return false;
    }
    if (pass.id == semantics::SemanticValidationPassId::ValidatorPasses) {
      if (pass.kind != semantics::SemanticValidationPassKind::Validation) {
        error = "semantic validation manifest validator pass has wrong kind";
        return false;
      }
      sawValidator = true;
    } else if (pass.kind == semantics::SemanticValidationPassKind::Validation) {
      error = "semantic validation manifest has unexpected validation pass: " +
              std::string(pass.name);
      return false;
    }
    if (pass.id == semantics::SemanticValidationPassId::SemanticProductPublication) {
      if (pass.kind != semantics::SemanticValidationPassKind::Publication) {
        error = "semantic validation manifest publication pass has wrong kind";
        return false;
      }
      sawPublication = true;
    } else if (pass.kind == semantics::SemanticValidationPassKind::Publication) {
      error = "semantic validation manifest has unexpected publication pass: " +
              std::string(pass.name);
      return false;
    }
  }

  if (!sawValidator) {
    error = "semantic validation manifest is missing validator-passes";
    return false;
  }
  if (!sawPublication) {
    error = "semantic validation manifest is missing semantic-product-publication";
    return false;
  }
  return true;
}

bool runSemanticValidationManifestValidatorPass(
    SemanticValidationManifestExecutionState &state) {
  if (state.validator != nullptr || state.validatorPassCompleted) {
    state.error = "semantic validation manifest attempted to run validator twice";
    return false;
  }

  state.validatorLifetimeBenchmark.captureBefore();
  state.validationBenchmark.captureBefore();
  state.validator = std::make_unique<semantics::SemanticsValidator>(
      state.program,
      state.entryPath,
      state.error,
      state.defaultEffects,
      state.entryDefaultEffects,
      state.diagnosticInfo,
      state.collectDiagnostics,
      state.benchmarkRuntime.definitionValidationWorkerCount,
      state.benchmarkRuntime.hasPhaseCounters(),
      state.benchmarkRuntime.disableMethodTargetMemoization,
      state.benchmarkRuntime.graphLocalAutoLegacyKeyShadow,
      state.benchmarkRuntime.graphLocalAutoLegacySideChannelShadow,
      state.benchmarkRuntime.disableGraphLocalAutoDependencyScratchPmr,
      nullptr,
      state.lazyStdlibModuleKeys);
  try {
    if (!state.validator->run()) {
      return false;
    }
  } catch (const std::exception &ex) {
    state.error = std::string("semantic validator exception: ") + ex.what();
    return false;
  }

  state.validationCounters = state.validator->validationCounters();
  eraseCompileTimeTypeBindings(state.program);
  state.validatorPassCompleted = true;
  return true;
}

bool runSemanticValidationManifestPublicationPass(
    SemanticValidationManifestExecutionState &state) {
  if (!state.validatorPassCompleted || state.validator == nullptr) {
    state.error =
        "semantic validation manifest reached publication without validator state";
    return false;
  }
  if (state.semanticProductPublicationCompleted) {
    state.error =
        "semantic validation manifest attempted to publish semantic product twice";
    return false;
  }

  semantics::SemanticPublicationSurface publicationSurface;
  if (state.semanticProgramOut != nullptr) {
    publicationSurface =
        state.validator->takeSemanticPublicationSurfaceForSemanticProduct(
            state.semanticProductBuildConfig);
  }
  semantics::maybeRelieveSemanticAllocatorPressure();
  state.validationBenchmark.captureAfter();
  state.validatorLifetimeBenchmark.captureAfterRun();
  if (state.semanticProgramOut != nullptr) {
    semantics::publishSemanticProgramAfterValidation(
        state.program,
        state.entryPath,
        std::move(publicationSurface),
        state.semanticProductBuildConfig,
        state.benchmarkRuntime,
        *state.semanticProgramOut);
  }
  state.validator.reset();
  state.semanticProductPublicationCompleted = true;
  return true;
}

bool runSemanticValidationManifestPass(
    const semantics::SemanticValidationPassManifestEntry &pass,
    SemanticValidationManifestExecutionState &state) {
  using PassId = semantics::SemanticValidationPassId;
  switch (pass.id) {
    case PassId::SemanticTransformRules:
      return semantics::applySemanticTransforms(
          state.program, state.semanticTransforms, state.error);
    case PassId::ExperimentalGfxConstructors:
      return semantics::rewriteExperimentalGfxConstructors(state.program, state.error);
    case PassId::ReflectionGeneratedHelpers:
      return semantics::rewriteReflectionGeneratedHelpers(state.program, state.error);
    case PassId::BuiltinSoaConversionMethods:
      return rewriteBuiltinSoaConversionMethods(state.program, state.error);
    case PassId::BuiltinSoaToAosCalls:
      return rewriteBuiltinSoaToAosCalls(state.program, state.error);
    case PassId::BuiltinSoaHelperReturnMetadata:
      return validateBuiltinSoaHelperReturnMetadataRequirements(
          state.program, state.error);
    case PassId::BuiltinSoaAccessCalls:
      return rewriteBuiltinSoaAccessCalls(state.program, state.error);
    case PassId::BuiltinSoaCountCalls:
      return rewriteBuiltinSoaCountCalls(state.program, state.error);
    case PassId::BuiltinSoaMutatorCalls:
      return rewriteBuiltinSoaMutatorCalls(state.program, state.error);
    case PassId::ExperimentalSoaInlineBorrowMethods:
      return rewriteExperimentalSoaInlineBorrowMethods(state.program, state.error);
    case PassId::ExperimentalSoaSamePathHelperMethods:
      return rewriteExperimentalSoaSamePathHelperMethods(state.program, state.error);
    case PassId::ExperimentalSoaToAosMethods:
      return rewriteExperimentalSoaToAosMethods(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewIndexes:
      return rewriteExperimentalSoaFieldViewIndexes(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewHelpers:
      return rewriteExperimentalSoaFieldViewHelpers(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewCarrierIndexes:
      return rewriteExperimentalSoaFieldViewCarrierIndexes(state.program, state.error);
    case PassId::ExperimentalSoaFieldViewAssignTargets:
      return rewriteExperimentalSoaFieldViewAssignTargets(state.program, state.error);
    case PassId::BorrowedExperimentalMapMethods:
      return rewriteBorrowedExperimentalKeyValueMethods(state.program, state.error);
    case PassId::ExperimentalMapValueMethods:
      return rewriteExperimentalKeyValueValueMethods(state.program, state.error);
    case PassId::BuiltinMapInsertMethods:
      return rewriteBuiltinKeyValueInsertMethods(state.program, state.error);
    case PassId::CompileTimeBranchPruning:
      return rewriteCompileTimeIfBranches(state.program, true, state.error);
    case PassId::TemplateMonomorphization:
      try {
        if (!semantics::monomorphizeTemplates(
                state.program, state.entryPath, state.error)) {
          return false;
        }
        return semantics::rewriteReflectionGeneratedHelpersForPackSpecializations(
            state.program, state.error);
      } catch (const std::exception &ex) {
        state.error = std::string("template monomorphization exception: ") +
                      ex.what();
        return false;
      }
    case PassId::CompileTimeSpecializedBranchPruning:
      return rewriteCompileTimeIfBranches(state.program, false, state.error);
    case PassId::ReflectionMetadataQueries:
      return semantics::rewriteReflectionMetadataQueries(state.program, state.error);
    case PassId::ConvertConstructors:
      return semantics::rewriteConvertConstructors(state.program, state.error);
    case PassId::ValidatorPasses:
      return runSemanticValidationManifestValidatorPass(state);
    case PassId::OmittedStructInitializers:
      if (!state.validatorPassCompleted) {
        state.error =
            "semantic validation manifest reached omitted initializer rewrite before validator";
        return false;
      }
      return rewriteOmittedStructInitializers(state.program, state.error);
    case PassId::SemanticNodeIdAssignment:
      if (!state.validatorPassCompleted || state.validator == nullptr) {
        state.error =
            "semantic validation manifest reached node-id assignment without validator state";
        return false;
      }
      semantics::assignSemanticNodeIds(state.program);
      state.validator->invalidatePilotRoutingSemanticCollectors();
      return true;
    case PassId::SemanticProductPublication:
      return runSemanticValidationManifestPublicationPass(state);
  }

  state.error = "semantic validation manifest has no runner for pass: " +
                std::string(pass.name);
  return false;
}

bool runSemanticValidationManifest(SemanticValidationManifestExecutionState &state) {
  if (!validateSemanticValidationManifestExecutableShape(state.error)) {
    return false;
  }

  using PassKind = semantics::SemanticValidationPassKind;

  // Internal stdlib modules (internal_*) are in canonical form and never need
  // compatibility rewrites. Deferring them from those passes avoids redundant
  // AST traversals on thousands of internal definitions per pass — the
  // 4,842-line internal_soa_storage.prime alone adds >30s on wildcard imports
  // like `import /std/collections/*` without this filter.
  auto isInternalStdlibDef = [](const Definition &def) {
    return def.namespacePrefix.find("/internal_") != std::string::npos;
  };

  std::vector<Definition> deferredInternalDefs;
  bool internalDefsInProgram = true;

  auto deferInternalDefs = [&]() {
    if (!internalDefsInProgram) {
      return;
    }
    auto &defs = state.program.definitions;
    auto pivot = std::stable_partition(defs.begin(), defs.end(),
                                       [&](const Definition &def) {
                                         return !isInternalStdlibDef(def);
                                       });
    if (pivot == defs.end()) {
      return;
    }
    deferredInternalDefs.insert(deferredInternalDefs.end(),
                                std::make_move_iterator(pivot),
                                std::make_move_iterator(defs.end()));
    defs.erase(pivot, defs.end());
    internalDefsInProgram = false;
  };

  auto restoreInternalDefs = [&]() {
    if (internalDefsInProgram || deferredInternalDefs.empty()) {
      return;
    }
    auto &defs = state.program.definitions;
    defs.insert(defs.end(),
                std::make_move_iterator(deferredInternalDefs.begin()),
                std::make_move_iterator(deferredInternalDefs.end()));
    deferredInternalDefs.clear();
    internalDefsInProgram = true;
  };

  for (const auto &pass : semantics::semanticValidationPassManifest()) {
    if (state.semanticProductPublicationCompleted) {
      state.error = "semantic validation manifest has pass after publication: " +
                    std::string(pass.name);
      return false;
    }
    if (pass.kind == PassKind::CompatibilityRewrite) {
      deferInternalDefs();
    } else {
      restoreInternalDefs();
    }
    if (!runSemanticValidationManifestPass(pass, state)) {
      return false;
    }
  }

  restoreInternalDefs();

  if (!state.validatorPassCompleted) {
    state.error = "semantic validation manifest is missing validator-passes";
    return false;
  }
  if (!state.semanticProductPublicationCompleted) {
    state.error =
        "semantic validation manifest is missing semantic-product-publication";
    return false;
  }
  return true;
}

bool runSemanticValidation(Program &program,
                           const std::string &entryPath,
                           std::string &error,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           const std::vector<std::string> &semanticTransforms,
                           SemanticDiagnosticInfo *diagnosticInfo,
                           bool collectDiagnostics,
                           SemanticProgram *semanticProgramOut,
                           const SemanticProductBuildConfig *semanticProductBuildConfig,
                           const SemanticValidationBenchmarkConfig *benchmarkConfig,
                           const SemanticValidationBenchmarkObserver *benchmarkObserver,
                           const std::unordered_set<std::string> *lazyStdlibModuleKeys) {
  const auto benchmarkRuntime =
      semantics::makeSemanticValidationBenchmarkRuntime(benchmarkConfig, benchmarkObserver);

  error.clear();
  if (benchmarkRuntime.phaseCounters != nullptr) {
    *benchmarkRuntime.phaseCounters = {};
  }
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
  const semantics::ScopedSemanticAllocatorReliefDisable scopedBenchmarkAllocatorReliefDisable(
      benchmarkRuntime.usesAllocatorSampling());
  auto validatorLifetimeBenchmark = semantics::SemanticValidatorLifetimeBenchmark::fromEnvironment();
  semantics::SemanticValidationBenchmarkPhase validationBenchmark(benchmarkRuntime);
  SemanticValidationManifestExecutionState manifestState{
      program,
      entryPath,
      error,
      defaultEffects,
      entryDefaultEffects,
      semanticTransforms,
      diagnosticInfo,
      collectDiagnostics,
      semanticProgramOut,
      semanticProductBuildConfig,
      benchmarkRuntime,
      validationBenchmark,
      validatorLifetimeBenchmark,
      nullptr,
      {},
      false,
      false,
      lazyStdlibModuleKeys,
  };
  if (!runSemanticValidationManifest(manifestState)) {
    return false;
  }

  validatorLifetimeBenchmark.captureAfterDestroyAndReport();
  validationBenchmark.publish(manifestState.validationCounters.callsVisited,
                              manifestState.validationCounters.peakLocalMapSize);
  error.clear();
  validationSucceeded = true;
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
                         bool collectDiagnostics,
                         SemanticProgram *semanticProgramOut,
                         const SemanticProductBuildConfig *semanticProductBuildConfig,
                         const std::unordered_set<std::string> *lazyStdlibModuleKeys) const {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               nullptr,
                               nullptr,
                               lazyStdlibModuleKeys);
}

bool validateSemanticsForBenchmark(
    Program &program,
    const std::string &entryPath,
    std::string &error,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::vector<std::string> &semanticTransforms,
    SemanticDiagnosticInfo *diagnosticInfo,
    bool collectDiagnostics,
    SemanticProgram *semanticProgramOut,
    const SemanticProductBuildConfig *semanticProductBuildConfig,
    const SemanticValidationBenchmarkConfig &benchmarkConfig,
    const SemanticValidationBenchmarkObserver &benchmarkObserver,
    const std::unordered_set<std::string> *lazyStdlibModuleKeys) {
  return runSemanticValidation(program,
                               entryPath,
                               error,
                               defaultEffects,
                               entryDefaultEffects,
                               semanticTransforms,
                               diagnosticInfo,
                               collectDiagnostics,
                               semanticProgramOut,
                               semanticProductBuildConfig,
                               &benchmarkConfig,
                               &benchmarkObserver,
                               lazyStdlibModuleKeys);
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
          entry.initializerDirectCallResolvedPath,
          entry.initializerDirectCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerDirectCallReturnKind)
              : std::string{},
          entry.initializerMethodCallResolvedPath,
          entry.initializerMethodCallReturnKind != ReturnKind::Unknown
              ? returnKindSnapshotName(entry.initializerMethodCallReturnKind)
              : std::string{},
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
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.typeText.empty()) {
        continue;
      }
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
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.binding.typeName.empty()) {
        continue;
      }
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
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (!entry.hasResultType) {
        continue;
      }
      out.entries.push_back(TypeResolutionQueryResultTypeSnapshotEntry{
          entry.scopePath,
          entry.callName,
          entry.resolvedPath,
          entry.sourceLine,
          entry.sourceColumn,
          entry.resultTypeHasValue,
          entry.resultValueType,
          entry.resultErrorType,
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
    const auto entries = validator.tryFactSnapshotForSemanticProduct();
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
    const auto entries = validator.queryFactSnapshotForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.receiverBinding.typeName.empty()) {
        continue;
      }
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
    const auto entries = validator.onErrorFactSnapshotForSemanticProduct();
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
    const auto entries = validator.takeCollectedCallableSummariesForSemanticProduct();
    out.entries.reserve(entries.size());
    for (const auto &entry : entries) {
      if (entry.isExecution) {
        continue;
      }
      out.entries.push_back(TypeResolutionValidationContextSnapshotEntry{
          entry.fullPath,
          returnKindSnapshotName(entry.returnKind),
          entry.isCompute,
          entry.isUnsafe,
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
