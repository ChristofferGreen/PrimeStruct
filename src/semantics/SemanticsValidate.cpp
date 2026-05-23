#include "primec/Semantics.h"
#include "primec/SemanticsBenchmark.h"
#include "primec/SemanticValidationPlan.h"
#include "primec/StdlibSurfaceRegistry.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
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
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

bool isExperimentalKeyValueTypeText(const std::string &typeText) {
  std::string keyType;
  std::string valueType;
  if (!semantics::extractKeyValueCollectionTypesFromTypeText(typeText, keyType, valueType)) {
    return false;
  }
  const std::string normalizedInner = semantics::normalizeBindingTypeName(typeText);
  std::string candidate = normalizedInner;
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalizedInner, base, argText) &&
      !base.empty()) {
    candidate = semantics::normalizeBindingTypeName(base);
  }
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", candidate);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueBinding(const Expr &expr) {
  semantics::BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "Reference" &&
        transform.templateArgs.size() == 1 &&
        isExperimentalKeyValueTypeText(transform.templateArgs.front())) {
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

bool isExperimentalKeyValueValueBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalKeyValueTypeText(bindingTypeText(binding));
}

std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueBinding(const Expr &expr) {
  static const std::unordered_set<std::string> emptyStructTypes;
  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  semantics::BindingInfo binding;
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!semantics::parseBindingInfo(
          expr, expr.namespacePrefix, emptyStructTypes, emptyImportAliases, binding, restrictType, parseError)) {
    return std::nullopt;
  }
  if (!isExperimentalKeyValueValueBinding(binding)) {
    return std::nullopt;
  }
  return binding;
}

bool isBorrowedExperimentalKeyValueBinding(const semantics::BindingInfo &binding) {
  return semantics::normalizeBindingTypeName(binding.typeName) == "Reference" &&
         isExperimentalKeyValueTypeText(binding.typeTemplateArg);
}

std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueReturnBinding(
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
        !isExperimentalKeyValueTypeText(args.front())) {
      continue;
    }
    semantics::BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = args.front();
    return info;
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueReturnBinding(const Definition &def) {
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
    if (isExperimentalKeyValueValueBinding(binding)) {
      return binding;
    }
  }
  return std::nullopt;
}

std::string borrowedExperimentalKeyValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "count_ref";
  }
  if (methodName == "contains") {
    return "contains_ref";
  }
  if (methodName == "tryAt") {
    return "tryAt_ref";
  }
  if (methodName == "at") {
    return "at_ref";
  }
  if (methodName == "at_unsafe") {
    return "at_unsafe_ref";
  }
  if (methodName == "insert") {
    return "insert_ref";
  }
  return {};
}

std::string experimentalKeyValueValueHelperName(std::string_view methodName) {
  if (methodName == "count") {
    return "count";
  }
  if (methodName == "contains") {
    return "contains";
  }
  if (methodName == "tryAt") {
    return "tryAt";
  }
  if (methodName == "at") {
    return "at";
  }
  if (methodName == "at_unsafe") {
    return "at_unsafe";
  }
  if (methodName == "insert") {
    return "insert";
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
  if (semantics::isExperimentalSoaVectorTypePath(rawType)) {
    return false;
  }
  const std::string normalizedType = semantics::normalizeBindingTypeName(typeText);
  return semantics::isInternalSoaCollectionTypeName(normalizedType);
}

bool isExperimentalSoaVectorBaseName(const std::string &typeName) {
  std::string normalizedType = typeName;
  if (!normalizedType.empty() && normalizedType.front() == '/') {
    normalizedType.erase(normalizedType.begin());
  }
  return semantics::isExperimentalSoaVectorTypePath(normalizedType);
}

bool isExperimentalSoaVectorBinding(const semantics::BindingInfo &binding) {
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return false;
  }
  return isExperimentalSoaVectorBaseName(binding.typeName);
}

bool isExperimentalSoaVectorOrBorrowedTypeText(std::string typeText) {
  typeText = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(typeText, base, argText) || base.empty()) {
      return isExperimentalSoaVectorBaseName(typeText);
    }
    const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      typeText = args.front();
      continue;
    }
    return isExperimentalSoaVectorBaseName(base);
  }
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut);

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

bool isBuiltinSoaVectorOrBorrowedBinding(const semantics::BindingInfo &binding) {
  if (isBuiltinSoaVectorBinding(binding)) {
    return true;
  }
  std::string elemType;
  return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType);
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

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed) {
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
      const std::string normalizedBase = semantics::normalizeBindingTypeName(base);
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !allowBorrowed) {
        continue;
      }
      binding.typeName = normalizedBase;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = normalizedReturnType;
    }
    if ((allowBorrowed && isBuiltinSoaVectorOrBorrowedBinding(binding)) ||
        (!allowBorrowed && isBuiltinSoaVectorBinding(binding))) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBinding(const Definition &def) {
  return extractBuiltinSoaVectorReturnBindingImpl(def, false);
}

std::optional<semantics::BindingInfo> extractBuiltinSoaVectorOrBorrowedReturnBinding(
    const Definition &def) {
  return extractBuiltinSoaVectorReturnBindingImpl(def, true);
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed) {
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
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !allowBorrowed) {
        continue;
      }
      binding.typeName = base;
      binding.typeTemplateArg = argText;
    } else {
      binding.typeName = returnType;
    }
    if ((allowBorrowed && isExperimentalSoaVectorOrBorrowedTypeText(bindingTypeText(binding))) ||
        (!allowBorrowed && isExperimentalSoaVectorBinding(binding))) {
      return binding;
    }
  }
  return std::nullopt;
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorOrBorrowedReturnBinding(
    const Definition &def) {
  return extractExperimentalSoaVectorReturnBindingImpl(def, true);
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                const std::unordered_map<std::string, std::string>
                                                                    &specializedSoaVectorElementTypes,
                                                                std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        if (!base.empty() && base.front() == '/') {
          base.erase(base.begin());
        }
        if (semantics::isInternalOrExperimentalSoaStorageTypePath(base) &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      auto specializedIt = specializedSoaVectorElementTypes.find(resolvedPath);
      if (specializedIt != specializedSoaVectorElementTypes.end()) {
        elemTypeOut = specializedIt->second;
        return true;
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      auto normalizedIt = specializedSoaVectorElementTypes.find("/" + normalizedResolvedPath);
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        normalizedIt = specializedSoaVectorElementTypes.find(normalizedResolvedPath);
      }
      if (normalizedIt == specializedSoaVectorElementTypes.end()) {
        return false;
      }
      elemTypeOut = normalizedIt->second;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut) {
  static const std::unordered_map<std::string, std::string> emptySpecializedSoaVectorElementTypes;
  return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
      binding, emptySpecializedSoaVectorElementTypes, elemTypeOut);
}

struct SoaFieldViewFieldInfo {
  size_t index = 0;
  std::string typeText;
};

Expr makeI32LiteralExpr(uint64_t value, int sourceLine, int sourceColumn) {
  Expr expr;
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = 32;
  expr.isUnsigned = false;
  expr.sourceLine = sourceLine;
  expr.sourceColumn = sourceColumn;
  return expr;
}

std::optional<size_t> extractNonNegativeI32LiteralIndex(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal || expr.isUnsigned ||
      (expr.intWidth != 0 && expr.intWidth != 32)) {
    return std::nullopt;
  }
  if (expr.literalValue > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return std::nullopt;
  }
  return static_cast<size_t>(expr.literalValue);
}

bool extractSoaFieldViewElementTypeText(std::string typeText, std::string &elemTypeOut) {
  elemTypeOut.clear();
  std::string normalized = semantics::normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!semantics::splitTemplateTypeName(normalized, base, argText) || base.empty()) {
      return false;
    }
    base = semantics::normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalized = semantics::normalizeBindingTypeName(args.front());
      continue;
    }
    if (semantics::isSoaFieldViewTypePath(base)) {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    }
    return false;
  }
}

bool extractSoaFieldViewElementTypeFromBinding(const semantics::BindingInfo &binding,
                                               std::string &elemTypeOut) {
  if (binding.typeName.empty()) {
    elemTypeOut.clear();
    return false;
  }
  const std::string typeText = binding.typeTemplateArg.empty()
                                   ? binding.typeName
                                   : binding.typeName + "<" + binding.typeTemplateArg + ">";
  return extractSoaFieldViewElementTypeText(typeText, elemTypeOut);
}

std::string qualifySoaFieldViewTypeText(const std::string &typeText,
                                        const std::string &namespacePrefix,
                                        const std::unordered_set<std::string> &structPaths) {
  const std::string normalized = semantics::normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
      return normalized;
    }
    for (std::string &arg : args) {
      arg = qualifySoaFieldViewTypeText(arg, namespacePrefix, structPaths);
    }
    const std::string resolvedBase = semantics::resolveStructTypePath(
        base, namespacePrefix, structPaths);
    const std::string qualifiedBase = resolvedBase.empty() ? base : resolvedBase;
    std::string rebuilt = qualifiedBase;
    rebuilt.push_back('<');
    for (size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        rebuilt.append(", ");
      }
      rebuilt.append(args[i]);
    }
    rebuilt.push_back('>');
    return rebuilt;
  }
  const std::string resolved = semantics::resolveStructTypePath(
      normalized, namespacePrefix, structPaths);
  return resolved.empty() ? normalized : resolved;
}

bool extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(
    const Definition &def,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (def.fullPath.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) != 0) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    if (semantics::normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" ||
        fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!semantics::splitTemplateTypeName(semantics::normalizeBindingTypeName(fieldBinding.typeTemplateArg),
                                          pointeeBase,
                                          pointeeArgText) ||
        semantics::normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    std::vector<std::string> pointeeArgs;
    if (!semantics::splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) ||
        pointeeArgs.size() != 1) {
      continue;
    }
    elemTypeOut = pointeeArgs.front();
    return true;
  }
  return false;
}

bool extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(
    const Definition &def,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  if (!semantics::isExperimentalSoaVectorSpecializedTypePath(def.fullPath)) {
    return false;
  }
  for (const auto &fieldExpr : def.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    semantics::BindingInfo fieldBinding;
    std::optional<std::string> restrictType;
    std::string parseError;
    static const std::unordered_set<std::string> emptyStructTypes;
    static const std::unordered_map<std::string, std::string> emptyImportAliases;
    if (!semantics::parseBindingInfo(fieldExpr,
                                     def.namespacePrefix,
                                     emptyStructTypes,
                                     emptyImportAliases,
                                     fieldBinding,
                                     restrictType,
                                     parseError)) {
      continue;
    }
    std::string normalizedFieldType = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (!fieldBinding.typeTemplateArg.empty() &&
        (normalizedFieldType == "SoaColumn" ||
         normalizedFieldType == "std/collections/internal_soa_storage/SoaColumn")) {
      std::vector<std::string> args;
      if (!semantics::splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      elemTypeOut = args.front();
      return true;
    }
    std::string resolvedFieldPath = semantics::normalizeBindingTypeName(fieldBinding.typeName);
    if (!resolvedFieldPath.empty() && resolvedFieldPath.front() != '/') {
      resolvedFieldPath.insert(resolvedFieldPath.begin(), '/');
    }
    auto defIt = definitionMap.find(resolvedFieldPath);
    if (defIt == definitionMap.end() || defIt->second == nullptr) {
      continue;
    }
    if (!extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(*defIt->second, elemTypeOut)) {
      continue;
    }
    return true;
  }
  return false;
}

std::unordered_map<std::string, std::string> buildSpecializedExperimentalSoaVectorElementTypes(
    const Program &program) {
  std::unordered_map<std::string, const Definition *> definitionMap;
  for (const Definition &def : program.definitions) {
    definitionMap[def.fullPath] = &def;
  }

  std::unordered_map<std::string, std::string> elemTypes;
  for (const Definition &def : program.definitions) {
    std::string elemType;
    if (!extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(def, definitionMap, elemType)) {
      continue;
    }
    elemTypes[def.fullPath] = elemType;
    if (!def.fullPath.empty() && def.fullPath.front() == '/') {
      elemTypes[def.fullPath.substr(1)] = elemType;
    }
  }
  return elemTypes;
}

bool extractExperimentalSoaVectorElementTypeForToAosRewrite(const semantics::BindingInfo &binding,
                                                            std::string &elemTypeOut) {
  auto extractFromTypeText = [&](std::string normalizedType) {
    while (true) {
      std::string base;
      std::string argText;
      if (semantics::splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        base = semantics::normalizeBindingTypeName(base);
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          normalizedType = semantics::normalizeBindingTypeName(args.front());
          continue;
        }
        std::string normalizedBase = base;
        if (!normalizedBase.empty() && normalizedBase.front() == '/') {
          normalizedBase.erase(normalizedBase.begin());
        }
        if (semantics::isExperimentalSoaVectorTypePath(normalizedBase) &&
            !argText.empty()) {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return false;
          }
          elemTypeOut = args.front();
          return true;
        }
      }

      std::string resolvedPath = normalizedType;
      if (!resolvedPath.empty() && resolvedPath.front() != '/') {
        resolvedPath.insert(resolvedPath.begin(), '/');
      }
      std::string normalizedResolvedPath = semantics::normalizeBindingTypeName(resolvedPath);
      if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
        normalizedResolvedPath.erase(normalizedResolvedPath.begin());
      }
      if (!semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
        return false;
      }
      elemTypeOut = resolvedPath;
      return true;
    }
  };

  elemTypeOut.clear();
  if (binding.typeTemplateArg.empty()) {
    return extractFromTypeText(semantics::normalizeBindingTypeName(binding.typeName));
  }
  return extractFromTypeText(
      semantics::normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
}

