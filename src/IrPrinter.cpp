#include "primec/IrPrinter.h"

#include <sstream>

namespace primec {

namespace {
enum class ReturnKind { Int, Void };

ReturnKind getReturnKind(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || !transform.templateArg) {
      continue;
    }
    if (*transform.templateArg == "void") {
      return ReturnKind::Void;
    }
    if (*transform.templateArg == "int") {
      return ReturnKind::Int;
    }
  }
  return ReturnKind::Int;
}

const char *returnTypeName(ReturnKind kind) {
  return kind == ReturnKind::Void ? "void" : "i32";
}

void indent(std::ostringstream &out, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
}

void printExpr(std::ostringstream &out, const Expr &expr) {
  switch (expr.kind) {
  case Expr::Kind::Literal:
    out << expr.literalValue;
    break;
  case Expr::Kind::Name:
    out << expr.name;
    break;
  case Expr::Kind::Call:
    out << expr.name << "(";
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, expr.args[i]);
    }
    out << ")";
    break;
  }
}

void printDefinition(std::ostringstream &out, const Definition &def, int depth) {
  ReturnKind kind = getReturnKind(def);
  indent(out, depth);
  out << "def " << def.fullPath << "(): " << returnTypeName(kind) << " {\n";
  for (const auto &stmt : def.statements) {
    indent(out, depth + 1);
    out << "call ";
    printExpr(out, stmt);
    out << "\n";
  }
  if (def.returnExpr) {
    indent(out, depth + 1);
    out << "return ";
    printExpr(out, *def.returnExpr);
    out << "\n";
  } else if (kind == ReturnKind::Void) {
    indent(out, depth + 1);
    out << "return\n";
  }
  indent(out, depth);
  out << "}\n";
}

void printExecution(std::ostringstream &out, const Execution &exec, int depth) {
  indent(out, depth);
  out << "exec ";
  if (!exec.transforms.empty()) {
    out << "[";
    for (size_t i = 0; i < exec.transforms.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << exec.transforms[i].name;
      if (exec.transforms[i].templateArg) {
        out << "<" << *exec.transforms[i].templateArg << ">";
      }
    }
    out << "] ";
  }
  out << exec.fullPath << "(";
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    printExpr(out, exec.arguments[i]);
  }
  out << ")";
  if (!exec.bodyArguments.empty()) {
    out << " { ";
    for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, exec.bodyArguments[i]);
    }
    out << " }";
  }
  out << "\n";
}
} // namespace

std::string IrPrinter::print(const Program &program) const {
  std::ostringstream out;
  out << "module {\n";
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
