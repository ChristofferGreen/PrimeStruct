#include "third_party/doctest.h"

#include <string>

#include "primec/Ast.h"
#include "primec/IrPrinter.h"

TEST_SUITE_BEGIN("primestruct.ir_printer");

TEST_CASE("ir printer prints empty program") {
  primec::Program program;
  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.empty());
}

TEST_CASE("ir printer prints single definition with return transform") {
  primec::Program program;
  primec::Definition def;
  def.path = "/main";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("[return<int>]") != std::string::npos);
  CHECK(output.find("/main") != std::string::npos);
}

TEST_CASE("ir printer prints definition with parameters") {
  primec::Program program;
  primec::Definition def;
  def.path = "/add";
  def.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.parameters.push_back(primec::Parameter{"a", {"int"}, {}});
  def.parameters.push_back(primec::Parameter{"b", {"int"}, {}});
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("/add") != std::string::npos);
  CHECK(output.find("[int] a") != std::string::npos);
  CHECK(output.find("[int] b") != std::string::npos);
}

TEST_CASE("ir printer prints execution envelope") {
  primec::Program program;
  primec::Execution exec;
  exec.path = "/print_line";
  exec.transforms.push_back(primec::Transform{"effects", {"io_err"}, {}});
  exec.arguments.push_back(primec::Expr{primec::Expr::Kind::Literal});
  exec.arguments.back().literalValue = "\"hello\"";
  program.executions.push_back(exec);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("[effects(io_err)]") != std::string::npos);
  CHECK(output.find("/print_line") != std::string::npos);
  CHECK(output.find("\"hello\"") != std::string::npos);
}

TEST_CASE("ir printer prints struct definition") {
  primec::Program program;
  primec::Definition def;
  def.path = "/Point";
  def.transforms.push_back(primec::Transform{"struct", {}, {}});
  def.transforms.push_back(primec::Transform{"return", {"Point"}, {}});

  primec::DefinitionField field;
  field.name = "x";
  field.transforms.push_back(primec::Transform{"return", {"int"}, {}});
  def.fields.push_back(field);

  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("[struct]") != std::string::npos);
  CHECK(output.find("/Point") != std::string::npos);
  CHECK(output.find("[int] x") != std::string::npos);
}

TEST_CASE("ir printer prints definition with template parameters") {
  primec::Program program;
  primec::Definition def;
  def.path = "/identity";
  def.transforms.push_back(primec::Transform{"return", {"T"}, {}});
  def.templateParameters.push_back("T");
  def.parameters.push_back(primec::Parameter{"value", {"T"}, {}});
  program.definitions.push_back(def);

  primec::IrPrinter printer;
  std::string output = printer.print(program);
  CHECK(output.find("<T>") != std::string::npos);
  CHECK(output.find("[T] value") != std::string::npos);
}

TEST_SUITE_END();
