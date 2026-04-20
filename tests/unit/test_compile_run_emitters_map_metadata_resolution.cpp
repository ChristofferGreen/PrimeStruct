#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter helper resolves direct slashless canonical map return metadata") {
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
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper resolves parser-shaped canonical map return metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "at";
  receiverCall.namespacePrefix = "/std/collections/map";

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

TEST_CASE("C++ emitter helper keeps canonical map access method unresolved without metadata") {
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

  std::string resolved;
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects removed full-path map method aliases") {
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
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  call.name = "/map/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  resolved = "/stale/path";
  call.name = "/std/collections/map/at";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects full-path map aliases without receiver type") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "/std/collections/map/count";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
}

TEST_CASE("C++ emitter helper normalizes slashless map type import alias method targets") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "tag";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "value";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "MapAtAlias";
  localTypes.emplace("value", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases = {
      {"MapAtAlias", "std/collections/map/at"},
      {"ThingAlias", "pkg/Thing"},
  };
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
      {"/std/collections/map/at", primec::emitter::ReturnKind::Int},
  };
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  receiverInfo.typeName = "std/collections/map/at";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  returnKinds.emplace("/map/at", primec::emitter::ReturnKind::Int);
  receiverInfo.typeName = "map/at";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/map/at/tag");

  receiverInfo.typeName = "ThingAlias";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "pkg/Thing/tag");
}

TEST_CASE("C++ emitter helper prefers canonical map method sugar over compatibility aliases") {
  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  primec::Expr key;
  key.kind = primec::Expr::Kind::Literal;
  key.intWidth = 32;
  key.literalValue = 1;
  primec::Expr value = key;
  value.literalValue = 2;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/map/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  primec::Definition aliasContainsDef;
  aliasContainsDef.fullPath = "/map/contains";
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasCountDef.fullPath, &aliasCountDef},
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {aliasContainsDef.fullPath, &aliasContainsDef},
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {aliasTryAtDef.fullPath, &aliasTryAtDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
  };
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
      {"/map/count_ref", primec::emitter::ReturnKind::Int},
      {"/std/collections/map/count_ref", primec::emitter::ReturnKind::Int},
      {"/map/contains_ref", primec::emitter::ReturnKind::Bool},
      {"/std/collections/map/contains_ref", primec::emitter::ReturnKind::Bool},
      {"/map/tryAt_ref", primec::emitter::ReturnKind::Int},
      {"/std/collections/map/tryAt_ref", primec::emitter::ReturnKind::Int},
      {"/map/at_ref", primec::emitter::ReturnKind::Int},
      {"/std/collections/map/at_ref", primec::emitter::ReturnKind::Int},
      {"/map/at_unsafe_ref", primec::emitter::ReturnKind::Int},
      {"/std/collections/map/at_unsafe_ref", primec::emitter::ReturnKind::Int},
      {"/map/insert", primec::emitter::ReturnKind::Void},
      {"/std/collections/map/insert", primec::emitter::ReturnKind::Void},
      {"/map/insert_ref", primec::emitter::ReturnKind::Void},
      {"/std/collections/map/insert_ref", primec::emitter::ReturnKind::Void},
  };
  std::unordered_map<std::string, std::string> returnStructs;
  auto expectResolved = [&](const char *methodName, const char *expectedPath, size_t extraArgCount) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.isMethodCall = true;
    call.name = methodName;
    call.args = {receiver};
    call.argNames = {std::nullopt};
    if (extraArgCount >= 1) {
      call.args.push_back(key);
      call.argNames.push_back(std::nullopt);
    }
    if (extraArgCount >= 2) {
      call.args.push_back(value);
      call.argNames.push_back(std::nullopt);
    }

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == expectedPath);
  };

  expectResolved("count", "/std/collections/map/count", 0);
  expectResolved("count_ref", "/std/collections/map/count_ref", 0);
  expectResolved("contains", "/std/collections/map/contains", 1);
  expectResolved("contains_ref", "/std/collections/map/contains_ref", 1);
  expectResolved("tryAt", "/std/collections/map/tryAt", 1);
  expectResolved("tryAt_ref", "/std/collections/map/tryAt_ref", 1);
  expectResolved("at_ref", "/std/collections/map/at_ref", 1);
  expectResolved("at_unsafe_ref", "/std/collections/map/at_unsafe_ref", 1);
  expectResolved("insert", "/std/collections/map/insert", 2);
  expectResolved("insert_ref", "/std/collections/map/insert_ref", 2);

  defMap.erase(canonicalCountDef.fullPath);
  defMap.erase(canonicalContainsDef.fullPath);
  defMap.erase(canonicalTryAtDef.fullPath);
  returnKinds.erase("/std/collections/map/count_ref");
  returnKinds.erase("/std/collections/map/contains_ref");
  returnKinds.erase("/std/collections/map/tryAt_ref");
  returnKinds.erase("/std/collections/map/at_ref");
  returnKinds.erase("/std/collections/map/at_unsafe_ref");
  returnKinds.erase("/std/collections/map/insert");
  returnKinds.erase("/std/collections/map/insert_ref");
  expectResolved("count", "/map/count", 0);
  expectResolved("count_ref", "/map/count_ref", 0);
  expectResolved("contains", "/map/contains", 1);
  expectResolved("contains_ref", "/map/contains_ref", 1);
  expectResolved("tryAt", "/map/tryAt", 1);
  expectResolved("tryAt_ref", "/map/tryAt_ref", 1);
  expectResolved("at_ref", "/map/at_ref", 1);
  expectResolved("at_unsafe_ref", "/map/at_unsafe_ref", 1);
  expectResolved("insert", "/map/insert", 2);
  expectResolved("insert_ref", "/map/insert_ref", 2);
}

