#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter helper rejects bare vector access methods without helper metadata") {
  auto expectRejected = [&](const char *receiverMethodName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";

    primec::Expr indexLiteral;
    indexLiteral.kind = primec::Expr::Kind::Literal;
    indexLiteral.intWidth = 32;
    indexLiteral.literalValue = 0;

    receiverCall.args.push_back(receiverName);
    receiverCall.args.push_back(indexLiteral);
    receiverCall.argNames.push_back(std::nullopt);
    receiverCall.argNames.push_back(std::nullopt);

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args.push_back(receiverCall);
    methodCall.argNames.push_back(std::nullopt);

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    receiverInfo.typeTemplateArg = "i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs;

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("at");
  expectRejected("at_unsafe");
}

TEST_CASE("C++ emitter helper resolves bare vector access methods through canonical helper metadata") {
  auto expectResolved = [&](const char *receiverMethodName, const char *canonicalPath, const char *expectedPath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";

    primec::Expr indexLiteral;
    indexLiteral.kind = primec::Expr::Kind::Literal;
    indexLiteral.intWidth = 32;
    indexLiteral.literalValue = 0;

    receiverCall.args.push_back(receiverName);
    receiverCall.args.push_back(indexLiteral);
    receiverCall.argNames.push_back(std::nullopt);
    receiverCall.argNames.push_back(std::nullopt);

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args.push_back(receiverCall);
    methodCall.argNames.push_back(std::nullopt);

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    receiverInfo.typeTemplateArg = "i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {canonicalPath, "/CanonicalMarker"},
    };

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == expectedPath);
  };

  expectResolved("at", "/std/collections/vector/at", "/CanonicalMarker/tag");
  expectResolved("at_unsafe", "/std/collections/vector/at_unsafe", "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper keeps bare vector access canonical metadata precedence") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/vector/at", "/AliasMarker"},
      {"/std/collections/vector/at", "/CanonicalMarker"},
  };

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper rejects canonical metadata fallback for explicit vector count capacity aliases") {
  auto expectRejected = [&](const char *helperName, const char *canonicalPath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = std::string("/vector/") + helperName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";
    receiverCall.args.push_back(receiverName);
    receiverCall.argNames.push_back(std::nullopt);

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args.push_back(receiverCall);
    methodCall.argNames.push_back(std::nullopt);

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    receiverInfo.typeTemplateArg = "i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {canonicalPath, "/CanonicalMarker"},
    };

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("count", "/std/collections/vector/count");
  expectRejected("capacity", "/std/collections/vector/capacity");
}

TEST_CASE("C++ emitter helper keeps /vector/count while rejecting removed full-path vector aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  call.name = "/array/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  resolved = "/stale/path";
  call.name = "/vector/count";
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK_FALSE(resolved.empty());
  CHECK(resolved.find("count") != std::string::npos);

  resolved = "/stale/path";
  call.name = "/std/collections/vector/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper keeps canonical receiver metadata for namespaced map access") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/map/at", "/AliasMarker");
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper falls back to canonical map receiver metadata when alias missing") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper rejects bare map access metadata-only struct forwarding") {
  auto expectRejected = [&](const char *accessName, const char *metadataPath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = accessName;

    primec::Expr receiverName;
    receiverName.kind = primec::Expr::Kind::Name;
    receiverName.name = "values";

    primec::Expr keyLiteral;
    keyLiteral.kind = primec::Expr::Kind::Literal;
    keyLiteral.intWidth = 32;
    keyLiteral.literalValue = 1;

    receiverCall.args.push_back(receiverName);
    receiverCall.args.push_back(keyLiteral);
    receiverCall.argNames.push_back(std::nullopt);
    receiverCall.argNames.push_back(std::nullopt);

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args.push_back(receiverCall);
    methodCall.argNames.push_back(std::nullopt);

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs;
    returnStructs.emplace(metadataPath, "/CanonicalMarker");

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectRejected("at", "/std/collections/map/at");
  expectRejected("at_unsafe", "/std/collections/map/at_unsafe");
}

TEST_CASE("C++ emitter helper rejects direct map access compatibility metadata fallback") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved = "/stale/path";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper keeps slash-path map access struct forwarding") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK_FALSE(resolved.empty());
}

TEST_CASE("C++ emitter helper normalizes slashless map import receiver metadata paths") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("/std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  structTypeMap.clear();
  resolved.clear();
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
}

TEST_CASE("C++ emitter helper resolves map method via import-alias return metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper normalizes slashless map metadata lookup targets") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");
}

TEST_CASE("C++ emitter helper resolves direct slashless canonical map receiver metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");
}

TEST_SUITE_END();
