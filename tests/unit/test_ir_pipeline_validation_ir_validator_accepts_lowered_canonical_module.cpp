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

TEST_CASE("ir lowerer helper keeps parser-shaped canonical vector constructor builtin") {
  primec::Expr canonicalVectorCall;
  canonicalVectorCall.kind = primec::Expr::Kind::Call;
  canonicalVectorCall.name = "vector";
  canonicalVectorCall.namespacePrefix = "/std/collections/vector";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(canonicalVectorCall, builtin));
  CHECK(builtin == "vector");
}

TEST_CASE("ir lowerer helper keeps bare array builtin inside namespaced stdlib internals") {
  primec::Expr namespacedArrayCall;
  namespacedArrayCall.kind = primec::Expr::Kind::Call;
  namespacedArrayCall.name = "array";
  namespacedArrayCall.namespacePrefix = "/std/collections/internal_soa_storage";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(namespacedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(namespacedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr namespacedSoaVectorCall;
  namespacedSoaVectorCall.kind = primec::Expr::Kind::Call;
  namespacedSoaVectorCall.name = "soa_vector";
  namespacedSoaVectorCall.namespacePrefix =
      "/std/collections/internal_soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(
      namespacedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
  CHECK(primec::emitter::getBuiltinCollectionName(
      namespacedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");

  primec::Expr rootedArrayCall;
  rootedArrayCall.kind = primec::Expr::Kind::Call;
  rootedArrayCall.name = "/std/collections/internal_soa_storage/array";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(rootedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(rootedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr rootedSoaVectorCall;
  rootedSoaVectorCall.kind = primec::Expr::Kind::Call;
  rootedSoaVectorCall.name = "/std/collections/internal_soa_storage/soa_vector";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(rootedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
  CHECK(primec::emitter::getBuiltinCollectionName(rootedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");

  primec::Expr generatedArrayCall;
  generatedArrayCall.kind = primec::Expr::Kind::Call;
  generatedArrayCall.name =
      "/std/collections/internal_soa_storage/SoaColumns11__tabcdef01/array";
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(generatedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(generatedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr generatedSoaVectorCall;
  generatedSoaVectorCall.kind = primec::Expr::Kind::Call;
  generatedSoaVectorCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/soa_vector";
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(generatedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
  CHECK(primec::emitter::getBuiltinCollectionName(generatedSoaVectorCall, builtin));
  CHECK(builtin == "soa_vector");
}

TEST_CASE("ir lowerer helper keeps bare pointer builtins inside namespaced stdlib internals") {
  primec::Expr namespacedDereferenceCall;
  namespacedDereferenceCall.kind = primec::Expr::Kind::Call;
  namespacedDereferenceCall.name = "dereference";
  namespacedDereferenceCall.namespacePrefix = "/std/collections/internal_soa_storage";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      namespacedDereferenceCall, builtin));
  CHECK(builtin == "dereference");
  char pointerOp = '\0';
  CHECK(primec::emitter::getBuiltinPointerOperator(
      namespacedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr rootedDereferenceCall;
  rootedDereferenceCall.kind = primec::Expr::Kind::Call;
  rootedDereferenceCall.name =
      "/std/collections/internal_soa_storage/dereference";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      rootedDereferenceCall, builtin));
  CHECK(builtin == "dereference");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      rootedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr namespacedLocationCall;
  namespacedLocationCall.kind = primec::Expr::Kind::Call;
  namespacedLocationCall.name = "location";
  namespacedLocationCall.namespacePrefix = "/std/collections/internal_soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      namespacedLocationCall, builtin));
  CHECK(builtin == "location");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      namespacedLocationCall, pointerOp));
  CHECK(pointerOp == '&');

  primec::Expr rootedLocationCall;
  rootedLocationCall.kind = primec::Expr::Kind::Call;
  rootedLocationCall.name = "/std/collections/internal_soa_storage/location";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      rootedLocationCall, builtin));
  CHECK(builtin == "location");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      rootedLocationCall, pointerOp));
  CHECK(pointerOp == '&');

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/dereference";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      generatedDereferenceCall, builtin));
  CHECK(builtin == "dereference");

  primec::Expr generatedLocationCall;
  generatedLocationCall.kind = primec::Expr::Kind::Call;
  generatedLocationCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/location";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      generatedLocationCall, builtin));
  CHECK(builtin == "location");
}

TEST_CASE("ir lowerer helper keeps bare array access builtins inside namespaced stdlib internals") {
  primec::Expr namespacedAtCall;
  namespacedAtCall.kind = primec::Expr::Kind::Call;
  namespacedAtCall.name = "at";
  namespacedAtCall.namespacePrefix = "/std/collections/internal_soa_storage";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedAtCall, builtin));
  CHECK(builtin == "at");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      namespacedAtCall, builtin));
  CHECK(builtin == "at");

  primec::Expr rootedAtCall;
  rootedAtCall.kind = primec::Expr::Kind::Call;
  rootedAtCall.name = "/std/collections/internal_soa_storage/at";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedAtCall, builtin));
  CHECK(builtin == "at");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedAtCall, builtin));
  CHECK(builtin == "at");

  primec::Expr namespacedAtUnsafeCall;
  namespacedAtUnsafeCall.kind = primec::Expr::Kind::Call;
  namespacedAtUnsafeCall.name = "at_unsafe";
  namespacedAtUnsafeCall.namespacePrefix = "/std/collections/internal_soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      namespacedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");

  primec::Expr rootedAtUnsafeCall;
  rootedAtUnsafeCall.kind = primec::Expr::Kind::Call;
  rootedAtUnsafeCall.name =
      "/std/collections/internal_soa_storage/at_unsafe";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");

  primec::Expr specializedInternalSoaColumnAccessCall;
  specializedInternalSoaColumnAccessCall.kind = primec::Expr::Kind::Call;
  specializedInternalSoaColumnAccessCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/at";

  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      specializedInternalSoaColumnAccessCall, builtin));
  CHECK(builtin == "at");
}