std::optional<semantics::BindingInfo> extractExperimentalSoaVectorFieldViewReceiverBinding(const Expr &expr) {
  if (auto directBinding = extractExperimentalSoaVectorBinding(expr); directBinding.has_value()) {
    return directBinding;
  }

  for (const auto &transform : expr.transforms) {
    if ((transform.name != "Reference" && transform.name != "Pointer") ||
        transform.templateArgs.size() != 1 || !transform.arguments.empty()) {
      continue;
    }
    semantics::BindingInfo binding;
    binding.typeName = transform.name;
    binding.typeTemplateArg = transform.templateArgs.front();
    std::string elemType;
    if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(binding, elemType)) {
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

bool hasVisibleRootSoaHelper(const Program &program, std::string_view helperName) {
  const std::string rootPath = "/" + std::string(helperName);
  const std::string samePath = "/soa" "_vector/" + std::string(helperName);
  const std::string canonicalPath =
      "/std/collections/" "soa" "_vector/" + std::string(helperName);
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
  const std::string samePath = "/soa" "_vector/" + std::string(helperName);
  const std::string canonicalPath =
      "/std/collections/" "soa" "_vector/" + std::string(helperName);
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
  const std::string samePath = "/soa" "_vector/" + std::string(helperName);
  const std::string canonicalPath =
      "/std/collections/" "soa" "_vector/" + std::string(helperName);
  for (const Definition &def : program.definitions) {
    if ((def.fullPath != samePath && def.fullPath != canonicalPath) ||
        def.parameters.empty()) {
      continue;
    }
    if (extractExperimentalSoaVectorBinding(def.parameters.front()).has_value()) {
      return true;
    }
  }
  const auto &importPaths = program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (localImportPathCoversTarget(importPath, samePath) ||
        localImportPathCoversTarget(importPath, canonicalPath)) {
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

struct BuiltinSoaReturnInfo {
  semantics::BindingInfo binding;
  std::string namespacePrefix;
  std::unordered_set<std::string> templateParams;
};

std::string builtinSoaAccessHelperName(std::string_view rawName);
std::string builtinSoaCountHelperName(std::string_view rawName);
std::optional<semantics::BindingInfo> extractParsedBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes);

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
    const std::unordered_set<std::string> *structTypes = nullptr) {
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
    const std::unordered_set<std::string> *structTypes = nullptr) {
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
        if (initPath == "/std/collections/" "soa/soa" ||
            initPath == "/std/collections/" "soa/single" ||
            initPath == "/std/collections/" "soa/from" "_aos" ||
            initPath == "/std/collections/internal_soa" "_vector/soa" "VectorNew" ||
            initPath == "/std/collections/internal_soa" "_vector/soa" "VectorSingle" ||
            initPath == "/std/collections/internal_soa" "_vector/soa" "VectorFromAos" ||
            initPath == "/std/collections/experimental" "_soa" "_vector/soa" "VectorNew" ||
            initPath == "/std/collections/experimental" "_soa" "_vector/soa" "VectorSingle" ||
            initPath == "/std/collections/experimental" "_soa" "_vector/soa" "VectorFromAos") {
          semantics::BindingInfo inferred = *parsed;
          inferred.typeName = "Soa" "Vector";
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

struct BuiltinSoaReceiverBindingInfo {
  semantics::BindingInfo binding;
  bool borrowed = false;
};

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
  if (normalized == "to" "_aos") {
    return "to" "_aos";
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

void rewriteBuiltinSoaConversionMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace);

void rewriteBuiltinSoaConversionMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaConversionMethodExpr(
        stmt, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaConversionMethodStatements(
          stmt.bodyArguments, bodyBindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
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
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaConversionMethodExpr(
        arg, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
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
    if (helperName == "to" "_aos") {
      return isBuiltinSoaVectorBinding(binding);
    }
    return false;
  };
  const auto &returnDefinitions =
      helperName == "to_soa" ? vectorReturnDefinitions : soaCollectionReturnDefinitions;
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
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
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
        def.statements, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaConversionMethodExpr(
          *def.returnExpr, bindings, vectorReturnDefinitions, soaCollectionReturnDefinitions, definitionNamespace);
    }
  }
  return true;
}

void rewriteBuiltinSoaToAosCallExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper);

void rewriteBuiltinSoaToAosCallStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveVisibleRootSoaHelper,
    bool preserveVisibleRootVectorHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaToAosCallExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaToAosCallStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
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
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
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
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate)
      -> std::optional<BuiltinSoaReceiverBindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinSoaReceiverBinding(bindingIt->second);
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == semantics::internalSoaCollectionTypeName() &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = candidate.templateArgs.front();
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        return extractBuiltinSoaReceiverBinding(returnIt->second);
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
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = vectorBinding->typeTemplateArg;
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
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
        soaCollectionReturnDefinitions,
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
      builtinSoaConversionMethodName(expr.name) != "to" "_aos") {
    return;
  }

  const bool hasBuiltinSoaReceiver =
      findBuiltinSoaValueBinding(expr.args.front()).has_value();
  const bool hasBuiltinVectorReceiver =
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }
  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (expr.isMethodCall &&
        ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
         (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper))) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = "/to" "_aos";
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper && expr.isMethodCall) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to" "_aos";
    expr.namespacePrefix.clear();
    expr.templateArgs.clear();
    return;
  }
  if ((hasBuiltinSoaReceiver && preserveVisibleRootSoaHelper) ||
      (hasBuiltinVectorReceiver && preserveVisibleRootVectorHelper)) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath("to" "_aos");
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
}

bool rewriteBuiltinSoaToAosCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveVisibleRootSoaHelper =
      hasVisibleRootSoaHelperForReceiverType(
          program, "to" "_aos", semantics::internalSoaCollectionTypeName());
  const bool preserveVisibleRootVectorHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "to" "_aos", "vector");
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
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveVisibleRootSoaHelper,
        preserveVisibleRootVectorHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaToAosCallExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
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
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper);

void rewriteBuiltinSoaAccessStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaAccessExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaAccessStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper);
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
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveGetHelper,
    bool preserveGetRefHelper,
    bool preserveRefHelper,
    bool preserveRefRefHelper) {
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
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate)
      -> std::optional<BuiltinSoaReceiverBindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinSoaReceiverBinding(bindingIt->second);
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == semantics::internalSoaCollectionTypeName() &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = candidate.templateArgs.front();
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        return extractBuiltinSoaReceiverBinding(returnIt->second);
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
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
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
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
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const std::string resolvedHelperName =
      receiverBinding.has_value() && receiverBinding->borrowed
          ? borrowedBuiltinSoaAccessHelperName(helperName)
          : helperName;
  if (resolvedHelperName.empty()) {
    return;
  }
  if (helperName == resolvedHelperName &&
      ((resolvedHelperName == "get" && preserveGetHelper) ||
       (resolvedHelperName == "get_ref" && preserveGetRefHelper) ||
       (resolvedHelperName == "ref" && preserveRefHelper) ||
       (resolvedHelperName == "ref_ref" && preserveRefRefHelper))) {
    return;
  }
  const bool hasBuiltinSoaReceiver = receiverBinding.has_value();
  const bool hasBuiltinVectorReceiver =
      !receiverBinding.has_value() &&
      findBuiltinVectorValueBinding(expr.args.front()).has_value();
  if (!hasBuiltinSoaReceiver && !hasBuiltinVectorReceiver) {
    return;
  }

  const auto fallbackVectorBinding = receiverBinding.has_value()
                                         ? std::optional<semantics::BindingInfo>{}
                                         : findBuiltinVectorValueBinding(expr.args.front());

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath(resolvedHelperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->binding.typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->binding.typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaAccessCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorOrBorrowedReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveGetHelper = hasVisibleRootSoaHelper(program, "get");
  const bool preserveGetRefHelper = hasVisibleRootSoaHelper(program, "get_ref");
  const bool preserveRefHelper = hasVisibleRootSoaHelper(program, "ref");
  const bool preserveRefRefHelper = hasVisibleRootSoaHelper(program, "ref_ref");
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
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveGetHelper,
        preserveGetRefHelper,
        preserveRefHelper,
        preserveRefRefHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaAccessExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveGetHelper,
          preserveGetRefHelper,
          preserveRefHelper,
          preserveRefRefHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaCountExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper);

void rewriteBuiltinSoaCountStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaCountExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaCountStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveCountHelper,
          preserveCountRefHelper);
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
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preserveCountHelper,
    bool preserveCountRefHelper) {
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
  auto findBuiltinSoaValueBinding = [&](const Expr &candidate)
      -> std::optional<BuiltinSoaReceiverBindingInfo> {
    if (candidate.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(candidate.name);
      if (bindingIt != bindings.end()) {
        return extractBuiltinSoaReceiverBinding(bindingIt->second);
      }
      return std::nullopt;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    std::string collectionName;
    if (semantics::getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == semantics::internalSoaCollectionTypeName() &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = candidate.templateArgs.front();
      return BuiltinSoaReceiverBindingInfo{binding, false};
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        return extractBuiltinSoaReceiverBinding(returnIt->second);
      }
    }
    if (!candidate.isMethodCall &&
        semantics::isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &derefTarget = candidate.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          if (auto binding = extractBuiltinCollectionBindingFromWrappedTypeText(
                  bindingTypeText(bindingIt->second),
                  semantics::internalSoaCollectionTypeName());
              binding.has_value()) {
            return BuiltinSoaReceiverBindingInfo{*binding, false};
          }
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
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 1 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string helperName = builtinSoaCountHelperName(expr.name);
  if (helperName.empty()) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const std::string resolvedHelperName =
      receiverBinding.has_value() && receiverBinding->borrowed
          ? borrowedBuiltinSoaCountHelperName(helperName)
          : helperName;
  if (resolvedHelperName.empty()) {
    return;
  }
  if (helperName == resolvedHelperName &&
      ((resolvedHelperName == "count" && preserveCountHelper) ||
       (resolvedHelperName == "count_ref" && preserveCountRefHelper))) {
    return;
  }
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
  expr.name = semantics::compatibilitySoaHelperTargetPath(resolvedHelperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->binding.typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->binding.typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() && !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaCountCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorOrBorrowedReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preserveCountHelper = hasVisibleRootSoaHelper(program, "count");
  const bool preserveCountRefHelper = hasVisibleRootSoaHelper(program, "count_ref");
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
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preserveCountHelper,
        preserveCountRefHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaCountExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preserveCountHelper,
          preserveCountRefHelper);
    }
  }
  return true;
}

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper);

void rewriteBuiltinSoaMutatorStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
  for (Expr &stmt : statements) {
    rewriteBuiltinSoaMutatorExpr(
        stmt,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteBuiltinSoaMutatorStatements(
          stmt.bodyArguments,
          bodyBindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
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

void rewriteBuiltinSoaMutatorExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &vectorReturnDefinitions,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    bool preservePushHelper,
    bool preserveReserveHelper,
    bool preserveVectorPushHelper,
    bool preserveVectorReserveHelper) {
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
        collectionName == semantics::internalSoaCollectionTypeName() &&
        candidate.templateArgs.size() == 1) {
      semantics::BindingInfo binding;
      binding.typeName = semantics::internalSoaCollectionTypeName();
      binding.typeTemplateArg = candidate.templateArgs.front();
      return binding;
    }
    for (const std::string &candidatePath : candidateDefinitionPaths(candidate, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() && isBuiltinSoaVectorBinding(returnIt->second)) {
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
              bindingTypeText(bindingIt->second),
              semantics::internalSoaCollectionTypeName());
        }
      }
      std::string accessName;
      if (semantics::getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(derefTarget.args.front().name);
        if (bindingIt != bindings.end()) {
          return extractBuiltinCollectionBindingFromWrappedTypeText(
              bindingTypeText(bindingIt->second),
              semantics::internalSoaCollectionTypeName());
        }
      }
    }
    return std::nullopt;
  };

  for (Expr &arg : expr.args) {
    rewriteBuiltinSoaMutatorExpr(
        arg,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
  }
  if (expr.kind != Expr::Kind::Call || expr.args.size() != 2 ||
      !expr.templateArgs.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return;
  }
  const std::string rawHelperName =
      expr.namespacePrefix.empty() ? expr.name : expr.namespacePrefix + "/" + expr.name;
  const std::string helperName = builtinSoaMutatorHelperName(rawHelperName);
  if (helperName.empty()) {
    return;
  }
  if ((helperName == "push" && preservePushHelper) ||
      (helperName == "reserve" && preserveReserveHelper)) {
    return;
  }
  const auto receiverBinding = findBuiltinSoaValueBinding(expr.args.front());
  const auto fallbackVectorBinding =
      receiverBinding.has_value()
          ? std::optional<semantics::BindingInfo>{}
          : findBuiltinVectorValueBinding(expr.args.front());
  const std::string explicitOldHelperName = oldExplicitSoaMutatorHelperName(rawHelperName);
  const bool preserveVectorHelper =
      (helperName == "push" && preserveVectorPushHelper) ||
      (helperName == "reserve" && preserveVectorReserveHelper);
  if (fallbackVectorBinding.has_value() && preserveVectorHelper) {
    if (expr.isMethodCall) {
      expr.isMethodCall = false;
      expr.isFieldAccess = false;
      expr.name = semantics::samePathSoaHelperTargetPath(helperName);
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    return;
  }
  if (fallbackVectorBinding.has_value() && explicitOldHelperName.empty()) {
    return;
  }
  if (!receiverBinding.has_value() && !fallbackVectorBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath(helperName);
  expr.namespacePrefix.clear();
  expr.templateArgs.clear();
  if (receiverBinding.has_value() && !receiverBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(receiverBinding->typeTemplateArg);
  } else if (fallbackVectorBinding.has_value() &&
             !fallbackVectorBinding->typeTemplateArg.empty()) {
    expr.templateArgs.push_back(fallbackVectorBinding->typeTemplateArg);
  }
}

bool rewriteBuiltinSoaMutatorCalls(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> vectorReturnDefinitions;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractBuiltinVectorReturnBinding(def); binding.has_value()) {
      vectorReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        vectorReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (auto binding = extractBuiltinSoaVectorReturnBinding(def); binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
  }
  const bool preservePushHelper = hasVisibleRootSoaHelper(program, "push");
  const bool preserveReserveHelper = hasVisibleRootSoaHelper(program, "reserve");
  const bool preserveVectorPushHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "push", "vector");
  const bool preserveVectorReserveHelper =
      hasVisibleRootSoaHelperForReceiverType(program, "reserve", "vector");
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
    rewriteBuiltinSoaMutatorStatements(
        def.statements,
        bindings,
        vectorReturnDefinitions,
        soaCollectionReturnDefinitions,
        definitionNamespace,
        preservePushHelper,
        preserveReserveHelper,
        preserveVectorPushHelper,
        preserveVectorReserveHelper);
    if (def.returnExpr.has_value()) {
      rewriteBuiltinSoaMutatorExpr(
          *def.returnExpr,
          bindings,
          vectorReturnDefinitions,
          soaCollectionReturnDefinitions,
          definitionNamespace,
          preservePushHelper,
          preserveReserveHelper,
          preserveVectorPushHelper,
          preserveVectorReserveHelper);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper);

std::vector<std::string> candidatePathsForExprCall(
    const Expr &callExpr,
    const std::string &definitionNamespace,
    const std::unordered_map<std::string, semantics::BindingInfo> *bindings,
    const std::unordered_set<std::string> *structPaths);

Expr canonicalizeResolvedCallPath(const Expr &callExpr, const std::string &resolvedPath);

bool isStructLikeDefinition(const Definition &def);

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers);

void rewriteExperimentalSoaSamePathHelperMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        stmt,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaSamePathHelperMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
          binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaSamePathHelperMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &visibleSoaHelpers) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaSamePathHelperMethodExpr(
        arg,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }

  std::string helperName = expr.name;
  if (!helperName.empty() && helperName.front() == '/') {
    helperName.erase(helperName.begin());
  }
  if (helperName.rfind("std/collections/" "soa" "_vector/", 0) == 0) {
    helperName = helperName.substr(std::string("std/collections/" "soa" "_vector/").size());
  } else if (helperName.rfind("soa" "_vector/", 0) == 0) {
    helperName = helperName.substr(std::string("soa" "_vector/").size());
  }
  if (helperName != "count" && helperName != "count_ref" &&
      helperName != "get" && helperName != "get_ref" &&
      helperName != "ref" && helperName != "ref_ref" &&
      helperName != "push" && helperName != "reserve") {
    return;
  }
  const std::string helperPath = "/soa" "_vector/" + helperName;
  const bool hasVisibleSamePathHelper =
      visibleSoaHelpers.count(helperPath) > 0;

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isExperimentalSoaVectorBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isExperimentalSoaVectorBinding(returnIt->second)) {
        receiverBinding = returnIt->second;
        canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
        break;
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = hasVisibleSamePathHelper ? helperPath
                                       : "/std/collections/" "soa" "_vector/" + helperName;
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaSamePathHelperMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaHelpers;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorReturnBindingImpl(def, false);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (std::string_view helperName : {
           std::string_view("count"),
           std::string_view("count_ref"),
           std::string_view("get"),
           std::string_view("get_ref"),
           std::string_view("ref"),
           std::string_view("ref_ref"),
           std::string_view("push"),
           std::string_view("reserve")}) {
    if (hasVisibleExperimentalSoaSamePathHelper(program, helperName)) {
      visibleSoaHelpers.insert("/soa" "_vector/" + std::string(helperName));
    }
  }
  for (Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa" "_vector/", 0) == 0 ||
        def.fullPath.rfind("/std/collections/" "soa" "_vector/", 0) == 0 ||
        def.fullPath.rfind("/std/collections/experimental" "_soa" "_vector/", 0) == 0) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths);
          binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaSamePathHelperMethodStatements(
        def.statements,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        visibleSoaHelpers);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
            binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaSamePathHelperMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          visibleSoaHelpers);
    }
  }
  return true;
}

void rewriteExperimentalSoaToAosMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaToAosMethodExpr(
        stmt,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaToAosMethodStatements(
          stmt.bodyArguments,
          bodyBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaToAosMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace,
    bool hasVisibleRootToAosHelper,
    bool hasVisibleCanonicalToAosHelper) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaToAosMethodExpr(
        arg,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
  }
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.args.empty() ||
      expr.args.front().kind == Expr::Kind::Literal) {
    return;
  }
  if (builtinSoaConversionMethodName(expr.name) != "to" "_aos") {
    return;
  }

  std::optional<semantics::BindingInfo> receiverBinding;
  std::optional<Expr> canonicalReceiverExpr;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForToAosRewrite(binding, ignoredElemType);
  };
  const Expr &receiver = expr.args.front();
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      receiverBinding = bindingIt->second;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (semantics::isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1 &&
        receiver.args.front().kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.args.front().name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        receiverBinding = bindingIt->second;
      }
    }
    const std::vector<std::string> candidatePaths =
        candidatePathsForExprCall(receiver, definitionNamespace, &bindings, &structPaths);
    if (!receiverBinding.has_value()) {
      for (const std::string &candidatePath : candidatePaths) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          receiverBinding = returnIt->second;
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          break;
        }
      }
    }
  }
  if (!receiverBinding.has_value()) {
    return;
  }

  const bool isExplicitRootToAosSurface =
      !expr.name.empty() && expr.name.front() == '/';
  if (isExplicitRootToAosSurface) {
    if (!hasVisibleRootToAosHelper) {
      return;
    }
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to" "_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleRootToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = "/to" "_aos";
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }
  if (hasVisibleCanonicalToAosHelper) {
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.name = semantics::compatibilitySoaHelperTargetPath("to" "_aos");
    expr.namespacePrefix.clear();
    if (canonicalReceiverExpr.has_value()) {
      expr.args.front() = *canonicalReceiverExpr;
    }
    return;
  }

  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.name = semantics::compatibilitySoaHelperTargetPath("to" "_aos");
  expr.namespacePrefix.clear();
  if (canonicalReceiverExpr.has_value()) {
    expr.args.front() = *canonicalReceiverExpr;
  }
}

bool rewriteExperimentalSoaToAosMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  const bool hasVisibleRootToAosHelper =
      hasVisibleRootExperimentalSoaHelper(program, "to" "_aos");
  bool hasVisibleCanonicalToAosHelper = false;
  auto canonicalizeSoaToAosDefinitionPath = [](std::string path) {
    const size_t specializationSuffix = path.find("__");
    if (specializationSuffix != std::string::npos) {
      path.erase(specializationSuffix);
    }
    return path;
  };
  auto isCanonicalSoaToAosDefinitionPath = [&](std::string_view path) {
    const std::string canonicalPath =
        canonicalizeSoaToAosDefinitionPath(std::string(path));
    return canonicalPath.rfind("/std/collections/" "soa" "_vector/", 0) == 0 &&
           semantics::isLegacyOrCanonicalSoaHelperPath(canonicalPath, "to" "_aos");
  };
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
    if (isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      hasVisibleCanonicalToAosHelper = true;
    }
  }
  const auto &importPaths =
      program.sourceImports.empty() ? program.imports : program.sourceImports;
  for (const auto &importPath : importPaths) {
    const std::string canonicalToAosImportTarget =
        semantics::compatibilitySoaHelperTargetPath("to" "_aos");
    if (isCanonicalSoaToAosDefinitionPath(canonicalToAosImportTarget) &&
        localImportPathCoversTarget(importPath, canonicalToAosImportTarget)) {
      hasVisibleCanonicalToAosHelper = true;
    }
    if (hasVisibleCanonicalToAosHelper) {
      break;
    }
  }
  for (Definition &def : program.definitions) {
    if (def.fullPath == "/to" "_aos" ||
        def.fullPath.rfind("/to" "_aos__", 0) == 0 ||
        isCanonicalSoaToAosDefinitionPath(def.fullPath)) {
      continue;
    }
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaToAosMethodStatements(
        def.statements,
        bindings,
        soaCollectionReturnDefinitions,
        structPaths,
        definitionNamespace,
        hasVisibleRootToAosHelper,
        hasVisibleCanonicalToAosHelper);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaToAosMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace,
          hasVisibleRootToAosHelper,
          hasVisibleCanonicalToAosHelper);
    }
  }
  return true;
}

std::vector<std::string> candidatePathsForExprCall(
    const Expr &callExpr,
    const std::string &definitionNamespace,
    const std::unordered_map<std::string, semantics::BindingInfo> *bindings = nullptr,
    const std::unordered_set<std::string> *structPaths = nullptr) {
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

std::optional<Expr> normalizeExperimentalSoaInlineBorrowReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> *soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> *structPaths) {
  auto canonicalExperimentalSoaReceiver = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind == Expr::Kind::Name) {
      const std::string &name = expr.name;
      auto bindingIt = bindings.find(name);
      if (bindingIt == bindings.end()) {
        return std::nullopt;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              bindingIt->second, ignoredElemType)) {
        return expr;
      }
      return std::nullopt;
    }
    if (expr.kind != Expr::Kind::Call || expr.isBinding || soaCollectionReturnDefinitions == nullptr) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions->find(candidatePath);
      if (returnIt == soaCollectionReturnDefinitions->end()) {
        continue;
      }
      std::string ignoredElemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              returnIt->second, ignoredElemType)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return std::nullopt;
  }
  if (semantics::isSimpleCallName(receiver, "location") && receiver.args.size() == 1) {
    if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(receiver.args.front());
        canonicalReceiver.has_value()) {
      return canonicalReceiver;
    }
  }
  if (semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedExpr = receiver.args.front();
    if (borrowedExpr.kind == Expr::Kind::Call &&
        !borrowedExpr.isBinding &&
        semantics::isSimpleCallName(borrowedExpr, "location") &&
        borrowedExpr.args.size() == 1) {
      if (auto canonicalReceiver = canonicalExperimentalSoaReceiver(borrowedExpr.args.front());
          canonicalReceiver.has_value()) {
        return canonicalReceiver;
      }
    }
  }
  return std::nullopt;
}

std::optional<Expr> normalizeExperimentalSoaBorrowedHelperReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &structPaths) {
  auto makeDereferenceCall = [](Expr borrowedExpr) {
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.sourceLine = borrowedExpr.sourceLine;
    dereferenceCall.sourceColumn = borrowedExpr.sourceColumn;
    dereferenceCall.args.push_back(std::move(borrowedExpr));
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    return dereferenceCall;
  };
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &expr) -> std::optional<Expr> {
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(expr, definitionNamespace, &bindings, &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(expr, candidatePath);
      }
    }
    return std::nullopt;
  };
  if (auto normalizedInline = normalizeExperimentalSoaInlineBorrowReceiver(
          receiver, bindings, &soaCollectionReturnDefinitions, definitionNamespace, &structPaths);
      normalizedInline.has_value()) {
    return normalizedInline;
  }
  if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
      return makeDereferenceCall(receiver);
    }
  }
  if (auto canonicalReceiver = canonicalBorrowedExperimentalSoaCall(receiver);
      canonicalReceiver.has_value()) {
    return makeDereferenceCall(*canonicalReceiver);
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding &&
      semantics::isSimpleCallName(receiver, "dereference") &&
      receiver.args.size() == 1) {
    const Expr &borrowedSource = receiver.args.front();
    if (borrowedSource.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(borrowedSource.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
    }
    if (auto canonicalBorrowedSource = canonicalBorrowedExperimentalSoaCall(borrowedSource);
        canonicalBorrowedSource.has_value()) {
      Expr normalizedReceiver = receiver;
      normalizedReceiver.args.front() = *canonicalBorrowedSource;
      return normalizedReceiver;
    }
  }
  return std::nullopt;
}

bool normalizeExperimentalSoaBorrowedHelperMethodCall(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  auto isBorrowedBinding = [&](const semantics::BindingInfo &binding) {
    const std::string normalizedType =
        semantics::normalizeBindingTypeName(binding.typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    std::string ignoredElemType;
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, ignoredElemType);
  };
  auto canonicalBorrowedExperimentalSoaCall = [&](const Expr &candidate)
      -> std::optional<Expr> {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(candidate,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end() &&
          isBorrowedBinding(returnIt->second)) {
        return canonicalizeResolvedCallPath(candidate, candidatePath);
      }
    }
    return std::nullopt;
  };
  auto normalizedBorrowedReceiver = [&](const Expr &receiver)
      -> std::optional<Expr> {
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
        return receiver;
      }
      return std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return target;
        }
      }
      if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(target);
          canonicalBorrowedCall.has_value()) {
        return canonicalBorrowedCall;
      }
      return std::nullopt;
    }
    if (auto canonicalBorrowedCall = canonicalBorrowedExperimentalSoaCall(receiver);
        canonicalBorrowedCall.has_value()) {
      return canonicalBorrowedCall;
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
          return borrowedSource;
        }
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          if (bindingIt != bindings.end() && isBorrowedBinding(bindingIt->second)) {
            return target;
          }
        }
        if (auto canonicalBorrowedTarget =
                canonicalBorrowedExperimentalSoaCall(target);
            canonicalBorrowedTarget.has_value()) {
          return canonicalBorrowedTarget;
        }
        return std::nullopt;
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        return canonicalBorrowedSource;
      }
    }
    return std::nullopt;
  };
  auto borrowedReceiverElementType = [&](const Expr &receiver)
      -> std::optional<std::string> {
    auto bindingElementType = [&](const semantics::BindingInfo &binding)
        -> std::optional<std::string> {
      std::string elemType;
      if (extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
              binding, elemType) &&
          !elemType.empty()) {
        return elemType;
      }
      return std::nullopt;
    };
    auto elementTypeForBorrowedSource = [&](const Expr &borrowedSource)
        -> std::optional<std::string> {
      if (borrowedSource.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(borrowedSource.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (borrowedSource.kind != Expr::Kind::Call || borrowedSource.isBinding) {
        return std::nullopt;
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(borrowedSource,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    };
    if (auto normalizedBorrowed =
            normalizeExperimentalSoaBorrowedHelperReceiver(
                receiver,
                bindings,
                soaCollectionReturnDefinitions,
                definitionNamespace,
                structPaths);
        normalizedBorrowed.has_value() &&
        normalizedBorrowed->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*normalizedBorrowed, "dereference") &&
        normalizedBorrowed->args.size() == 1) {
      if (auto elemType = elementTypeForBorrowedSource(normalizedBorrowed->args.front());
          elemType.has_value()) {
        return elemType;
      }
    }
    if (receiver.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(receiver.name);
      return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                         : std::nullopt;
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidateDefinitionPaths(receiver, definitionNamespace)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "location") &&
        receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto bindingIt = bindings.find(target.name);
        return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                           : std::nullopt;
      }
      if (target.kind == Expr::Kind::Call && !target.isBinding) {
        for (const std::string &candidatePath :
             candidateDefinitionPaths(target, definitionNamespace)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
      for (const std::string &candidatePath :
           candidatePathsForExprCall(target,
                                     definitionNamespace,
                                     &bindings,
                                     &structPaths)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end()) {
          if (auto elemType = bindingElementType(returnIt->second);
              elemType.has_value()) {
            return elemType;
          }
        }
      }
      return std::nullopt;
    }
    for (const std::string &candidatePath :
         candidatePathsForExprCall(receiver,
                                   definitionNamespace,
                                   &bindings,
                                   &structPaths)) {
      auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
      if (returnIt != soaCollectionReturnDefinitions.end()) {
        if (auto elemType = bindingElementType(returnIt->second);
            elemType.has_value()) {
          return elemType;
        }
      }
    }
    if (semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (auto elemType = elementTypeForBorrowedSource(borrowedSource);
          elemType.has_value()) {
        return elemType;
      }
      if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding &&
          semantics::isSimpleCallName(borrowedSource, "location") &&
          borrowedSource.args.size() == 1) {
        const Expr &target = borrowedSource.args.front();
        if (target.kind == Expr::Kind::Name) {
          auto bindingIt = bindings.find(target.name);
          return bindingIt != bindings.end() ? bindingElementType(bindingIt->second)
                                             : std::nullopt;
        }
        if (target.kind == Expr::Kind::Call && !target.isBinding) {
          for (const std::string &candidatePath :
               candidateDefinitionPaths(target, definitionNamespace)) {
            auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
            if (returnIt != soaCollectionReturnDefinitions.end()) {
              if (auto elemType = bindingElementType(returnIt->second);
                  elemType.has_value()) {
                return elemType;
              }
            }
          }
        }
      }
      if (auto canonicalBorrowedSource =
              canonicalBorrowedExperimentalSoaCall(borrowedSource);
          canonicalBorrowedSource.has_value()) {
        for (const std::string &candidatePath :
             candidatePathsForExprCall(borrowedSource,
                                       definitionNamespace,
                                       &bindings,
                                       &structPaths)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end()) {
            if (auto elemType = bindingElementType(returnIt->second);
                elemType.has_value()) {
              return elemType;
            }
          }
        }
      }
    }
    return std::nullopt;
  };
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return false;
  }
  const std::string normalizedMethodName = [&]() {
    std::string name = expr.name;
    if (!name.empty() && name.front() == '/') {
      name.erase(name.begin());
    }
    if (name.rfind("std/collections/" "soa/", 0) == 0) {
      name = name.substr(std::string("std/collections/" "soa/").size());
    } else if (name.rfind("std/collections/" "soa" "_vector/", 0) == 0) {
      name = name.substr(std::string("std/collections/" "soa" "_vector/").size());
    } else if (name.rfind("soa/", 0) == 0) {
      name = name.substr(std::string("soa/").size());
    } else if (name.rfind("soa" "_vector/", 0) == 0) {
      name = name.substr(std::string("soa" "_vector/").size());
    }
    return name;
  }();
  if (normalizedMethodName != "count" &&
      normalizedMethodName != "count_ref" &&
      normalizedMethodName != "get" &&
      normalizedMethodName != "get_ref" &&
      normalizedMethodName != "ref" &&
      normalizedMethodName != "ref_ref" &&
      normalizedMethodName != "to" "_aos" &&
      normalizedMethodName != "to" "_aos_ref") {
    return false;
  }
  const bool isCanonicalBorrowedSoaWrapperBodyCall =
      definitionNamespace == "/std/collections/" "soa" "_vector" &&
      (normalizedMethodName == "count" || normalizedMethodName == "get" ||
       normalizedMethodName == "ref" || normalizedMethodName == "to" "_aos") &&
      expr.args.front().kind == Expr::Kind::Call &&
      semantics::isSimpleCallName(expr.args.front(), "dereference") &&
      expr.args.front().args.size() == 1 &&
      expr.args.front().args.front().kind == Expr::Kind::Name;
  if (isCanonicalBorrowedSoaWrapperBodyCall) {
    return false;
  }
  if (auto borrowedReceiver = normalizedBorrowedReceiver(expr.args.front());
      borrowedReceiver.has_value()) {
    const auto borrowedElemType = borrowedReceiverElementType(expr.args.front());
    const bool usesPublicSoaPath =
        expr.namespacePrefix == "/std/collections/" "soa" ||
        expr.namespacePrefix == "std/collections/" "soa" ||
        expr.name == "/std/collections/" "soa/count" ||
        expr.name == "/std/collections/" "soa/count_ref" ||
        expr.name == "/std/collections/" "soa/get" ||
        expr.name == "/std/collections/" "soa/get_ref" ||
        expr.name == "/std/collections/" "soa/ref" ||
        expr.name == "/std/collections/" "soa/ref_ref" ||
        expr.name == "/soa/count" ||
        expr.name == "/soa/count_ref" ||
        expr.name == "/soa/get" ||
        expr.name == "/soa/get_ref" ||
        expr.name == "/soa/ref" ||
        expr.name == "/soa/ref_ref";
    expr.isMethodCall = false;
    expr.isFieldAccess = false;
    expr.namespacePrefix.clear();
    expr.args.front() = *borrowedReceiver;
    if (borrowedElemType.has_value()) {
      expr.templateArgs.clear();
      expr.templateArgs.push_back(*borrowedElemType);
    }
    const std::string borrowedHelperRoot =
        usesPublicSoaPath ? "/std/collections/" "soa/"
                          : "/std/collections/" "soa" "_vector/";
    if (normalizedMethodName == "count" ||
        normalizedMethodName == "count_ref") {
      expr.name = borrowedHelperRoot + "count_ref";
      return true;
    }
    if (normalizedMethodName == "get" ||
        normalizedMethodName == "get_ref") {
      expr.name = borrowedHelperRoot + "get_ref";
      return true;
    }
    if (normalizedMethodName == "ref" ||
        normalizedMethodName == "ref_ref") {
      expr.name = borrowedHelperRoot + "ref_ref";
      return true;
    }
    expr.name = semantics::compatibilitySoaHelperTargetPath("to" "_aos_ref");
    return true;
  }
  if (!expr.isMethodCall && expr.name.find('/') != std::string::npos) {
    return false;
  }
  const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
      expr.args.front(), bindings, soaCollectionReturnDefinitions, definitionNamespace, structPaths);
  if (!normalizedReceiver.has_value()) {
    return false;
  }
  expr.args.front() = *normalizedReceiver;
  if (!expr.isMethodCall) {
    expr.isMethodCall = true;
  }
  return true;
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

