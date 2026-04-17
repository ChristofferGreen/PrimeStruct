#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");


TEST_CASE("ir validator accepts lowered canonical module") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Glsl, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer rejects non-eliminated reflection query paths") {
  const std::string source = R"(
[return<int>]
main() {
  /meta/type_name<i32>()
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  REQUIRE(parser.parse(program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error == "native backend requires compile-time reflection query elimination before IR emission: /meta/type_name");
}

TEST_CASE("ir lowerer reflection queries leave no runtime call state") {
  const std::string source = R"(
[struct reflect]
Item() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [string] typeName{meta.type_name<Item>()}
  [string] fieldName{meta.field_name<Item>(0i32)}
  [bool] hasReflect{meta.has_transform<Item>(reflect)}
  [bool] hasComparable{meta.has_trait<i32>(Comparable)}
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  REQUIRE(module.entryIndex >= 0);
  REQUIRE(static_cast<size_t>(module.entryIndex) < module.functions.size());
  const auto &entryFunction = module.functions[static_cast<size_t>(module.entryIndex)];
  for (const auto &instruction : entryFunction.instructions) {
    CHECK(instruction.op != primec::IrOpcode::Call);
    CHECK(instruction.op != primec::IrOpcode::CallVoid);
  }
  for (const auto &function : module.functions) {
    CHECK(function.name.rfind("/meta/", 0) != 0);
  }
  for (const auto &entry : module.stringTable) {
    CHECK(entry.rfind("/meta/", 0) != 0);
  }
}

TEST_CASE("ir lowerer map contains avoids missing-key runtime helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  if (/std/collections/map/contains(values, 1i32)) {
    return(1i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(std::find(module.stringTable.begin(), module.stringTable.end(), "map key not found") == module.stringTable.end());
}

TEST_CASE("ir lowerer guarded map Result lookup avoids missing-key runtime helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<Result<i32, MyError>>]
probe([map<i32, i32>] values, [i32] key) {
  if(/std/collections/map/contains(values, key),
     then(){ return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else(){ return(multiply(convert<i64>(1i32), 4294967296i64)) })
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 7i32)}
  probe(values, 1i32)
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(std::find(module.stringTable.begin(), module.stringTable.end(), "map key not found") == module.stringTable.end());
}

TEST_CASE("ir lowerer effects unit resolves entry and non-entry defaults") {
  const std::vector<primec::Transform> transforms;
  const std::vector<std::string> defaultEffects = {"io_out"};
  const std::vector<std::string> entryDefaultEffects = {"io_err"};

  const auto entryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, true, defaultEffects, entryDefaultEffects);
  CHECK(entryActive.size() == 1);
  CHECK(entryActive.count("io_err") == 1);
  CHECK(entryActive.count("io_out") == 0);

  const auto nonEntryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, false, defaultEffects, entryDefaultEffects);
  CHECK(nonEntryActive.size() == 1);
  CHECK(nonEntryActive.count("io_out") == 1);
  CHECK(nonEntryActive.count("io_err") == 0);
}

TEST_CASE("ir lowerer effects unit rejects software numeric envelopes") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("decimal");
  mainDef.transforms.push_back(returnTransform);
  program.definitions.push_back(std::move(mainDef));

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNoSoftwareNumericTypes(program, error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer helper classifies soa_vector as collection builtin") {
  primec::Expr soaVectorCall;
  soaVectorCall.kind = primec::Expr::Kind::Call;
  soaVectorCall.name = "soa_vector";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(soaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
}

TEST_CASE("ir lowerer helper rejects array namespaced vector constructor alias builtin") {
  primec::Expr arrayVectorCall;
  arrayVectorCall.kind = primec::Expr::Kind::Call;
  arrayVectorCall.name = "/array/vector";

  std::string builtin;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinCollectionName(arrayVectorCall, builtin));
}