TEST_CASE("simple-call helpers keep rooted and namespaced internal soa storage bare builtins") {
  primec::Expr rootedAssignCall;
  rootedAssignCall.kind = primec::Expr::Kind::Call;
  rootedAssignCall.name = "/std/collections/internal_soa_storage/assign";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedAssignCall, "assign"));
  CHECK(primec::emitter::isSimpleCallName(rootedAssignCall, "assign"));

  primec::Expr rootedIfCall;
  rootedIfCall.kind = primec::Expr::Kind::Call;
  rootedIfCall.name = "/std/collections/internal_soa_storage/if";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedIfCall, "if"));
  CHECK(primec::emitter::isSimpleCallName(rootedIfCall, "if"));

  primec::Expr rootedTakeCall;
  rootedTakeCall.kind = primec::Expr::Kind::Call;
  rootedTakeCall.name = "/std/collections/internal_soa_storage/take";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedTakeCall, "take"));
  CHECK(primec::emitter::isSimpleCallName(rootedTakeCall, "take"));

  auto makeNamespacedInternalSoaCall = [](const char *name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.namespacePrefix = "/std/collections/internal_soa_storage";
    return expr;
  };

  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("assign"), "assign"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("if"), "if"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("take"), "take"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("borrow"), "borrow"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("init"), "init"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("drop"), "drop"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("while"), "while"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("do"), "do"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("location"), "location"));
  CHECK(primec::ir_lowerer::isSimpleCallName(
      makeNamespacedInternalSoaCall("dereference"), "dereference"));

  primec::Expr generatedAssignCall;
  generatedAssignCall.kind = primec::Expr::Kind::Call;
  generatedAssignCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/assign";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedAssignCall, "assign"));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedIfCall, "if"));

  primec::Expr generatedTakeCall;
  generatedTakeCall.kind = primec::Expr::Kind::Call;
  generatedTakeCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/take";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedTakeCall, "take"));

  primec::Expr generatedLocationCall;
  generatedLocationCall.kind = primec::Expr::Kind::Call;
  generatedLocationCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/location";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedLocationCall, "location"));

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/dereference";
  CHECK(primec::ir_lowerer::isSimpleCallName(
      generatedDereferenceCall, "dereference"));
}