bool isStructLikeDefinition(const Definition &def);

void rewriteExperimentalSoaInlineBorrowMethodStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        stmt, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaInlineBorrowMethodStatements(
          stmt.bodyArguments, bodyBindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaInlineBorrowMethodExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaInlineBorrowMethodExpr(
        arg, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call) {
    return;
  }
  if (expr.isFieldAccess && !expr.args.empty()) {
    Expr &receiverExpr = expr.args.front();
    normalizeExperimentalSoaBorrowedHelperMethodCall(
        receiverExpr, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
  }
  normalizeExperimentalSoaBorrowedHelperMethodCall(
      expr, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
}

bool rewriteExperimentalSoaInlineBorrowMethods(Program &program, std::string &error) {
  error.clear();
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  std::unordered_set<std::string> structPaths;
  for (const Definition &def : program.definitions) {
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaInlineBorrowMethodStatements(
        def.statements, bindings, soaCollectionReturnDefinitions, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      auto returnBindings = bindings;
      for (const Expr &stmt : def.statements) {
        if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
          returnBindings[stmt.name] = *binding;
        }
      }
      rewriteExperimentalSoaInlineBorrowMethodExpr(
          *def.returnExpr,
          returnBindings,
          soaCollectionReturnDefinitions,
          structPaths,
          definitionNamespace);
    }
  }
  return true;
}

bool isStructLikeDefinition(const Definition &def) {
  bool hasStructTransform = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return false;
    }
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (semantics::isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
  }
  bool fieldOnlyStruct = false;
  if (!hasStructTransform && !hasReturnTransform && def.parameters.empty() &&
      !def.hasReturnStatement && !def.returnExpr.has_value()) {
    fieldOnlyStruct = true;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        fieldOnlyStruct = false;
        break;
      }
    }
  }
  return hasStructTransform || fieldOnlyStruct;
}

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        stmt,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewIndexStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding =
              extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths);
          binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_set<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewIndexExpr(
        arg,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  if (fieldViewExpr.kind != Expr::Kind::Call || fieldViewExpr.isBinding ||
      fieldViewExpr.name.empty() ||
      fieldViewExpr.name.find('/') != std::string::npos ||
      !fieldViewExpr.templateArgs.empty() || fieldViewExpr.hasBodyArguments ||
      !fieldViewExpr.bodyArguments.empty() ||
      semantics::hasNamedArguments(fieldViewExpr.argNames) ||
      fieldViewExpr.args.size() != 1) {
    return;
  }

  if (visibleSoaFieldHelpers.count("/soa" "_vector/" + fieldViewExpr.name) > 0) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  bool receiverUsesCanonicalSoaVector = false;
  const Expr &receiver = fieldViewExpr.args.front();
  std::optional<Expr> canonicalReceiverExpr;
  const Expr *getReceiverExpr = &receiver;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    receiverNeedsDereference =
        semantics::normalizeBindingTypeName(binding.typeName) == "Reference" ||
        semantics::normalizeBindingTypeName(binding.typeName) == "Pointer";
    auto markCanonicalSoaVector = [&](std::string typeText) {
      while (true) {
        std::string base;
        std::string argText;
        if (!semantics::splitTemplateTypeName(
                semantics::normalizeBindingTypeName(typeText), base, argText)) {
          base = semantics::normalizeBindingTypeName(typeText);
        } else {
          base = semantics::normalizeBindingTypeName(base);
        }
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) ||
              args.size() != 1) {
            return;
          }
          typeText = args.front();
          continue;
        }
        receiverUsesCanonicalSoaVector =
            semantics::isInternalOrExperimentalSoaStorageTypePath(base);
        return;
      }
    };
    markCanonicalSoaVector(
        binding.typeTemplateArg.empty()
            ? binding.typeName
            : binding.typeName + "<" + binding.typeTemplateArg + ">");
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, specializedSoaVectorElementTypes, receiverElemType);
  };
  auto candidatePathsForCall = [&](const Expr &callExpr) {
    return candidatePathsForExprCall(callExpr, definitionNamespace, &allBindings, &structPaths);
  };
  auto tryLocationReceiverBinding = [&](const Expr &locationExpr) -> bool {
    if (!semantics::isSimpleCallName(locationExpr, "location") ||
        locationExpr.args.size() != 1) {
      return false;
    }
    const Expr &locationTarget = locationExpr.args.front();
    if (locationTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(locationTarget.name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
      auto allBindingIt = allBindings.find(locationTarget.name);
      if (allBindingIt != allBindings.end() &&
          tryReceiverBinding(allBindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
    } else if (locationTarget.kind == Expr::Kind::Call && !locationTarget.isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(locationTarget)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(locationTarget, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          return true;
        }
      }
    }
    return false;
  };
  if (const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
          receiver, bindings, soaCollectionReturnDefinitions, definitionNamespace, structPaths);
      normalizedReceiver.has_value()) {
    canonicalReceiverExpr = *normalizedReceiver;
    getReceiverExpr = &*canonicalReceiverExpr;
    const Expr *normalizedBindingSource = getReceiverExpr;
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      receiverNeedsDereference = true;
      normalizedBindingSource = &getReceiverExpr->args.front();
    }
    if (normalizedBindingSource->kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(normalizedBindingSource->name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        // handled
      } else {
        auto allBindingIt = allBindings.find(normalizedBindingSource->name);
        if (allBindingIt != allBindings.end()) {
          tryReceiverBinding(allBindingIt->second);
        }
      }
    } else if (normalizedBindingSource->kind == Expr::Kind::Call &&
               !normalizedBindingSource->isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(*normalizedBindingSource)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          break;
        }
      }
    }
  } else if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      // handled
    } else {
      auto allBindingIt = allBindings.find(receiver.name);
      if (allBindingIt != allBindings.end()) {
        tryReceiverBinding(allBindingIt->second);
      }
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (!tryLocationReceiverBinding(receiver) &&
        semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (tryLocationReceiverBinding(borrowedSource)) {
        // handled
      } else if (borrowedSource.kind == Expr::Kind::Name) {
        const std::string &sourceName = borrowedSource.name;
        auto bindingIt = bindings.find(sourceName);
        if (bindingIt != bindings.end()) {
          tryReceiverBinding(bindingIt->second);
        }
        if (receiverElemType.empty()) {
          auto allBindingIt = allBindings.find(sourceName);
          if (allBindingIt != allBindings.end()) {
            tryReceiverBinding(allBindingIt->second);
          }
        }
      } else if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding) {
        for (const std::string &candidatePath : candidatePathsForCall(borrowedSource)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end() &&
              tryReceiverBinding(returnIt->second)) {
            canonicalReceiverExpr = canonicalizeResolvedCallPath(borrowedSource, candidatePath);
            getReceiverExpr = &*canonicalReceiverExpr;
            break;
          }
        }
      }
    }
    if (receiverElemType.empty()) {
      for (const std::string &candidatePath : candidatePathsForCall(receiver)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          break;
        }
      }
    }
  }
  if (receiverElemType.empty()) {
    return;
  }

  const std::string normalizedElemType =
      semantics::normalizeBindingTypeName(receiverElemType);
  if (normalizedElemType.empty()) {
    return;
  }
  const std::string lookupNamespace =
      !getReceiverExpr->namespacePrefix.empty() ? getReceiverExpr->namespacePrefix : definitionNamespace;
  const std::string elementStructPath =
      semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structPaths);
  auto fieldIt = structFieldNames.find(elementStructPath);
  if (elementStructPath.empty() || fieldIt == structFieldNames.end() ||
      fieldIt->second.count(fieldViewExpr.name) == 0) {
    return;
  }

  Expr getCall;
  getCall.kind = Expr::Kind::Call;
  const bool useBorrowedGetHelper = receiverNeedsDereference;
  const std::string getHelperName = useBorrowedGetHelper ? "get_ref" : "get";
  getCall.name = receiverUsesCanonicalSoaVector
                     ? semantics::publicSoaHelperTargetPath(getHelperName)
                     : semantics::compatibilitySoaHelperTargetPath(getHelperName);
  getCall.templateArgs = {receiverElemType};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!useBorrowedGetHelper) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(getReceiverExpr->args.front());
      return;
    }
    callExpr.args.push_back(*getReceiverExpr);
  };
  appendReceiverValueExpr(getCall);
  getCall.args.push_back(expr.args[1]);
  getCall.argNames.resize(getCall.args.size());
  getCall.sourceLine = expr.sourceLine;
  getCall.sourceColumn = expr.sourceColumn;

  expr = {};
  expr.kind = Expr::Kind::Call;
  expr.name = fieldViewExpr.name;
  expr.isMethodCall = true;
  expr.isFieldAccess = true;
  expr.args.push_back(std::move(getCall));
  expr.argNames.push_back(std::nullopt);
  expr.sourceLine = fieldViewExpr.sourceLine;
  expr.sourceColumn = fieldViewExpr.sourceColumn;
}