TEST_CASE("shared collection helpers reject removed rooted vector constructor alias") {
  primec::Expr removedAliasCall;
  removedAliasCall.kind = primec::Expr::Kind::Call;
  removedAliasCall.name = "/vector/vector";

  std::string builtin;
  CHECK_FALSE(primec::semantics::getBuiltinCollectionName(removedAliasCall, builtin));
  CHECK_FALSE(primec::ir_lowerer::getBuiltinCollectionName(removedAliasCall, builtin));
  CHECK_FALSE(primec::emitter::getBuiltinCollectionName(removedAliasCall, builtin));
}

TEST_CASE("shared simple-call helpers reject removed rooted vector constructor alias") {
  primec::Expr bareVectorCall;
  bareVectorCall.kind = primec::Expr::Kind::Call;
  bareVectorCall.name = "vector";
  CHECK(primec::semantics::isSimpleCallName(bareVectorCall, "vector"));
  CHECK(primec::ir_lowerer::isSimpleCallName(bareVectorCall, "vector"));
  CHECK(primec::emitter::isSimpleCallName(bareVectorCall, "vector"));

  primec::Expr removedAliasCall = bareVectorCall;
  removedAliasCall.name = "/vector/vector";
  CHECK_FALSE(primec::semantics::isSimpleCallName(removedAliasCall, "vector"));
  CHECK_FALSE(primec::ir_lowerer::isSimpleCallName(removedAliasCall, "vector"));
  CHECK_FALSE(primec::emitter::isSimpleCallName(removedAliasCall, "vector"));
}

TEST_CASE("ir lowerer helper keeps canonical vector constructor builtin") {
  primec::Expr canonicalVectorCall;
  canonicalVectorCall.kind = primec::Expr::Kind::Call;
  canonicalVectorCall.name = "/std/collections/vector/vector";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(canonicalVectorCall, builtin));
  CHECK(builtin == "vector");
}

TEST_CASE("shared simple-call helpers reject removed array count alias") {
  primec::Expr bareCountCall;
  bareCountCall.kind = primec::Expr::Kind::Call;
  bareCountCall.name = "count";
  CHECK(primec::semantics::isSimpleCallName(bareCountCall, "count"));
  CHECK(primec::ir_lowerer::isSimpleCallName(bareCountCall, "count"));
  CHECK(primec::emitter::isSimpleCallName(bareCountCall, "count"));

  primec::Expr canonicalCountCall = bareCountCall;
  canonicalCountCall.name = "/std/collections/vector/count";
  CHECK(primec::semantics::isSimpleCallName(canonicalCountCall, "count"));
  CHECK(primec::ir_lowerer::isSimpleCallName(canonicalCountCall, "count"));
  CHECK(primec::emitter::isSimpleCallName(canonicalCountCall, "count"));

  primec::Expr removedAliasCall = bareCountCall;
  removedAliasCall.name = "/array/count";
  CHECK_FALSE(primec::semantics::isSimpleCallName(removedAliasCall, "count"));
  CHECK_FALSE(primec::ir_lowerer::isSimpleCallName(removedAliasCall, "count"));
  CHECK_FALSE(primec::emitter::isSimpleCallName(removedAliasCall, "count"));
}

TEST_CASE("semantics namespaced vector helper detection rejects removed rooted aliases") {
  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/vector/count";

  std::string collectionName;
  std::string helperName;
  CHECK(primec::semantics::getNamespacedCollectionHelperName(
      canonicalCountCall, collectionName, helperName));
  CHECK(collectionName == "vector");
  CHECK(helperName == "count");

  primec::Expr removedAliasCall = canonicalCountCall;
  removedAliasCall.name = "/vector/count";
  collectionName.clear();
  helperName.clear();
  CHECK_FALSE(primec::semantics::getNamespacedCollectionHelperName(
      removedAliasCall, collectionName, helperName));
  CHECK(collectionName.empty());
  CHECK(helperName.empty());
}

TEST_CASE("ir lowerer access helper rejects removed rooted vector access aliases") {
  primec::Expr canonicalAccessCall;
  canonicalAccessCall.kind = primec::Expr::Kind::Call;
  canonicalAccessCall.name = "/std/collections/vector/at";

  std::string helperName;
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      canonicalAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr removedAliasCall = canonicalAccessCall;
  removedAliasCall.name = "/vector/at";
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      removedAliasCall, helperName));
  CHECK(helperName.empty());
}