TEST_CASE("emitter builtin assign keeps internal soa storage helper paths") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr rootedAssignCall;
  rootedAssignCall.kind = primec::Expr::Kind::Call;
  rootedAssignCall.name = "/std/collections/internal_soa_storage/assign";
  CHECK(primec::emitter::isBuiltinAssign(rootedAssignCall, nameMap));

  primec::Expr namespacedAssignCall;
  namespacedAssignCall.kind = primec::Expr::Kind::Call;
  namespacedAssignCall.name = "assign";
  namespacedAssignCall.namespacePrefix = "/std/collections/internal_soa_storage";
  CHECK(primec::emitter::isBuiltinAssign(namespacedAssignCall, nameMap));
}

TEST_CASE("emitter control helpers keep internal soa storage helper paths") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr namespacedIfCall;
  namespacedIfCall.kind = primec::Expr::Kind::Call;
  namespacedIfCall.name = "if";
  namespacedIfCall.namespacePrefix = "/std/collections/internal_soa_storage";
  CHECK(primec::emitter::isBuiltinIf(namespacedIfCall, nameMap));

  primec::Expr namespacedBlockCall;
  namespacedBlockCall.kind = primec::Expr::Kind::Call;
  namespacedBlockCall.name = "block";
  namespacedBlockCall.namespacePrefix = "/std/collections/internal_soa_storage";
  CHECK(primec::emitter::isBuiltinBlock(namespacedBlockCall, nameMap));

  primec::Expr namespacedWhileCall;
  namespacedWhileCall.kind = primec::Expr::Kind::Call;
  namespacedWhileCall.name = "while";
  namespacedWhileCall.namespacePrefix = "/std/collections/internal_soa_storage";
  CHECK(primec::emitter::isWhileCall(namespacedWhileCall));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::emitter::isBuiltinIf(generatedIfCall, nameMap));

  primec::Expr generatedLoopCall;
  generatedLoopCall.kind = primec::Expr::Kind::Call;
  generatedLoopCall.name =
      "/std/collections/internal_soa_storage/SoaColumns2__tabcdef01/loop";
  CHECK(primec::emitter::isLoopCall(generatedLoopCall));

  primec::Expr generatedReturnCall;
  generatedReturnCall.kind = primec::Expr::Kind::Call;
  generatedReturnCall.name =
      "/std/collections/internal_soa_storage/SoaColumns2__tabcdef01/return";
  CHECK(primec::emitter::isReturnCall(generatedReturnCall));
}