bool rewriteExperimentalSoaFieldViewIndexes(Program &program, std::string &error) {
  error.clear();

  std::unordered_map<std::string, std::unordered_set<std::string>> structFieldNames;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa" "_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/" "soa" "_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/" "soa" "_vector/").size());
      visibleSoaFieldHelpers.insert("/soa" "_vector/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (!isStructLikeDefinition(def)) {
      continue;
    }
    structPaths.insert(def.fullPath);
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    std::unordered_set<std::string> fieldNames;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      fieldNames.insert(stmt.name);
    }
    if (fieldNames.empty()) {
      continue;
    }
    structFieldNames.emplace(def.fullPath, std::move(fieldNames));
  }

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewIndexStatements(
        def.statements,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldNames,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewIndexExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldNames,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewHelperStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, semantics::BindingInfo> bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        stmt,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = bindings;
      rewriteExperimentalSoaFieldViewHelperStatements(
          stmt.bodyArguments,
          bodyBindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
        bindings[stmt.name] = *binding;
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewHelperExpr(
    Expr &expr,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &allBindings,
    const std::unordered_map<std::string, semantics::BindingInfo>
        &soaCollectionReturnDefinitions,
    const std::unordered_map<std::string, std::string> &specializedSoaVectorElementTypes,
    const std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>>
        &structFieldInfo,
    const std::unordered_set<std::string> &structPaths,
    const std::unordered_set<std::string> &visibleSoaFieldHelpers,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewHelperExpr(
        arg,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call || expr.isBinding ||
      expr.hasBodyArguments || !expr.bodyArguments.empty() ||
      semantics::hasNamedArguments(expr.argNames) ||
      !expr.templateArgs.empty()) {
    return;
  }

  std::string fieldName;
  if (!expr.name.empty() && expr.name.front() == '/' &&
      semantics::splitSoaFieldViewHelperPath(expr.name, &fieldName)) {
  } else {
    if (!expr.namespacePrefix.empty()) {
      return;
    }
    fieldName = expr.name;
    if (!fieldName.empty() && fieldName.front() == '/') {
      fieldName.erase(fieldName.begin());
    }
    if (fieldName.empty() || fieldName.find('/') != std::string::npos ||
        fieldName == "count" || fieldName == "count_ref" ||
        fieldName == "get" || fieldName == "get_ref" ||
        fieldName == "ref" || fieldName == "ref_ref" ||
        fieldName == "to_soa" || fieldName == "to" "_aos" ||
        fieldName == "to" "_aos_ref") {
      return;
    }
  }

  if (visibleSoaFieldHelpers.count("/soa" "_vector/" + fieldName) > 0) {
    return;
  }
  if (expr.args.size() != 1) {
    return;
  }

  std::string receiverElemType;
  bool receiverNeedsDereference = false;
  bool receiverUsesCanonicalSoaVector = false;
  const Expr &receiver = expr.args.front();
  std::optional<Expr> canonicalReceiverExpr;
  const Expr *getReceiverExpr = &receiver;
  auto tryReceiverBinding = [&](const semantics::BindingInfo &binding) {
    receiverNeedsDereference =
        semantics::normalizeBindingTypeName(binding.typeName) == "Reference" ||
        semantics::normalizeBindingTypeName(binding.typeName) == "Pointer";
    auto markCanonicalSoaVector = [&](std::string typeText) {
      while (true) {
        std::string base;
        std::string argText;
        if (!semantics::splitTemplateTypeName(
                semantics::normalizeBindingTypeName(typeText), base, argText)) {
          base = semantics::normalizeBindingTypeName(typeText);
        } else {
          base = semantics::normalizeBindingTypeName(base);
        }
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!semantics::splitTopLevelTemplateArgs(argText, args) ||
              args.size() != 1) {
            return;
          }
          typeText = args.front();
          continue;
        }
        receiverUsesCanonicalSoaVector =
            semantics::isInternalOrExperimentalSoaStorageTypePath(base);
        return;
      }
    };
    markCanonicalSoaVector(
        binding.typeTemplateArg.empty()
            ? binding.typeName
            : binding.typeName + "<" + binding.typeTemplateArg + ">");
    return extractExperimentalSoaVectorElementTypeForFieldViewRewrite(
        binding, specializedSoaVectorElementTypes, receiverElemType);
  };
  auto candidatePathsForCall = [&](const Expr &callExpr) {
    return candidatePathsForExprCall(callExpr, definitionNamespace, &allBindings, &structPaths);
  };
  auto tryLocationReceiverBinding = [&](const Expr &locationExpr) -> bool {
    if (!semantics::isSimpleCallName(locationExpr, "location") ||
        locationExpr.args.size() != 1) {
      return false;
    }
    const Expr &locationTarget = locationExpr.args.front();
    if (locationTarget.kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(locationTarget.name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
      auto allBindingIt = allBindings.find(locationTarget.name);
      if (allBindingIt != allBindings.end() &&
          tryReceiverBinding(allBindingIt->second)) {
        getReceiverExpr = &locationTarget;
        return true;
      }
    } else if (locationTarget.kind == Expr::Kind::Call && !locationTarget.isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(locationTarget)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(locationTarget, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          return true;
        }
      }
    }
    return false;
  };
  if (const auto normalizedReceiver = normalizeExperimentalSoaBorrowedHelperReceiver(
          receiver, bindings, soaCollectionReturnDefinitions, definitionNamespace, structPaths);
      normalizedReceiver.has_value()) {
    canonicalReceiverExpr = *normalizedReceiver;
    getReceiverExpr = &*canonicalReceiverExpr;
    const Expr *normalizedBindingSource = getReceiverExpr;
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      receiverNeedsDereference = true;
      normalizedBindingSource = &getReceiverExpr->args.front();
    }
    if (normalizedBindingSource->kind == Expr::Kind::Name) {
      auto bindingIt = bindings.find(normalizedBindingSource->name);
      if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
        // handled
      } else {
        auto allBindingIt = allBindings.find(normalizedBindingSource->name);
        if (allBindingIt != allBindings.end()) {
          tryReceiverBinding(allBindingIt->second);
        }
      }
    } else if (normalizedBindingSource->kind == Expr::Kind::Call &&
               !normalizedBindingSource->isBinding) {
      for (const std::string &candidatePath : candidatePathsForCall(*normalizedBindingSource)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          break;
        }
      }
    }
  } else if (receiver.kind == Expr::Kind::Name) {
    auto bindingIt = bindings.find(receiver.name);
    if (bindingIt != bindings.end() && tryReceiverBinding(bindingIt->second)) {
      // handled
    } else {
      auto allBindingIt = allBindings.find(receiver.name);
      if (allBindingIt != allBindings.end()) {
        tryReceiverBinding(allBindingIt->second);
      }
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    if (!tryLocationReceiverBinding(receiver) &&
        semantics::isSimpleCallName(receiver, "dereference") &&
        receiver.args.size() == 1) {
      const Expr &borrowedSource = receiver.args.front();
      if (tryLocationReceiverBinding(borrowedSource)) {
        // handled
      } else if (borrowedSource.kind == Expr::Kind::Name) {
        const std::string &sourceName = borrowedSource.name;
        auto bindingIt = bindings.find(sourceName);
        if (bindingIt != bindings.end()) {
          tryReceiverBinding(bindingIt->second);
        }
        if (receiverElemType.empty()) {
          auto allBindingIt = allBindings.find(sourceName);
          if (allBindingIt != allBindings.end()) {
            tryReceiverBinding(allBindingIt->second);
          }
        }
      } else if (borrowedSource.kind == Expr::Kind::Call && !borrowedSource.isBinding) {
        for (const std::string &candidatePath : candidatePathsForCall(borrowedSource)) {
          auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
          if (returnIt != soaCollectionReturnDefinitions.end() &&
              tryReceiverBinding(returnIt->second)) {
            canonicalReceiverExpr = canonicalizeResolvedCallPath(borrowedSource, candidatePath);
            getReceiverExpr = &*canonicalReceiverExpr;
            break;
          }
        }
      }
    }
    if (receiverElemType.empty()) {
      for (const std::string &candidatePath : candidatePathsForCall(receiver)) {
        auto returnIt = soaCollectionReturnDefinitions.find(candidatePath);
        if (returnIt != soaCollectionReturnDefinitions.end() &&
            tryReceiverBinding(returnIt->second)) {
          canonicalReceiverExpr = canonicalizeResolvedCallPath(receiver, candidatePath);
          getReceiverExpr = &*canonicalReceiverExpr;
          break;
        }
      }
    }
  }
  if (receiverElemType.empty()) {
    return;
  }

  const std::string normalizedElemType =
      semantics::normalizeBindingTypeName(receiverElemType);
  if (normalizedElemType.empty()) {
    return;
  }
  const std::string lookupNamespace =
      !getReceiverExpr->namespacePrefix.empty() ? getReceiverExpr->namespacePrefix : definitionNamespace;
  const std::string elementStructPath =
      semantics::resolveStructTypePath(normalizedElemType, lookupNamespace, structPaths);
  auto structIt = structFieldInfo.find(elementStructPath);
  if (elementStructPath.empty() || structIt == structFieldInfo.end()) {
    return;
  }
  auto fieldIt = structIt->second.find(fieldName);
  if (fieldIt == structIt->second.end()) {
    return;
  }

  Expr fieldViewCall;
  fieldViewCall.kind = Expr::Kind::Call;
  fieldViewCall.name = receiverUsesCanonicalSoaVector
                           ? "/std/collections/" "soa/field_view"
                           : "/std/collections/experimental" "_soa" "_vector/soa" "VectorFieldView";
  fieldViewCall.templateArgs = {receiverElemType, fieldIt->second.typeText};
  auto appendReceiverValueExpr = [&](Expr &callExpr) {
    if (!receiverNeedsDereference) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    if (getReceiverExpr->kind == Expr::Kind::Call &&
        semantics::isSimpleCallName(*getReceiverExpr, "dereference") &&
        getReceiverExpr->args.size() == 1) {
      callExpr.args.push_back(*getReceiverExpr);
      return;
    }
    Expr dereferenceCall;
    dereferenceCall.kind = Expr::Kind::Call;
    dereferenceCall.name = "dereference";
    dereferenceCall.args.push_back(*getReceiverExpr);
    dereferenceCall.argNames.resize(dereferenceCall.args.size());
    dereferenceCall.sourceLine = getReceiverExpr->sourceLine;
    dereferenceCall.sourceColumn = getReceiverExpr->sourceColumn;
    callExpr.args.push_back(std::move(dereferenceCall));
  };
  appendReceiverValueExpr(fieldViewCall);
  fieldViewCall.args.push_back(makeI32LiteralExpr(
      static_cast<uint64_t>(fieldIt->second.index),
      expr.sourceLine,
      expr.sourceColumn));
  fieldViewCall.argNames.resize(fieldViewCall.args.size());
  fieldViewCall.sourceLine = expr.sourceLine;
  fieldViewCall.sourceColumn = expr.sourceColumn;

  expr = std::move(fieldViewCall);
  expr.isMethodCall = false;
  expr.isFieldAccess = false;
  expr.namespacePrefix.clear();
}

bool rewriteExperimentalSoaFieldViewHelpers(Program &program, std::string &error) {
  error.clear();

  std::unordered_map<std::string, std::unordered_map<std::string, SoaFieldViewFieldInfo>> structFieldInfo;
  std::unordered_set<std::string> structPaths;
  std::unordered_set<std::string> visibleSoaFieldHelpers;
  std::unordered_map<std::string, semantics::BindingInfo> soaCollectionReturnDefinitions;
  const auto specializedSoaVectorElementTypes =
      buildSpecializedExperimentalSoaVectorElementTypes(program);

  for (const Definition &def : program.definitions) {
    if (def.fullPath.rfind("/soa" "_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
    } else if (def.fullPath.rfind("/std/collections/" "soa" "_vector/", 0) == 0) {
      visibleSoaFieldHelpers.insert(def.fullPath);
      const std::string helperSuffix =
          def.fullPath.substr(std::string("/std/collections/" "soa" "_vector/").size());
      visibleSoaFieldHelpers.insert("/soa" "_vector/" + helperSuffix);
    }
    if (auto binding = extractExperimentalSoaVectorOrBorrowedReturnBinding(def);
        binding.has_value()) {
      soaCollectionReturnDefinitions[def.fullPath] = *binding;
      const size_t slash = def.fullPath.find_last_of('/');
      if (slash != std::string::npos && slash + 1 < def.fullPath.size()) {
        soaCollectionReturnDefinitions[def.fullPath.substr(slash + 1)] = *binding;
      }
    }
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
    }
  }

  static const std::unordered_map<std::string, std::string> emptyImportAliases;
  for (const Definition &def : program.definitions) {
    if (!isStructLikeDefinition(def)) {
      continue;
    }
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    std::unordered_map<std::string, SoaFieldViewFieldInfo> fields;
    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticField(stmt)) {
        continue;
      }
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (semantics::parseBindingInfo(stmt,
                                      def.namespacePrefix,
                                      structPaths,
                                      emptyImportAliases,
                                      binding,
                                      restrictType,
                                      parseError)) {
        std::string typeText = binding.typeName;
        if (!binding.typeTemplateArg.empty()) {
          typeText += "<" + binding.typeTemplateArg + ">";
        }
        fields.emplace(stmt.name,
                       SoaFieldViewFieldInfo{
                           fieldIndex,
                           qualifySoaFieldViewTypeText(typeText,
                                                       def.namespacePrefix,
                                                       structPaths)});
      }
      ++fieldIndex;
    }
    if (!fields.empty()) {
      structFieldInfo.emplace(def.fullPath, std::move(fields));
    }
  }

  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, semantics::BindingInfo> bindings;
    for (const Expr &param : def.parameters) {
      if (auto binding = extractParsedOrExperimentalSoaBindingInfo(param, &structPaths); binding.has_value()) {
        bindings[param.name] = *binding;
      }
    }
    auto allBindings = bindings;
    std::function<void(const std::vector<Expr> &)> collectBindings =
        [&](const std::vector<Expr> &statements) {
          for (const Expr &stmt : statements) {
            if (stmt.isBinding) {
              if (auto binding = extractParsedOrExperimentalSoaBindingInfo(stmt, &structPaths); binding.has_value()) {
                allBindings[stmt.name] = *binding;
              }
            }
            if (!stmt.bodyArguments.empty()) {
              collectBindings(stmt.bodyArguments);
            }
          }
        };
    collectBindings(def.statements);
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewHelperStatements(
        def.statements,
        bindings,
        allBindings,
        soaCollectionReturnDefinitions,
        specializedSoaVectorElementTypes,
        structFieldInfo,
        structPaths,
        visibleSoaFieldHelpers,
        definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewHelperExpr(
          *def.returnExpr,
          bindings,
          allBindings,
          soaCollectionReturnDefinitions,
          specializedSoaVectorElementTypes,
          structFieldInfo,
          structPaths,
          visibleSoaFieldHelpers,
          definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace);

void rewriteExperimentalSoaFieldViewCarrierIndexStatements(
    std::vector<Expr> &statements,
    std::unordered_map<std::string, std::string> fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &stmt : statements) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        stmt, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (!stmt.bodyArguments.empty()) {
      auto bodyBindings = fieldViewBindings;
      rewriteExperimentalSoaFieldViewCarrierIndexStatements(
          stmt.bodyArguments, bodyBindings, structFieldNames, structPaths, definitionNamespace);
    }
    if (stmt.isBinding) {
      if (auto parsed = extractParsedBindingInfo(stmt, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[stmt.name] = elemType;
        }
      }
      if (stmt.args.size() == 1 && fieldViewBindings.count(stmt.name) == 0) {
        const Expr &initializer = stmt.args.front();
        if (initializer.kind == Expr::Kind::Call && !initializer.isBinding) {
          std::string initPath = initializer.name;
          if (!initPath.empty() && initPath.front() != '/') {
            initPath.insert(initPath.begin(), '/');
          }
          if (semantics::isExperimentalSoaFieldViewHelperPath(initPath)) {
            if (initializer.templateArgs.size() >= 2) {
              fieldViewBindings[stmt.name] =
                  qualifySoaFieldViewTypeText(initializer.templateArgs[1],
                                              definitionNamespace,
                                              structPaths);
            }
          }
        }
      }
    }
  }
}

void rewriteExperimentalSoaFieldViewCarrierIndexExpr(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &fieldViewBindings,
    const std::unordered_map<std::string, std::vector<std::string>> &structFieldNames,
    const std::unordered_set<std::string> &structPaths,
    const std::string &definitionNamespace) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewCarrierIndexExpr(
        arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
  }
  if (expr.kind != Expr::Kind::Call ||
      expr.templateArgs.size() != 0 || expr.hasBodyArguments ||
      !expr.bodyArguments.empty() || semantics::hasNamedArguments(expr.argNames) ||
      expr.args.size() != 2) {
    return;
  }

  std::string builtinAccessName;
  if (!semantics::getBuiltinArrayAccessName(expr, builtinAccessName) ||
      builtinAccessName != "at") {
    return;
  }

  const Expr &fieldViewExpr = expr.args.front();
  std::string elemType;
  if (fieldViewExpr.kind == Expr::Kind::Name) {
    auto bindingIt = fieldViewBindings.find(fieldViewExpr.name);
    if (bindingIt != fieldViewBindings.end()) {
      elemType = bindingIt->second;
    }
  } else if (fieldViewExpr.kind == Expr::Kind::Call && !fieldViewExpr.isBinding) {
    std::string callPath = fieldViewExpr.name;
    if (!callPath.empty() && callPath.front() != '/') {
      callPath.insert(callPath.begin(), '/');
    }
    if (callPath == "/std/collections/" "soa/field_view" &&
        fieldViewExpr.templateArgs.size() >= 2 &&
        fieldViewExpr.args.size() == 2 &&
        !semantics::hasNamedArguments(fieldViewExpr.argNames)) {
      const std::string structPath = semantics::resolveStructTypePath(
          fieldViewExpr.templateArgs.front(), definitionNamespace, structPaths);
      auto fieldsIt = structFieldNames.find(structPath);
      const auto fieldIndex =
          extractNonNegativeI32LiteralIndex(fieldViewExpr.args[1]);
      if (fieldsIt != structFieldNames.end() && fieldIndex.has_value() &&
          *fieldIndex < fieldsIt->second.size()) {
        Expr getCall;
        getCall.kind = Expr::Kind::Call;
        getCall.name = "/std/collections/" "soa/get";
        getCall.templateArgs = {fieldViewExpr.templateArgs.front()};
        getCall.args.push_back(fieldViewExpr.args.front());
        getCall.args.push_back(expr.args[1]);
        getCall.argNames.resize(getCall.args.size());
        getCall.sourceLine = fieldViewExpr.sourceLine;
        getCall.sourceColumn = fieldViewExpr.sourceColumn;

        Expr fieldAccess;
        fieldAccess.kind = Expr::Kind::Call;
        fieldAccess.name = fieldsIt->second[*fieldIndex];
        fieldAccess.isMethodCall = true;
        fieldAccess.isFieldAccess = true;
        fieldAccess.args.push_back(std::move(getCall));
        fieldAccess.argNames.resize(fieldAccess.args.size());
        fieldAccess.sourceLine = fieldViewExpr.sourceLine;
        fieldAccess.sourceColumn = fieldViewExpr.sourceColumn;
        expr = std::move(fieldAccess);
        return;
      }
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(callPath)) {
      if (fieldViewExpr.templateArgs.size() >= 2) {
        elemType = qualifySoaFieldViewTypeText(fieldViewExpr.templateArgs[1],
                                               definitionNamespace,
                                               structPaths);
      }
    }
  }
  if (elemType.empty()) {
    return;
  }

  Expr readCall;
  readCall.kind = Expr::Kind::Call;
  readCall.name = "/std/collections/internal_soa_storage/soaFieldViewRead";
  readCall.templateArgs = {elemType};
  readCall.args.push_back(fieldViewExpr);
  readCall.args.push_back(expr.args[1]);
  readCall.argNames.resize(readCall.args.size());
  readCall.sourceLine = expr.sourceLine;
  readCall.sourceColumn = expr.sourceColumn;
  expr = std::move(readCall);
}