TEST_CASE("emitter cpp keeps canonical vector count builtin fallback") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(/std/collections/vector/count(values))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");
  CHECK(cpp.find("ps_array_count(") != std::string::npos);
}

TEST_CASE("emitter cpp keeps explicit canonical vector count same-path during emission") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(/vector/count(values))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  bool rewroteCall = false;
  for (auto &def : program.definitions) {
    if (def.fullPath != "/main" || !def.returnExpr.has_value()) {
      continue;
    }
    def.returnExpr->name = "/std/collections/vector/count";
    rewroteCall = true;
    break;
  }
  REQUIRE(rewroteCall);

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");
  CHECK(cpp.find("ps_array_count(") != std::string::npos);
}

TEST_CASE("emitter cpp keeps array count builtin fallback") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  bool rewroteAlias = false;
  for (auto &def : program.definitions) {
    if (def.fullPath != "/main" || !def.returnExpr.has_value()) {
      continue;
    }
    def.returnExpr->name = "/array/count";
    rewroteAlias = true;
    break;
  }
  REQUIRE(rewroteAlias);

  primec::Emitter emitter;
  const std::string cpp = emitter.emitCpp(program, "/main");
  CHECK(cpp.find("ps_array_count(") != std::string::npos);
}

TEST_CASE("semantics accepts and lowerer emits empty soa_vector literals") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<i32>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
  CHECK_FALSE(module.functions[0].instructions.empty());
}

TEST_CASE("canonical soa_vector count helper lowers through wrapper routing") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(/std/collections/vector/count(/std/collections/soa_vector/to_aos<Particle>(values)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("bare soa_vector count helper lowers through wrapper return routing") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorNew<Particle>())
  }
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.cloneValues()))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("nested struct-body soa_vector constructor-bearing helper returns lower through direct and bound expressions") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [SoaVector<Particle>] values{holder.cloneValues()}
  return(plus(plus(plus(holder.cloneValues().count(), holder.cloneValues().get(0i32).x),
                    values.ref(0i32).x),
              count(values.to_aos())))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("bare soa_vector get helper lowers through wrapper return routing") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  [SoaVector<Particle>, mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(get(holder.cloneValues(), 0i32).x)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer lowers non-empty soa_vector literals through substrate helper routing") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int> effects(heap_alloc)]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>(Particle(1i32))}
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
}

