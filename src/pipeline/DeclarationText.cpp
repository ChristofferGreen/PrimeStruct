#include "DeclarationText.h"

#include <cstdint>

namespace primec {
namespace {

void appendTemplateArgs(std::string &out, const std::vector<std::string> &args) {
  if (args.empty()) {
    return;
  }
  out += "<";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  out += ">";
}

void appendLiteral(std::string &out, const Expr &expr) {
  if (expr.isUnsigned) {
    out += std::to_string(expr.literalValue);
    out += expr.intWidth == 8 ? "u8" : "u64";
    return;
  }
  out += std::to_string(static_cast<int64_t>(expr.literalValue));
  out += expr.intWidth == 64 ? "i64" : "i32";
}

void appendExpr(std::string &out, const Expr &expr);

// `T{}` parses to a constructor with one argument, an empty `block() {}`; printed as an argument it would be an
// expression without a value, so the empty initializer is left out.
bool isEmptyInitializer(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.name == "block" && expr.args.empty() &&
         expr.bodyArguments.empty() && !expr.isLambda && !expr.isMethodCall;
}

void appendArgs(std::string &out, const Expr &expr, size_t first) {
  if (expr.args.size() == first + 1 && isEmptyInitializer(expr.args[first]) &&
      (expr.isBinding || expr.isBraceConstructor)) {
    return;
  }
  for (size_t i = first; i < expr.args.size(); ++i) {
    if (i > first) {
      out += ", ";
    }
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      out += "[" + *expr.argNames[i] + "] ";
    }
    appendExpr(out, expr.args[i]);
  }
}

void appendBodyArguments(std::string &out, const Expr &expr) {
  if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
    return;
  }
  out += " {";
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    out += i == 0 ? " " : "; ";
    appendExpr(out, expr.bodyArguments[i]);
  }
  out += expr.bodyArguments.empty() ? "}" : " }";
}

void appendCall(std::string &out, const Expr &expr) {
  if (expr.isLambda) {
    out += "[";
    for (size_t i = 0; i < expr.lambdaCaptures.size(); ++i) {
      out += i == 0 ? "" : " ";
      out += expr.lambdaCaptures[i];
    }
    for (const Expr &capture : expr.lambdaInitCaptures) {
      out += " ";
      appendExpr(out, capture);
    }
    for (const Transform &transform : expr.transforms) {
      out += " " + transform.name;
      appendTemplateArgs(out, transform.templateArgs);
    }
    out += "] ";
  }
  if (expr.isMethodCall && !expr.args.empty()) {
    appendExpr(out, expr.args.front());
    out += "." + expr.name;
    if (!expr.isFieldAccess) {
      appendTemplateArgs(out, expr.templateArgs);
      out += "(";
      appendArgs(out, expr, 1);
      out += ")";
    }
  } else {
    out += declarationTransformListText(expr.transforms);
    out += expr.name;
    appendTemplateArgs(out, expr.templateArgs);
    const bool braces = expr.isBinding || expr.isBraceConstructor;
    out += braces ? "{" : "(";
    appendArgs(out, expr, 0);
    out += braces ? "}" : ")";
  }
  appendBodyArguments(out, expr);
}

void appendExpr(std::string &out, const Expr &expr) {
  if (expr.isSpread) {
    out += "[spread] ";
  }
  switch (expr.kind) {
  case Expr::Kind::Literal:
    appendLiteral(out, expr);
    break;
  case Expr::Kind::BoolLiteral:
    out += expr.boolValue ? "true" : "false";
    break;
  case Expr::Kind::FloatLiteral:
    out += expr.floatValue;
    out += expr.floatWidth == 64 ? "f64" : "f32";
    break;
  case Expr::Kind::StringLiteral:
    out += expr.stringValue;
    break;
  case Expr::Kind::Name:
    out += declarationTransformListText(expr.transforms);
    out += expr.name;
    break;
  case Expr::Kind::Call:
    appendCall(out, expr);
    break;
  }
}

} // namespace

std::string declarationTransformText(const Transform &transform) {
  std::string out = transform.name;
  appendTemplateArgs(out, transform.templateArgs);
  if (transform.isPackExpansion) {
    out += "...";
  }
  if (!transform.arguments.empty()) {
    out += "(";
    for (size_t i = 0; i < transform.arguments.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += transform.arguments[i];
    }
    out += ")";
  }
  return out;
}

std::string declarationTransformListText(const std::vector<Transform> &transforms) {
  if (transforms.empty()) {
    return "";
  }
  std::string out = "[";
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += declarationTransformText(transforms[i]);
  }
  out += "] ";
  return out;
}

std::string declarationExprText(const Expr &expr) {
  std::string out;
  appendExpr(out, expr);
  return out;
}

std::string declarationParameterText(const Expr &parameter) {
  std::string out = declarationTransformListText(parameter.transforms) + parameter.name;
  if (!parameter.args.empty()) {
    out += "{";
    appendArgs(out, parameter, 0);
    out += "}";
  }
  return out;
}

std::string declarationBindingText(const Expr &binding) {
  std::string out = declarationTransformListText(binding.transforms) + binding.name;
  out += "{";
  appendArgs(out, binding, 0);
  out += "}";
  return out;
}

std::string declarationBodyText(const std::vector<Expr> &statements) {
  std::string out;
  for (const Expr &statement : statements) {
    out += "  ";
    appendExpr(out, statement);
    out += "\n";
  }
  return out;
}

std::string declarationQuotedString(const std::string &text) {
  std::string out = "\"";
  for (const char c : text) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  out += "\"";
  return out;
}

} // namespace primec