bool rewriteExperimentalSoaFieldViewCarrierIndexes(Program &program, std::string &error) {
  error.clear();
  std::unordered_set<std::string> structPaths;
  std::unordered_map<std::string, std::vector<std::string>> structFieldNames;
  for (const Definition &def : program.definitions) {
    if (isStructLikeDefinition(def)) {
      structPaths.insert(def.fullPath);
      auto isStaticField = [](const Expr &stmt) {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::vector<std::string> fields;
      for (const Expr &stmt : def.statements) {
        if (stmt.isBinding && !isStaticField(stmt)) {
          fields.push_back(stmt.name);
        }
      }
      if (!fields.empty()) {
        structFieldNames.emplace(def.fullPath, std::move(fields));
      }
    }
  }
  for (Definition &def : program.definitions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    for (const Expr &param : def.parameters) {
      if (auto parsed = extractParsedBindingInfo(param, &structPaths); parsed.has_value()) {
        std::string elemType;
        if (extractSoaFieldViewElementTypeFromBinding(*parsed, elemType)) {
          fieldViewBindings[param.name] = elemType;
        }
      }
    }
    std::string definitionNamespace;
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = def.fullPath.substr(0, slash);
    }
    rewriteExperimentalSoaFieldViewCarrierIndexStatements(
        def.statements, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          *def.returnExpr, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  for (auto &exec : program.executions) {
    std::unordered_map<std::string, std::string> fieldViewBindings;
    std::string definitionNamespace;
    const size_t slash = exec.fullPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      definitionNamespace = exec.fullPath.substr(0, slash);
    }
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewCarrierIndexExpr(
          arg, fieldViewBindings, structFieldNames, structPaths, definitionNamespace);
    }
  }
  return true;
}

void rewriteExperimentalSoaFieldViewAssignTargetsExpr(Expr &expr) {
  for (Expr &arg : expr.args) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteExperimentalSoaFieldViewAssignTargetsExpr(bodyArg);
  }

  if (!semantics::isAssignCall(expr) || expr.args.size() != 2) {
    return;
  }

  Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::Call && !target.isBinding) {
    static constexpr std::string_view fieldRefPrefix =
        "/std/collections/internal_soa_storage/soaFieldViewRef";
    if (semantics::isExperimentalSoaFieldViewReadHelperPath(target.name) &&
        target.args.size() == 2 && !target.templateArgs.empty()) {
      Expr refCall;
      refCall.kind = Expr::Kind::Call;
      refCall.name = std::string(fieldRefPrefix);
      refCall.templateArgs = {target.templateArgs.front()};
      refCall.args = target.args;
      refCall.argNames.resize(refCall.args.size());
      refCall.sourceLine = target.sourceLine;
      refCall.sourceColumn = target.sourceColumn;

      Expr dereferenceCall;
      dereferenceCall.kind = Expr::Kind::Call;
      dereferenceCall.name = "dereference";
      dereferenceCall.args.push_back(std::move(refCall));
      dereferenceCall.argNames.resize(dereferenceCall.args.size());
      dereferenceCall.sourceLine = target.sourceLine;
      dereferenceCall.sourceColumn = target.sourceColumn;

      target = std::move(dereferenceCall);
      return;
    }
    if (semantics::isExperimentalSoaFieldViewHelperPath(target.name)) {
      return;
    }
  }
  if (target.kind != Expr::Kind::Call || !target.isFieldAccess ||
      target.args.size() != 1) {
    return;
  }

  Expr &receiver = target.args.front();
  if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
    return;
  }

  static constexpr std::string_view getPrefix =
      "/std/collections/experimental" "_soa" "_vector/soa" "VectorGet";
  static constexpr std::string_view refPrefix =
      "/std/collections/experimental" "_soa" "_vector/soa" "VectorRef";
  static constexpr std::string_view fieldReadPrefix =
      "/std/collections/internal_soa_storage/soaFieldViewRead";
  static constexpr std::string_view fieldRefPrefix =
      "/std/collections/internal_soa_storage/soaFieldViewRef";
  auto rewriteSamePathSoaGetCarrierToRef = [](std::string &path) -> bool {
    const size_t specializationSuffix = path.find("__t");
    const std::string specializationText =
        specializationSuffix == std::string::npos
            ? std::string{}
            : path.substr(specializationSuffix);
    std::string basePath =
        specializationSuffix == std::string::npos
            ? path
            : path.substr(0, specializationSuffix);
    const std::string canonicalGetPath =
        semantics::canonicalizeLegacySoaGetHelperPath(basePath);
    if (canonicalGetPath == "/std/collections/" "soa" "_vector/get") {
      path = (basePath.rfind("/soa" "_vector/", 0) == 0 ? "/soa" "_vector/ref"
                                                     : "/std/collections/" "soa" "_vector/ref") +
             specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/" "soa/get") {
      path = "/std/collections/" "soa/ref" + specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/" "soa" "_vector/get_ref") {
      path =
          (basePath.rfind("/soa" "_vector/", 0) == 0 ? "/soa" "_vector/ref_ref"
                                                  : "/std/collections/" "soa" "_vector/ref_ref") +
          specializationText;
      return true;
    }
    if (canonicalGetPath == "/std/collections/" "soa/get_ref") {
      path = "/std/collections/" "soa/ref_ref" + specializationText;
      return true;
    }
    return false;
  };
  if (!semantics::isExperimentalSoaGetLikeHelperPath(receiver.name)) {
    if (!semantics::isExperimentalSoaFieldViewReadHelperPath(receiver.name)) {
      if (!rewriteSamePathSoaGetCarrierToRef(receiver.name)) {
        return;
      }
      return;
    }
    receiver.name.replace(0, fieldReadPrefix.size(), fieldRefPrefix);
    return;
  }
  receiver.name.replace(0, getPrefix.size(), refPrefix);
}

bool rewriteExperimentalSoaFieldViewAssignTargets(Program &program,
                                                  std::string &error) {
  error.clear();
  for (Definition &def : program.definitions) {
    for (Expr &stmt : def.statements) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(stmt);
    }
    if (def.returnExpr.has_value()) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(*def.returnExpr);
    }
  }
  for (auto &exec : program.executions) {
    for (Expr &arg : exec.arguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
    for (Expr &arg : exec.bodyArguments) {
      rewriteExperimentalSoaFieldViewAssignTargetsExpr(arg);
    }
  }
  return true;
}

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
  const std::string normalizedType = semantics::normalizeBindingTypeName(binding.typeName);
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
  const bool matchesBuiltinAccessCall =
      directReadHelper == "at" || directReadHelper == "at_unsafe";
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
    if (isStructLikeDefinition(def)) {
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

bool rewriteOmittedStructInitializers(Program &program, std::string &error) {
  std::unordered_set<std::string> structNames;
  structNames.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    bool hasStructTransform = false;
    bool hasReturnTransform = false;
    bool hasSumTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        hasSumTransform = true;
      }
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (semantics::isStructTransformName(transform.name)) {
        hasStructTransform = true;
      }
    }
    bool fieldOnlyStruct = false;
    if (!hasSumTransform && !hasStructTransform && !hasReturnTransform && def.parameters.empty() &&
        !def.hasReturnStatement && !def.returnExpr.has_value()) {
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
      if (normalizedType != "vector" &&
          normalizedType != semantics::internalSoaCollectionTypeName()) {
        error = "omitted initializer requires vector or soa" "_vector type: " + info.typeName;
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
    call.isBraceConstructor = true;
    const std::string structPath =
        semantics::resolveStructTypePath(info.typeName,
                                         expr.namespacePrefix,
                                         structNames);
    call.name = structPath.empty() ? info.typeName : structPath;
    call.namespacePrefix = expr.namespacePrefix;
    expr.args.clear();
    expr.argNames.clear();
    expr.args.push_back(std::move(call));
    expr.argNames.push_back(std::nullopt);
    return true;
  };

  for (auto &def : program.definitions) {
    const bool isSumDefinition =
        std::any_of(def.transforms.begin(), def.transforms.end(), [](const Transform &transform) {
          return transform.name == "sum";
        });
    if (isSumDefinition) {
      continue;
    }
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

namespace {

void eraseCompileTimeTypeBindingsFromExpr(Expr &expr);

std::string joinTypeLocalTemplateArgs(const std::vector<std::string> &args) {
  std::string text;
  for (size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    text += args[index];
  }
  return text;
}

bool splitTypeLocalTypeText(const std::string &typeText,
                            std::string &baseOut,
                            std::vector<std::string> &argsOut) {
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(typeText, base, argText)) {
    baseOut = semantics::normalizeBindingTypeName(typeText);
    argsOut.clear();
    return !baseOut.empty();
  }
  argsOut.clear();
  if (!semantics::splitTopLevelTemplateArgs(argText, argsOut)) {
    return false;
  }
  baseOut = semantics::normalizeBindingTypeName(base);
  return !baseOut.empty();
}

void applyTypeLocalFactsToBindingEnvelope(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &typeFacts) {
  if (!expr.isBinding || semantics::isCompileTimeTypeBinding(expr)) {
    return;
  }
  for (Transform &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        transform.name == "return" ||
        semantics::isBindingAuxTransformName(transform.name)) {
      continue;
    }
    for (std::string &templateArg : transform.templateArgs) {
      auto argIt = typeFacts.find(templateArg);
      if (argIt != typeFacts.end()) {
        templateArg = argIt->second;
      }
    }
    auto typeIt = typeFacts.find(transform.name);
    if (typeIt == typeFacts.end()) {
      continue;
    }
    std::string base;
    std::vector<std::string> args;
    if (splitTypeLocalTypeText(typeIt->second, base, args)) {
      transform.name = std::move(base);
      transform.templateArgs = std::move(args);
    }
  }
}

bool bindingEnvelopeTypeText(const Expr &expr, std::string &typeTextOut) {
  if (!expr.isBinding || !semantics::hasExplicitBindingTypeTransform(expr)) {
    return false;
  }
  for (const Transform &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        transform.name == "return" ||
        semantics::isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeTextOut = semantics::normalizeBindingTypeName(transform.name);
    if (!transform.templateArgs.empty()) {
      typeTextOut += "<" + joinTypeLocalTemplateArgs(transform.templateArgs) + ">";
    }
    return true;
  }
  return false;
}

bool resolveTypeLocalInitializer(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &valueFacts,
    const std::unordered_map<std::string, std::string> &typeFacts,
    std::string &typeTextOut) {
  if (!semantics::isCompileTimeTypeBinding(expr) || expr.args.size() != 1) {
    return false;
  }
  const Expr *typeExpr = &expr.args.front();
  const Expr &initializer = expr.args.front();
  if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
      initializer.hasBodyArguments && initializer.args.empty() &&
      initializer.templateArgs.empty() &&
      !semantics::hasNamedArguments(initializer.argNames)) {
    if (initializer.bodyArguments.size() != 1) {
      return false;
    }
    typeExpr = &initializer.bodyArguments.front();
  }
  if (typeExpr->kind == Expr::Kind::Name) {
    auto typeIt = typeFacts.find(typeExpr->name);
    if (typeIt != typeFacts.end()) {
      typeTextOut = typeIt->second;
      return true;
    }
    if (!typeExpr->name.empty() && typeExpr->name != "auto") {
      typeTextOut = semantics::normalizeBindingTypeName(typeExpr->name);
      return true;
    }
    return false;
  }
  if (typeExpr->kind != Expr::Kind::Call || typeExpr->name != "typeof" ||
      typeExpr->templateArgs.size() != 1 ||
      typeExpr->templateArgDetails.size() != 1 ||
      typeExpr->templateArgDetails.front().kind !=
          TemplateArgumentKind::Symbol) {
    return false;
  }
  const std::string &symbol = typeExpr->templateArgs.front();
  auto valueIt = valueFacts.find(symbol);
  if (valueIt != valueFacts.end()) {
    typeTextOut = valueIt->second;
    return true;
  }
  auto typeIt = typeFacts.find(symbol);
  if (typeIt != typeFacts.end()) {
    typeTextOut = typeIt->second;
    return true;
  }
  return false;
}

void eraseCompileTimeTypeBindingsFromExprs(
    std::vector<Expr> &exprs,
    std::unordered_map<std::string, std::string> valueFacts = {}) {
  std::unordered_map<std::string, std::string> typeFacts;
  for (Expr &expr : exprs) {
    if (semantics::isCompileTimeTypeBinding(expr)) {
      std::string typeText;
      if (resolveTypeLocalInitializer(expr, valueFacts, typeFacts, typeText)) {
        typeFacts[expr.name] = std::move(typeText);
      }
      continue;
    }
    applyTypeLocalFactsToBindingEnvelope(expr, typeFacts);
    eraseCompileTimeTypeBindingsFromExpr(expr);
    std::string valueType;
    if (bindingEnvelopeTypeText(expr, valueType)) {
      valueFacts[expr.name] = std::move(valueType);
    }
  }
  exprs.erase(std::remove_if(exprs.begin(), exprs.end(), [](const Expr &expr) {
                return semantics::isCompileTimeTypeBinding(expr);
              }),
              exprs.end());
}

void eraseCompileTimeTypeBindingsFromExpr(Expr &expr) {
  eraseCompileTimeTypeBindingsFromExprs(expr.args);
  eraseCompileTimeTypeBindingsFromExprs(expr.bodyArguments);
}

void applyEnclosingTypeLocalsToNestedStructs(Program &program) {
  std::unordered_map<std::string, Definition *> definitionsByPath;
  definitionsByPath.reserve(program.definitions.size());
  for (Definition &def : program.definitions) {
    definitionsByPath[def.fullPath] = &def;
  }

  auto sourceComesBefore = [](const Expr &expr, const Definition &def) {
    if (expr.sourceLine <= 0 || def.sourceLine <= 0) {
      return false;
    }
    if (expr.sourceLine != def.sourceLine) {
      return expr.sourceLine < def.sourceLine;
    }
    return expr.sourceColumn > 0 && expr.sourceColumn < def.sourceColumn;
  };

  for (Definition &nestedDef : program.definitions) {
    if (!nestedDef.isNested || !isStructLikeDefinition(nestedDef)) {
      continue;
    }
    auto parentIt = definitionsByPath.find(nestedDef.namespacePrefix);
    if (parentIt == definitionsByPath.end() || parentIt->second == nullptr ||
        isStructLikeDefinition(*parentIt->second)) {
      continue;
    }
    const Definition &parent = *parentIt->second;
    std::unordered_map<std::string, std::string> valueFacts;
    std::unordered_map<std::string, std::string> typeFacts;
    for (const Expr &param : parent.parameters) {
      std::string valueType;
      if (bindingEnvelopeTypeText(param, valueType)) {
        valueFacts[param.name] = std::move(valueType);
      }
    }
    for (const Expr &stmt : parent.statements) {
      if (!sourceComesBefore(stmt, nestedDef)) {
        continue;
      }
      if (semantics::isCompileTimeTypeBinding(stmt)) {
        std::string typeText;
        if (resolveTypeLocalInitializer(stmt, valueFacts, typeFacts, typeText)) {
          typeFacts[stmt.name] = std::move(typeText);
        }
        continue;
      }
      std::string valueType;
      if (bindingEnvelopeTypeText(stmt, valueType)) {
        valueFacts[stmt.name] = std::move(valueType);
      }
    }
    for (Expr &field : nestedDef.statements) {
      applyTypeLocalFactsToBindingEnvelope(field, typeFacts);
    }
  }
}

