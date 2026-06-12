#include "third_party/doctest.h"

#include <string>

#include "primec/Ast.h"
#include "primec/IrPrinter.h"

TEST_SUITE_BEGIN("primestruct.ir_printer");

TEST_CASE("ir printer prints empty program") {
  primec::Program program;
  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("module") != std::string::npos);
}

TEST_CASE("ir printer prints single definition") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/main";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/main") != std::string::npos);
}

TEST_CASE("ir printer prints definition with parameters") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/add";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});

  primec::Expr paramA;
  paramA.kind = primec::Expr::Kind::Name;
  paramA.name = "a";
  paramA.isBinding = true;
  paramA.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.parameters.push_back(paramA);

  primec::Expr paramB;
  paramB.kind = primec::Expr::Kind::Name;
  paramB.name = "b";
  paramB.isBinding = true;
  paramB.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.parameters.push_back(paramB);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/add") != std::string::npos);
}

TEST_CASE("ir printer prints execution envelope") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/print_line";
  exec.transforms.push_back(primec::Transform{"effects", {"io_err"}, {}});

  primec::Expr arg;
  arg.kind = primec::Expr::Kind::StringLiteral;
  arg.stringValue = "hello";
  exec.arguments.push_back(arg);

  program.executions.push_back(exec);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/print_line") != std::string::npos);
}

TEST_CASE("ir printer prints definition with template args") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/identity";
  def.transforms.push_back(primec::Transform{"return", {"T"}, {}});
  def.templateArgs.push_back("T");

  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = "value";
  param.isBinding = true;
  param.transforms.push_back(primec::Transform{"return", {"T"}, {}});
  def.parameters.push_back(param);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/identity") != std::string::npos);
}

TEST_SUITE_END();
