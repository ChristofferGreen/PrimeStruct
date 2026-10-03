#include <filesystem>
#include <fstream>
#include <iterator>

#include "test_ir_pipeline_validation_callback_factories.h"

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
  // Per TODO-4728 (ir_lowerer effects-unit test fixtures missing
  // semantic-product callable summaries): this fixture parses without
  // running semantics validation, so `semanticProgram` stays empty and
  // lowering now rejects the call earlier, at the generic
  // missing-semantic-id gate, before ever reaching the
  // reflection-query-elimination check this test's name describes.
  CHECK(error == "missing semantic-product direct-call semantic id: /main -> /meta/type_name");
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

TEST_CASE("ir lowerer effects unit rejects published software numeric preflight facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.firstSoftwareNumericTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "decimal");
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNativeNoSoftwareNumericTypes(&semanticProgram, error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer effects unit rejects published runtime reflection preflight facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/meta/type_name");
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNativeNoRuntimeReflectionQueries(&semanticProgram, error));
  CHECK(error ==
        "native backend requires compile-time reflection query elimination before IR emission: /meta/type_name");
}

TEST_CASE("ir lowerer gpu effects unit rejects published software numeric preflight facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.firstSoftwareNumericTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "decimal");
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateGpuNoSoftwareNumericTypes(&semanticProgram, error));
  CHECK(error == "gpu backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer gpu effects unit rejects published runtime reflection preflight facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/meta/type_name");
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateGpuNoRuntimeReflectionQueries(&semanticProgram, error));
  CHECK(error ==
        "gpu backend requires compile-time reflection query elimination before IR emission: /meta/type_name");
}

TEST_CASE("ir lowerer helper classifies soa as collection builtin") {
  primec::Expr soaVectorCall;
  soaVectorCall.kind = primec::Expr::Kind::Call;
  soaVectorCall.name = "soa";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(soaVectorCall, builtin));
  CHECK(builtin == "soa");
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

TEST_CASE("ir lowerer helper recognizes experimental vector element alias constructor") {
  primec::Expr rewrittenVectorCall;
  rewrittenVectorCall.kind = primec::Expr::Kind::Call;
  rewrittenVectorCall.name = "i32";
  rewrittenVectorCall.namespacePrefix = "/std/collections/experimental_vector";

  std::string elementType;
  CHECK(primec::ir_lowerer::getExperimentalVectorConstructorElementTypeAlias(
      rewrittenVectorCall, elementType));
  CHECK(elementType == "i32");

  CHECK(primec::ir_lowerer::getExperimentalVectorConstructorElementTypeAliasFromPath(
      "/std/collections/experimental_vector/i32", elementType));
  CHECK(elementType == "i32");

  CHECK_FALSE(primec::ir_lowerer::getExperimentalVectorConstructorElementTypeAliasFromPath(
      "/std/collections/experimental_vector/vectorCount", elementType));

  primec::Expr helperCall = rewrittenVectorCall;
  helperCall.name = "/std/collections/vector/count";
  CHECK_FALSE(primec::ir_lowerer::getExperimentalVectorConstructorElementTypeAlias(
      helperCall, elementType));

  helperCall.name = "vector";
  CHECK_FALSE(primec::ir_lowerer::getExperimentalVectorConstructorElementTypeAlias(
      helperCall, elementType));
}

TEST_CASE("ir lowerer helper keeps bare array builtin inside namespaced stdlib internals") {
  primec::Expr namespacedArrayCall;
  namespacedArrayCall.kind = primec::Expr::Kind::Call;
  namespacedArrayCall.name = "array";
  namespacedArrayCall.namespacePrefix = "/std/collections/soa_storage";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(namespacedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(namespacedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr namespacedSoaVectorCall;
  namespacedSoaVectorCall.kind = primec::Expr::Kind::Call;
  namespacedSoaVectorCall.name = "soa";
  namespacedSoaVectorCall.namespacePrefix =
      "/std/collections/soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(
      namespacedSoaVectorCall, builtin));
  CHECK(builtin == "soa");
  CHECK(primec::emitter::getBuiltinCollectionName(
      namespacedSoaVectorCall, builtin));
  CHECK(builtin == "soa");

  primec::Expr rootedArrayCall;
  rootedArrayCall.kind = primec::Expr::Kind::Call;
  rootedArrayCall.name = "/std/collections/soa_storage/array";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(rootedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(rootedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr rootedSoaVectorCall;
  rootedSoaVectorCall.kind = primec::Expr::Kind::Call;
  rootedSoaVectorCall.name = "/std/collections/soa_storage/soa";

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(rootedSoaVectorCall, builtin));
  CHECK(builtin == "soa");
  CHECK(primec::emitter::getBuiltinCollectionName(rootedSoaVectorCall, builtin));
  CHECK(builtin == "soa");

  primec::Expr generatedArrayCall;
  generatedArrayCall.kind = primec::Expr::Kind::Call;
  generatedArrayCall.name =
      "/std/collections/soa_storage/SoaColumns11__tabcdef01/array";
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(generatedArrayCall, builtin));
  CHECK(builtin == "array");
  CHECK(primec::emitter::getBuiltinCollectionName(generatedArrayCall, builtin));
  CHECK(builtin == "array");

  primec::Expr generatedSoaVectorCall;
  generatedSoaVectorCall.kind = primec::Expr::Kind::Call;
  generatedSoaVectorCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/soa";
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(generatedSoaVectorCall, builtin));
  CHECK(builtin == "soa");
  CHECK(primec::emitter::getBuiltinCollectionName(generatedSoaVectorCall, builtin));
  CHECK(builtin == "soa");
}

