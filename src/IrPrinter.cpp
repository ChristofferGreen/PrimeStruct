#include "primec/IrPrinter.h"

#include <sstream>

namespace primec {

namespace {
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
  indent(out, depth);
  out << "def " << def.fullPath << "(): i32 {\n";
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
  }
  indent(out, depth);
  out << "}\n";
}

} // namespace

std::string IrPrinter::print(const Program &program) const {
  std::ostringstream out;
  out << "module {\n";
  for (const auto &def : program.definitions) {
    printDefinition(out, def, 1);
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