TEST_CASE("shared return helpers keep scoped stdlib and custom paths builtin") {
  primec::Expr namespacedBufferReturnCall;
  namespacedBufferReturnCall.kind = primec::Expr::Kind::Call;
  namespacedBufferReturnCall.name = "return";
  namespacedBufferReturnCall.namespacePrefix =
      "/std/collections/internal_buffer_checked";
  CHECK(primec::ir_lowerer::isReturnCall(namespacedBufferReturnCall));
  CHECK(primec::emitter::isReturnCall(namespacedBufferReturnCall));

  primec::Expr namespacedSoaVectorReturnCall;
  namespacedSoaVectorReturnCall.kind = primec::Expr::Kind::Call;
  namespacedSoaVectorReturnCall.name = "return";
  namespacedSoaVectorReturnCall.namespacePrefix =
      "/std/collections/experimental_soa_vector";
  CHECK(primec::ir_lowerer::isReturnCall(namespacedSoaVectorReturnCall));
  CHECK(primec::emitter::isReturnCall(namespacedSoaVectorReturnCall));

  primec::Expr rootedCustomReturnCall;
  rootedCustomReturnCall.kind = primec::Expr::Kind::Call;
  rootedCustomReturnCall.name = "/MyError/return";
  CHECK(primec::ir_lowerer::isReturnCall(rootedCustomReturnCall));
  CHECK(primec::emitter::isReturnCall(rootedCustomReturnCall));

  primec::Expr namespacedSoaWhileCall;
  namespacedSoaWhileCall.kind = primec::Expr::Kind::Call;
  namespacedSoaWhileCall.name = "while";
  namespacedSoaWhileCall.namespacePrefix =
      "/std/collections/experimental_soa_vector";
  CHECK(primec::ir_lowerer::isWhileCall(namespacedSoaWhileCall));
  CHECK(primec::emitter::isWhileCall(namespacedSoaWhileCall));

  primec::Expr namespacedSoaDoCall;
  namespacedSoaDoCall.kind = primec::Expr::Kind::Call;
  namespacedSoaDoCall.name = "do";
  namespacedSoaDoCall.namespacePrefix =
      "/std/collections/experimental_soa_vector";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaDoCall, "do"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaDoCall, "do"));
}