TEST_CASE("ir lowerer helper keeps bare pointer builtins inside namespaced stdlib internals") {
  primec::Expr namespacedDereferenceCall;
  namespacedDereferenceCall.kind = primec::Expr::Kind::Call;
  namespacedDereferenceCall.name = "dereference";
  namespacedDereferenceCall.namespacePrefix = "/std/collections/soa_storage";

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
      "/std/collections/soa_storage/dereference";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      rootedDereferenceCall, builtin));
  CHECK(builtin == "dereference");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      rootedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr namespacedLocationCall;
  namespacedLocationCall.kind = primec::Expr::Kind::Call;
  namespacedLocationCall.name = "location";
  namespacedLocationCall.namespacePrefix = "/std/collections/soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      namespacedLocationCall, builtin));
  CHECK(builtin == "location");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      namespacedLocationCall, pointerOp));
  CHECK(pointerOp == '&');

  primec::Expr rootedLocationCall;
  rootedLocationCall.kind = primec::Expr::Kind::Call;
  rootedLocationCall.name = "/std/collections/soa_storage/location";

  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      rootedLocationCall, builtin));
  CHECK(builtin == "location");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      rootedLocationCall, pointerOp));
  CHECK(pointerOp == '&');

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/dereference";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      generatedDereferenceCall, builtin));
  CHECK(builtin == "dereference");

  primec::Expr generatedLocationCall;
  generatedLocationCall.kind = primec::Expr::Kind::Call;
  generatedLocationCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/location";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      generatedLocationCall, builtin));
  CHECK(builtin == "location");

  primec::Expr namespacedCheckedDereferenceCall;
  namespacedCheckedDereferenceCall.kind = primec::Expr::Kind::Call;
  namespacedCheckedDereferenceCall.name = "dereference";
  namespacedCheckedDereferenceCall.namespacePrefix =
      "/std/collections/buffer_checked";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      namespacedCheckedDereferenceCall, builtin));
  CHECK(builtin == "dereference");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      namespacedCheckedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr namespacedUncheckedLocationCall;
  namespacedUncheckedLocationCall.kind = primec::Expr::Kind::Call;
  namespacedUncheckedLocationCall.name = "location";
  namespacedUncheckedLocationCall.namespacePrefix =
      "/std/collections/buffer_unchecked";
  CHECK(primec::ir_lowerer::getBuiltinPointerName(
      namespacedUncheckedLocationCall, builtin));
  CHECK(builtin == "location");
  CHECK(primec::emitter::getBuiltinPointerOperator(
      namespacedUncheckedLocationCall, pointerOp));
  CHECK(pointerOp == '&');
}

TEST_CASE("ir lowerer helper keeps bare array access builtins inside namespaced stdlib internals") {
  primec::Expr namespacedAtCall;
  namespacedAtCall.kind = primec::Expr::Kind::Call;
  namespacedAtCall.name = "at";
  namespacedAtCall.namespacePrefix = "/std/collections/soa_storage";

  std::string builtin;
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedAtCall, builtin));
  CHECK(builtin == "at");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      namespacedAtCall, builtin));
  CHECK(builtin == "at");

  primec::Expr rootedAtCall;
  rootedAtCall.kind = primec::Expr::Kind::Call;
  rootedAtCall.name = "/std/collections/soa_storage/at";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedAtCall, builtin));
  CHECK(builtin == "at");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedAtCall, builtin));
  CHECK(builtin == "at");

  primec::Expr namespacedAtUnsafeCall;
  namespacedAtUnsafeCall.kind = primec::Expr::Kind::Call;
  namespacedAtUnsafeCall.name = "at_unsafe";
  namespacedAtUnsafeCall.namespacePrefix = "/std/collections/soa_storage";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      namespacedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");

  primec::Expr rootedAtUnsafeCall;
  rootedAtUnsafeCall.kind = primec::Expr::Kind::Call;
  rootedAtUnsafeCall.name =
      "/std/collections/soa_storage/at_unsafe";

  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedAtUnsafeCall, builtin));
  CHECK(builtin == "at_unsafe");

  primec::Expr specializedInternalSoaColumnAccessCall;
  specializedInternalSoaColumnAccessCall.kind = primec::Expr::Kind::Call;
  specializedInternalSoaColumnAccessCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/at";

  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      specializedInternalSoaColumnAccessCall, builtin));
  CHECK(builtin == "at");
}

TEST_CASE("simple-call helpers keep rooted and namespaced internal soa storage bare builtins") {
  primec::Expr rootedAssignCall;
  rootedAssignCall.kind = primec::Expr::Kind::Call;
  rootedAssignCall.name = "/std/collections/soa_storage/assign";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedAssignCall, "assign"));
  CHECK(primec::emitter::isSimpleCallName(rootedAssignCall, "assign"));

  primec::Expr rootedIfCall;
  rootedIfCall.kind = primec::Expr::Kind::Call;
  rootedIfCall.name = "/std/collections/soa_storage/if";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedIfCall, "if"));
  CHECK(primec::emitter::isSimpleCallName(rootedIfCall, "if"));

  primec::Expr rootedTakeCall;
  rootedTakeCall.kind = primec::Expr::Kind::Call;
  rootedTakeCall.name = "/std/collections/soa_storage/take";
  CHECK(primec::ir_lowerer::isSimpleCallName(rootedTakeCall, "take"));
  CHECK(primec::emitter::isSimpleCallName(rootedTakeCall, "take"));

  auto makeNamespacedInternalSoaCall = [](const char *name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.namespacePrefix = "/std/collections/soa_storage";
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
      "/std/collections/soa_storage/SoaColumn__tabcdef01/assign";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedAssignCall, "assign"));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedIfCall, "if"));

  primec::Expr generatedTakeCall;
  generatedTakeCall.kind = primec::Expr::Kind::Call;
  generatedTakeCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/take";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedTakeCall, "take"));

  primec::Expr generatedLocationCall;
  generatedLocationCall.kind = primec::Expr::Kind::Call;
  generatedLocationCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/location";
  CHECK(primec::ir_lowerer::isSimpleCallName(generatedLocationCall, "location"));

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/dereference";
  CHECK(primec::ir_lowerer::isSimpleCallName(
      generatedDereferenceCall, "dereference"));
}

