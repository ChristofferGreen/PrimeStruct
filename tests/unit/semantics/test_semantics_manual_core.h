#pragma once

TEST_CASE("duplicate definitions fail") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(2)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate definition") != std::string::npos);
}

TEST_CASE("return transform requires template argument") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return")}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform requires a type") != std::string::npos);
}

TEST_CASE("return transform rejects multiple template arguments") {
  primec::Program program;
  primec::Transform transform;
  transform.name = "return";
  transform.templateArgs = {"int", "i32"};
  program.definitions.push_back(
      makeDefinition("/main", {transform}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform requires a type") != std::string::npos);
}

TEST_CASE("import math root is rejected") {
  primec::Program program;
  program.imports.push_back("/std/math");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("import /std/math is not supported") != std::string::npos);
}

TEST_CASE("return transform rejects arguments in manual AST") {
  primec::Program program;
  primec::Transform transform = makeTransform("return", std::string("int"));
  transform.arguments = {"bad"};
  program.definitions.push_back(makeDefinition("/main", {transform}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform does not accept arguments") != std::string::npos);
}