TEST_CASE("root get helper forms lower through canonical helper routing") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [Particle] valueA{get(values, 0i32)}
  [Particle] valueB{values.get(0i32)}
  [Particle] valueC{/soa_vector/get(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("root get vector receiver rejects template arguments") {
  const std::string source = R"(
[return<void>]
main() {
  [vector<i32>] values{vector<i32>()}
  [i32] valueA{get(values, 0i32)}
  [i32] valueB{values.get(0i32)}
  [i32] valueC{/soa_vector/get(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("root ref helper forms stop in semantics on borrowed-view pending diagnostic") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [Particle] valueA{ref(values, 0i32)}
  [Particle] valueB{values.ref(0i32)}
  [Particle] valueC{/soa_vector/ref(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/ref") != std::string::npos);
}

TEST_CASE("root ref vector receiver rejects non-soa target") {
  const std::string source = R"(
[return<void>]
main() {
  [vector<i32>] values{vector<i32>()}
  [i32] valueA{ref(values, 0i32)}
  [i32] valueB{values.ref(0i32)}
  [i32] valueC{/soa_vector/ref(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("unknown method: /std/collections/soa_vector/ref") != std::string::npos);
}

TEST_CASE("semantics accepts to_soa before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_soa(values)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error ==
        "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/"
        "pointer/assign/increment/decrement calls in expressions");
}

TEST_CASE("semantics accepts to_soa method forms before lowerer rejection") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  values.to_soa()
  values./to_soa()
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error ==
        "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/"
        "pointer/assign/increment/decrement calls in expressions");
}

TEST_CASE("semantics accepts to_aos before lowerer rejection") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(to_soa(values))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("semantics rejects explicit soa_vector reserve on vector target through canonical helper path") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /soa_vector/reserve(values, 4i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("/std/collections/soa_vector/reserve") != std::string::npos);
}

TEST_CASE("explicit soa_vector mutators share builtin lowerer reject path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  /soa_vector/reserve(values, 4i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error == "reserve requires mutable vector binding");
}

TEST_CASE("root to_aos bare and direct helper forms still need compile-pipeline helper materialization") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("imported root to_aos bare and direct helper forms still need compile-pipeline helper materialization") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("root to_aos method helper forms still need compile-pipeline helper materialization") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{values.to_aos()}
  [vector<Particle>] unpackedB{values./to_aos()}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("imported root to_aos method helper forms still need compile-pipeline helper materialization") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{values.to_aos()}
  [vector<Particle>] unpackedB{values./to_aos()}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("imported builtin soa_vector bare helper forms reach canonical lowerer mismatch") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32))
  [i32] total{count(values)}
  [Particle] first{get(values, 0i32)}
  [Particle] second{ref(values, 0i32)}
  return(plus(total, plus(first.x, second.x)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("struct parameter type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/experimental_soa_vector/SoaVector__") != std::string::npos);
}

TEST_CASE("imported builtin soa_vector method access forms stop in semantics on borrowed-view pending diagnostic") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [Particle] first{values.get(0i32)}
  [Particle] second{values.ref(0i32)}
  return(plus(first.x, second.x))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const bool parsed = parseAndValidate(source, program, semanticProgram, error);
  if (!parsed) {
    CHECK((error.find("unknown method: /std/collections/soa_vector/ref") !=
           std::string::npos ||
           error.find("field access requires struct receiver") !=
               std::string::npos));
  } else {
    CHECK(error.empty());
  }
}

TEST_CASE("imported builtin soa_vector method mutators reach canonical lowerer mismatch") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  values.reserve(2i32)
  values.push(Particle(4i32))
  return(values.get(0i32).x)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const bool parsed = parseAndValidate(source, program, semanticProgram, error);
  if (!parsed) {
    CHECK((error.find("unknown method: /std/collections/soa_vector/ref") !=
           std::string::npos ||
           error.find("field access requires struct receiver") !=
               std::string::npos));
  } else {
    CHECK(error.empty());
    primec::IrLowerer lowerer;
    primec::IrModule module;
    CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
    CHECK(error.find("struct parameter type mismatch") != std::string::npos);
    CHECK((error.find("got /soa_vector") != std::string::npos ||
           error.find("got /std/collections/soa_vector") != std::string::npos));
  }
}


TEST_CASE("canonical experimental wrapper to_aos slash-method lowers successfully") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa_vector/to_aos()}
  return(count(unpacked))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("borrowed helper-return experimental wrapper lowers through conversion helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [vector<Particle>] unpacked{
      /std/collections/experimental_soa_vector_conversions/soaVectorToAosRef<Particle>(
          pickBorrowed(location(values)))}
  return(count(unpacked))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
}

TEST_CASE("borrowed helper-return experimental wrapper bare conversion alias lowers through generic wildcard import") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [vector<Particle>] unpacked{
      soaVectorToAosRef<Particle>(pickBorrowed(location(values)))}
  return(count(unpacked))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
}

TEST_CASE("root to_aos canonical routing ignores vector-only user helper") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([vector<Particle>] values) {
  return(6i32)
}

[return<void>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpacked{to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("argument type mismatch for /to_aos parameter values") != std::string::npos);
}

TEST_CASE("root to_aos vector receiver keeps canonical reject contract") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  [vector<Particle>] unpackedC{values.to_aos()}
  [vector<Particle>] unpackedD{values./to_aos()}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("/std/collections/soa_vector/to_aos") != std::string::npos);
}

TEST_CASE("semantics rejects soa_vector field-view before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.x()
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa_vector/field_view/x");
}