TEST_CASE("emitter builtin assign keeps internal soa storage helper paths") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr rootedAssignCall;
  rootedAssignCall.kind = primec::Expr::Kind::Call;
  rootedAssignCall.name = "/std/collections/soa_storage/assign";
  CHECK(primec::emitter::isBuiltinAssign(rootedAssignCall, nameMap));

  primec::Expr namespacedAssignCall;
  namespacedAssignCall.kind = primec::Expr::Kind::Call;
  namespacedAssignCall.name = "assign";
  namespacedAssignCall.namespacePrefix = "/std/collections/soa_storage";
  CHECK(primec::emitter::isBuiltinAssign(namespacedAssignCall, nameMap));
}

TEST_CASE("emitter control helpers keep internal soa storage helper paths") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr namespacedIfCall;
  namespacedIfCall.kind = primec::Expr::Kind::Call;
  namespacedIfCall.name = "if";
  namespacedIfCall.namespacePrefix = "/std/collections/soa_storage";
  CHECK(primec::emitter::isBuiltinIf(namespacedIfCall, nameMap));

  primec::Expr namespacedBlockCall;
  namespacedBlockCall.kind = primec::Expr::Kind::Call;
  namespacedBlockCall.name = "block";
  namespacedBlockCall.namespacePrefix = "/std/collections/soa_storage";
  CHECK(primec::emitter::isBuiltinBlock(namespacedBlockCall, nameMap));

  primec::Expr namespacedWhileCall;
  namespacedWhileCall.kind = primec::Expr::Kind::Call;
  namespacedWhileCall.name = "while";
  namespacedWhileCall.namespacePrefix = "/std/collections/soa_storage";
  CHECK(primec::emitter::isWhileCall(namespacedWhileCall));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::emitter::isBuiltinIf(generatedIfCall, nameMap));

  primec::Expr generatedLoopCall;
  generatedLoopCall.kind = primec::Expr::Kind::Call;
  generatedLoopCall.name =
      "/std/collections/soa_storage/SoaColumns2__tabcdef01/loop";
  CHECK(primec::emitter::isLoopCall(generatedLoopCall));

  primec::Expr generatedReturnCall;
  generatedReturnCall.kind = primec::Expr::Kind::Call;
  generatedReturnCall.name =
      "/std/collections/soa_storage/SoaColumns2__tabcdef01/return";
  CHECK(primec::emitter::isReturnCall(generatedReturnCall));
}

TEST_CASE("shared return helpers keep scoped stdlib and custom paths builtin") {
  primec::Expr namespacedBufferReturnCall;
  namespacedBufferReturnCall.kind = primec::Expr::Kind::Call;
  namespacedBufferReturnCall.name = "return";
  namespacedBufferReturnCall.namespacePrefix =
      "/std/collections/buffer_checked";
  CHECK(primec::ir_lowerer::isReturnCall(namespacedBufferReturnCall));
  CHECK(primec::emitter::isReturnCall(namespacedBufferReturnCall));

  primec::Expr namespacedSoaVectorReturnCall;
  namespacedSoaVectorReturnCall.kind = primec::Expr::Kind::Call;
  namespacedSoaVectorReturnCall.name = "return";
  namespacedSoaVectorReturnCall.namespacePrefix =
      "/std/collections/experimental_soa";
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
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isWhileCall(namespacedSoaWhileCall));
  CHECK(primec::emitter::isWhileCall(namespacedSoaWhileCall));

  primec::Expr namespacedSoaDoCall;
  namespacedSoaDoCall.kind = primec::Expr::Kind::Call;
  namespacedSoaDoCall.name = "do";
  namespacedSoaDoCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaDoCall, "do"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaDoCall, "do"));

  primec::Expr namespacedSoaAssignCall;
  namespacedSoaAssignCall.kind = primec::Expr::Kind::Call;
  namespacedSoaAssignCall.name = "assign";
  namespacedSoaAssignCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaAssignCall, "assign"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaAssignCall, "assign"));

  primec::Expr namespacedSoaIncrementCall;
  namespacedSoaIncrementCall.kind = primec::Expr::Kind::Call;
  namespacedSoaIncrementCall.name = "increment";
  namespacedSoaIncrementCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaIncrementCall, "increment"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaIncrementCall, "increment"));

  primec::Expr namespacedSoaLocationCall;
  namespacedSoaLocationCall.kind = primec::Expr::Kind::Call;
  namespacedSoaLocationCall.name = "location";
  namespacedSoaLocationCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaLocationCall, "location"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaLocationCall, "location"));

  primec::Expr namespacedSoaDereferenceCall;
  namespacedSoaDereferenceCall.kind = primec::Expr::Kind::Call;
  namespacedSoaDereferenceCall.name = "dereference";
  namespacedSoaDereferenceCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaDereferenceCall, "dereference"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaDereferenceCall, "dereference"));

  primec::Expr namespacedSoaGetCall;
  namespacedSoaGetCall.kind = primec::Expr::Kind::Call;
  namespacedSoaGetCall.name = "get";
  namespacedSoaGetCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaGetCall, "get"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaGetCall, "get"));

  primec::Expr namespacedSoaRefCall;
  namespacedSoaRefCall.kind = primec::Expr::Kind::Call;
  namespacedSoaRefCall.name = "ref";
  namespacedSoaRefCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaRefCall, "ref"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaRefCall, "ref"));

  primec::Expr namespacedSoaGetRefCall;
  namespacedSoaGetRefCall.kind = primec::Expr::Kind::Call;
  namespacedSoaGetRefCall.name = "get_ref";
  namespacedSoaGetRefCall.namespacePrefix =
      "/std/collections/soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaGetRefCall, "get_ref"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaGetRefCall, "get_ref"));

  primec::Expr namespacedSoaCountRefCall;
  namespacedSoaCountRefCall.kind = primec::Expr::Kind::Call;
  namespacedSoaCountRefCall.name = "count_ref";
  namespacedSoaCountRefCall.namespacePrefix =
      "/std/collections/soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaCountRefCall, "count_ref"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaCountRefCall, "count_ref"));

  primec::Expr namespacedSoaCountCall;
  namespacedSoaCountCall.kind = primec::Expr::Kind::Call;
  namespacedSoaCountCall.name = "count";
  namespacedSoaCountCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaCountCall, "count"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaCountCall, "count"));

  primec::Expr namespacedSoaToAosCall;
  namespacedSoaToAosCall.kind = primec::Expr::Kind::Call;
  namespacedSoaToAosCall.name = "to_aos";
  namespacedSoaToAosCall.namespacePrefix =
      "/std/collections/experimental_soa_conversions";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaToAosCall, "to_aos"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaToAosCall, "to_aos"));

  primec::Expr namespacedSoaToAosRefCall;
  namespacedSoaToAosRefCall.kind = primec::Expr::Kind::Call;
  namespacedSoaToAosRefCall.name = "to_aos_ref";
  namespacedSoaToAosRefCall.namespacePrefix =
      "/std/collections/soa_conversions";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaToAosRefCall, "to_aos_ref"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaToAosRefCall, "to_aos_ref"));

  primec::Expr namespacedSoaRefRefCall;
  namespacedSoaRefRefCall.kind = primec::Expr::Kind::Call;
  namespacedSoaRefRefCall.name = "ref_ref";
  namespacedSoaRefRefCall.namespacePrefix =
      "/std/collections/soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaRefRefCall, "ref_ref"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaRefRefCall, "ref_ref"));

  primec::Expr namespacedSoaPushCall;
  namespacedSoaPushCall.kind = primec::Expr::Kind::Call;
  namespacedSoaPushCall.name = "push";
  namespacedSoaPushCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaPushCall, "push"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaPushCall, "push"));

  primec::Expr namespacedSoaReserveCall;
  namespacedSoaReserveCall.kind = primec::Expr::Kind::Call;
  namespacedSoaReserveCall.name = "reserve";
  namespacedSoaReserveCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaReserveCall, "reserve"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaReserveCall, "reserve"));

  primec::Expr namespacedSoaPlusCall;
  namespacedSoaPlusCall.kind = primec::Expr::Kind::Call;
  namespacedSoaPlusCall.name = "plus";
  namespacedSoaPlusCall.namespacePrefix =
      "/std/collections/buffer_checked";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaPlusCall, "plus"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaPlusCall, "plus"));

  primec::Expr namespacedSoaLessThanCall;
  namespacedSoaLessThanCall.kind = primec::Expr::Kind::Call;
  namespacedSoaLessThanCall.name = "less_than";
  namespacedSoaLessThanCall.namespacePrefix =
      "/std/collections/experimental_soa";
  CHECK(primec::ir_lowerer::isSimpleCallName(namespacedSoaLessThanCall, "less_than"));
  CHECK(primec::emitter::isSimpleCallName(namespacedSoaLessThanCall, "less_than"));
}

