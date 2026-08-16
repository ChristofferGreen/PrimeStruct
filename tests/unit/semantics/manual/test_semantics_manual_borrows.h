#pragma once

TEST_CASE("borrow checker rejects overlapping mutable and immutable references with future use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker rejects overlapping mutable references with future use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker allows overlapping immutable references") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign to borrowed binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, assign, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker rejects move from borrowed binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr moveValue = makeCall("move", {makeName("value")});
  primec::Expr movedBinding = makeBinding("out", {makeTransform("i32")}, {moveValue});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, movedBinding, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after borrow scope ends") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr innerRef =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr block = makeCall("block", {}, {}, {innerRef, makeCall("/noop")});
  block.hasBodyArguments = true;
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/noop",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")}));
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, block, assign, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker tracks references through location(ref)") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("refA")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, refB, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrow conflict") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after last use in same scope") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, assign, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows mutable borrow after immutable last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr refA =
      makeBinding("refA", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("refA")})});
  primec::Expr refB =
      makeBinding("refB", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr writeThroughRef = makeCall("assign", {makeName("refB"), makeLiteral(3)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, refA, observed, refB, writeThroughRef, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr assign = makeCall("assign", {makeName("value"), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, assign, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker rejects mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate = makeCall("increment", {makeName("value")});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows mutation after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate = makeCall("increment", {makeName("value")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer dereference assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {makeName("ptr")}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer dereference assign after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {makeName("ptr")}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer dereference mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {makeName("ptr")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer dereference mutation after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {makeName("ptr")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows pointer arithmetic dereference assign without borrow") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ptr, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer arithmetic assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer arithmetic assign after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr write = makeCall("assign", {makeCall("dereference", {offsetPtr}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects pointer arithmetic mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows pointer arithmetic mutation after last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ptr =
      makeBinding("ptr", {makeTransform("Pointer", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr offsetPtr = makeCall("plus", {makeName("ptr"), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ptr, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows assign through location of mutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("ref")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker allows pointer arithmetic write through location of mutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32")), makeTransform("mut")},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("ref")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects assign through location of immutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("ref")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, write, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding: ref") != std::string::npos);
}

TEST_CASE("borrow checker rejects mutation through location of immutable reference") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("ref")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, ref, mutate, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("increment target must be a mutable binding: ref") != std::string::npos);
}

TEST_CASE("borrow checker reports immutable location root for assign") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, write, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding: value") != std::string::npos);
}

TEST_CASE("borrow checker reports immutable location arithmetic root for mutation") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr offsetPtr = makeCall("plus", {makeCall("location", {makeName("value")}), makeLiteral(0)});
  primec::Expr mutate = makeCall("increment", {makeCall("dereference", {offsetPtr})});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, mutate, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("increment target must be a mutable binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign through location of mutable binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("void"))}, {valueBinding, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects location(value) assign before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, write, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows location(value) assign after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr write =
      makeCall("assign", {makeCall("dereference", {makeCall("location", {makeName("value")})}), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, write, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("borrow checker rejects location(value) mutation before last reference use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr mutate =
      makeCall("increment", {makeCall("dereference", {makeCall("location", {makeName("value")})})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, mutate, observed, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("borrowed binding") != std::string::npos);
}

TEST_CASE("borrow checker allows location(value) mutation after reference last use") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(1)});
  primec::Expr ref =
      makeBinding("ref", {makeTransform("Reference", std::string("i32"))},
                  {makeCall("location", {makeName("value")})});
  primec::Expr observed =
      makeBinding("observed", {makeTransform("i32")},
                  {makeCall("dereference", {makeName("ref")})});
  primec::Expr mutate =
      makeCall("increment", {makeCall("dereference", {makeCall("location", {makeName("value")})})});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, ref, observed, mutate, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

