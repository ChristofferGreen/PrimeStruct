#include "primec/AstPrinter.h"

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
    out << expr.literalValue;
    break;
  case Expr::Kind::Name:
    out << expr.name;
    break;
  case Expr::Kind::Call:
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
    out << "(";
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, expr.args[i]);
    }
    out << ")";
    if (!expr.bodyArguments.empty()) {
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
    if (transforms[i].templateArg) {
      out << "<" << *transforms[i].templateArg << ">";
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
    out << def.parameters[i];
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