TEST_CASE("emitter helpers keep generated internal soa helper paths builtin") {
  std::unordered_map<std::string, std::string> nameMap;

  primec::Expr generatedAssignCall;
  generatedAssignCall.kind = primec::Expr::Kind::Call;
  generatedAssignCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/assign";
  CHECK(primec::emitter::isBuiltinAssign(generatedAssignCall, nameMap));

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/if";
  CHECK(primec::emitter::isSimpleCallName(generatedIfCall, "if"));

  primec::Expr generatedTakeCall;
  generatedTakeCall.kind = primec::Expr::Kind::Call;
  generatedTakeCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/take";
  CHECK(primec::emitter::isSimpleCallName(generatedTakeCall, "take"));

  primec::Expr generatedDereferenceCall;
  generatedDereferenceCall.kind = primec::Expr::Kind::Call;
  generatedDereferenceCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/dereference";
  CHECK(primec::emitter::isSimpleCallName(
      generatedDereferenceCall, "dereference"));
  char pointerOp = '\0';
  CHECK(primec::emitter::getBuiltinPointerOperator(
      generatedDereferenceCall, pointerOp));
  CHECK(pointerOp == '*');

  primec::Expr generatedLocationCall;
  generatedLocationCall.kind = primec::Expr::Kind::Call;
  generatedLocationCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/location";
  CHECK(primec::emitter::isSimpleCallName(
      generatedLocationCall, "location"));

  primec::Expr generatedPlusCall;
  generatedPlusCall.kind = primec::Expr::Kind::Call;
  generatedPlusCall.name =
      "/std/collections/soa_storage/SoaColumns2__tabcdef01/plus";
  char op = '\0';
  CHECK(primec::emitter::getBuiltinOperator(generatedPlusCall, op));
  CHECK(op == '+');

  primec::Expr generatedLessThanCall;
  generatedLessThanCall.kind = primec::Expr::Kind::Call;
  generatedLessThanCall.name =
      "/std/collections/soa_storage/SoaColumns2__tabcdef01/less_than";
  const char *comparison = nullptr;
  CHECK(primec::emitter::getBuiltinComparison(
      generatedLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr generatedIncrementCall;
  generatedIncrementCall.kind = primec::Expr::Kind::Call;
  generatedIncrementCall.name =
      "/std/collections/soa_storage/SoaColumn__tabcdef01/increment";
  std::string mutation;
  CHECK(primec::emitter::getBuiltinMutationName(
      generatedIncrementCall, mutation));
  CHECK(mutation == "increment");
}

TEST_CASE("ir lowerer helper accepts parser-shaped canonical map entry constructors as builtin map") {
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
  CHECK(primec::ir_lowerer::getBuiltinCollectionName(canonicalMapCall, builtin));
  CHECK(builtin == "map");
  builtin.clear();
  CHECK_FALSE(primec::emitter::getBuiltinCollectionName(canonicalMapCall, builtin));

  primec::Expr experimentalEntryCall = entryCall;
  experimentalEntryCall.namespacePrefix = "/std/collections/experimental_map";

  primec::Expr experimentalMapCall = canonicalMapCall;
  experimentalMapCall.args = {experimentalEntryCall};

  CHECK(primec::ir_lowerer::getBuiltinCollectionName(experimentalMapCall, builtin));
  CHECK(builtin == "map");
  builtin.clear();
  CHECK(primec::emitter::getBuiltinCollectionName(experimentalMapCall, builtin));
  CHECK(builtin == "map");
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
  CHECK(primec::semantics::isExplicitRemovedCollectionCallAlias("/soa/count_ref"));
  CHECK_FALSE(primec::semantics::isExplicitRemovedCollectionCallAlias("/vector/push"));

  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias("/array", "/array/push"));
  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias(
      "/vector", "/std/collections/vector/push"));
  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias(
      "/soa", "/soa/count_ref"));
  CHECK(primec::semantics::isExplicitRemovedCollectionMethodAlias(
      "/std/collections/soa", "/std/collections/soa/get_ref"));
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

  primec::Expr namespaceSplitCountCall;
  namespaceSplitCountCall.kind = primec::Expr::Kind::Call;
  namespaceSplitCountCall.namespacePrefix = "/std/collections/vector";
  namespaceSplitCountCall.name = "count";
  collectionName.clear();
  helperName.clear();
  CHECK(primec::semantics::getNamespacedCollectionHelperName(
      namespaceSplitCountCall, collectionName, helperName));
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

  // TODO-4726 (still open): getBuiltinArrayAccessName does not recognize
  // this rooted canonical spelling (verified current behavior: returns
  // false, helperName stays unset).
  std::string helperName;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      canonicalAccessCall, helperName));
  CHECK(helperName.empty());

  primec::Expr removedAliasCall = canonicalAccessCall;
  removedAliasCall.name = "/vector/at";
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      removedAliasCall, helperName));
  CHECK(helperName.empty());
}