void eraseCompileTimeTypeBindings(Program &program) {
  applyEnclosingTypeLocalsToNestedStructs(program);
  for (Definition &def : program.definitions) {
    eraseCompileTimeTypeBindingsFromExprs(def.parameters);
    std::unordered_map<std::string, std::string> parameterValueFacts;
    for (const Expr &param : def.parameters) {
      std::string valueType;
      if (bindingEnvelopeTypeText(param, valueType)) {
        parameterValueFacts[param.name] = std::move(valueType);
      }
    }
    eraseCompileTimeTypeBindingsFromExprs(def.statements,
                                          std::move(parameterValueFacts));
    if (def.returnExpr.has_value()) {
      eraseCompileTimeTypeBindingsFromExpr(*def.returnExpr);
    }
  }
  for (Execution &exec : program.executions) {
    eraseCompileTimeTypeBindingsFromExprs(exec.arguments);
    eraseCompileTimeTypeBindingsFromExprs(exec.bodyArguments);
  }
}

std::string joinCompileTimeIfTemplateArgs(const std::vector<std::string> &args) {
  std::ostringstream out;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << args[i];
  }
  return out.str();
}

bool serializeCompileTimeIfPredicateExpr(const Expr &expr, std::string &out) {
  out.clear();
  switch (expr.kind) {
  case Expr::Kind::Name:
    out = expr.name;
    return !out.empty();
  case Expr::Kind::BoolLiteral:
    out = expr.boolValue ? "true" : "false";
    return true;
  case Expr::Kind::Literal:
    out = std::to_string(static_cast<std::int64_t>(expr.literalValue));
    if (expr.isUnsigned) {
      out += expr.intWidth == 64 ? "u64" : "u32";
    } else {
      out += expr.intWidth == 64 ? "i64" : "i32";
    }
    return true;
  case Expr::Kind::StringLiteral:
    out = expr.stringValue;
    return !out.empty();
  case Expr::Kind::FloatLiteral:
    out = expr.floatValue + (expr.floatWidth == 64 ? "f64" : "f32");
    return true;
  case Expr::Kind::Call:
    break;
  }

  if (expr.isMethodCall || expr.isFieldAccess || expr.isBinding ||
      expr.isBraceConstructor || !expr.transforms.empty() ||
      expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return false;
  }
  std::ostringstream printed;
  printed << (expr.sourceName.empty() ? expr.name : expr.sourceName);
  if (!expr.templateArgs.empty()) {
    printed << "<" << joinCompileTimeIfTemplateArgs(expr.templateArgs) << ">";
  }
  printed << "(";
  for (std::size_t i = 0; i < expr.args.size(); ++i) {
    if (i != 0) {
      printed << ", ";
    }
    std::string argText;
    if (!serializeCompileTimeIfPredicateExpr(expr.args[i], argText)) {
      return false;
    }
    printed << argText;
  }
  printed << ")";
  out = printed.str();
  return !expr.name.empty();
}

bool isTransformNamed(const std::vector<Transform> &transforms,
                      std::string_view name) {
  for (const Transform &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

std::string compileTimeIfBindingTypeText(
    const semantics::BindingInfo &binding) {
  if (!binding.typeTemplateArg.empty()) {
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  }
  return binding.typeName;
}

std::string compileTimeIfReturnTypeText(const Definition &definition) {
  for (const Transform &transform : definition.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      return transform.templateArgs.front();
    }
  }
  return {};
}

void addCompileTimeIfImportAlias(
    std::unordered_map<std::string, std::string> &aliases,
    const std::string &path) {
  if (path.empty()) {
    return;
  }
  const std::size_t slash = path.find_last_of('/');
  const std::string alias =
      slash == std::string::npos ? path : path.substr(slash + 1);
  if (!alias.empty()) {
    aliases.try_emplace(alias, path);
  }
}

semantics::RequirementPredicateDefinitionContext
makeCompileTimeIfRequirementContext(
    const Program &program,
    const Definition &definition,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_set<std::string> &sumNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  semantics::RequirementPredicateDefinitionContext context;
  context.definitionPath = definition.fullPath;
  context.namespacePrefix = definition.namespacePrefix;
  context.templateArgs = definition.templateArgs;
  context.structNames = structNames;
  context.sumNames = sumNames;
  context.importAliases = importAliases;

  for (const Expr &param : definition.parameters) {
    semantics::BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string ignoredError;
    if (semantics::parseBindingInfo(param,
                                    definition.namespacePrefix,
                                    structNames,
                                    importAliases,
                                    binding,
                                    restrictType,
                                    ignoredError)) {
      context.params.push_back(
          semantics::ParameterInfo{param.name, binding, nullptr});
    }
  }

  for (const Definition &candidate : program.definitions) {
    semantics::RequirementPredicateDefinitionContext::CallableFact callable;
    callable.fullPath = candidate.fullPath;
    callable.namespacePrefix = candidate.namespacePrefix;
    callable.templateArgs = candidate.templateArgs;
    callable.returnType = compileTimeIfReturnTypeText(candidate);
    callable.isPrivate = isTransformNamed(candidate.transforms, "private");
    for (const Transform &transform : candidate.transforms) {
      if (transform.name == "effects") {
        callable.effectNames.insert(callable.effectNames.end(),
                                    transform.arguments.begin(),
                                    transform.arguments.end());
      }
    }
    callable.hasReturnExpr = candidate.returnExpr.has_value();
    if (candidate.returnExpr.has_value() &&
        candidate.returnExpr->kind == Expr::Kind::BoolLiteral) {
      callable.returnExprIsBoolLiteral = true;
      callable.returnBoolValue = candidate.returnExpr->boolValue;
    }
    bool paramsOk = true;
    for (const Expr &param : candidate.parameters) {
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string ignoredError;
      if (!semantics::parseBindingInfo(param,
                                       candidate.namespacePrefix,
                                       structNames,
                                       importAliases,
                                       binding,
                                       restrictType,
                                       ignoredError)) {
        paramsOk = false;
        break;
      }
      callable.parameterTypes.push_back(compileTimeIfBindingTypeText(binding));
    }
    if (!callable.returnType.empty() && paramsOk) {
      context.callables.push_back(std::move(callable));
    }

    if (structNames.count(candidate.fullPath) == 0) {
      continue;
    }
    for (const Expr &stmt : candidate.statements) {
      if (!stmt.isBinding || isTransformNamed(stmt.transforms, "static") ||
          semantics::isCompileTimeTypeBinding(stmt)) {
        continue;
      }
      semantics::BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string ignoredError;
      if (!semantics::parseBindingInfo(stmt,
                                       candidate.namespacePrefix,
                                       structNames,
                                       importAliases,
                                       binding,
                                       restrictType,
                                       ignoredError)) {
        continue;
      }
      semantics::RequirementPredicateDefinitionContext::StructFieldFact field;
      field.structPath = candidate.fullPath;
      field.fieldName = stmt.name;
      field.typeText = compileTimeIfBindingTypeText(binding);
      field.isPrivate = isTransformNamed(stmt.transforms, "private");
      context.structFields.push_back(std::move(field));
    }
  }

  return context;
}

bool isCompileTimeIfEnvelope(const Expr &expr, std::string_view expectedName) {
  return expr.kind == Expr::Kind::Call && expr.name == expectedName &&
         expr.hasBodyArguments;
}

enum class CompileTimeIfDecision {
  SelectedThen,
  SelectedElse,
  Deferred,
};

std::string compileTimeIfDecisionText(CompileTimeIfDecision decision) {
  switch (decision) {
  case CompileTimeIfDecision::SelectedThen:
    return "then";
  case CompileTimeIfDecision::SelectedElse:
    return "else";
  case CompileTimeIfDecision::Deferred:
    break;
  }
  return "deferred";
}

bool evaluateCompileTimeIfDecision(
    const Expr &stmt,
    const semantics::RequirementPredicateDefinitionContext &context,
    const std::string &definitionPath,
    bool allowDeferred,
    CompileTimeIfDecision &decision,
    std::string &error) {
  if (stmt.args.size() != 3 ||
      !isCompileTimeIfEnvelope(stmt.args[1], "then") ||
      !isCompileTimeIfEnvelope(stmt.args[2], "else")) {
    error = "ct_if requires condition, then, else on " + definitionPath;
    return false;
  }
  std::string conditionText;
  if (!serializeCompileTimeIfPredicateExpr(stmt.args[0], conditionText)) {
    error = "ct_if condition must be a compile-time predicate call on " +
            definitionPath;
    return false;
  }
  semantics::RequirementPredicateFactDraft fact =
      semantics::buildRequirementPredicateFactDraft(conditionText,
                                                    stmt.sourceLine,
                                                    stmt.sourceColumn,
                                                    context);
  if (fact.evaluationOutcome != "satisfied" &&
      fact.evaluationOutcome != "unsatisfied") {
    const bool templatedUnknownTypeFact =
        !context.templateArgs.empty() &&
        fact.evaluationDiagnostic.find("unknown type fact") !=
            std::string::npos;
    if (allowDeferred &&
        (fact.evaluationDiagnostic.find("deferred") != std::string::npos ||
         templatedUnknownTypeFact)) {
      decision = CompileTimeIfDecision::Deferred;
      return true;
    }
    error = "invalid ct_if condition on " + definitionPath + ": " +
            fact.evaluationDiagnostic;
    return false;
  }
  decision = fact.evaluationOutcome == "satisfied"
                 ? CompileTimeIfDecision::SelectedThen
                 : CompileTimeIfDecision::SelectedElse;
  return true;
}

std::string rewriteCompileTimeIfBranchTypeText(
    const std::string &typeText,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  auto exactIt = branchTypes.find(typeText);
  if (exactIt != branchTypes.end()) {
    return exactIt->second;
  }
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(typeText, base, argText) ||
      base.empty()) {
    return typeText;
  }
  std::vector<std::string> args;
  if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
    return typeText;
  }
  base = rewriteCompileTimeIfBranchTypeText(base, branchTypes);
  for (std::string &arg : args) {
    arg = rewriteCompileTimeIfBranchTypeText(arg, branchTypes);
  }
  std::ostringstream rebuilt;
  rebuilt << base << "<";
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      rebuilt << ", ";
    }
    rebuilt << args[i];
  }
  rebuilt << ">";
  return rebuilt.str();
}

void rewriteCompileTimeIfBranchTemplateArgs(
    std::vector<std::string> &templateArgs,
    std::vector<TemplateArgument> &templateArgDetails,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  for (std::string &arg : templateArgs) {
    arg = rewriteCompileTimeIfBranchTypeText(arg, branchTypes);
  }
  for (TemplateArgument &detail : templateArgDetails) {
    if (detail.kind == TemplateArgumentKind::Type) {
      detail.text = rewriteCompileTimeIfBranchTypeText(detail.text,
                                                       branchTypes);
    }
  }
}

void rewriteCompileTimeIfBranchTypeReferences(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &branchTypes);

void rewriteCompileTimeIfBranchTransformReferences(
    Transform &transform,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  transform.name =
      rewriteCompileTimeIfBranchTypeText(transform.name, branchTypes);
  rewriteCompileTimeIfBranchTemplateArgs(transform.templateArgs,
                                         transform.templateArgDetails,
                                         branchTypes);
}

void rewriteCompileTimeIfBranchTypeReferences(
    Expr &expr,
    const std::unordered_map<std::string, std::string> &branchTypes) {
  expr.name = rewriteCompileTimeIfBranchTypeText(expr.name, branchTypes);
  rewriteCompileTimeIfBranchTemplateArgs(expr.templateArgs,
                                         expr.templateArgDetails,
                                         branchTypes);
  for (Transform &transform : expr.transforms) {
    rewriteCompileTimeIfBranchTransformReferences(transform, branchTypes);
  }
  for (Expr &arg : expr.args) {
    rewriteCompileTimeIfBranchTypeReferences(arg, branchTypes);
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    rewriteCompileTimeIfBranchTypeReferences(bodyArg, branchTypes);
  }
}

bool isCompileTimeIfBranchGeneratedStructExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !expr.isBinding ||
      expr.name.empty() || expr.args.size() != 1) {
    return false;
  }
  if (!std::any_of(expr.transforms.begin(),
                   expr.transforms.end(),
                   [](const Transform &transform) {
                     return transform.name == "struct";
                   })) {
    return false;
  }
  const Expr &initializer = expr.args.front();
  return initializer.kind == Expr::Kind::Call &&
         initializer.name == "struct" &&
         initializer.isBraceConstructor;
}

void normalizeCompileTimeIfGeneratedStructField(Expr &field) {
  if (field.isBinding || field.transforms.empty()) {
    return;
  }
  field.isBinding = true;
  if (field.args.size() != 1) {
    return;
  }
  Expr &initializer = field.args.front();
  if (initializer.kind != Expr::Kind::Call || initializer.name != "block" ||
      !initializer.hasBodyArguments || initializer.bodyArguments.size() != 1) {
    return;
  }
  field.args.front() = std::move(initializer.bodyArguments.front());
  field.argNames.assign(1, std::nullopt);
}

std::vector<Expr> takeCompileTimeIfGeneratedStructFields(Expr &initializer) {
  std::vector<Expr> fields = std::move(initializer.args);
  for (std::size_t i = 0; i < fields.size() && i < initializer.argNames.size();
       ++i) {
    if (!initializer.argNames[i].has_value() || !fields[i].transforms.empty()) {
      continue;
    }
    Transform typeTransform;
    typeTransform.name = *initializer.argNames[i];
    typeTransform.sourceLine = fields[i].sourceLine;
    typeTransform.sourceColumn = fields[i].sourceColumn;
    fields[i].transforms.push_back(std::move(typeTransform));
  }
  for (Expr &field : fields) {
    normalizeCompileTimeIfGeneratedStructField(field);
  }
  return fields;
}

std::string makeCompileTimeIfGeneratedStructName(
    const Expr &expr,
    CompileTimeIfDecision decision) {
  return expr.name + "__ct_if_" + compileTimeIfDecisionText(decision) +
         "_" + std::to_string(expr.sourceLine) +
         "_" + std::to_string(expr.sourceColumn);
}

