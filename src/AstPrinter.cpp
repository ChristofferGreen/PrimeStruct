#include "primec/AstPrinter.h"

#include <cstdint>
#include <sstream>

namespace primec {

namespace {
void indent(std::ostringstream &out, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
}

void printTransforms(std::ostringstream &out, const std::vector<Transform> &transforms);

bool isReturnCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "return";
}

void printExpr(std::ostringstream &out, const Expr &expr) {
  switch (expr.kind) {
  case Expr::Kind::Literal:
    if (!expr.isUnsigned && expr.intWidth == 32) {
      out << static_cast<int64_t>(expr.literalValue);
    } else {
      if (expr.isUnsigned) {
        out << expr.literalValue;
        out << (expr.intWidth == 64 ? "u64" : "u32");
      } else {
        out << static_cast<int64_t>(expr.literalValue);
        out << (expr.intWidth == 64 ? "i64" : "i32");
      }
    }
    break;
  case Expr::Kind::BoolLiteral:
    out << (expr.boolValue ? "true" : "false");
    break;
  case Expr::Kind::FloatLiteral:
    out << expr.floatValue << (expr.floatWidth == 64 ? "f64" : "f32");
    break;
  case Expr::Kind::StringLiteral:
    out << expr.stringValue;
    break;
  case Expr::Kind::Name:
    out << expr.name;
    break;
  case Expr::Kind::Call:
    if (expr.isMethodCall && !expr.args.empty()) {
      printExpr(out, expr.args.front());
      out << "." << expr.name;
      if (expr.isFieldAccess) {
        break;
      }
      if (!expr.templateArgs.empty()) {
        out << "<";
        for (size_t i = 0; i < expr.templateArgs.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << expr.templateArgs[i];
        }
        out << ">";
      }
      out << "(";
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (i > 1) {
          out << ", ";
        }
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          out << "[" << *expr.argNames[i] << "] ";
        }
        printExpr(out, expr.args[i]);
      }
      out << ")";
    } else {
      if (!expr.transforms.empty()) {
        printTransforms(out, expr.transforms);
      }
      out << expr.name;
      if (!expr.templateArgs.empty()) {
        out << "<";
        for (size_t i = 0; i < expr.templateArgs.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << expr.templateArgs[i];
        }
        out << ">";
      }
      const bool useBraces = expr.isBinding;
      out << (useBraces ? "{" : "(");
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          out << "[" << *expr.argNames[i] << "] ";
        }
        printExpr(out, expr.args[i]);
      }
      out << (useBraces ? "}" : ")");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      out << " { ";
      for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        printExpr(out, expr.bodyArguments[i]);
      }
      out << " }";
    }
    break;
  }
}

void printTransforms(std::ostringstream &out, const std::vector<Transform> &transforms) {
  if (transforms.empty()) {
    return;
  }
  out << "[";
  for (size_t i = 0; i < transforms.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << transforms[i].name;
    if (!transforms[i].templateArgs.empty()) {
      out << "<";
      for (size_t t = 0; t < transforms[i].templateArgs.size(); ++t) {
        if (t > 0) {
          out << ", ";
        }
        out << transforms[i].templateArgs[t];
      }
      out << ">";
    }
    if (!transforms[i].arguments.empty()) {
      out << "(";
      for (size_t argIndex = 0; argIndex < transforms[i].arguments.size(); ++argIndex) {
        if (argIndex > 0) {
          out << ", ";
        }
        out << transforms[i].arguments[argIndex];
      }
      out << ")";
    }
  }
  out << "] ";
}

void printTemplateArgs(std::ostringstream &out, const std::vector<std::string> &args) {
  if (args.empty()) {
    return;
  }
  out << "<";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << args[i];
  }
  out << ">";
}

void printParameter(std::ostringstream &out, const Expr &param) {
  printTransforms(out, param.transforms);
  out << param.name;
  if (!param.args.empty()) {
    out << "(";
    for (size_t i = 0; i < param.args.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, param.args[i]);
    }
    out << ")";
  }
}

void printDefinition(std::ostringstream &out, const Definition &def, int depth) {
  indent(out, depth);
  printTransforms(out, def.transforms);
  out << def.fullPath;
  printTemplateArgs(out, def.templateArgs);
  out << "(";
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    printParameter(out, def.parameters[i]);
  }
  out << ") {\n";
  for (const auto &stmt : def.statements) {
    indent(out, depth + 1);
    if (isReturnCall(stmt)) {
      out << "return";
      if (!stmt.args.empty()) {
        out << " ";
        printExpr(out, stmt.args.front());
      }
    } else {
      printExpr(out, stmt);
    }
    out << "\n";
  }
  indent(out, depth);
  out << "}\n";
}

void printExecution(std::ostringstream &out, const Execution &exec, int depth) {
  indent(out, depth);
  printTransforms(out, exec.transforms);
  out << exec.fullPath;
  printTemplateArgs(out, exec.templateArgs);
  out << "(";
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    if (i < exec.argumentNames.size() && exec.argumentNames[i].has_value()) {
      out << "[" << *exec.argumentNames[i] << "] ";
    }
    printExpr(out, exec.arguments[i]);
  }
  out << ")";
  if (exec.hasBodyArguments || !exec.bodyArguments.empty()) {
    if (exec.bodyArguments.empty()) {
      out << " { }";
    } else {
      out << " { ";
      for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        printExpr(out, exec.bodyArguments[i]);
      }
      out << " }";
    }
  }
  out << "\n";
}
} // namespace

std::string AstPrinter::print(const Program &program) const {
  std::ostringstream out;
  out << "ast {\n";
  for (const auto &def : program.definitions) {
    printDefinition(out, def, 1);
  }
  for (const auto &exec : program.executions) {
    printExecution(out, exec, 1);
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
