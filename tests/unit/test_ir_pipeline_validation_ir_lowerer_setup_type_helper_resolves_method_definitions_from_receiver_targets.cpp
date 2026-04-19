#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper resolves method definitions from receiver targets") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  primec::Definition vectorCountDef;
  vectorCountDef.fullPath = "/vector/count";
  primec::Definition stdCountDef;
  stdCountDef.fullPath = "/std/collections/vector/count";
  primec::Definition vectorCapacityDef;
  vectorCapacityDef.fullPath = "/vector/capacity";
  primec::Definition stdCapacityDef;
  stdCapacityDef.fullPath = "/std/collections/vector/capacity";
  primec::Definition vectorAtDef;
  vectorAtDef.fullPath = "/vector/at";
  primec::Definition stdAtDef;
  stdAtDef.fullPath = "/std/collections/vector/at";
  primec::Definition vectorAtUnsafeDef;
  vectorAtUnsafeDef.fullPath = "/vector/at_unsafe";
  primec::Definition stdAtUnsafeDef;
  stdAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  primec::Definition mapCountDef;
  mapCountDef.fullPath = "/map/count";
  primec::Definition stdMapCountDef;
  stdMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition mapAtDef;
  mapAtDef.fullPath = "/map/at";
  primec::Definition stdMapAtDef;
  stdMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition bufferCountDef;
  bufferCountDef.fullPath = "/std/gfx/Buffer/count";
  primec::Definition bufferEmptyDef;
  bufferEmptyDef.fullPath = "/std/gfx/Buffer/empty";
  primec::Definition structMethodDef;
  structMethodDef.fullPath = "/pkg/Ctor/length";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
      {"/vector/count", &vectorCountDef},
      {"/std/collections/vector/count", &stdCountDef},
      {"/vector/capacity", &vectorCapacityDef},
      {"/std/collections/vector/capacity", &stdCapacityDef},
      {"/vector/at", &vectorAtDef},
      {"/std/collections/vector/at", &stdAtDef},
      {"/vector/at_unsafe", &vectorAtUnsafeDef},
      {"/std/collections/vector/at_unsafe", &stdAtUnsafeDef},
      {"/map/count", &mapCountDef},
      {"/std/collections/map/count", &stdMapCountDef},
      {"/map/at", &mapAtDef},
      {"/std/collections/map/at", &stdMapAtDef},
      {"/std/gfx/Buffer/count", &bufferCountDef},
      {"/std/gfx/Buffer/empty", &bufferEmptyDef},
      {"/pkg/Ctor/length", &structMethodDef},
  };

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("count", "array", "", defMap, error) ==
        &arrayCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("length", "", "/pkg/Ctor", defMap, error) ==
        &structMethodDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "vector", "", defMap, error) == &stdCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "std/collections/vector", "", defMap, error) == &stdCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "capacity", "vector", "", defMap, error) == &stdCapacityDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "capacity", "std/collections/vector", "", defMap, error) == &stdCapacityDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "vector", "", defMap, error) == &stdAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "std/collections/vector", "", defMap, error) == &stdAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at_unsafe", "vector", "", defMap, error) == &stdAtUnsafeDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at_unsafe", "std/collections/vector", "", defMap, error) == &stdAtUnsafeDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "map", "", defMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "std/collections/map", "", defMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "/map", "", defMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "/std/collections/map", "", defMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "", "std/collections/map", defMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "std/collections/map", "", defMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "/map", "", defMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "/std/collections/map", "", defMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "", "std/collections/map", defMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  const std::unordered_map<std::string, const primec::Definition *> canonicalMapDefMap = {
      {"/std/collections/map/count", &stdMapCountDef},
      {"/std/collections/map/at", &stdMapAtDef},
  };
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "std/collections/map", "", canonicalMapDefMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "/std/collections/map", "", canonicalMapDefMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "", "std/collections/map", canonicalMapDefMap, error) == &stdMapCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "std/collections/map", "", canonicalMapDefMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "/std/collections/map", "", canonicalMapDefMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "at", "", "std/collections/map", canonicalMapDefMap, error) == &stdMapAtDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "count", "Buffer", "", defMap, error) == &bufferCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "empty", "std/gfx/Buffer", "", defMap, error) == &bufferEmptyDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects rooted vector slash methods while honoring /array/count") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  primec::Definition vectorCountDef;
  vectorCountDef.fullPath = "/vector/count";
  primec::Definition stdCountDef;
  stdCountDef.fullPath = "/std/collections/vector/count";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
      {"/vector/count", &vectorCountDef},
      {"/std/collections/vector/count", &stdCountDef},
  };

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/array/count", "vector", "", defMap, error) == &arrayCountDef);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/vector/count", "vector", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /vector/count");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/vector/count", "std/collections/vector", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /vector/count");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/std/collections/vector/count", "vector", "", defMap, error) == &stdCountDef);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/std/collections/vector/count", "std/collections/vector", "", defMap, error) ==
        &stdCountDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects slash-path map helpers on map receivers") {
  primec::Definition mapAtDef;
  mapAtDef.fullPath = "/map/at";
  primec::Definition stdMapAtDef;
  stdMapAtDef.fullPath = "/std/collections/map/at";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/map/at", &mapAtDef},
      {"/std/collections/map/at", &stdMapAtDef},
  };

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/map/at", "map", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /map/at");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/std/collections/map/at", "map", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /std/collections/map/at");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/std/collections/map/at", "std/collections/map", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /std/collections/map/at");
}