bool materializeCompileTimeIfGeneratedStructs(
    std::vector<Expr> &selected,
    const std::string &definitionPath,
    CompileTimeIfDecision decision,
    std::unordered_set<std::string> &structNames,
    std::vector<Definition> &generatedDefinitions,
    std::string &error) {
  std::unordered_map<std::string, std::string> branchTypes;
  std::vector<Expr> retained;
  retained.reserve(selected.size());
  const std::size_t generatedStart = generatedDefinitions.size();

  for (Expr &stmt : selected) {
    if (!isCompileTimeIfBranchGeneratedStructExpr(stmt)) {
      retained.push_back(std::move(stmt));
      continue;
    }
    if (branchTypes.count(stmt.name) > 0) {
      error = "duplicate branch-local generated struct: " + stmt.name +
              " on " + definitionPath;
      return false;
    }
    Definition generated;
    generated.name = makeCompileTimeIfGeneratedStructName(stmt, decision);
    generated.namespacePrefix = definitionPath;
    generated.fullPath = definitionPath + "/" + generated.name;
    generated.sourceLine = stmt.sourceLine;
    generated.sourceColumn = stmt.sourceColumn;
    generated.transforms = stmt.transforms;
    generated.isNested = true;
    generated.statements =
        takeCompileTimeIfGeneratedStructFields(stmt.args.front());
    if (structNames.count(generated.fullPath) > 0) {
      error = "duplicate branch-local generated struct path: " +
              generated.fullPath;
      return false;
    }
    branchTypes.emplace(stmt.name, generated.fullPath);
    structNames.insert(generated.fullPath);
    generatedDefinitions.push_back(std::move(generated));
  }

  if (branchTypes.empty()) {
    selected = std::move(retained);
    return true;
  }

  for (Expr &stmt : retained) {
    if (stmt.isBinding && branchTypes.count(stmt.name) > 0) {
      error = "branch-local generated struct name shadows local binding: " +
              stmt.name;
      return false;
    }
    rewriteCompileTimeIfBranchTypeReferences(stmt, branchTypes);
  }
  for (std::size_t i = generatedStart; i < generatedDefinitions.size(); ++i) {
    for (Expr &field : generatedDefinitions[i].statements) {
      rewriteCompileTimeIfBranchTypeReferences(field, branchTypes);
    }
  }
  selected = std::move(retained);
  return true;
}

bool rewriteCompileTimeIfStatements(std::vector<Expr> &statements,
                                    semantics::RequirementPredicateDefinitionContext context,
                                    const std::string &definitionPath,
                                    std::unordered_set<std::string> &structNames,
                                    std::vector<Definition> &generatedDefinitions,
                                    bool allowDeferred,
                                    std::string &error);

bool rewriteCompileTimeIfExpression(
    Expr &expr,
    const semantics::RequirementPredicateDefinitionContext &context,
    const std::string &definitionPath,
    bool allowDeferred,
    std::string &error);

bool rewriteCompileTimeIfStatement(Expr &stmt,
                                   std::vector<Expr> &out,
                                   semantics::RequirementPredicateDefinitionContext context,
                                   const std::string &definitionPath,
                                   std::unordered_set<std::string> &structNames,
                                   std::vector<Definition> &generatedDefinitions,
                                   bool allowDeferred,
                                   std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.name != "ct_if") {
    if (!rewriteCompileTimeIfExpression(
            stmt, context, definitionPath, allowDeferred, error)) {
      return false;
    }
    out.push_back(std::move(stmt));
    return true;
  }
  CompileTimeIfDecision decision = CompileTimeIfDecision::Deferred;
  if (!evaluateCompileTimeIfDecision(
          stmt, context, definitionPath, allowDeferred, decision, error)) {
    return false;
  }
  if (decision == CompileTimeIfDecision::Deferred) {
    out.push_back(std::move(stmt));
    return true;
  }
  std::vector<Expr> selected =
      decision == CompileTimeIfDecision::SelectedThen
          ? std::move(stmt.args[1].bodyArguments)
          : std::move(stmt.args[2].bodyArguments);
  if (!materializeCompileTimeIfGeneratedStructs(selected,
                                                definitionPath,
                                                decision,
                                                structNames,
                                                generatedDefinitions,
                                                error)) {
    return false;
  }
  context.structNames = structNames;
  if (!rewriteCompileTimeIfStatements(
          selected,
          context,
          definitionPath,
          structNames,
          generatedDefinitions,
          allowDeferred,
          error)) {
    return false;
  }
  out.insert(out.end(),
             std::make_move_iterator(selected.begin()),
             std::make_move_iterator(selected.end()));
  return true;
}

bool rewriteCompileTimeIfExpression(
    Expr &expr,
    const semantics::RequirementPredicateDefinitionContext &context,
    const std::string &definitionPath,
    bool allowDeferred,
    std::string &error) {
  if (expr.kind == Expr::Kind::Call && expr.name == "ct_if") {
    CompileTimeIfDecision decision = CompileTimeIfDecision::Deferred;
    if (!evaluateCompileTimeIfDecision(
            expr, context, definitionPath, allowDeferred, decision, error)) {
      return false;
    }
    if (decision == CompileTimeIfDecision::Deferred) {
      return true;
    }
    std::vector<Expr> &selected =
        decision == CompileTimeIfDecision::SelectedThen
            ? expr.args[1].bodyArguments
            : expr.args[2].bodyArguments;
    if (selected.size() != 1 || selected.front().isBinding) {
      error = "ct_if expression requires exactly one selected branch value on " +
              definitionPath;
      return false;
    }
    Expr selectedExpr = std::move(selected.front());
    if (!rewriteCompileTimeIfExpression(
            selectedExpr, context, definitionPath, allowDeferred, error)) {
      return false;
    }
    expr = std::move(selectedExpr);
    return true;
  }

  for (Expr &arg : expr.args) {
    if (!rewriteCompileTimeIfExpression(
            arg, context, definitionPath, allowDeferred, error)) {
      return false;
    }
  }
  for (Expr &bodyArg : expr.bodyArguments) {
    if (!rewriteCompileTimeIfExpression(
            bodyArg, context, definitionPath, allowDeferred, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteCompileTimeIfStatements(std::vector<Expr> &statements,
                                    semantics::RequirementPredicateDefinitionContext context,
                                    const std::string &definitionPath,
                                    std::unordered_set<std::string> &structNames,
                                    std::vector<Definition> &generatedDefinitions,
                                    bool allowDeferred,
                                    std::string &error) {
  std::vector<Expr> rewritten;
  rewritten.reserve(statements.size());
  for (Expr &stmt : statements) {
    if (!rewriteCompileTimeIfStatement(
            stmt,
            rewritten,
            context,
            definitionPath,
            structNames,
            generatedDefinitions,
            allowDeferred,
            error)) {
      return false;
    }
  }
  statements = std::move(rewritten);
  return true;
}

bool rewriteCompileTimeIfBranches(Program &program,
                                  bool allowDeferred,
                                  std::string &error) {
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> sumNames;
  std::unordered_map<std::string, std::string> importAliases;
  for (const std::string &path : program.imports) {
    addCompileTimeIfImportAlias(importAliases, path);
  }
  for (const std::string &path : program.sourceImports) {
    addCompileTimeIfImportAlias(importAliases, path);
  }
  for (const Definition &definition : program.definitions) {
    if (isTransformNamed(definition.transforms, "sum")) {
      sumNames.insert(definition.fullPath);
    } else if (definition.returnExpr.has_value()) {
      continue;
    } else {
      structNames.insert(definition.fullPath);
    }
  }

  std::vector<Definition> generatedDefinitions;
  for (Definition &definition : program.definitions) {
    semantics::RequirementPredicateDefinitionContext context =
        makeCompileTimeIfRequirementContext(program,
                                            definition,
                                            structNames,
                                            sumNames,
                                            importAliases);
    if (!rewriteCompileTimeIfStatements(
            definition.statements,
            context,
            definition.fullPath,
            structNames,
            generatedDefinitions,
            allowDeferred,
            error)) {
      return false;
    }
    if (definition.returnExpr.has_value() &&
        !rewriteCompileTimeIfExpression(*definition.returnExpr,
                                        context,
                                        definition.fullPath,
                                        allowDeferred,
                                        error)) {
      return false;
    }
  }
  program.definitions.insert(program.definitions.end(),
                             std::make_move_iterator(
                                 generatedDefinitions.begin()),
                             std::make_move_iterator(
                                 generatedDefinitions.end()));
  return true;
}

bool runSemanticValidationManifestAstPass(
    const semantics::SemanticValidationPassManifestEntry &pass,
    Program &program,
    const std::string &entryPath,
    const std::vector<std::string> &semanticTransforms,
    std::string &error) {
  if (pass.name == "semantic-transform-rules") {
    return semantics::applySemanticTransforms(program, semanticTransforms, error);
  }
  if (pass.name == "experimental-gfx-constructors") {
    return semantics::rewriteExperimentalGfxConstructors(program, error);
  }
  if (pass.name == "reflection-generated-helpers") {
    return semantics::rewriteReflectionGeneratedHelpers(program, error);
  }
  if (pass.name == "builtin-soa-conversion-methods") {
    return rewriteBuiltinSoaConversionMethods(program, error);
  }
  if (pass.name == "builtin-soa-to-aos-calls") {
    return rewriteBuiltinSoaToAosCalls(program, error);
  }
  if (pass.name == "builtin-soa-helper-return-metadata") {
    return validateBuiltinSoaHelperReturnMetadataRequirements(program, error);
  }
  if (pass.name == "builtin-soa-access-calls") {
    return rewriteBuiltinSoaAccessCalls(program, error);
  }
  if (pass.name == "builtin-soa-count-calls") {
    return rewriteBuiltinSoaCountCalls(program, error);
  }
  if (pass.name == "builtin-soa-mutator-calls") {
    return rewriteBuiltinSoaMutatorCalls(program, error);
  }
  if (pass.name == "experimental-soa-inline-borrow-methods") {
    return rewriteExperimentalSoaInlineBorrowMethods(program, error);
  }
  if (pass.name == "experimental-soa-same-path-helper-methods") {
    return rewriteExperimentalSoaSamePathHelperMethods(program, error);
  }
  if (pass.name == "experimental-soa-to-aos-methods") {
    return rewriteExperimentalSoaToAosMethods(program, error);
  }
  if (pass.name == "experimental-soa-field-view-indexes") {
    return rewriteExperimentalSoaFieldViewIndexes(program, error);
  }
  if (pass.name == "experimental-soa-field-view-helpers") {
    return rewriteExperimentalSoaFieldViewHelpers(program, error);
  }
  if (pass.name == "experimental-soa-field-view-carrier-indexes") {
    return rewriteExperimentalSoaFieldViewCarrierIndexes(program, error);
  }
  if (pass.name == "experimental-soa-field-view-assign-targets") {
    return rewriteExperimentalSoaFieldViewAssignTargets(program, error);
  }
  if (pass.name == "borrowed-experimental-map-methods") {
    return rewriteBorrowedExperimentalKeyValueMethods(program, error);
  }
  if (pass.name == "experimental-map-value-methods") {
    return rewriteExperimentalKeyValueValueMethods(program, error);
  }
  if (pass.name == "builtin-map-insert-methods") {
    return rewriteBuiltinKeyValueInsertMethods(program, error);
  }
  if (pass.name == "template-monomorphization") {
    try {
      if (!semantics::monomorphizeTemplates(program, entryPath, error)) {
        return false;
      }
      return semantics::rewriteReflectionGeneratedHelpersForPackSpecializations(
          program, error);
    } catch (const std::exception &ex) {
      error = std::string("template monomorphization exception: ") + ex.what();
      return false;
    }
  }
  if (pass.name == "reflection-metadata-queries") {
    return semantics::rewriteReflectionMetadataQueries(program, error);
  }
  if (pass.name == "convert-constructors") {
    return semantics::rewriteConvertConstructors(program, error);
  }
  if (pass.name == "compile-time-branch-pruning") {
    return rewriteCompileTimeIfBranches(program, true, error);
  }
  if (pass.name == "compile-time-specialized-branch-pruning") {
    return rewriteCompileTimeIfBranches(program, false, error);
  }

  error = "semantic validation manifest has no pre-validator runner for pass: " +
          std::string(pass.name);
  return false;
}

bool runSemanticValidationManifestAstPasses(
    Program &program,
    const std::string &entryPath,
    const std::vector<std::string> &semanticTransforms,
    std::string &error) {
  for (const auto &pass : semantics::semanticValidationPassManifest()) {
    if (pass.name == "validator-passes") {
      return true;
    }
    if (pass.kind == semantics::SemanticValidationPassKind::Validation ||
        pass.kind == semantics::SemanticValidationPassKind::Publication) {
      error = "semantic validation manifest reached unexpected boundary before validator: " +
              std::string(pass.name);
      return false;
    }
    if (!runSemanticValidationManifestAstPass(
            pass, program, entryPath, semanticTransforms, error)) {
      return false;
    }
  }
  error = "semantic validation manifest is missing validator-passes";
  return false;
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
                           const SemanticValidationBenchmarkObserver *benchmarkObserver) {
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
  if (!runSemanticValidationManifestAstPasses(
          program, entryPath, semanticTransforms, error)) {
    return false;
  }
  auto validatorLifetimeBenchmark = semantics::SemanticValidatorLifetimeBenchmark::fromEnvironment();
  validatorLifetimeBenchmark.captureBefore();

  semantics::SemanticsValidator::ValidationCounters validationCounters;
  semantics::SemanticValidationBenchmarkPhase validationBenchmark(benchmarkRuntime);
  validationBenchmark.captureBefore();
  {
    semantics::SemanticsValidator validator(
        program,
        entryPath,
        error,
        defaultEffects,
        entryDefaultEffects,
        diagnosticInfo,
        collectDiagnostics,
        benchmarkRuntime.definitionValidationWorkerCount,
        benchmarkRuntime.hasPhaseCounters(),
        benchmarkRuntime.disableMethodTargetMemoization,
        benchmarkRuntime.graphLocalAutoLegacyKeyShadow,
        benchmarkRuntime.graphLocalAutoLegacySideChannelShadow,
        benchmarkRuntime.disableGraphLocalAutoDependencyScratchPmr);
    try {
      if (!validator.run()) {
        return false;
      }
    } catch (const std::exception &ex) {
      error = std::string("semantic validator exception: ") + ex.what();
      return false;
    }
    validationCounters = validator.validationCounters();
    eraseCompileTimeTypeBindings(program);
    if (!rewriteOmittedStructInitializers(program, error)) {
      return false;
    }
    semantics::assignSemanticNodeIds(program);
    validator.invalidatePilotRoutingSemanticCollectors();
    semantics::SemanticPublicationSurface publicationSurface;
    if (semanticProgramOut != nullptr) {
      publicationSurface =
          validator.takeSemanticPublicationSurfaceForSemanticProduct(
              semanticProductBuildConfig);
    }
    semantics::maybeRelieveSemanticAllocatorPressure();
    validationBenchmark.captureAfter();
    validatorLifetimeBenchmark.captureAfterRun();
    if (semanticProgramOut != nullptr) {
      semantics::publishSemanticProgramAfterValidation(program,
                                                       entryPath,
                                                       std::move(publicationSurface),
                                                       semanticProductBuildConfig,
                                                       benchmarkRuntime,
                                                       *semanticProgramOut);
    }
  }

  validatorLifetimeBenchmark.captureAfterDestroyAndReport();
  validationBenchmark.publish(validationCounters.callsVisited, validationCounters.peakLocalMapSize);
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
                         const SemanticProductBuildConfig *semanticProductBuildConfig) const {
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
                               nullptr);
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
    const SemanticValidationBenchmarkObserver &benchmarkObserver) {
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
                               &benchmarkObserver);
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