TEST_CASE("semantics rejects soa_vector field-view call-form before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  x(values)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa_vector/field_view/x");
}

TEST_CASE("semantics rejects soa_vector field-view direct-call index shape before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.x(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa_vector/field_view/x");
}

TEST_CASE("semantics rejects soa_vector field-view call-form index shape before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  x(values, 0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa_vector/field_view/x");
}

TEST_CASE("semantics rejects soa_vector get method named args before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.get([index] 0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "named arguments not supported for builtin calls");
}

TEST_CASE("semantics rejects soa_vector ref method named args before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa_vector<Particle>] values) {
  values.ref([index] 0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "named arguments not supported for builtin calls");
}

TEST_CASE("ir lowerer effects unit validates program effect traversal") {
  auto makeEffectsTransform = [](const std::vector<std::string> &effects) {
    primec::Transform transform;
    transform.name = "effects";
    transform.arguments = effects;
    return transform;
  };

  primec::Program program;

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.transforms.push_back(makeEffectsTransform({"io_out"}));

  primec::Expr parameterExpr;
  parameterExpr.transforms.push_back(makeEffectsTransform({"io_err"}));
  primec::Expr nestedParameterArg;
  nestedParameterArg.transforms.push_back(makeEffectsTransform({"heap_alloc"}));
  parameterExpr.args.push_back(nestedParameterArg);
  entryDef.parameters.push_back(parameterExpr);

  primec::Expr statementExpr;
  primec::Expr nestedBodyArg;
  nestedBodyArg.transforms.push_back(makeEffectsTransform({"file_write"}));
  statementExpr.bodyArguments.push_back(nestedBodyArg);
  entryDef.statements.push_back(statementExpr);

  primec::Expr returnExpr;
  returnExpr.transforms.push_back(makeEffectsTransform({"gpu_dispatch"}));
  entryDef.returnExpr = returnExpr;
  program.definitions.push_back(entryDef);

  primec::Execution execution;
  execution.fullPath = "/run";
  execution.transforms.push_back(makeEffectsTransform({"pathspace_notify"}));
  primec::Expr execArg;
  execArg.transforms.push_back(makeEffectsTransform({"pathspace_insert"}));
  execution.arguments.push_back(execArg);
  primec::Expr execBodyArg;
  execBodyArg.transforms.push_back(makeEffectsTransform({"pathspace_take"}));
  execution.bodyArguments.push_back(execBodyArg);
  program.executions.push_back(execution);

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit rejects unsupported nested expression effects") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr statementExpr;
  primec::Expr nestedArg;
  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  nestedArg.transforms.push_back(badEffects);
  statementExpr.args.push_back(nestedArg);
  entryDef.statements.push_back(statementExpr);
  program.definitions.push_back(entryDef);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer effects unit prefers semantic product callable summaries") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  entryDef.transforms.push_back(badEffects);
  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out"},
      .activeCapabilities = {"io_out"},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffects(program, &semanticProgram, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit keeps nested expression effect checks syntax owned") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr statementExpr;
  primec::Expr nestedArg;
  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  nestedArg.transforms.push_back(badEffects);
  statementExpr.args.push_back(nestedArg);
  entryDef.statements.push_back(statementExpr);
  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out"},
      .activeCapabilities = {"io_out"},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
  });

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateProgramEffects(program, &semanticProgram, "/main", {}, {}, error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer effects unit resolves entry metadata masks") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out", "heap_alloc"};
  entryDef.transforms.push_back(effects);

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, "/main", {"io_err"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error.empty());
  CHECK(entryEffectMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
  CHECK(entryCapabilityMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
}

TEST_CASE("ir lowerer effects unit resolves entry metadata masks from semantic product") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out", "heap_alloc"},
      .activeCapabilities = {"io_out"},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
          primec::semanticProgramInternCallTargetString(semanticProgram, "heap_alloc"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
  });

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, &semanticProgram, "/main", {"io_err"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error.empty());
  CHECK(entryEffectMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
  CHECK(entryCapabilityMask == primec::EffectIoOut);
}

TEST_SUITE_END();