TEST_CASE("emitter helpers keep generated internal soa helper paths builtin") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr generatedAssignCall;
  generatedAssignCall.kind = primec::Expr::Kind::Call;
  generatedAssignCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/assign";
  CHECK(primec::emitter::isBuiltinAssign(generatedAssignCall, nameMap));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::emitter::isSimpleCallName(generatedIfCall, "if"));

  primec::Expr generatedTakeCall;
  generatedTakeCall.kind = primec::Expr::Kind::Call;
  generatedTakeCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/take";
  CHECK(primec::emitter::isSimpleCallName(generatedTakeCall, "take"));

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/dereference";
  char pointerOp = '\0';
  CHECK(primec::emitter::getBuiltinPointerOperator(
      generatedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr generatedPlusCall;
  generatedPlusCall.kind = primec::Expr::Kind::Call;
  generatedPlusCall.name =
      "/std/collections/internal_soa_storage/SoaColumns2__tabcdef01/plus";
  char op = '\0';
  CHECK(primec::emitter::getBuiltinOperator(generatedPlusCall, op));
  CHECK(op == '+');

  primec::Expr generatedLessThanCall;
  generatedLessThanCall.kind = primec::Expr::Kind::Call;
  generatedLessThanCall.name =
      "/std/collections/internal_soa_storage/SoaColumns2__tabcdef01/less_than";
  const char *comparison = nullptr;
  CHECK(primec::emitter::getBuiltinComparison(
      generatedLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr generatedIncrementCall;
  generatedIncrementCall.kind = primec::Expr::Kind::Call;
  generatedIncrementCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/increment";
  std::string mutation;
  CHECK(primec::emitter::getBuiltinMutationName(
      generatedIncrementCall, mutation));
  CHECK(mutation == "increment");
}

TEST_CASE("ir lowerer helper rejects parser-shaped canonical map entry constructors as builtin map") {
  primec::Expr entryCall;
  entryCall.kind = primec::Expr::Kind::Call;
  entryCall.name = "entry";
  entryCall.namespacePrefix = "/std/collections/map";

  primec::Expr canonicalMapCall;
  canonicalMapCall.kind = primec::Expr::Kind::Call;
  canonicalMapCall.name = "map";
  canonicalMapCall.namespacePrefix = "/std/collections/map";
  canonicalMapCall.args = {entryCall};

  std::string builtin;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinCollectionName(canonicalMapCall, builtin));
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

TEST_CASE("semantics removed-alias helpers reject rooted vector spellings") {
  CHECK(primec::semantics::isExplicitRemovedCollectionCallAlias("/array/push"));
  CHECK_FALSE(primec::semantics::isExplicitRemovedCollectionCallAlias("/vector/push"));

  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias("/array", "/array/push"));
  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias(
      "/vector", "/std/collections/vector/push"));
  CHECK_FALSE(primec::semantics::isExplicitRemovedCollectionMethodAlias("/vector", "/vector/push"));
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

TEST_CASE("ir lowerer setup-type vector helper detection rejects removed rooted aliases") {
  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/vector/count";

  std::string collectionName;
  std::string helperName;
  CHECK(primec::ir_lowerer::getNamespacedCollectionHelperName(
      canonicalCountCall, collectionName, helperName));
  CHECK(collectionName == "vector");
  CHECK(helperName == "count");

  primec::Expr removedAliasCall = canonicalCountCall;
  removedAliasCall.name = "/vector/count";
  collectionName.clear();
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getNamespacedCollectionHelperName(
      removedAliasCall, collectionName, helperName));
  CHECK(collectionName.empty());
  CHECK(helperName.empty());
}

TEST_CASE("ir lowerer setup-type removed vector method alias helper rejects rooted aliases") {
  CHECK(primec::ir_lowerer::isExplicitRemovedVectorMethodAliasPath("/array/count"));
  CHECK(primec::ir_lowerer::isExplicitRemovedVectorMethodAliasPath("/std/collections/vector/count"));
  CHECK_FALSE(primec::ir_lowerer::isExplicitRemovedVectorMethodAliasPath("/vector/count"));
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

TEST_CASE("ir lowerer access helper recognizes namespaced canonical access helpers") {
  primec::Expr namespacedVectorAccessCall;
  namespacedVectorAccessCall.kind = primec::Expr::Kind::Call;
  namespacedVectorAccessCall.namespacePrefix = "/std/collections/vector";
  namespacedVectorAccessCall.name = "at";

  std::string helperName;
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedVectorAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr namespacedMapAccessCall;
  namespacedMapAccessCall.kind = primec::Expr::Kind::Call;
  namespacedMapAccessCall.namespacePrefix = "/std/collections/map";
  namespacedMapAccessCall.name = "at_unsafe";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedMapAccessCall, helperName));
  CHECK(helperName == "at_unsafe");

  primec::Expr namespacedExperimentalVectorAccessCall;
  namespacedExperimentalVectorAccessCall.kind = primec::Expr::Kind::Call;
  namespacedExperimentalVectorAccessCall.namespacePrefix =
      "/std/collections/experimental_vector";
  namespacedExperimentalVectorAccessCall.name = "at";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedExperimentalVectorAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr specializedExperimentalVectorMethodAccessCall;
  specializedExperimentalVectorMethodAccessCall.kind = primec::Expr::Kind::Call;
  specializedExperimentalVectorMethodAccessCall.namespacePrefix =
      "/std/collections/experimental_vector/Vector__t12345678";
  specializedExperimentalVectorMethodAccessCall.name = "at_unsafe";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      specializedExperimentalVectorMethodAccessCall, helperName));
  CHECK(helperName == "at_unsafe");

  primec::Expr specializedVectorMethodAccessCall;
  specializedVectorMethodAccessCall.kind = primec::Expr::Kind::Call;
  specializedVectorMethodAccessCall.namespacePrefix =
      "/std/collections/vector/Vector__tabcdef01";
  specializedVectorMethodAccessCall.name = "at";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      specializedVectorMethodAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr namespacedExperimentalSoaStorageAccessCall;
  namespacedExperimentalSoaStorageAccessCall.kind = primec::Expr::Kind::Call;
  namespacedExperimentalSoaStorageAccessCall.namespacePrefix =
      "/std/collections/internal_soa_storage";
  namespacedExperimentalSoaStorageAccessCall.name = "at_unsafe";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedExperimentalSoaStorageAccessCall, helperName));
  CHECK(helperName == "at_unsafe");

  primec::Expr specializedExperimentalSoaColumnAccessCall;
  specializedExperimentalSoaColumnAccessCall.kind = primec::Expr::Kind::Call;
  specializedExperimentalSoaColumnAccessCall.namespacePrefix =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01";
  specializedExperimentalSoaColumnAccessCall.name = "at";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      specializedExperimentalSoaColumnAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr removedAliasCall = namespacedVectorAccessCall;
  removedAliasCall.namespacePrefix = "/vector";
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      removedAliasCall, helperName));
  CHECK(helperName.empty());
}

