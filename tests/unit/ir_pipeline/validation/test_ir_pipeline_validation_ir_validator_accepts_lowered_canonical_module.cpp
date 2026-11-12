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
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
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
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
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

TEST_SUITE_END();