TEST_CASE("C++ emitter helper rejects canonical metadata fallback for explicit map slash methods") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr key;
  key.kind = primec::Expr::Kind::Literal;
  key.intWidth = 32;
  key.literalValue = 1;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/std/collections/map/count", "/CanonicalCountMarker"},
      {"/std/collections/map/contains", "/CanonicalContainsMarker"},
      {"/std/collections/map/tryAt", "/CanonicalTryAtMarker"},
  };

  auto expectUnresolved = [&](const char *receiverMethodName, bool includeKeyArg) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName};
    receiverCall.argNames = {std::nullopt};
    if (includeKeyArg) {
      receiverCall.args.push_back(key);
      receiverCall.argNames.push_back(std::nullopt);
    }

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/contains", true);
  expectUnresolved("/map/tryAt", true);
}

TEST_CASE("C++ emitter helper rejects compatibility metadata fallback for canonical map slash methods") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr key;
  key.kind = primec::Expr::Kind::Literal;
  key.intWidth = 32;
  key.literalValue = 1;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/map/count", "/AliasCountMarker"},
      {"/map/contains", "/AliasContainsMarker"},
      {"/map/tryAt", "/AliasTryAtMarker"},
  };

  auto expectUnresolved = [&](const char *receiverMethodName, bool includeKeyArg) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName};
    receiverCall.argNames = {std::nullopt};
    if (includeKeyArg) {
      receiverCall.args.push_back(key);
      receiverCall.argNames.push_back(std::nullopt);
    }

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/std/collections/map/contains", true);
  expectUnresolved("/std/collections/map/tryAt", true);
}

TEST_CASE("C++ emitter helper rejects same-path map contains and tryAt slash-method metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr key;
  key.kind = primec::Expr::Kind::Literal;
  key.intWidth = 32;
  key.literalValue = 1;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/map/contains", "/AliasContainsMarker"},
      {"/std/collections/map/contains", "/CanonicalContainsMarker"},
      {"/map/tryAt", "/AliasTryAtMarker"},
      {"/std/collections/map/tryAt", "/CanonicalTryAtMarker"},
  };

  auto expectUnresolved = [&](const char *receiverMethodName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName};
    receiverCall.argNames = {std::nullopt};
    receiverCall.args.push_back(key);
    receiverCall.argNames.push_back(std::nullopt);

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/contains");
  expectUnresolved("/std/collections/map/contains");
  expectUnresolved("/map/tryAt");
  expectUnresolved("/std/collections/map/tryAt");
}

TEST_CASE("C++ emitter helper keeps same-path map access slash-method metadata precedence") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr key;
  key.kind = primec::Expr::Kind::Literal;
  key.intWidth = 32;
  key.literalValue = 1;

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/map/at", "/AliasAtMarker"},
      {"/std/collections/map/at", "/CanonicalAtMarker"},
      {"/map/at_unsafe", "/AliasAtUnsafeMarker"},
      {"/std/collections/map/at_unsafe", "/CanonicalAtUnsafeMarker"},
  };

  auto expectResolved = [&](const char *receiverMethodName, const char *expectedPath) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName, key};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == expectedPath);
  };

  expectResolved("/map/at", "/AliasAtMarker/tag");
  expectResolved("/std/collections/map/at", "/CanonicalAtMarker/tag");
  expectResolved("/map/at_unsafe", "/AliasAtUnsafeMarker/tag");
  expectResolved("/std/collections/map/at_unsafe", "/CanonicalAtUnsafeMarker/tag");
}