TEST_CASE("ir lowerer setup type helper reports method target lookup diagnostics") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string error;

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("count", "", "", defMap, error) == nullptr);
  CHECK(error == "unknown method target for count");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("missing", "array", "", defMap, error) ==
        nullptr);
  CHECK(error == "unknown method: /array/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "missing", "", "/pkg/Ctor", defMap, error) == nullptr);
  CHECK(error == "unknown method: /pkg/Ctor/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "missing", "", "std/collections/map", defMap, error) == nullptr);
  CHECK(error == "unknown method: /std/collections/map/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/array/missing", "vector", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /vector/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "missing", "std/collections/vector", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "/std/collections/map/missing", "map", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /std/collections/map/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "missing", "/map", "", defMap, error) == nullptr);
  CHECK(error == "unknown method: /map/missing");
}

TEST_CASE("ir lowerer setup type helper rejects canonical vector receiver fallback to removed aliases") {
  primec::Definition vectorCountDef;
  vectorCountDef.fullPath = "/vector/count";
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  std::string error;

  {
    const std::unordered_map<std::string, const primec::Definition *> defMap = {
        {"/vector/count", &vectorCountDef},
    };
    CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
              "count", "std/collections/vector", "", defMap, error) == nullptr);
    CHECK(error == "unknown method: /std/collections/vector/count");
  }

  {
    error.clear();
    const std::unordered_map<std::string, const primec::Definition *> defMap = {
        {"/array/count", &arrayCountDef},
    };
    CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
              "count", "std/collections/vector", "", defMap, error) == nullptr);
    CHECK(error == "unknown method: /std/collections/vector/count");
  }
}

TEST_CASE("ir lowerer setup type helper resolves name receiver targets") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "items";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("items", itemsLocal);

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, locals, "count", typeName, resolvedTypePath, error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper reports name receiver diagnostics") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "ptr";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  const primec::ir_lowerer::LocalMap noLocals;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, noLocals, "count", typeName, resolvedTypePath, error));
  CHECK(error == "native backend does not know identifier: ptr");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerLocal;
  pointerLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  locals.emplace("ptr", pointerLocal);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, locals, "count", typeName, resolvedTypePath, error));
  CHECK(error == "unknown method target for count");
}

TEST_CASE("ir lowerer setup type helper dispatches receiver target resolution") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo nameLocal;
  nameLocal.kind = LocalInfo::Kind::Array;
  locals.emplace("items", nameLocal);

  primec::Expr nameReceiver;
  nameReceiver.kind = primec::Expr::Kind::Name;
  nameReceiver.name = "items";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(nameReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());

  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "/pkg/make";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "length",
                                                        {},
                                                        {"/pkg/Ctor"},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string("/pkg/Ctor"); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath == "/pkg/Ctor");
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Vector;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "vector");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack soa_vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Vector;
  valuesLocal.isSoaVector = true;
  valuesLocal.valueKind = ValueKind::Unknown;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "soa_vector");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack array receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Array;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack borrowed array receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Reference;
  valuesLocal.referenceToArray = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack pointer array receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToArray = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack borrowed map receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Reference;
  valuesLocal.referenceToMap = true;
  valuesLocal.mapKeyKind = ValueKind::Int32;
  valuesLocal.mapValueKind = ValueKind::Int32;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "at";
  callReceiver.args = {receiverName, indexExpr};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "map");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_SUITE_END();