TEST_CASE("ir lowerer stdlib surface metadata recognizes experimental map lowering helpers") {
  const auto *countMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapCount");
  REQUIRE(countMetadata != nullptr);
  CHECK(countMetadata->id == primec::StdlibSurfaceId::CollectionsMapHelpers);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *countMetadata, "/std/collections/experimental_map/mapCount") == "count");

  const auto *insertMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapInsert");
  REQUIRE(insertMetadata != nullptr);
  CHECK(insertMetadata->id == primec::StdlibSurfaceId::CollectionsMapHelpers);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *insertMetadata, "/std/collections/experimental_map/mapInsert") == "insert");

  const auto *atUnsafeMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapAtUnsafe");
  REQUIRE(atUnsafeMetadata != nullptr);
  CHECK(atUnsafeMetadata->id == primec::StdlibSurfaceId::CollectionsMapHelpers);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *atUnsafeMetadata, "/std/collections/experimental_map/mapAtUnsafe") == "at_unsafe");
}

TEST_CASE("stdlib surface metadata resolves collection helper member tokens") {
  const auto *vectorMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsVectorHelpers);
  REQUIRE(vectorMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "vectorCount") == "count");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "vectorRemoveSwap") ==
        "remove_swap");

  const auto *vectorCtorMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsVectorConstructors);
  REQUIRE(vectorCtorMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorCtorMetadata, "vectorSingle") ==
        "vectorSingle");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorCtorMetadata, "vectorOct") ==
        "vectorOct");

  const auto *mapMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsMapHelpers);
  REQUIRE(mapMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "mapAtUnsafeRef") ==
        "at_unsafe_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "Insert") == "insert");
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "MapInsertRef") ==
        "insert_ref");
}

TEST_CASE("stdlib surface metadata classifies collection helper categories") {
  CHECK(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsVectorHelpers, "capacity"));
  CHECK(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsMapHelpers, "tryAt_ref"));
  CHECK_FALSE(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsVectorHelpers, "insert"));

  CHECK(primec::isStdlibVectorStatementHelperName("push"));
  CHECK(primec::isStdlibVectorStatementHelperName("remove_swap"));
  CHECK_FALSE(primec::isStdlibVectorStatementHelperName("count"));

  CHECK(primec::isStdlibMapBaseHelperName("contains"));
  CHECK(primec::isStdlibMapBaseHelperName("insert"));
  CHECK_FALSE(primec::isStdlibMapBaseHelperName("contains_ref"));

  CHECK(primec::isStdlibMapBorrowedHelperName("contains_ref"));
  CHECK(primec::isStdlibMapBorrowedHelperName("at_ref"));
  CHECK_FALSE(primec::isStdlibMapBorrowedHelperName("contains"));
}

TEST_CASE("ir lowerer helper keeps parser-shaped intrinsic memory builtins") {
  primec::Expr allocCall;
  allocCall.kind = primec::Expr::Kind::Call;
  allocCall.name = "alloc";
  allocCall.namespacePrefix = "/std/intrinsics/memory";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinMemoryName(allocCall, builtin));
  CHECK(builtin == "alloc");

  primec::Expr reinterpretCall;
  reinterpretCall.kind = primec::Expr::Kind::Call;
  reinterpretCall.name = "reinterpret";
  reinterpretCall.namespacePrefix = "/std/intrinsics/memory";

  CHECK(primec::ir_lowerer::getBuiltinMemoryName(reinterpretCall, builtin));
  CHECK(builtin == "reinterpret");
}

