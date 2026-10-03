#include <filesystem>
#include <fstream>
#include <iterator>

#include "test_ir_pipeline_validation_callback_factories.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter collection inference keeps namespaced internal soa builtins out of string-value access inference") {
  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;

  primec::emitter::BindingInfo arrayInfo;
  arrayInfo.typeName = "array";
  arrayInfo.typeTemplateArg = "i32";
  localTypes.emplace("items", arrayInfo);

  primec::Expr namespacedCountCall;
  namespacedCountCall.kind = primec::Expr::Kind::Call;
  namespacedCountCall.name = "count";
  namespacedCountCall.namespacePrefix = "/std/collections/soa_storage";

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
  namespacedCapacityCall.namespacePrefix = "/std/collections/soa_storage";

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
  namespacedAtCall.namespacePrefix = "/std/collections/soa_storage";

  primec::Expr textName;
  textName.kind = primec::Expr::Kind::Name;
  textName.name = "text";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  namespacedAtCall.args.push_back(textName);
  namespacedAtCall.args.push_back(indexLiteral);

  CHECK_FALSE(primec::emitter::isStringValue(namespacedAtCall, localTypes));

  primec::Expr namespacedStringCountCall;
  namespacedStringCountCall.kind = primec::Expr::Kind::Call;
  namespacedStringCountCall.name = "count";
  namespacedStringCountCall.namespacePrefix = "/std/collections/soa_storage";
  namespacedStringCountCall.args.push_back(textName);

  CHECK(primec::emitter::isStringCountCall(namespacedStringCountCall, localTypes));
}

