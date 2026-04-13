#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedFieldStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveSlot,
      out,
      resolved,
      error));
  CHECK(resolved);
  CHECK(error.empty());
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveSlot,
      out,
      resolved,
      error));
  CHECK_FALSE(resolved);

  storageExpr.name = "slot";
  auto resolveMissingSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveMissingSlot,
      out,
      resolved,
      error));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      localExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      fieldExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);

  localExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      localExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK_FALSE(resolved);

  auto resolveMissingSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedStorageAccess(fieldExpr,
                                                                    locals,
                                                                    findField,
                                                                    resolveNamespacePrefix,
                                                                    resolveTypeInfo,
                                                                    resolveMissingSlot,
                                                                    out,
                                                                    resolved,
                                                                    error));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve indirect storage access") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 7;
  pointerInfo.targetsUninitializedStorage = true;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("ptr", pointerInfo);
  const auto pointerIt = locals.find("ptr");
  REQUIRE(pointerIt != locals.end());

  primec::ir_lowerer::LocalInfo structRefInfo;
  structRefInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  structRefInfo.index = 8;
  structRefInfo.targetsUninitializedStorage = true;
  structRefInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ref", structRefInfo);
  const auto refIt = locals.find("ref");
  REQUIRE(refIt != locals.end());

  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.index = 9;
  localStorage.isUninitializedStorage = true;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto slotIt = locals.find("slot");
  REQUIRE(slotIt != locals.end());

  auto findField = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveNamespacePrefix = [](const std::string &) { return std::string(); };
  auto resolveTypeInfo = [](const std::string &,
                            const std::string &,
                            primec::ir_lowerer::UninitializedTypeInfo &) { return false; };
  auto resolveSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  primec::Expr pointerName;
  pointerName.kind = primec::Expr::Kind::Name;
  pointerName.name = "ptr";
  primec::Expr derefPointer;
  derefPointer.kind = primec::Expr::Kind::Call;
  derefPointer.name = "dereference";
  derefPointer.args = {pointerName};

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      derefPointer, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Indirect);
  CHECK(out.pointer == &pointerIt->second);
  REQUIRE(out.pointerExpr != nullptr);
  CHECK(out.pointerExpr->kind == primec::Expr::Kind::Name);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr refName;
  refName.kind = primec::Expr::Kind::Name;
  refName.name = "ref";
  primec::Expr derefRef;
  derefRef.kind = primec::Expr::Kind::Call;
  derefRef.name = "dereference";
  derefRef.args = {refName};
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      derefRef, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Indirect);
  CHECK(out.pointer == &refIt->second);
  CHECK(out.typeInfo.structPath == "/pkg/Pair");

  primec::Expr slotName;
  slotName.kind = primec::Expr::Kind::Name;
  slotName.name = "slot";
  primec::Expr locationExpr;
  locationExpr.kind = primec::Expr::Kind::Call;
  locationExpr.name = "location";
  locationExpr.args = {slotName};
  primec::Expr derefLocation;
  derefLocation.kind = primec::Expr::Kind::Call;
  derefLocation.name = "dereference";
  derefLocation.args = {locationExpr};
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      derefLocation, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &slotIt->second);
}

TEST_CASE("ir lowerer uninitialized type helpers resolve indexed args-pack pointer storage access") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo argsInfo;
  argsInfo.index = 12;
  argsInfo.isArgsPack = true;
  argsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  argsInfo.targetsUninitializedStorage = true;
  argsInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", argsInfo);
  const auto valuesIt = locals.find("values");
  REQUIRE(valuesIt != locals.end());

  auto findField = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveNamespacePrefix = [](const std::string &) { return std::string(); };
  auto resolveTypeInfo = [](const std::string &,
                            const std::string &,
                            primec::ir_lowerer::UninitializedTypeInfo &) { return false; };
  auto resolveSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 0;

  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args = {valuesName, indexExpr};

  primec::Expr derefExpr;
  derefExpr.kind = primec::Expr::Kind::Call;
  derefExpr.name = "dereference";
  derefExpr.args = {atExpr};

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      derefExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Indirect);
  CHECK(out.pointer == &valuesIt->second);
  REQUIRE(out.pointerExpr != nullptr);
  CHECK(out.pointerExpr->kind == primec::Expr::Kind::Call);
  CHECK(out.pointerExpr->name == "at");
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage with field bindings") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"slot", "uninitialized", "i64", false});
    return true;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(localExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(fieldExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);

  localExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(localExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK_FALSE(resolved);
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage from definitions") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"slot", "uninitialized", "MyStruct", false});
    return true;
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitions(localExpr,
                                                                                locals,
                                                                                collectFields,
                                                                                defMap,
                                                                                resolveTypeInfo,
                                                                                resolveSlot,
                                                                                out,
                                                                                resolved,
                                                                                error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitions(fieldExpr,
                                                                                locals,
                                                                                collectFields,
                                                                                defMap,
                                                                                resolveTypeInfo,
                                                                                resolveSlot,
                                                                                out,
                                                                                resolved,
                                                                                error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage from definition field index") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };

  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitionFieldIndex(fieldExpr,
                                                                                         locals,
                                                                                         fieldIndex,
                                                                                         defMap,
                                                                                         resolveTypeInfo,
                                                                                         resolveSlot,
                                                                                         out,
                                                                                         resolved,
                                                                                         error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}


TEST_SUITE_END();
