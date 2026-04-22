#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack pointer map receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToMap = true;
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

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack pointer vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToVector = true;
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

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed array receivers") {
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
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
                                                        locals,
                                                        "at",
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

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack pointer array receivers") {
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
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
                                                        locals,
                                                        "at",
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

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Reference;
  valuesLocal.referenceToVector = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
                                                        locals,
                                                        "at",
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

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack pointer vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToVector = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
                                                        locals,
                                                        "at",
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

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack pointer soa_vector receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToVector = true;
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

TEST_CASE("ir lowerer setup type helper keeps borrowed local soa_vector receivers distinct from vector") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;

  LocalInfo borrowedLocal;
  borrowedLocal.kind = LocalInfo::Kind::Reference;
  borrowedLocal.referenceToVector = true;
  borrowedLocal.isSoaVector = true;
  borrowedLocal.valueKind = ValueKind::Unknown;
  locals.emplace("borrowed", borrowedLocal);

  LocalInfo pointerLocal;
  pointerLocal.kind = LocalInfo::Kind::Pointer;
  pointerLocal.pointerToVector = true;
  pointerLocal.isSoaVector = true;
  pointerLocal.valueKind = ValueKind::Unknown;
  locals.emplace("pointerValues", pointerLocal);

  primec::Expr borrowedName;
  borrowedName.kind = primec::Expr::Kind::Name;
  borrowedName.name = "borrowed";

  primec::Expr pointerName;
  pointerName.kind = primec::Expr::Kind::Name;
  pointerName.name = "pointerValues";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(
      borrowedName,
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

  typeName.clear();
  resolvedTypePath.clear();
  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(
      pointerName,
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

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack map receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Map;
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

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack Buffer receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Buffer;
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
                                                        "empty",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack borrowed Buffer receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Reference;
  valuesLocal.referenceToBuffer = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
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
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed args-pack pointer Buffer receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo valuesLocal;
  valuesLocal.kind = LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.argsPackElementKind = LocalInfo::Kind::Pointer;
  valuesLocal.pointerToBuffer = true;
  valuesLocal.valueKind = ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessReceiver;
  accessReceiver.kind = primec::Expr::Kind::Call;
  accessReceiver.name = "at";
  accessReceiver.args = {receiverName, indexExpr};
  primec::Expr dereferenceReceiver;
  dereferenceReceiver.kind = primec::Expr::Kind::Call;
  dereferenceReceiver.name = "dereference";
  dereferenceReceiver.args = {accessReceiver};

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(dereferenceReceiver,
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
  CHECK(typeName == "Buffer");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper dispatch reports missing name receiver diagnostics") {
  primec::Expr nameReceiver;
  nameReceiver.kind = primec::Expr::Kind::Name;
  nameReceiver.name = "missing";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTarget(nameReceiver,
                                                               {},
                                                               "count",
                                                               {},
                                                               {},
                                                               [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                                 return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
                                                               },
                                                               [](const primec::Expr &) { return std::string(); },
                                                               typeName,
                                                               resolvedTypePath,
                                                               error));
  CHECK(error == "native backend does not know identifier: missing");
}

TEST_CASE("ir lowerer setup type helper selects method call receiver expression") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  const primec::Expr *receiverOut = nullptr;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  REQUIRE(receiverOut != nullptr);
  CHECK(receiverOut->kind == primec::Expr::Kind::Name);
  CHECK(receiverOut->name == "items");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper skips non-method call receiver selection") {
  primec::Expr nonMethodCall;
  nonMethodCall.kind = primec::Expr::Kind::Call;
  nonMethodCall.name = "count";
  nonMethodCall.isMethodCall = false;

  const primec::Expr *receiverOut = &nonMethodCall;
  std::string error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      nonMethodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer setup type helper reports method receiver selection diagnostics") {
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  const primec::Expr *receiverOut = &methodCall;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "method call missing receiver");

  primec::Expr entryArgsReceiver;
  entryArgsReceiver.kind = primec::Expr::Kind::Name;
  entryArgsReceiver.name = "argv";
  methodCall.args.push_back(entryArgsReceiver);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error.empty());
}


TEST_SUITE_END();