TEST_CASE("ir lowerer access helper classifies namespaced access helpers") {
  primec::Expr namespacedVectorAccessCall;
  namespacedVectorAccessCall.kind = primec::Expr::Kind::Call;
  namespacedVectorAccessCall.namespacePrefix = "/std/collections/vector";
  namespacedVectorAccessCall.name = "at";

  std::string helperName;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedVectorAccessCall, helperName));
  CHECK(helperName.empty());

  // TODO-4726 (still open): namespacePrefix-qualified canonical access
  // helper spellings are not recognized either (verified current behavior).
  primec::Expr namespacedMapAccessCall;
  namespacedMapAccessCall.kind = primec::Expr::Kind::Call;
  namespacedMapAccessCall.namespacePrefix = "/std/collections/map";
  namespacedMapAccessCall.name = "at_unsafe";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedMapAccessCall, helperName));
  CHECK(helperName.empty());

  primec::Expr namespacedExperimentalVectorAccessCall;
  namespacedExperimentalVectorAccessCall.kind = primec::Expr::Kind::Call;
  namespacedExperimentalVectorAccessCall.namespacePrefix =
      "/std/collections/experimental_vector";
  namespacedExperimentalVectorAccessCall.name = "at";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedExperimentalVectorAccessCall, helperName));
  CHECK(helperName == "at");

  // TODO-4726 (still open): same gap for the rooted/namespaced canonical
  // soa access-verb spellings (verified current behavior).
  primec::Expr rootedCanonicalSoaGetCall;
  rootedCanonicalSoaGetCall.kind = primec::Expr::Kind::Call;
  rootedCanonicalSoaGetCall.name = "/std/collections/soa/get";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedCanonicalSoaGetCall, helperName));
  CHECK(helperName.empty());

  primec::Expr namespacedCanonicalSoaGetRefCall;
  namespacedCanonicalSoaGetRefCall.kind = primec::Expr::Kind::Call;
  namespacedCanonicalSoaGetRefCall.namespacePrefix = "/std/collections/soa";
  namespacedCanonicalSoaGetRefCall.name = "get_ref";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedCanonicalSoaGetRefCall, helperName));
  CHECK(helperName.empty());

  // TODO-4726 (still open): same gap for the rooted bare /soa/get
  // spelling (verified current behavior).
  primec::Expr rootedLegacySoaGetCall;
  rootedLegacySoaGetCall.kind = primec::Expr::Kind::Call;
  rootedLegacySoaGetCall.name = "/soa/get";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedLegacySoaGetCall, helperName));
  CHECK(helperName.empty());

  primec::Expr specializedExperimentalVectorMethodAccessCall;
  specializedExperimentalVectorMethodAccessCall.kind = primec::Expr::Kind::Call;
  specializedExperimentalVectorMethodAccessCall.namespacePrefix =
      "/std/collections/vector/Vector__t12345678";
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

  primec::Expr rootedLegacyVectorAccessCall;
  rootedLegacyVectorAccessCall.kind = primec::Expr::Kind::Call;
  rootedLegacyVectorAccessCall.name = "/std/collections/vectorAt__t12345678";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedLegacyVectorAccessCall, helperName));
  CHECK(helperName == "at");
  helperName.clear();
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedLegacyVectorAccessCall, helperName));
  CHECK(helperName == "at");

  primec::Expr rootedLegacyVectorUnsafeAccessCall;
  rootedLegacyVectorUnsafeAccessCall.kind = primec::Expr::Kind::Call;
  rootedLegacyVectorUnsafeAccessCall.name =
      "/std/collections/experimental_vector/vectorAtUnsafe__t12345678";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedLegacyVectorUnsafeAccessCall, helperName));
  CHECK(helperName == "at_unsafe");
  helperName.clear();
  CHECK(primec::emitter::getBuiltinArrayAccessNameLocal(
      rootedLegacyVectorUnsafeAccessCall, helperName));
  CHECK(helperName == "at_unsafe");

  primec::Expr removedExperimentalMapVectorAccessAliasCall;
  removedExperimentalMapVectorAccessAliasCall.kind = primec::Expr::Kind::Call;
  removedExperimentalMapVectorAccessAliasCall.name =
      "/std/collections/experimental_map/vectorAt__t12345678";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      removedExperimentalMapVectorAccessAliasCall, helperName));
  CHECK(helperName.empty());
  helperName.clear();
  CHECK_FALSE(primec::emitter::getBuiltinArrayAccessNameLocal(
      removedExperimentalMapVectorAccessAliasCall, helperName));
  CHECK(helperName.empty());

  // TODO-4726 (still open): same gap for this rooted-canonical (as
  // opposed to legacy-folder) generated internal vector helper spelling
  // (verified current behavior).
  primec::Expr rootedInternalVectorAccessCall;
  rootedInternalVectorAccessCall.kind = primec::Expr::Kind::Call;
  rootedInternalVectorAccessCall.name =
      "/std/collections/vector/vectorAt__t12345678";

  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(
      rootedInternalVectorAccessCall, helperName));
  CHECK(helperName.empty());

  primec::Expr namespacedExperimentalSoaStorageAccessCall;
  namespacedExperimentalSoaStorageAccessCall.kind = primec::Expr::Kind::Call;
  namespacedExperimentalSoaStorageAccessCall.namespacePrefix =
      "/std/collections/soa_storage";
  namespacedExperimentalSoaStorageAccessCall.name = "at_unsafe";

  helperName.clear();
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(
      namespacedExperimentalSoaStorageAccessCall, helperName));
  CHECK(helperName == "at_unsafe");

  primec::Expr specializedExperimentalSoaColumnAccessCall;
  specializedExperimentalSoaColumnAccessCall.kind = primec::Expr::Kind::Call;
  specializedExperimentalSoaColumnAccessCall.namespacePrefix =
      "/std/collections/soa_storage/SoaColumn__tabcdef01";
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