TEST_CASE("ir lowerer helper keeps parser-shaped gpu builtins") {
  primec::Expr globalIdXCall;
  globalIdXCall.kind = primec::Expr::Kind::Call;
  globalIdXCall.name = "global_id_x";
  globalIdXCall.namespacePrefix = "/std/gpu";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinGpuName(globalIdXCall, builtin));
  CHECK(builtin == "global_id_x");

  primec::Expr globalIdZCall;
  globalIdZCall.kind = primec::Expr::Kind::Call;
  globalIdZCall.name = "global_id_z";
  globalIdZCall.namespacePrefix = "/std/gpu";

  CHECK(primec::ir_lowerer::getBuiltinGpuName(globalIdZCall, builtin));
  CHECK(builtin == "global_id_z");
}

TEST_CASE("ir lowerer helper keeps parser-shaped rooted convert builtin") {
  primec::Expr convertCall;
  convertCall.kind = primec::Expr::Kind::Call;
  convertCall.name = "convert";
  convertCall.namespacePrefix = "/";

  CHECK(primec::ir_lowerer::getBuiltinConvertName(convertCall));
  std::string builtin;
  CHECK(primec::emitter::getBuiltinConvertName(convertCall, builtin));
  CHECK(builtin == "convert");
}

TEST_CASE("emitter helper keeps parser-shaped rooted negate builtin") {
  primec::Expr negateCall;
  negateCall.kind = primec::Expr::Kind::Call;
  negateCall.name = "negate";
  negateCall.namespacePrefix = "/";

  CHECK(primec::emitter::isBuiltinNegate(negateCall));
}

TEST_CASE("emitter helpers keep parser-shaped std math builtins") {
  primec::Expr minCall;
  minCall.kind = primec::Expr::Kind::Call;
  minCall.name = "min";
  minCall.namespacePrefix = "/std/math";

  std::string builtin;
  CHECK(primec::emitter::getBuiltinMinMaxName(minCall, builtin, false));
  CHECK(builtin == "min");

  primec::Expr absCall;
  absCall.kind = primec::Expr::Kind::Call;
  absCall.name = "abs";
  absCall.namespacePrefix = "/std/math";

  CHECK(primec::emitter::getBuiltinAbsSignName(absCall, builtin, false));
  CHECK(builtin == "abs");

  primec::Expr clampCall;
  clampCall.kind = primec::Expr::Kind::Call;
  clampCall.name = "clamp";
  clampCall.namespacePrefix = "/std/math";

  CHECK(primec::emitter::isBuiltinClamp(clampCall, false));

  primec::Expr sqrtCall;
  sqrtCall.kind = primec::Expr::Kind::Call;
  sqrtCall.name = "sqrt";
  sqrtCall.namespacePrefix = "/std/math";

  CHECK(primec::emitter::getBuiltinMathName(sqrtCall, builtin, false));
  CHECK(builtin == "sqrt");
}

