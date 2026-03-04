#include "IrLowererStructFieldBindingHelpers.h"

#include <functional>

#include "IrLowererHelpers.h"
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

} // namespace primec::ir_lowerer