TEST_CASE("ir lowerer stdlib surface metadata rejects experimental map lowering helpers") {
  const auto *countMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapCount");
  CHECK(countMetadata == nullptr);

  const auto *insertMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapInsert");
  CHECK(insertMetadata == nullptr);

  const auto *atUnsafeMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_map/mapAtUnsafe");
  CHECK(atUnsafeMetadata == nullptr);

  const auto *canonicalMapMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/map/at_unsafe");
  REQUIRE(canonicalMapMetadata != nullptr);
  CHECK(canonicalMapMetadata->id == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *canonicalMapMetadata, "/std/collections/map/at_unsafe") == "at_unsafe");

  const auto *soaGetRefMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/experimental_soa/soaVectorGetRef");
  CHECK(soaGetRefMetadata == nullptr);

  const auto *publicSoaFieldViewMetadata =
      primec::findStdlibSurfaceMetadataByResolvedPath("/std/collections/soa/field_view");
  REQUIRE(publicSoaFieldViewMetadata != nullptr);
  CHECK(publicSoaFieldViewMetadata->id ==
        primec::StdlibSurfaceId::CollectionsColumnarHelpers);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *publicSoaFieldViewMetadata, "/std/collections/soa/field_view") ==
        "field_view");
}

TEST_CASE("stdlib surface metadata resolves collection helper member tokens") {
  const auto *vectorMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsManifestSurface0);
  REQUIRE(vectorMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "count") == "count");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "remove_swap") ==
        "remove_swap");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "/std/collections/vector/count") ==
        "count");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorMetadata, "/std/collections/vector/remove_swap") ==
        "remove_swap");

  const auto *vectorCtorMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsManifestSurface1);
  REQUIRE(vectorCtorMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorCtorMetadata, "vector") ==
        "vector");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorCtorMetadata, "/std/collections/vector/vector") ==
        "vector");
  CHECK(primec::resolveStdlibSurfaceMemberName(*vectorCtorMetadata, "/std/collections/vector/vector") ==
        "vector");

  const auto *mapMetadata =
      primec::findStdlibSurfaceMetadata(primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  REQUIRE(mapMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "at_unsafe_ref") ==
        "at_unsafe_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *mapMetadata, "/std/collections/map/insert_ref") ==
        "insert_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "mapAtUnsafeRef").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "Insert").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*mapMetadata, "MapInsertRef").empty());

  const auto *soaMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsColumnarHelpers);
  REQUIRE(soaMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "/std/collections/soa/count") ==
        "count");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "/std/collections/soa/to_aos") ==
        "to_aos");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "count_ref") ==
        "count_ref");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "field_view") ==
        "field_view");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "to_aos") ==
        "to_aos");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "soaVectorCountRef").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "soaVectorFieldView").empty());
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaMetadata, "soaVectorToAos").empty());

  const auto *soaCtorMetadata =
      primec::findStdlibSurfaceMetadata(primec::StdlibSurfaceId::CollectionsColumnarConstructors);
  REQUIRE(soaCtorMetadata != nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaCtorMetadata, "/std/collections/soa/soa") ==
        "soa");
  CHECK(primec::resolveStdlibSurfaceMemberName(*soaCtorMetadata, "/std/collections/soa/from_aos") ==
        "from_aos");
}

TEST_CASE("stdlib surface metadata classifies collection helper categories") {
  CHECK(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "capacity"));
  CHECK(primec::isStdlibSurfaceMemberName(
      primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id, "tryAt_ref"));
  CHECK(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsColumnarHelpers, "ref_ref"));
  CHECK_FALSE(primec::isStdlibSurfaceMemberName(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "insert"));

  CHECK(primec::isStdlibSurfaceStatementMemberName(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "push"));
  CHECK(primec::isStdlibSurfaceStatementMemberName(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "remove_swap"));
  CHECK_FALSE(primec::isStdlibSurfaceStatementMemberName(
      primec::StdlibSurfaceId::CollectionsManifestSurface0, "count"));

  const auto *mapHelperMetadata =
      primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
  REQUIRE(mapHelperMetadata != nullptr);
  auto resolvedMapHelperName = [&](std::string_view memberName) {
    return primec::resolveStdlibSurfaceMemberName(*mapHelperMetadata, memberName);
  };
  CHECK(resolvedMapHelperName("contains") == "contains");
  CHECK(resolvedMapHelperName("insert") == "insert");
  CHECK_FALSE(resolvedMapHelperName("contains_ref").empty());
  CHECK(resolvedMapHelperName("contains_ref").ends_with("_ref"));
  CHECK(resolvedMapHelperName("at_ref").ends_with("_ref"));
  CHECK_FALSE(resolvedMapHelperName("contains").ends_with("_ref"));
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
  CHECK(primec::emitter::getBuiltinMemoryName(allocCall, builtin));
  CHECK(builtin == "alloc");
  CHECK(primec::emitter::getBuiltinMemoryName(reinterpretCall, builtin));
  CHECK(builtin == "reinterpret");

  primec::Expr invalidCall;
  invalidCall.kind = primec::Expr::Kind::Call;
  invalidCall.name = "not_builtin";
  invalidCall.namespacePrefix = "/std/intrinsics/memory";
  CHECK_FALSE(primec::emitter::getBuiltinMemoryName(invalidCall, builtin));
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