TEST_CASE("C++ emitter helper rejects explicit map slash-method count receiver fallback") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
      {"/map/count", primec::emitter::ReturnKind::Int},
      {"/std/collections/map/count", primec::emitter::ReturnKind::Int},
  };
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/map/count", "/AliasCountMarker"},
      {"/std/collections/map/count", "/CanonicalCountMarker"},
  };

  auto expectUnresolved = [&](const char *receiverMethodName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.isMethodCall = true;
    receiverCall.name = receiverMethodName;
    receiverCall.args = {receiverName};
    receiverCall.argNames = {std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/count");
  expectUnresolved("/std/collections/map/count");
}

TEST_CASE("C++ emitter helper rejects cross-path direct map count and contains receiver metadata fallback") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectUnresolved = [&](const char *receiverPath,
                              bool includeKeyArg,
                              std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverPath;
    receiverCall.args = {receiverName};
    receiverCall.argNames = {std::nullopt};
    if (includeKeyArg) {
      receiverCall.args.push_back(keyLiteral);
      receiverCall.argNames.push_back(std::nullopt);
    }

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, std::string> returnStructs;

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/count",
                   false,
                   {{"/std/collections/map/count", primec::emitter::ReturnKind::Int}});
  expectUnresolved("/std/collections/map/count",
                   false,
                   {{"/map/count", primec::emitter::ReturnKind::Int}});
  expectUnresolved("/map/contains",
                   true,
                   {{"/std/collections/map/contains", primec::emitter::ReturnKind::Bool}});
  expectUnresolved("/std/collections/map/contains",
                   true,
                   {{"/map/contains", primec::emitter::ReturnKind::Bool}});
}

TEST_CASE("C++ emitter helper rejects canonical return-struct fallback for direct map tryAt compatibility calls") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/tryAt";

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
  std::unordered_map<std::string, std::string> returnStructs = {
      {"/std/collections/map/tryAt", "/CanonicalTryAtMarker"},
  };

  std::string resolved = "/stale/path";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects rooted map contains and tryAt direct-call return metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectUnresolved = [&](const char *receiverPath, const std::string &markerName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverPath;
    receiverCall.args = {receiverName, keyLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {receiverPath, markerName},
    };

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/contains", "/AliasContainsMarker");
  expectUnresolved("/map/tryAt", "/AliasTryAtMarker");
  expectUnresolved("/map/contains_ref", "/AliasContainsRefMarker");
  expectUnresolved("/map/tryAt_ref", "/AliasTryAtRefMarker");
}

TEST_CASE("C++ emitter helper keeps canonical map contains and tryAt direct-call return metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectResolved = [&](const char *receiverPath, const std::string &markerName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverPath;
    receiverCall.args = {receiverName, keyLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {receiverPath, markerName},
    };

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == markerName + "/tag");
  };

  expectResolved("/std/collections/map/contains", "/CanonicalContainsMarker");
  expectResolved("/std/collections/map/tryAt", "/CanonicalTryAtMarker");
  expectResolved("/std/collections/map/contains_ref", "/CanonicalContainsRefMarker");
  expectResolved("/std/collections/map/tryAt_ref", "/CanonicalTryAtRefMarker");
}

TEST_CASE("C++ emitter helper rejects rooted map access direct-call return metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectUnresolved = [&](const char *receiverPath, const std::string &markerName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverPath;
    receiverCall.args = {receiverName, keyLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {receiverPath, markerName},
    };

    std::string resolved = "/stale/path";
    CHECK_FALSE(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved.empty());
  };

  expectUnresolved("/map/at", "/AliasAtMarker");
  expectUnresolved("/map/at_unsafe", "/AliasAtUnsafeMarker");
  expectUnresolved("/map/at_ref", "/AliasAtRefMarker");
  expectUnresolved("/map/at_unsafe_ref", "/AliasAtUnsafeRefMarker");
}

TEST_CASE("C++ emitter helper keeps canonical map access direct-call return metadata") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  auto expectResolved = [&](const char *receiverPath, const std::string &markerName) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverPath;
    receiverCall.args = {receiverName, keyLiteral};
    receiverCall.argNames = {std::nullopt, std::nullopt};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.isMethodCall = true;
    methodCall.name = "tag";
    methodCall.args = {receiverCall};
    methodCall.argNames = {std::nullopt};

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "map";
    receiverInfo.typeTemplateArg = "i32, i32";
    localTypes.emplace("values", receiverInfo);

    std::unordered_map<std::string, const primec::Definition *> defMap;
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs = {
        {receiverPath, markerName},
    };

    std::string resolved;
    CHECK(primec::emitter::resolveMethodCallPath(
        methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == markerName + "/tag");
  };

  expectResolved("/std/collections/map/at", "/CanonicalAtMarker");
  expectResolved("/std/collections/map/at_unsafe", "/CanonicalAtUnsafeMarker");
  expectResolved("/std/collections/map/at_ref", "/CanonicalAtRefMarker");
  expectResolved("/std/collections/map/at_unsafe_ref", "/CanonicalAtUnsafeRefMarker");
}

TEST_CASE("C++ emitter helper keeps cross-path vector alias access struct-return metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";

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
  returnStructs.emplace("/std/collections/vector/at", "/Marker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/i32/tag");
}

TEST_SUITE_END();
