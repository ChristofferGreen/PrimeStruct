#include "IrLowererStructFieldBindingHelpers.h"

#include <functional>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructReturnPathHelpers.h"
#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

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
    bindingOut.typeName = normalizeCollectionBindingTypeName(transform.name);
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
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  fieldsByStructOut.clear();
  for (const auto &def : program.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    std::vector<LayoutFieldBinding> fields;
    std::unordered_map<std::string, LayoutFieldBinding> knownFields;
    fields.reserve(def.statements.size());
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      LayoutFieldBinding binding;
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
      knownFields[stmt.name] = binding;
      fields.push_back(std::move(binding));
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
                                          fieldsByStructOut,
                                          errorOut);
}

std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace primec::ir_lowerer