TEST_CASE("ir lowerer helper keeps namespaced convert builtin tails") {
  primec::Expr namespacedConvertCall;
  namespacedConvertCall.kind = primec::Expr::Kind::Call;
  namespacedConvertCall.name = "convert";
  namespacedConvertCall.namespacePrefix = "/std/gfx/GfxError";

  CHECK(primec::ir_lowerer::getBuiltinConvertName(namespacedConvertCall));

  std::string builtin;
  CHECK(primec::emitter::getBuiltinConvertName(namespacedConvertCall, builtin));
  CHECK(builtin == "convert");
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
  rootedPlusCall.name = "/std/collections/soa_storage/plus";

  char op = '\0';
  CHECK(primec::emitter::getBuiltinOperator(rootedPlusCall, op));
  CHECK(op == '+');

  primec::Expr namespacedLessThanCall;
  namespacedLessThanCall.kind = primec::Expr::Kind::Call;
  namespacedLessThanCall.name = "less_than";
  namespacedLessThanCall.namespacePrefix = "/std/collections/soa_storage";

  const char *comparison = nullptr;
  CHECK(primec::emitter::getBuiltinComparison(namespacedLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr rootedIncrementCall;
  rootedIncrementCall.kind = primec::Expr::Kind::Call;
  rootedIncrementCall.name = "/std/collections/soa_storage/increment";

  std::string mutation;
  CHECK(primec::emitter::getBuiltinMutationName(rootedIncrementCall, mutation));
  CHECK(mutation == "increment");
}

TEST_CASE("shared helper bodies keep scoped stdlib builtins normalized") {
  primec::Expr namespacedCheckedPlusCall;
  namespacedCheckedPlusCall.kind = primec::Expr::Kind::Call;
  namespacedCheckedPlusCall.name = "plus";
  namespacedCheckedPlusCall.namespacePrefix =
      "/std/collections/buffer_checked";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getBuiltinOperatorName(
      namespacedCheckedPlusCall, builtinName));
  CHECK(builtinName == "plus");

  primec::Expr rootedCheckedPlusCall;
  rootedCheckedPlusCall.kind = primec::Expr::Kind::Call;
  rootedCheckedPlusCall.name =
      "/std/collections/buffer_checked/plus";
  CHECK(primec::ir_lowerer::getBuiltinOperatorName(
      rootedCheckedPlusCall, builtinName));
  CHECK(builtinName == "plus");

  char op = '\0';
  CHECK(primec::emitter::getBuiltinOperator(namespacedCheckedPlusCall, op));
  CHECK(op == '+');

  primec::Expr namespacedUncheckedPlusCall;
  namespacedUncheckedPlusCall.kind = primec::Expr::Kind::Call;
  namespacedUncheckedPlusCall.name = "plus";
  namespacedUncheckedPlusCall.namespacePrefix =
      "/std/collections/buffer_unchecked";

  CHECK(primec::ir_lowerer::getBuiltinOperatorName(
      namespacedUncheckedPlusCall, builtinName));
  CHECK(builtinName == "plus");
  CHECK(primec::emitter::getBuiltinOperator(namespacedUncheckedPlusCall, op));
  CHECK(op == '+');

  primec::Expr namespacedSoaLessThanCall;
  namespacedSoaLessThanCall.kind = primec::Expr::Kind::Call;
  namespacedSoaLessThanCall.name = "less_than";
  namespacedSoaLessThanCall.namespacePrefix =
      "/std/collections/experimental_soa";

  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      namespacedSoaLessThanCall, builtinName));
  CHECK(builtinName == "less_than");

  primec::Expr rootedSoaLessThanCall;
  rootedSoaLessThanCall.kind = primec::Expr::Kind::Call;
  rootedSoaLessThanCall.name =
      "/std/collections/experimental_soa/less_than";
  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      rootedSoaLessThanCall, builtinName));
  CHECK(builtinName == "less_than");

  const char *comparison = nullptr;
  CHECK(primec::emitter::getBuiltinComparison(
      namespacedSoaLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr namespacedSoaIncrementCall;
  namespacedSoaIncrementCall.kind = primec::Expr::Kind::Call;
  namespacedSoaIncrementCall.name = "increment";
  namespacedSoaIncrementCall.namespacePrefix =
      "/std/collections/experimental_soa";

  std::string mutation;
  CHECK(primec::emitter::getBuiltinMutationName(
      namespacedSoaIncrementCall, mutation));
  CHECK(mutation == "increment");

  primec::Expr namespacedSoaMoveCall;
  namespacedSoaMoveCall.kind = primec::Expr::Kind::Call;
  namespacedSoaMoveCall.name = "move";
  namespacedSoaMoveCall.namespacePrefix =
      "/std/collections/experimental_soa";

  CHECK(primec::ir_lowerer::isSimpleCallName(
      namespacedSoaMoveCall, "move"));
  CHECK(primec::emitter::isSimpleCallName(
      namespacedSoaMoveCall, "move"));

  primec::Expr namespacedSoaConversionsLessThanCall;
  namespacedSoaConversionsLessThanCall.kind = primec::Expr::Kind::Call;
  namespacedSoaConversionsLessThanCall.name = "less_than";
  namespacedSoaConversionsLessThanCall.namespacePrefix =
      "/std/collections/experimental_soa_conversions";

  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      namespacedSoaConversionsLessThanCall, builtinName));
  CHECK(builtinName == "less_than");
  CHECK(primec::emitter::getBuiltinComparison(
      namespacedSoaConversionsLessThanCall, comparison));
  CHECK(std::string(comparison) == "<");

  primec::Expr namespacedCanonicalSoaConversionsIncrementCall;
  namespacedCanonicalSoaConversionsIncrementCall.kind =
      primec::Expr::Kind::Call;
  namespacedCanonicalSoaConversionsIncrementCall.name = "increment";
  namespacedCanonicalSoaConversionsIncrementCall.namespacePrefix =
      "/std/collections/soa_conversions";

  CHECK(primec::ir_lowerer::isSimpleCallName(
      namespacedCanonicalSoaConversionsIncrementCall, "increment"));
  CHECK(primec::emitter::isSimpleCallName(
      namespacedCanonicalSoaConversionsIncrementCall, "increment"));
  CHECK(primec::emitter::getBuiltinMutationName(
      namespacedCanonicalSoaConversionsIncrementCall, mutation));
  CHECK(mutation == "increment");

  primec::Expr namespacedUiMultiplyCall;
  namespacedUiMultiplyCall.kind = primec::Expr::Kind::Call;
  namespacedUiMultiplyCall.name = "multiply";
  namespacedUiMultiplyCall.namespacePrefix = "/std/ui";

  CHECK(primec::ir_lowerer::getBuiltinOperatorName(
      namespacedUiMultiplyCall, builtinName));
  CHECK(builtinName == "multiply");
  CHECK(primec::emitter::getBuiltinOperator(
      namespacedUiMultiplyCall, op));
  CHECK(op == '*');

  primec::Expr namespacedContainerErrorMultiplyCall;
  namespacedContainerErrorMultiplyCall.kind = primec::Expr::Kind::Call;
  namespacedContainerErrorMultiplyCall.name = "multiply";
  namespacedContainerErrorMultiplyCall.namespacePrefix =
      "/std/collections/ContainerError";

  CHECK(primec::ir_lowerer::getBuiltinOperatorName(
      namespacedContainerErrorMultiplyCall, builtinName));
  CHECK(builtinName == "multiply");
  CHECK(primec::emitter::getBuiltinOperator(
      namespacedContainerErrorMultiplyCall, op));
  CHECK(op == '*');

  primec::Expr namespacedExperimentalVectorOrCall;
  namespacedExperimentalVectorOrCall.kind = primec::Expr::Kind::Call;
  namespacedExperimentalVectorOrCall.name = "or";
  namespacedExperimentalVectorOrCall.namespacePrefix =
      "/std/collections/experimental_vector";

  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      namespacedExperimentalVectorOrCall, builtinName));
  CHECK(builtinName == "or");
  CHECK(primec::emitter::getBuiltinComparison(
      namespacedExperimentalVectorOrCall, comparison));
  CHECK(std::string(comparison) == "||");

  primec::Expr namespacedFileErrorEqualCall;
  namespacedFileErrorEqualCall.kind = primec::Expr::Kind::Call;
  namespacedFileErrorEqualCall.name = "equal";
  namespacedFileErrorEqualCall.namespacePrefix = "/std/file/FileError";

  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      namespacedFileErrorEqualCall, builtinName));
  CHECK(builtinName == "equal");
  CHECK(primec::emitter::getBuiltinComparison(
      namespacedFileErrorEqualCall, comparison));
  CHECK(std::string(comparison) == "==");

  primec::Expr rootedFileErrorEqualCall;
  rootedFileErrorEqualCall.kind = primec::Expr::Kind::Call;
  rootedFileErrorEqualCall.name = "/std/file/FileError/equal";

  CHECK(primec::ir_lowerer::getBuiltinComparisonName(
      rootedFileErrorEqualCall, builtinName));
  CHECK(builtinName == "equal");
  CHECK(primec::emitter::getBuiltinComparison(
      rootedFileErrorEqualCall, comparison));
  CHECK(std::string(comparison) == "==");

  primec::Expr namespacedImageTryCall;
  namespacedImageTryCall.kind = primec::Expr::Kind::Call;
  namespacedImageTryCall.name = "try";
  namespacedImageTryCall.namespacePrefix = "/std/image/png";

  CHECK(primec::ir_lowerer::isSimpleCallName(
      namespacedImageTryCall, "try"));
  CHECK(primec::emitter::isSimpleCallName(
      namespacedImageTryCall, "try"));

  primec::Expr namespacedImageConvertCall;
  namespacedImageConvertCall.kind = primec::Expr::Kind::Call;
  namespacedImageConvertCall.name = "convert";
  namespacedImageConvertCall.namespacePrefix = "/std/image";

  CHECK(primec::ir_lowerer::getBuiltinConvertName(
      namespacedImageConvertCall));
  CHECK(primec::emitter::getBuiltinConvertName(
      namespacedImageConvertCall, builtinName));
  CHECK(builtinName == "convert");

  primec::Expr arbitraryImageHelperCall;
  arbitraryImageHelperCall.kind = primec::Expr::Kind::Call;
  arbitraryImageHelperCall.name = "/std/image/work";

  CHECK_FALSE(primec::ir_lowerer::isSimpleCallName(
      arbitraryImageHelperCall, "work"));
  CHECK_FALSE(primec::emitter::isSimpleCallName(
      arbitraryImageHelperCall, "work"));
  CHECK_FALSE(primec::ir_lowerer::getBuiltinOperatorName(
      arbitraryImageHelperCall, builtinName));
  CHECK_FALSE(primec::emitter::getBuiltinOperator(
      arbitraryImageHelperCall, op));
  CHECK_FALSE(primec::ir_lowerer::getBuiltinConvertName(
      arbitraryImageHelperCall));
  CHECK_FALSE(primec::emitter::getBuiltinConvertName(
      arbitraryImageHelperCall, builtinName));
}

TEST_SUITE_END();
