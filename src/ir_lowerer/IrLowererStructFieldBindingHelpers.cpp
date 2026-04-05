#include "IrLowererStructFieldBindingHelpers.h"

#include <functional>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructReturnPathHelpers.h"
#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isSpecializedExperimentalCollectionTypeName(const std::string &typeName) {
  return typeName.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
         typeName.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
         typeName.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
         typeName.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

LayoutFieldBinding layoutFieldBindingFromSemanticProduct(
    const SemanticProgramStructFieldMetadata &fieldMetadata) {
  LayoutFieldBinding binding;
  if (!splitTemplateTypeName(fieldMetadata.bindingTypeText, binding.typeName, binding.typeTemplateArg)) {
    binding.typeName = fieldMetadata.bindingTypeText;
    binding.typeTemplateArg.clear();
  }
  return binding;
}

} // namespace

const Expr *getEnvelopeValueExpr(const Expr &candidate, bool allowAnyName) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return nullptr;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
    return nullptr;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return nullptr;
  }
  if (!allowAnyName && !isBlockCall(candidate)) {
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
}

bool extractExplicitLayoutFieldBinding(const Expr &expr, LayoutFieldBinding &bindingOut) {
  bindingOut = {};
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isLayoutQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.templateArgs.empty() && isSpecializedExperimentalCollectionTypeName(transform.name)) {
      bindingOut.typeName = transform.name;
    } else {
      bindingOut.typeName = normalizeCollectionBindingTypeName(transform.name);
    }
    if (!transform.templateArgs.empty()) {
      bindingOut.typeTemplateArg = joinTemplateArgsText(transform.templateArgs);
    } else {
      bindingOut.typeTemplateArg.clear();
    }
    return true;
  }
  return false;
}

bool inferPrimitiveFieldBinding(const Expr &initializer,
                                const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
                                LayoutFieldBinding &bindingOut) {
  std::function<bool(const Expr &, LayoutFieldBinding &)> infer;
  infer = [&](const Expr &expr, LayoutFieldBinding &out) -> bool {
    if (expr.kind == Expr::Kind::Literal) {
      out.typeName = expr.isUnsigned ? "u64" : (expr.intWidth == 64 ? "i64" : "i32");
      out.typeTemplateArg.clear();
      return true;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      out.typeName = "bool";
      out.typeTemplateArg.clear();
      return true;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      out.typeName = expr.floatWidth == 64 ? "f64" : "f32";
      out.typeTemplateArg.clear();
      return true;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      out.typeName = "string";
      out.typeTemplateArg.clear();
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      auto fieldIt = knownFields.find(expr.name);
      if (fieldIt != knownFields.end()) {
        out = fieldIt->second;
        return true;
      }
      return false;
    }
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isFieldAccess) {
      return false;
    }
    if (expr.hasBodyArguments && expr.args.empty()) {
      const Expr *valueExpr = getEnvelopeValueExpr(expr, false);
      return valueExpr != nullptr && infer(*valueExpr, out);
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "Buffer") &&
          expr.templateArgs.size() == 1) {
        out.typeName = collection;
        out.typeTemplateArg = expr.templateArgs.front();
        return true;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        out.typeName = "map";
        out.typeTemplateArg = joinTemplateArgsText(expr.templateArgs);
        return true;
      }
      return false;
    }
    if (getBuiltinConvertName(expr) && expr.templateArgs.size() == 1) {
      const std::string &target = expr.templateArgs.front();
      out.typeName = (target == "int") ? "i32" : target;
      out.typeTemplateArg.clear();
      return out.typeName == "i32" || out.typeName == "i64" || out.typeName == "u64" ||
             out.typeName == "f32" || out.typeName == "f64" || out.typeName == "bool" ||
             out.typeName == "string" || out.typeName == "integer" || out.typeName == "decimal" ||
             out.typeName == "complex";
    }
    return false;
  };

  return infer(initializer, bindingOut);
}

