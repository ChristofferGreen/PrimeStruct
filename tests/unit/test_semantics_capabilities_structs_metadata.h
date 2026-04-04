#pragma once

TEST_CASE("reflection core type primitives validate") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [string] typeName{meta.type_name<Item>()}
  [string] typeKind{meta.type_kind<Item>()}
  [bool] isStructType{meta.is_struct<Item>()}
  [i32] fieldCount{meta.field_count<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("meta path core type primitives validate") {
  const std::string source = R"(
[return<int>]
main() {
  [string] typeName{/meta/type_name<i32>()}
  [string] typeKind{/meta/type_kind<i32>()}
  [bool] isStructType{/meta/is_struct<i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field_count rejects non-struct type targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] fieldCount{meta.field_count<i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("field_count rejects non-reflect struct targets") {
  const std::string source = R"(
[struct]
Item() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [i32] fieldCount{meta.field_count<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_count requires reflect-enabled struct type argument: /Item") !=
        std::string::npos);
}

TEST_CASE("field metadata core primitives validate") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
  [private string] label{"a"utf8}
}

[return<int>]
main() {
  [string] firstName{meta.field_name<Item>(0i32)}
  [string] firstType{meta.field_type<Item>(0i32)}
  [string] firstVisibility{meta.field_visibility<Item>(0i32)}
  [string] secondName{/meta/field_name<Item>(1i32)}
  [string] secondType{/meta/field_type<Item>(1i32)}
  [string] secondVisibility{/meta/field_visibility<Item>(1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field metadata queries reject non-constant index") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(plus(0i32, 0i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires constant integer index argument") != std::string::npos);
}

TEST_CASE("field metadata queries reject out-of-range index") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name field index out of range for /Item: 1") != std::string::npos);
}

TEST_CASE("field metadata queries reject non-struct type targets") {
  const std::string source = R"(
[return<int>]
main() {
  [string] name{meta.field_name<i32>(0i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires struct type argument: i32") != std::string::npos);
}

TEST_CASE("field metadata queries reject non-reflect struct targets") {
  const std::string source = R"(
[struct]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] name{meta.field_name<Item>(0i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.field_name requires reflect-enabled struct type argument: /Item") != std::string::npos);
}

TEST_CASE("generate SoaSchema emits reflected dynamic field-dispatch helpers") {
  const std::string source = R"(
[struct reflect generate(SoaSchema)]
Item() {
  [i32] x{1i32}
  [private string] label{"a"utf8}
  [static i32] shared{7i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *countHelper = nullptr;
  const primec::Definition *nameHelper = nullptr;
  const primec::Definition *typeHelper = nullptr;
  const primec::Definition *visibilityHelper = nullptr;
  const primec::Definition *chunkCountHelper = nullptr;
  const primec::Definition *chunkStartHelper = nullptr;
  const primec::Definition *chunkFieldCountHelper = nullptr;
  const primec::Definition *storageStruct = nullptr;
  const primec::Definition *storageNewHelper = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Item/SoaSchemaFieldCount") {
      countHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaFieldName") {
      nameHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaFieldType") {
      typeHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaFieldVisibility") {
      visibilityHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaChunkCount") {
      chunkCountHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaChunkFieldStart") {
      chunkStartHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaChunkFieldCount") {
      chunkFieldCountHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaStorage") {
      storageStruct = &def;
    } else if (def.fullPath == "/Item/SoaSchemaStorageNew") {
      storageNewHelper = &def;
    }
  }

  REQUIRE(countHelper != nullptr);
  REQUIRE(nameHelper != nullptr);
  REQUIRE(typeHelper != nullptr);
  REQUIRE(visibilityHelper != nullptr);
  REQUIRE(chunkCountHelper != nullptr);
  REQUIRE(chunkStartHelper != nullptr);
  REQUIRE(chunkFieldCountHelper != nullptr);
  REQUIRE(storageStruct != nullptr);
  REQUIRE(storageNewHelper != nullptr);

  CHECK(countHelper->parameters.empty());
  REQUIRE(countHelper->returnExpr.has_value());
  CHECK(countHelper->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(countHelper->returnExpr->literalValue == 2);
  REQUIRE(chunkCountHelper->returnExpr.has_value());
  CHECK(chunkCountHelper->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(chunkCountHelper->returnExpr->literalValue == 1);

  const auto assertIndexedStringHelper = [](const primec::Definition &helperDef,
                                            const std::string &expectedFirst,
                                            const std::string &expectedSecond) {
    REQUIRE(helperDef.parameters.size() == 1);
    CHECK(helperDef.parameters.front().name == "index");
    REQUIRE(helperDef.statements.size() == 2);
    for (size_t i = 0; i < helperDef.statements.size(); ++i) {
      const primec::Expr &guardStmt = helperDef.statements[i];
      REQUIRE(guardStmt.kind == primec::Expr::Kind::Call);
      CHECK(guardStmt.name == "if");
      REQUIRE(guardStmt.args.size() == 3);
      const primec::Expr &condition = guardStmt.args[0];
      REQUIRE(condition.kind == primec::Expr::Kind::Call);
      CHECK(condition.name == "equal");
      REQUIRE(condition.args.size() == 2);
      CHECK(condition.args[0].kind == primec::Expr::Kind::Name);
      CHECK(condition.args[0].name == "index");
      CHECK(condition.args[1].kind == primec::Expr::Kind::Literal);
      CHECK(condition.args[1].literalValue == i);
      const primec::Expr &thenEnvelope = guardStmt.args[1];
      REQUIRE(thenEnvelope.kind == primec::Expr::Kind::Call);
      CHECK(thenEnvelope.name == "then");
      REQUIRE(thenEnvelope.bodyArguments.size() == 1);
      const primec::Expr &returnCall = thenEnvelope.bodyArguments.front();
      REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
      CHECK(returnCall.name == "return");
      REQUIRE(returnCall.args.size() == 1);
      REQUIRE(returnCall.args[0].kind == primec::Expr::Kind::StringLiteral);
      CHECK(returnCall.args[0].stringValue == (i == 0 ? expectedFirst : expectedSecond));
    }
    REQUIRE(helperDef.returnExpr.has_value());
    CHECK(helperDef.returnExpr->kind == primec::Expr::Kind::StringLiteral);
    CHECK(helperDef.returnExpr->stringValue == "\"\"utf8");
  };

  const auto assertIndexedI32Helper = [](const primec::Definition &helperDef,
                                         uint64_t expectedFirst,
                                         std::optional<uint64_t> expectedSecond = std::nullopt) {
    REQUIRE(helperDef.parameters.size() == 1);
    CHECK(helperDef.parameters.front().name == "index");
    REQUIRE(helperDef.statements.size() == (expectedSecond.has_value() ? 2 : 1));
    for (size_t i = 0; i < helperDef.statements.size(); ++i) {
      const primec::Expr &guardStmt = helperDef.statements[i];
      REQUIRE(guardStmt.kind == primec::Expr::Kind::Call);
      CHECK(guardStmt.name == "if");
      REQUIRE(guardStmt.args.size() == 3);
      const primec::Expr &condition = guardStmt.args[0];
      REQUIRE(condition.kind == primec::Expr::Kind::Call);
      CHECK(condition.name == "equal");
      REQUIRE(condition.args.size() == 2);
      CHECK(condition.args[0].kind == primec::Expr::Kind::Name);
      CHECK(condition.args[0].name == "index");
      CHECK(condition.args[1].kind == primec::Expr::Kind::Literal);
      CHECK(condition.args[1].literalValue == i);
      const primec::Expr &thenEnvelope = guardStmt.args[1];
      REQUIRE(thenEnvelope.kind == primec::Expr::Kind::Call);
      CHECK(thenEnvelope.name == "then");
      REQUIRE(thenEnvelope.bodyArguments.size() == 1);
      const primec::Expr &returnCall = thenEnvelope.bodyArguments.front();
      REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
      CHECK(returnCall.name == "return");
      REQUIRE(returnCall.args.size() == 1);
      REQUIRE(returnCall.args[0].kind == primec::Expr::Kind::Literal);
      CHECK(returnCall.args[0].literalValue == (i == 0 ? expectedFirst : *expectedSecond));
    }
    REQUIRE(helperDef.returnExpr.has_value());
    CHECK(helperDef.returnExpr->kind == primec::Expr::Kind::Literal);
    CHECK(helperDef.returnExpr->literalValue == 0);
  };

  assertIndexedStringHelper(*nameHelper, "\"x\"utf8", "\"label\"utf8");
  assertIndexedStringHelper(*typeHelper, "\"i32\"utf8", "\"string\"utf8");
  assertIndexedStringHelper(*visibilityHelper, "\"public\"utf8", "\"private\"utf8");
  assertIndexedI32Helper(*chunkStartHelper, 0);
  assertIndexedI32Helper(*chunkFieldCountHelper, 2);

  REQUIRE(storageStruct->statements.size() == 1);
  const primec::Expr &storageChunk = storageStruct->statements.front();
  CHECK(storageChunk.name == "chunk0");
  REQUIRE(storageChunk.args.size() == 1);
  REQUIRE(storageChunk.transforms.size() >= 3);
  CHECK(storageChunk.transforms[0].name == "/std/collections/experimental_soa_storage/SoaColumns2");
  REQUIRE(storageChunk.transforms[0].templateArgs.size() == 2);
  CHECK(storageChunk.transforms[0].templateArgs[0] == "i32");
  CHECK(storageChunk.transforms[0].templateArgs[1] == "string");
  CHECK(storageChunk.transforms[1].name == "mut");
  CHECK(storageChunk.transforms[2].name == "public");
  REQUIRE(storageNewHelper->returnExpr.has_value());
  CHECK(storageNewHelper->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(storageNewHelper->returnExpr->name == "/Item/SoaSchemaStorage");
}

TEST_CASE("generate SoaSchema chunk helpers split wide reflected schemas deterministically") {
  const std::string source = R"(
[struct reflect generate(SoaSchema)]
Item() {
  [i32] f0{0i32}
  [i32] f1{0i32}
  [i32] f2{0i32}
  [i32] f3{0i32}
  [i32] f4{0i32}
  [i32] f5{0i32}
  [i32] f6{0i32}
  [i32] f7{0i32}
  [i32] f8{0i32}
  [i32] f9{0i32}
  [i32] f10{0i32}
  [i32] f11{0i32}
  [i32] f12{0i32}
  [i32] f13{0i32}
  [i32] f14{0i32}
  [i32] f15{0i32}
  [i32] f16{0i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  auto program = parseProgram(source);
  primec::Semantics semantics;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults));
  CHECK(error.empty());

  const primec::Definition *chunkCountHelper = nullptr;
  const primec::Definition *chunkStartHelper = nullptr;
  const primec::Definition *chunkFieldCountHelper = nullptr;
  const primec::Definition *storageStruct = nullptr;
  const primec::Definition *storageNewHelper = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/Item/SoaSchemaChunkCount") {
      chunkCountHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaChunkFieldStart") {
      chunkStartHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaChunkFieldCount") {
      chunkFieldCountHelper = &def;
    } else if (def.fullPath == "/Item/SoaSchemaStorage") {
      storageStruct = &def;
    } else if (def.fullPath == "/Item/SoaSchemaStorageNew") {
      storageNewHelper = &def;
    }
  }

  REQUIRE(chunkCountHelper != nullptr);
  REQUIRE(chunkStartHelper != nullptr);
  REQUIRE(chunkFieldCountHelper != nullptr);
  REQUIRE(storageStruct != nullptr);
  REQUIRE(storageNewHelper != nullptr);
  REQUIRE(chunkCountHelper->returnExpr.has_value());
  CHECK(chunkCountHelper->returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(chunkCountHelper->returnExpr->literalValue == 2);
  REQUIRE(chunkStartHelper->statements.size() == 2);
  REQUIRE(chunkFieldCountHelper->statements.size() == 2);
  CHECK(chunkStartHelper->statements[0].args[1].bodyArguments[0].args[0].literalValue == 0);
  CHECK(chunkStartHelper->statements[1].args[1].bodyArguments[0].args[0].literalValue == 16);
  CHECK(chunkFieldCountHelper->statements[0].args[1].bodyArguments[0].args[0].literalValue == 16);
  CHECK(chunkFieldCountHelper->statements[1].args[1].bodyArguments[0].args[0].literalValue == 1);

  REQUIRE(storageStruct->statements.size() == 2);
  const primec::Expr &firstChunk = storageStruct->statements[0];
  const primec::Expr &secondChunk = storageStruct->statements[1];
  CHECK(firstChunk.name == "chunk0");
  CHECK(secondChunk.name == "chunk1");
  REQUIRE(firstChunk.transforms.size() >= 3);
  REQUIRE(secondChunk.transforms.size() >= 3);
  CHECK(firstChunk.transforms[0].name == "/std/collections/experimental_soa_storage/SoaColumns16");
  REQUIRE(firstChunk.transforms[0].templateArgs.size() == 16);
  CHECK(secondChunk.transforms[0].name == "/std/collections/experimental_soa_storage/SoaColumn");
  REQUIRE(secondChunk.transforms[0].templateArgs.size() == 1);
  CHECK(secondChunk.transforms[0].templateArgs[0] == "i32");
  REQUIRE(storageNewHelper->returnExpr.has_value());
  CHECK(storageNewHelper->returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(storageNewHelper->returnExpr->name == "/Item/SoaSchemaStorage");
}

TEST_CASE("generate SoaSchema rejects helper collisions deterministically") {
  const std::string source = R"(
[struct reflect generate(SoaSchema)]
Item() {
  [i32] x{1i32}
}

[public return<string>]
/Item/SoaSchemaFieldName([i32] index) {
  return(""utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/Item/SoaSchemaFieldName", error));
  CHECK(error.find("generated reflection helper already exists: /Item/SoaSchemaFieldName") != std::string::npos);
}

TEST_CASE("has_transform query validates") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[public return<int>]
Helper() {
  return(0i32)
}

[return<int>]
main() {
  [bool] itemHasReflect{meta.has_transform<Item>("reflect"utf8)}
  [bool] itemHasGenerate{meta.has_transform<Item>("generate"utf8)}
  [bool] helperIsPublic{meta.has_transform<Helper>(public)}
  [bool] helperIsCompute{/meta/has_transform<Helper>("compute"utf8)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("has_transform rejects non-constant transform-name argument") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [bool] has{meta.has_transform<Item>(plus(1i32, 2i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_transform requires constant string or identifier argument") != std::string::npos);
}

TEST_CASE("has_transform rejects invalid argument count") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [bool] has{meta.has_transform<Item>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_transform requires exactly one transform-name argument") != std::string::npos);
}

TEST_CASE("has_trait query validates") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<Vec2>]
/Vec2/plus([Vec2] left, [Vec2] right) {
  return(Vec2())
}

[return<Vec2>]
/Vec2/multiply([Vec2] left, [Vec2] right) {
  return(Vec2())
}

[return<bool>]
/Vec2/equal([Vec2] left, [Vec2] right) {
  return(true)
}

[return<bool>]
/Vec2/less_than([Vec2] left, [Vec2] right) {
  return(false)
}

[return<i32>]
/Vec2/count([Vec2] value) {
  return(2i32)
}

[return<i32>]
/Vec2/at([Vec2] value, [i32] index) {
  return(0i32)
}

[return<int>]
main() {
  [bool] vecAdditive{meta.has_trait<Vec2>(Additive)}
  [bool] vecMultiplicative{meta.has_trait<Vec2>("Multiplicative"utf8)}
  [bool] vecComparable{meta.has_trait<Vec2>(Comparable)}
  [bool] vecIndexable{meta.has_trait<Vec2, i32>(Indexable)}
  [bool] i32Comparable{meta.has_trait<i32>(Comparable)}
  [bool] i32Additive{/meta/has_trait<i32>(Additive)}
  [bool] arrayIndexable{meta.has_trait<array<i32>, i32>("Indexable"utf8)}
  [bool] stringIndexable{/meta/has_trait<string, i32>(Indexable)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("has_trait rejects non-constant trait-name argument") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(plus(1i32, 2i32))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait requires constant string or identifier argument") != std::string::npos);
}

TEST_CASE("has_trait rejects unsupported trait name") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(Hash)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait does not support trait: Hash") != std::string::npos);
}

TEST_CASE("has_trait rejects invalid template argument count") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] has{meta.has_trait<i32>(Indexable)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("meta.has_trait Indexable requires type and element template arguments") !=
        std::string::npos);
}

TEST_CASE("unsupported reflection metadata queries are rejected") {
  struct Scenario {
    const char *label;
    const char *query;
    const char *expected;
  };
  const Scenario scenarios[] = {
      {"method", "meta.missing<Item>()", "unsupported reflection metadata query: meta.missing"},
      {"path", "/meta/missing<Item>()", "unsupported reflection metadata query: /meta/missing"},
  };
  for (const auto &scenario : scenarios) {
    CAPTURE(scenario.label);
    const std::string source =
        std::string("[struct reflect]\n"
                    "Item() {\n"
                    "  [i32] value{1i32}\n"
                    "}\n\n"
                    "[return<int>]\n"
                    "main() {\n"
                    "  [bool] has{") +
        scenario.query + "}\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find(scenario.expected) != std::string::npos);
  }
}

TEST_CASE("runtime reflection object queries are rejected") {
  struct Scenario {
    const char *label;
    const char *query;
    const char *expected;
  };
  const Scenario scenarios[] = {
      {"method-object", "meta.object<Item>()",
       "runtime reflection objects/tables are unsupported: meta.object"},
      {"method-table", "meta.table<Item>()",
       "runtime reflection objects/tables are unsupported: meta.table"},
      {"path-object", "/meta/object()",
       "runtime reflection objects/tables are unsupported: /meta/object"},
      {"path-table", "/meta/table()",
       "runtime reflection objects/tables are unsupported: /meta/table"},
  };
  for (const auto &scenario : scenarios) {
    CAPTURE(scenario.label);
    const std::string source =
        std::string("[struct reflect]\n"
                    "Item() {\n"
                    "  [i32] value{1i32}\n"
                    "}\n\n"
                    "[return<int>]\n"
                    "main() {\n  ") +
        scenario.query + "\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find(scenario.expected) != std::string::npos);
  }
}

TEST_CASE("struct transform rejects return transform") {
  const std::string source = R"(
[struct, return<int>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot declare return types") != std::string::npos);
}

TEST_CASE("struct transform rejects return statements") {
  const std::string source = R"(
[struct]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot contain return statements") != std::string::npos);
}

TEST_CASE("placement transforms are rejected") {
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[") + placement +
        "]\n"
        "main() {\n"
        "  [i32] value{1i32}\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported") != std::string::npos);
  }
}

TEST_CASE("struct definitions allow missing field initializers") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct definitions allow initialized fields") {
  const std::string source = R"(
[struct]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct fields infer omitted envelopes from struct initializers") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{7i32}
  [return<i32>]
  getX() {
    return(this.x)
  }
}

[struct]
Sphere() {
  [mut] center{Vec3()}
}

[return<i32>]
main() {
  [Sphere] s{Sphere()}
  return(s.center.getX())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct fields reject ambiguous omitted envelopes") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{1i32}
}

[struct]
Vec3() {
  [i32] x{2i32}
}

[struct]
Shape() {
  center{if(true, then() { Vec2() }, else() { Vec3() })}
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unresolved or ambiguous omitted struct field envelope: /Shape/center") != std::string::npos);
}

TEST_CASE("recursive struct layouts are rejected") {
  const std::string source = R"(
[struct]
Node() {
  [Node] next{Node()}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("recursive struct layout not supported") != std::string::npos);
}
