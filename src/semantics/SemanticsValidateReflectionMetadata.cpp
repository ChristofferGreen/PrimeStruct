#include "SemanticsValidateReflectionMetadata.h"

#include "SemanticsHelpers.h"
#include "SemanticsValidateReflectionMetadataInternal.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

bool rewriteReflectionMetadataQueries(Program &program, std::string &error) {
  ReflectionMetadataRewriteContext rewriteContext = buildReflectionMetadataRewriteContext(program);
  const auto &structNames = rewriteContext.structNames;
  const auto &reflectedStructNames = rewriteContext.reflectedStructNames;
  const auto &structFieldMetadata = rewriteContext.structFieldMetadata;
  const auto &definitionByPath = rewriteContext.definitionByPath;
  const auto &importAliases = rewriteContext.importAliases;

  auto trim = [](const std::string &text) -> std::string {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
  };
  auto escapeStringLiteral = [](const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    escaped.push_back('"');
    for (char c : text) {
      if (c == '"' || c == '\\') {
        escaped.push_back('\\');
      }
      escaped.push_back(c);
    }
    escaped += "\"utf8";
    return escaped;
  };

  auto typeKindForCanonical = [](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return std::string("struct");
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(canonicalType, base, arg)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "map" ||
          normalizedBase == "soa_vector" || normalizedBase == "Result") {
        return normalizedBase;
      }
      if (normalizedBase == "Pointer") {
        return std::string("pointer");
      }
      if (normalizedBase == "Reference") {
        return std::string("reference");
      }
      if (normalizedBase == "Buffer") {
        return std::string("buffer");
      }
      if (normalizedBase == "uninitialized") {
        return std::string("uninitialized");
      }
      return std::string("template");
    }
    const std::string normalized = normalizeBindingTypeName(canonicalType);
    if (normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "integer") {
      return std::string("integer");
    }
    if (normalized == "f32" || normalized == "f64") {
      return std::string("float");
    }
    if (normalized == "decimal") {
      return std::string("decimal");
    }
    if (normalized == "complex") {
      return std::string("complex");
    }
    if (normalized == "bool") {
      return std::string("bool");
    }
    if (normalized == "string") {
      return std::string("string");
    }
    return std::string("type");
  };
  auto parseFieldIndex = [&](const Expr &indexExpr, const std::string &queryName, size_t &indexOut) {
    if (indexExpr.kind != Expr::Kind::Literal) {
      error = "meta." + queryName + " requires constant integer index argument";
      return false;
    }
    if (indexExpr.isUnsigned) {
      indexOut = static_cast<size_t>(indexExpr.literalValue);
      return true;
    }
    if (indexExpr.intWidth == 64) {
      const int64_t value = static_cast<int64_t>(indexExpr.literalValue);
      if (value < 0) {
        error = "meta." + queryName + " requires non-negative field index";
        return false;
      }
      indexOut = static_cast<size_t>(value);
      return true;
    }
    const int32_t value = static_cast<int32_t>(static_cast<uint32_t>(indexExpr.literalValue));
    if (value < 0) {
      error = "meta." + queryName + " requires non-negative field index";
      return false;
    }
    indexOut = static_cast<size_t>(value);
    return true;
  };
  auto resolveDefinitionTarget = [&](const std::string &canonicalType,
                                     const std::string &structPath,
                                     const std::string &namespacePrefix) -> const Definition * {
    auto findByPath = [&](const std::string &path) -> const Definition * {
      auto it = definitionByPath.find(path);
      return it == definitionByPath.end() ? nullptr : it->second;
    };
    if (!structPath.empty()) {
      return findByPath(structPath);
    }
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return nullptr;
    }
    if (canonicalType[0] == '/') {
      return findByPath(canonicalType);
    }
    if (!namespacePrefix.empty()) {
      std::string prefix = namespacePrefix;
      while (!prefix.empty()) {
        const std::string candidate = prefix + "/" + canonicalType;
        if (const Definition *resolved = findByPath(candidate)) {
          return resolved;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
    }
    auto importIt = importAliases.find(canonicalType);
    if (importIt != importAliases.end()) {
      if (const Definition *resolved = findByPath(importIt->second)) {
        return resolved;
      }
    }
    return findByPath("/" + canonicalType);
  };
  auto parseNamedQueryName = [&](const Expr &argExpr,
                                 const std::string &queryName,
                                 const std::string &nameLabel,
                                 std::string &nameOut) {
    if (argExpr.kind == Expr::Kind::Name) {
      nameOut = argExpr.name;
      return true;
    }
    if (argExpr.kind != Expr::Kind::StringLiteral) {
      error = "meta." + queryName + " requires constant string or identifier argument";
      return false;
    }
    ParsedStringLiteral parsed;
    std::string parseError;
    if (!parseStringLiteralToken(argExpr.stringValue, parsed, parseError)) {
      error = "meta." + queryName + " requires utf8/ascii string literal argument";
      return false;
    }
    nameOut = parsed.decoded;
    if (nameOut.empty()) {
      error = "meta." + queryName + " requires non-empty " + nameLabel + " name";
      return false;
    }
    return true;
  };
  auto typePathForCanonical = [&](const std::string &canonicalType, const std::string &structPath) {
    if (!structPath.empty()) {
      return structPath;
    }
    if (!canonicalType.empty() && canonicalType.front() == '/') {
      return canonicalType;
    }
    return std::string("/") + canonicalType;
  };
  auto isNumericTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = normalizeBindingTypeName(canonicalType);
    return normalized == "i32" || normalized == "i64" || normalized == "u64" || normalized == "f32" ||
           normalized == "f64" || normalized == "integer" || normalized == "decimal" ||
           normalized == "complex";
  };
  auto isComparableBuiltinTraitType = [&](const std::string &canonicalType) {
    if (canonicalType.empty() || canonicalType.find('<') != std::string::npos) {
      return false;
    }
    const std::string normalized = normalizeBindingTypeName(canonicalType);
    if (normalized == "complex") {
      return false;
    }
    if (isNumericTraitType(canonicalType)) {
      return true;
    }
    return normalized == "bool" || normalized == "string";
  };
  auto isIndexableBuiltinTraitType = [&](const std::string &canonicalType, const std::string &elemCanonicalType) {
    const std::string normalized = normalizeBindingTypeName(canonicalType);
    if (normalized == "string") {
      return elemCanonicalType == "i32";
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(canonicalType, base, arg)) {
      return false;
    }
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (normalizedBase != "array" && normalizedBase != "vector") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
      return false;
    }
    return args.front() == elemCanonicalType;
  };
  auto resolveBindingCanonicalType = [&](const Definition &ownerDef,
                                         const Expr &bindingExpr,
                                         std::string &canonicalOut) -> bool {
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(bindingExpr, ownerDef.namespacePrefix, structNames, importAliases, binding, restrictType,
                          parseError)) {
      return false;
    }
    std::string typeName = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeName += "<" + binding.typeTemplateArg + ">";
    }
    if (typeName.empty()) {
      return false;
    }
    std::string ignoredStructPath;
    return rewriteContext.canonicalizeTypeName(typeName, ownerDef.namespacePrefix, canonicalOut, ignoredStructPath,
                                               error);
  };
  auto resolveDefinitionReturnCanonicalType = [&](const Definition &definition, std::string &canonicalOut) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return") {
        continue;
      }
      if (transform.templateArgs.size() != 1 || transform.templateArgs.front() == "auto") {
        return false;
      }
      std::string ignoredStructPath;
      return rewriteContext.canonicalizeTypeName(transform.templateArgs.front(), definition.namespacePrefix,
                                                 canonicalOut, ignoredStructPath, error);
    }
    return false;
  };
  auto definitionMatchesSignature = [&](const std::string &functionPath,
                                        const std::vector<std::string> &expectedParamTypes,
                                        const std::string &expectedReturnType) -> bool {
    auto defIt = definitionByPath.find(functionPath);
    if (defIt == definitionByPath.end()) {
      return false;
    }
    const Definition &definition = *defIt->second;
    if (definition.parameters.size() != expectedParamTypes.size()) {
      return false;
    }
    for (size_t index = 0; index < expectedParamTypes.size(); ++index) {
      std::string canonicalParamType;
      if (!resolveBindingCanonicalType(definition, definition.parameters[index], canonicalParamType)) {
        return false;
      }
      if (canonicalParamType != expectedParamTypes[index]) {
        return false;
      }
    }
    std::string canonicalReturnType;
    if (!resolveDefinitionReturnCanonicalType(definition, canonicalReturnType)) {
      return false;
    }
    return canonicalReturnType == expectedReturnType;
  };
  auto hasTraitConformance = [&](const std::string &traitName,
                                 const std::string &canonicalType,
                                 const std::string &structPath,
                                 const std::optional<std::string> &elemCanonicalType) {
    if (traitName == "Additive") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/plus", params, canonicalType);
    }
    if (traitName == "Multiplicative") {
      if (isNumericTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      return definitionMatchesSignature(basePath + "/multiply", params, canonicalType);
    }
    if (traitName == "Comparable") {
      if (isComparableBuiltinTraitType(canonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> params = {canonicalType, canonicalType};
      if (!definitionMatchesSignature(basePath + "/equal", params, "bool")) {
        return false;
      }
      return definitionMatchesSignature(basePath + "/less_than", params, "bool");
    }
    if (traitName == "Indexable") {
      if (!elemCanonicalType.has_value()) {
        return false;
      }
      if (isIndexableBuiltinTraitType(canonicalType, *elemCanonicalType)) {
        return true;
      }
      const std::string basePath = typePathForCanonical(canonicalType, structPath);
      std::vector<std::string> countParams = {canonicalType};
      if (!definitionMatchesSignature(basePath + "/count", countParams, "i32")) {
        return false;
      }
      std::vector<std::string> atParams = {canonicalType, "i32"};
      return definitionMatchesSignature(basePath + "/at", atParams, *elemCanonicalType);
    }
    return false;
  };

  auto rewriteExpr = [&](auto &self, Expr &expr) -> bool {
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
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return true;
    }

    std::string queryName;
    if (expr.isMethodCall) {
      if (expr.args.empty() || expr.args.front().kind != Expr::Kind::Name || expr.args.front().name != "meta") {
        return true;
      }
      queryName = expr.name;
    } else if (expr.name.rfind("/meta/", 0) == 0) {
      queryName = expr.name.substr(6);
    } else {
      return true;
    }

    if (queryName != "type_name" && queryName != "type_kind" && queryName != "is_struct" &&
        queryName != "field_count" && queryName != "field_name" && queryName != "field_type" &&
        queryName != "field_visibility" && queryName != "has_transform" && queryName != "has_trait") {
      return true;
    }

    const bool needsTransformName = queryName == "has_transform";
    const bool needsTraitName = queryName == "has_trait";
    const bool needsFieldIndex =
        queryName == "field_name" || queryName == "field_type" || queryName == "field_visibility";
    const size_t expectedArgs =
        expr.isMethodCall ? (needsFieldIndex || needsTransformName || needsTraitName ? 2u : 1u)
                          : (needsFieldIndex || needsTransformName || needsTraitName ? 1u : 0u);
    if (expr.args.size() != expectedArgs) {
      if (needsFieldIndex) {
        error = "meta." + queryName + " requires exactly one index argument";
      } else if (needsTransformName) {
        error = "meta.has_transform requires exactly one transform-name argument";
      } else if (needsTraitName) {
        error = "meta.has_trait requires exactly one trait-name argument";
      } else {
        error = "meta." + queryName + " does not accept call arguments";
      }
      return false;
    }
    if (hasNamedArguments(expr.argNames)) {
      error = "meta." + queryName + " does not accept named arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "meta." + queryName + " does not accept block arguments";
      return false;
    }
    if (queryName == "has_trait") {
      if (expr.templateArgs.empty() || expr.templateArgs.size() > 2) {
        error = "meta.has_trait requires one or two template arguments";
        return false;
      }
    } else if (expr.templateArgs.size() != 1) {
      error = "meta." + queryName + " requires exactly one template argument";
      return false;
    }

    std::string canonicalType;
    std::string structPath;
    if (!rewriteContext.canonicalizeTypeName(expr.templateArgs.front(), expr.namespacePrefix, canonicalType,
                                             structPath, error)) {
      return false;
    }

    Expr rewritten;
    rewritten.sourceLine = expr.sourceLine;
    rewritten.sourceColumn = expr.sourceColumn;
    if (queryName == "type_name") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(canonicalType);
    } else if (queryName == "type_kind") {
      rewritten.kind = Expr::Kind::StringLiteral;
      rewritten.stringValue = escapeStringLiteral(typeKindForCanonical(canonicalType, structPath));
    } else if (queryName == "is_struct") {
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = !structPath.empty();
    } else if (queryName == "field_count") {
      if (structPath.empty()) {
        error = "meta.field_count requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta.field_count requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      size_t fieldCount = 0;
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt != structFieldMetadata.end()) {
        fieldCount = fieldMetaIt->second.size();
      }
      rewritten.kind = Expr::Kind::Literal;
      rewritten.intWidth = 32;
      rewritten.isUnsigned = false;
      rewritten.literalValue = static_cast<uint64_t>(fieldCount);
    } else if (queryName == "has_transform") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string transformName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_transform", "transform", transformName)) {
        return false;
      }
      bool hasTransform = false;
      if (const Definition *targetDef = resolveDefinitionTarget(canonicalType, structPath, expr.namespacePrefix)) {
        for (const auto &transform : targetDef->transforms) {
          if (transform.name == transformName) {
            hasTransform = true;
            break;
          }
        }
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTransform;
    } else if (queryName == "has_trait") {
      const size_t nameArg = expr.isMethodCall ? 1u : 0u;
      std::string traitName;
      if (!parseNamedQueryName(expr.args[nameArg], "has_trait", "trait", traitName)) {
        return false;
      }
      traitName = trim(traitName);
      if (traitName.empty()) {
        error = "meta.has_trait requires non-empty trait name";
        return false;
      }
      const bool isAdditive = traitName == "Additive";
      const bool isMultiplicative = traitName == "Multiplicative";
      const bool isComparable = traitName == "Comparable";
      const bool isIndexable = traitName == "Indexable";
      if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable) {
        error = "meta.has_trait does not support trait: " + traitName;
        return false;
      }
      if (isIndexable) {
        if (expr.templateArgs.size() != 2) {
          error = "meta.has_trait Indexable requires type and element template arguments";
          return false;
        }
      } else if (expr.templateArgs.size() != 1) {
        error = "meta.has_trait " + traitName + " requires exactly one type template argument";
        return false;
      }
      std::optional<std::string> elemCanonicalType;
      if (isIndexable) {
        std::string elemStructPath;
        std::string elemCanonical;
        if (!rewriteContext.canonicalizeTypeName(expr.templateArgs[1], expr.namespacePrefix, elemCanonical,
                                                 elemStructPath, error)) {
          return false;
        }
        elemCanonicalType = std::move(elemCanonical);
      }
      rewritten.kind = Expr::Kind::BoolLiteral;
      rewritten.boolValue = hasTraitConformance(traitName, canonicalType, structPath, elemCanonicalType);
    } else {
      if (structPath.empty()) {
        error = "meta." + queryName + " requires struct type argument: " + canonicalType;
        return false;
      }
      if (reflectedStructNames.count(structPath) == 0) {
        error = "meta." + queryName + " requires reflect-enabled struct type argument: " + canonicalType;
        return false;
      }
      const size_t indexArg = expr.isMethodCall ? 1u : 0u;
      size_t fieldIndex = 0;
      if (!parseFieldIndex(expr.args[indexArg], queryName, fieldIndex)) {
        return false;
      }
      auto fieldMetaIt = structFieldMetadata.find(structPath);
      if (fieldMetaIt == structFieldMetadata.end() || fieldIndex >= fieldMetaIt->second.size()) {
        error = "meta." + queryName + " field index out of range for " + structPath + ": " +
                std::to_string(fieldIndex);
        return false;
      }
      const ReflectionFieldMetadata &field = fieldMetaIt->second[fieldIndex];
      rewritten.kind = Expr::Kind::StringLiteral;
      if (queryName == "field_name") {
        rewritten.stringValue = escapeStringLiteral(field.name);
      } else if (queryName == "field_type") {
        rewritten.stringValue = escapeStringLiteral(field.typeName);
      } else {
        rewritten.stringValue = escapeStringLiteral(field.visibility);
      }
    }
    expr = std::move(rewritten);
    return true;
  };

  for (auto &def : program.definitions) {
    for (auto &param : def.parameters) {
      if (!rewriteExpr(rewriteExpr, param)) {
        return false;
      }
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

} // namespace primec::semantics