bool resolveLayoutFieldBinding(
    const Definition &def,
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut,
    std::string &errorOut) {
  if (extractExplicitLayoutFieldBinding(expr, bindingOut)) {
    return true;
  }

  const std::string fieldPath = def.fullPath + "/" + expr.name;
  if (expr.args.size() != 1) {
    errorOut = "omitted struct field envelope requires exactly one initializer: " + fieldPath;
    return false;
  }

  if (inferPrimitiveFieldBinding(expr.args.front(), knownFields, bindingOut)) {
    return true;
  }

  std::string inferredStruct = inferStructReturnPathFromExpr(
      expr.args.front(), knownFields, structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap);
  if (!inferredStruct.empty()) {
    bindingOut.typeName = std::move(inferredStruct);
    bindingOut.typeTemplateArg.clear();
    return true;
  }

  errorOut = "unresolved or ambiguous omitted struct field envelope: " + fieldPath;
  return false;
}

bool collectStructLayoutFieldBindings(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  fieldsByStructOut.clear();
  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def, semanticProductTargets)) {
      continue;
    }
    const std::vector<const SemanticProgramStructFieldMetadata *> *semanticFields = nullptr;
    if (semanticProductTargets != nullptr) {
      semanticFields = findSemanticProductStructFieldMetadata(*semanticProductTargets, def.fullPath);
    }
    std::vector<LayoutFieldBinding> fields;
    std::unordered_map<std::string, LayoutFieldBinding> knownFields;
    size_t semanticIndex = 0;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      LayoutFieldBinding binding;
      if (semanticFields != nullptr && !semanticFields->empty() && !isStaticStructBinding(stmt)) {
        if (semanticIndex >= semanticFields->size()) {
          errorOut = "internal error: mismatched struct field info for " + def.fullPath;
          return false;
        }
        binding = layoutFieldBindingFromSemanticProduct(*(*semanticFields)[semanticIndex]);
        ++semanticIndex;
      } else {
        if (!resolveLayoutFieldBinding(def,
                                       stmt,
                                       knownFields,
                                       structNames,
                                       resolveStructTypePath,
                                       resolveStructLayoutExprPath,
                                       defMap,
                                       binding,
                                       errorOut)) {
          return false;
        }
      }
      knownFields[stmt.name] = binding;
      fields.push_back(std::move(binding));
    }
    if (semanticFields != nullptr && !semanticFields->empty() && semanticIndex != semanticFields->size()) {
      errorOut = "internal error: mismatched struct field info for " + def.fullPath;
      return false;
    }
    fieldsByStructOut.emplace(def.fullPath, std::move(fields));
  }
  return true;
}

bool collectStructLayoutFieldBindingsFromProgramContext(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  const auto resolveStructLayoutExprPath = [&](const Expr &expr) -> std::string {
    return resolveStructLayoutExprPathFromScope(expr, defMap, importAliases);
  };
  return collectStructLayoutFieldBindings(program,
                                          structNames,
                                          resolveStructTypePath,
                                          resolveStructLayoutExprPath,
                                          defMap,
                                          semanticProductTargets,
                                          fieldsByStructOut,
                                          errorOut);
}

bool resolveStructLayoutFieldBinding(
    const std::string &structPath,
    const std::string &fieldName,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStruct,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut) {
  bindingOut = LayoutFieldBinding{};

  auto fieldInfoIt = fieldsByStruct.find(structPath);
  auto defIt = defMap.find(structPath);
  if (fieldInfoIt == fieldsByStruct.end() || defIt == defMap.end() || defIt->second == nullptr) {
    return false;
  }

  const Definition &def = *defIt->second;
  const auto &fieldBindings = fieldInfoIt->second;
  size_t fieldIndex = 0;
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      continue;
    }
    if (fieldIndex >= fieldBindings.size()) {
      return false;
    }
    if (stmt.name == fieldName) {
      bindingOut = fieldBindings[fieldIndex];
      return true;
    }
    ++fieldIndex;
  }

  return false;
}

std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace primec::ir_lowerer