TEST_CASE("emitter helpers keep internal soa builtins under rooted and namespaced paths") {
  primec::Expr rootedPlusCall;
  rootedPlusCall.kind = primec::Expr::Kind::Call;
  rootedPlusCall.name = "/std/collections/internal_soa_storage/plus";

  char op = '\0';
  CHECK(primec::emitter::getBuiltinOperator(rootedPlusCall, op));
  CHECK(op == '+');

  primec::Expr namespacedLessThanCall;
  namespacedLessThanCall.kind = primec::Expr::Kind::Call;
  namespacedLessThanCall.name = "less_than";
  namespacedLessThanCall.namespacePrefix = "/std/collections/internal_soa_storage";

  const char *comparison = nullptr;
  CHECK(primec::emitter::getBuiltinComparison(namespacedLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr rootedIncrementCall;
  rootedIncrementCall.kind = primec::Expr::Kind::Call;
  rootedIncrementCall.name = "/std/collections/internal_soa_storage/increment";

  std::string mutation;
  CHECK(primec::emitter::getBuiltinMutationName(rootedIncrementCall, mutation));
  CHECK(mutation == "increment");
}

TEST_CASE("emitter collection inference keeps namespaced internal soa builtins") {
  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;

  primec::emitter::BindingInfo arrayInfo;
  arrayInfo.typeName = "array";
  arrayInfo.typeTemplateArg = "i32";
  localTypes.emplace("items", arrayInfo);

  primec::Expr namespacedCountCall;
  namespacedCountCall.kind = primec::Expr::Kind::Call;
  namespacedCountCall.name = "count";
  namespacedCountCall.namespacePrefix = "/std/collections/internal_soa_storage";

  primec::Expr itemsName;
  itemsName.kind = primec::Expr::Kind::Name;
  itemsName.name = "items";
  namespacedCountCall.args.push_back(itemsName);

  CHECK(primec::emitter::isArrayCountCall(namespacedCountCall, localTypes));

  primec::emitter::BindingInfo vectorInfo;
  vectorInfo.typeName = "vector";
  vectorInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", vectorInfo);

  primec::Expr namespacedCapacityCall;
  namespacedCapacityCall.kind = primec::Expr::Kind::Call;
  namespacedCapacityCall.name = "capacity";
  namespacedCapacityCall.namespacePrefix = "/std/collections/internal_soa_storage";

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  namespacedCapacityCall.args.push_back(valuesName);

  CHECK(primec::emitter::isVectorCapacityCall(namespacedCapacityCall, localTypes));

  primec::emitter::BindingInfo stringInfo;
  stringInfo.typeName = "string";
  localTypes.emplace("text", stringInfo);

  primec::Expr namespacedAtCall;
  namespacedAtCall.kind = primec::Expr::Kind::Call;
  namespacedAtCall.name = "at";
  namespacedAtCall.namespacePrefix = "/std/collections/internal_soa_storage";

  primec::Expr textName;
  textName.kind = primec::Expr::Kind::Name;
  textName.name = "text";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  namespacedAtCall.args.push_back(textName);
  namespacedAtCall.args.push_back(indexLiteral);

  CHECK(primec::emitter::isStringValue(namespacedAtCall, localTypes));

  primec::Expr namespacedStringCountCall;
  namespacedStringCountCall.kind = primec::Expr::Kind::Call;
  namespacedStringCountCall.name = "count";
  namespacedStringCountCall.namespacePrefix = "/std/collections/internal_soa_storage";
  namespacedStringCountCall.args.push_back(textName);

  CHECK(primec::emitter::isStringCountCall(namespacedStringCountCall, localTypes));
}

TEST_CASE("stdlib surface metadata resolves collection alias paths") {
  const auto *vectorMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_vector/vectorPush");
  REQUIRE(vectorMetadata != nullptr);
  CHECK(vectorMetadata->id == primec::StdlibSurfaceId::CollectionsVectorHelpers);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorMetadata, "/std/collections/experimental_vector/vectorPush") == "push");

  const auto *vectorCtorMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_vector/vectorPair");
  REQUIRE(vectorCtorMetadata != nullptr);
  CHECK(vectorCtorMetadata->id == primec::StdlibSurfaceId::CollectionsVectorConstructors);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorCtorMetadata, "/std/collections/experimental_vector/vectorPair") ==
        "vectorPair");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorCtorMetadata, "/std/collections/vectorSingle__tabcd") ==
        "vectorSingle");

  const auto *mapCtorMetadata = primec::findStdlibSurfaceMetadataByResolvedPath("/map/map");
  REQUIRE(mapCtorMetadata != nullptr);
  CHECK(mapCtorMetadata->id == primec::StdlibSurfaceId::CollectionsMapConstructors);
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapCtorMetadata, "/map/map") == "map");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *mapCtorMetadata, "/std/collections/mapPair__t1234") == "mapPair");
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

TEST_CASE("root to_aos bare and direct helper forms reject during semantics") {
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
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
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

TEST_CASE("root to_aos method helper forms reject during semantics") {
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
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
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