TEST_CASE("stdlib surface metadata resolves collection alias paths") {
  const auto *vectorMetadata =
      primec::findStdlibSurfaceMetadataByResolvedPath("/std/collections/vector/push");
  REQUIRE(vectorMetadata != nullptr);
  CHECK(vectorMetadata->id == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorMetadata, "/std/collections/vector/push") == "push");
  CHECK(primec::findStdlibSurfaceMetadataByResolvedPath(
            "/std/collections/experimental_vector/vectorPush") == nullptr);
  const auto *vectorWrapperMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/vectorPush");
  CHECK(vectorWrapperMetadata == nullptr);

  const auto *vectorCtorMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/vector/vector");
  REQUIRE(vectorCtorMetadata != nullptr);
  CHECK(vectorCtorMetadata->id == primec::StdlibSurfaceId::CollectionsManifestSurface1);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorCtorMetadata, "/std/collections/vector/vector") == "vector");
  CHECK(primec::findStdlibSurfaceMetadataByResolvedPath(
            "/std/collections/experimental_vector/vectorPair") == nullptr);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *vectorCtorMetadata, "/std/collections/vectorSingle__tabcd").empty());

  CHECK(primec::findStdlibSurfaceMetadataByResolvedPath("/map/map") == nullptr);
  const auto *mapCtorMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/map/map");
  REQUIRE(mapCtorMetadata != nullptr);
  CHECK(mapCtorMetadata->id == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors")->id);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *mapCtorMetadata, "/std/collections/map/map") == "map");
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *mapCtorMetadata, "/std/collections/mapPair__t1234").empty());

  const auto *mapRefMetadata = primec::findStdlibSurfaceMetadataByResolvedPath(
      "/std/collections/map/tryAt_ref");
  REQUIRE(mapRefMetadata != nullptr);
  CHECK(mapRefMetadata->id == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  CHECK(primec::resolveStdlibSurfaceMemberName(
            *mapRefMetadata, "/std/collections/map/tryAt_ref") == "tryAt_ref");
  CHECK(primec::findStdlibSurfaceMetadataByResolvedPath(
            "/std/collections/map/tryAt_ref") == mapRefMetadata);
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

TEST_CASE("semantics accepts and lowerer emits empty soa literals") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<i32>]
main() {
  [soa<Particle>] values{soa<Particle>()}
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

TEST_CASE("public soa count helper lowers through wrapper routing") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(/std/collections/vector/count(/std/collections/soa/to_aos<Particle>(values)))
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

TEST_CASE("bare soa count helper lowers through wrapper return routing compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(soaVectorNew<Particle>())
}

[return<int>]
main() {
  return(count(cloneValues()))
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

TEST_CASE("nested struct-body soa constructor-bearing helper returns lower through") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  [SoaVector<Particle>] values{Holder{}.cloneValues()}
  return(plus(plus(plus(Holder{}.cloneValues().count(), Holder{}.cloneValues().get(0i32).x),
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

TEST_CASE("bare soa get helper lowers through wrapper return routing compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  [SoaVector<Particle>, mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(get(cloneValues(), 0i32).x)
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

TEST_CASE("ir lowerer lowers non-empty soa literals through substrate helper routing") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int> effects(heap_alloc)]
main() {
  [soa<Particle>] values{soa<Particle>(Particle(1i32))}
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
  [soa<Particle>] values{soa<Particle>()}
  [Particle] valueA{get(values, 0i32)}
  [Particle] valueB{values.get(0i32)}
  [Particle] valueC{/soa/get(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  INFO(error);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK_ERROR_CONTAINS(error, "soa helper requires import /std/collections/soa/*: get");
}

TEST_CASE("root get vector receiver rejects template arguments") {
  const std::string source = R"(
[return<void>]
main() {
  [vector<i32>] values{vector<i32>()}
  [i32] valueA{get(values, 0i32)}
  [i32] valueB{values.get(0i32)}
  [i32] valueC{/soa/get(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK_ERROR_CONTAINS(error, "get requires soa target");
}

TEST_CASE("root ref helper forms stop in semantics on borrowed-view pending diagnostic") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [Particle] valueA{ref(values, 0i32)}
  [Particle] valueB{values.ref(0i32)}
  [Particle] valueC{/soa/ref(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK_ERROR_CONTAINS(error, "binding initializer validateExpr failed");
}

TEST_CASE("root ref vector receiver rejects non-soa target") {
  const std::string source = R"(
[return<void>]
main() {
  [vector<i32>] values{vector<i32>()}
  [i32] valueA{ref(values, 0i32)}
  [i32] valueB{values.ref(0i32)}
  [i32] valueC{/soa/ref(values, 0i32)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK_ERROR_CONTAINS(error, "unknown method: /std/collections/soa_vect");
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
  CHECK(error.find(
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/"
            "convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK_ERROR_CONTAINS(error, "call=/to_soa");
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
  CHECK(error.find(
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/"
            "convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK_ERROR_CONTAINS(error, "call=/to_soa");
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
  // Residual TODO-4731 gap (e): the no-import to_soa/to_aos rejection still leaks the retired family spelling.
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  INFO(error);
  CHECK_ERROR_CONTAINS(error, "unknown method: /std/collections/soa_vector");
}

TEST_CASE("semantics rejects explicit soa reserve on vector target through canonical helper path") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /soa/reserve(values, 4i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK_ERROR_CONTAINS(error, "reserve is only supported as a statement");
}

TEST_CASE("explicit soa mutators lower through canonical helper routing") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /soa/reserve(values, 4i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  // Explicit old-surface /soa/reserve without an import or shadow is rejected per the same-path contract.
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  INFO(error);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK_ERROR_CONTAINS(error, "soa helper requires import /std/collections/soa/*: reserve");
}

TEST_CASE("root to_aos bare and direct helper forms reject during semantics") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK_ERROR_CONTAINS(error, "soa helper requires import /std/collections/soa/*: to_aos");
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
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  // The root /to_aos method spelling is not part of the canonical surface; semantics rejects it now.
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  INFO(error);
  CHECK_ERROR_CONTAINS(error, "unknown method: /to_aos");
}

TEST_CASE("root to_aos method helper forms reject during semantics") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{values.to_aos()}
  [vector<Particle>] unpackedB{values./to_aos()}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK_ERROR_CONTAINS(error, "soa helper requires import /std/collections/soa/*: to_aos");
}

TEST_CASE("imported root to_aos method helper forms still need compile-pipeline helper materialization") {
  // The to_aos method desugar now reaches monomorphization, so both
  // method spellings materialize their helpers and lower successfully.
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa<Particle>] values{soa<Particle>()}
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
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("imported builtin soa bare helper forms lower through canonical helper routing") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
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
  CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
}

TEST_CASE("imported builtin soa method access forms stop in semantics on borrowed-view pending diagnostic") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
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
    CHECK((error.find("unknown method: /std/collections/soa/ref") !=
           std::string::npos ||
           error.find("field access requires struct receiver") !=
               std::string::npos));
  } else {
    CHECK(error.empty());
  }
}

TEST_CASE("imported builtin soa method mutators lower through canonical helper routing") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
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
    CHECK((error.find("unknown method: /std/collections/soa/ref") !=
           std::string::npos ||
           error.find("field access requires struct receiver") !=
               std::string::npos));
  } else {
    CHECK(error.empty());
    primec::IrLowerer lowerer;
    primec::IrModule module;
    CHECK(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
    CHECK(error.empty());
  }
}

TEST_CASE("canonical experimental wrapper to_aos slash-method lowers successfully") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa/to_aos()}
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
import /std/collections/soa/*
import /std/collections/soa/*

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
      /std/collections/soa/to_aos_ref<Particle>(
          pickBorrowed(location(values)))}
  return(count(unpacked))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  // TODO-5050 to_aos_ref gap (RESOLVED): the explicit rooted-path call now
  // resolves to the real `to_aos_ref<T>` stdlib function, so semantic
  // validation succeeds (superseding the old TODO-4731 gap (h) rejection
  // this test pinned). A standalone `--emit=vm` probe shows the program
  // still fails to compile (exit 2), but with no diagnostic text printed
  // at all - a distinct, narrower, not-yet-investigated gap somewhere
  // between semantic validation and VM code generation for this call
  // shape, out of scope for TODO-5050's routing fixes.
  CHECK(parseAndValidate(source, program, semanticProgram, error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("borrowed helper-return experimental wrapper bare conversion alias lowers through generic wildcard import") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpacked{to_aos(values)}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK_ERROR_CONTAINS(error, "argument type mismatch for /to_aos parameter values");
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
  CHECK_ERROR_CONTAINS(error, "/std/collections/soa/to_aos");
}

TEST_CASE("semantics rejects soa field-view before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa<Particle>] values) {
  values.x()
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa/field_view/x");
}

TEST_CASE("semantics rejects soa field-view call-form before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa<Particle>] values) {
  x(values)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error == "unknown method: /std/collections/soa/field_view/x");
}

TEST_CASE("semantics rejects soa get method named args before lowerer") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
/use([soa<Particle>] values) {
  values.get([index] 0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, semanticProgram, error));
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK_ERROR_CONTAINS(error, "soa helper requires import /std/collections/soa/*: get");
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
  CHECK(primec::ir_lowerer::validateNativeProgramEffects(program, "/main", {}, {}, error));
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
  CHECK_FALSE(primec::ir_lowerer::validateNativeProgramEffects(program, "/main", {}, {}, error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer vm effects unit reports vm surface diagnostics") {
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
  CHECK_FALSE(primec::ir_lowerer::validateVmProgramEffects(program, nullptr, "/main", {}, {}, error));
  CHECK(error == "vm backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer gpu effects unit reports gpu surface diagnostics") {
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
  CHECK_FALSE(primec::ir_lowerer::validateGpuProgramEffects(program, nullptr, "/main", {}, {}, error));
  CHECK(error == "gpu backend does not support effect: unsupported_effect on /main");
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
  // The lookup that validateNativeProgramEffects uses
  // (findSemanticProductCallableSummary) resolves through
  // publishedRoutingLookups.callableSummaryIndicesByPathId rather than
  // scanning callableSummaries directly - a real semantic-validation
  // publication pass populates this index, so this hand-built fixture must
  // register it too.
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
      [primec::semanticProgramInternCallTargetString(semanticProgram, "/main")] =
      semanticProgram.callableSummaries.size() - 1;

  std::string error;
  CHECK(primec::ir_lowerer::validateNativeProgramEffects(program, &semanticProgram, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit skips semantic callable summaries for sum types") {
  primec::Program program;
  primec::Definition sumDef;
  sumDef.fullPath = "/Choice";
  sumDef.transforms.push_back(primec::Transform{.name = "sum"});
  sumDef.sumVariants.push_back(primec::SumVariant{
      .name = "left",
      .hasPayload = true,
      .payloadType = "i32",
      .payloadTemplateArgs = {},
      .payloadTypeText = "i32",
      .variantIndex = 0,
  });
  program.definitions.push_back(sumDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  program.definitions.push_back(entryDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
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
      .activeEffectIds = {},
      .activeCapabilityIds = {},
  });
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
      [primec::semanticProgramInternCallTargetString(semanticProgram, "/main")] =
      semanticProgram.callableSummaries.size() - 1;

  std::string error;
  CHECK(primec::ir_lowerer::validateNativeProgramEffects(program, &semanticProgram, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer entry setup uses vm effects surface when requested") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  entryDef.transforms.push_back(badEffects);
  program.definitions.push_back(entryDef);

  const primec::Definition *entryDefOut = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerEntrySetup(program,
                                                     nullptr,
                                                     "/main",
                                                     {},
                                                     {},
                                                     primec::IrValidationTarget::Vm,
                                                     entryDefOut,
                                                     entryEffectMask,
                                                     entryCapabilityMask,
                                                     error));
  CHECK(error == "vm backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer entry setup uses native effects surface when requested") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  entryDef.transforms.push_back(badEffects);
  program.definitions.push_back(entryDef);

  const primec::Definition *entryDefOut = nullptr;
  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerEntrySetup(program,
                                                     nullptr,
                                                     "/main",
                                                     {},
                                                     {},
                                                     primec::IrValidationTarget::Native,
                                                     entryDefOut,
                                                     entryEffectMask,
                                                     entryCapabilityMask,
                                                     error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
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
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
      [primec::semanticProgramInternCallTargetString(semanticProgram, "/main")] =
      semanticProgram.callableSummaries.size() - 1;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNativeProgramEffects(program, &semanticProgram, "/main", {}, {}, error));
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
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
      [primec::semanticProgramInternCallTargetString(semanticProgram, "/main")] =
      semanticProgram.callableSummaries.size() - 1;

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
