#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct type helpers skip unsupported struct value paths") {
  auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo unresolvedInfo;
  unresolvedInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, unresolvedInfo);
  CHECK(unresolvedInfo.structTypeName.empty());

  primec::ir_lowerer::LocalInfo nonValueInfo;
  nonValueInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, nonValueInfo);
  CHECK(nonValueInfo.structTypeName.empty());

  primec::Expr pointerWithoutTemplate;
  pointerWithoutTemplate.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerWithoutTemplate.transforms.push_back(pointerType);

  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(pointerWithoutTemplate, resolveStruct, pointerInfo);
  CHECK(pointerInfo.structTypeName.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers classify supported types") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedTypeInfo info;
  std::string error;

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("array<i64>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("vector<bool>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("map<i32, f64>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "/std/collections/map<i32, bool>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("Pointer<i32>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("MyStruct", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.structPath == "/pkg/MyStruct");

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("string", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer uninitialized type helpers build type resolver") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveTypeInfo = primec::ir_lowerer::makeResolveUninitializedTypeInfo(resolveStruct, error);

  primec::ir_lowerer::UninitializedTypeInfo info;
  REQUIRE(resolveTypeInfo("MyStruct", "/pkg", info));
  CHECK(info.structPath == "/pkg/MyStruct");

  CHECK_FALSE(resolveTypeInfo("Thing<i32>", "/pkg", info));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers report diagnostics") {
  auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };

  primec::ir_lowerer::UninitializedTypeInfo info;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "array<i32, i64>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend requires array to have exactly one template argument");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "map<i32, string>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend only supports numeric/bool map values for uninitialized storage");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "/std/collections/map<i32>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend requires map to have exactly two template arguments");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "Thing<i32>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage metadata") {
  primec::ir_lowerer::LocalInfo local;
  local.isUninitializedStorage = true;
  local.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  local.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  local.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  local.structTypeName = "/pkg/MyStruct";

  primec::ir_lowerer::UninitializedTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfoFromLocalStorage(local, out));
  CHECK(out.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(out.structPath == "/pkg/MyStruct");

  local.isUninitializedStorage = false;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfoFromLocalStorage(local, out));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage candidates") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.isUninitializedStorage = true;
  localInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localInfo);
  auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  const primec::ir_lowerer::LocalInfo *localOut = nullptr;
  primec::ir_lowerer::UninitializedTypeInfo typeInfoOut;
  bool resolved = false;
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      storageExpr, locals, localOut, typeInfoOut, resolved));
  CHECK(resolved);
  CHECK(localOut == &localIt->second);
  CHECK(typeInfoOut.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(typeInfoOut.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      storageExpr, locals, localOut, typeInfoOut, resolved));
  CHECK_FALSE(resolved);
  CHECK(localOut == nullptr);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      callExpr, locals, localOut, typeInfoOut, resolved));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.isUninitializedStorage = true;
  localInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localInfo);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::ir_lowerer::UninitializedLocalStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(storageExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.local == &localIt->second);
  CHECK(out.typeInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(storageExpr, locals, out, resolved));
  CHECK_FALSE(resolved);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(callExpr, locals, out, resolved));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field template args by struct path") {
  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"skip_static", "uninitialized", "i64", true});
    fieldsOut.push_back({"wrong_type", "i64", "", false});
    fieldsOut.push_back({"slot", "uninitialized", "map<i32, f64>", false});
    return true;
  };

  std::string typeTemplateArg;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "slot", collectFields, typeTemplateArg));
  CHECK(typeTemplateArg == "map<i32, f64>");

  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Missing", "slot", collectFields, typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "missing", collectFields, typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "wrong_type", collectFields, typeTemplateArg));
}

TEST_CASE("ir lowerer uninitialized type helpers collect field bindings from index") {
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "i64", false},
      {"ignored", "i64", "", false},
  };

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");

  CHECK_FALSE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Missing", fieldsOut));
}

TEST_CASE("ir lowerer uninitialized type helpers build field binding index") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          2,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendFieldBinding("/pkg/Container", {"static_slot", "uninitialized", "i32", true});
            appendFieldBinding("/pkg/Other", {"value", "i64", "", false});
          });

  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "static_slot");
  CHECK(fieldsOut[1].isStatic);

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "value");

  const primec::ir_lowerer::UninitializedFieldBindingIndex emptyIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(4, {});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build field binding index from struct layout field index") {
  const primec::ir_lowerer::StructLayoutFieldIndex layoutFieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendStructLayoutField("/pkg/Container", {"static_slot", "uninitialized", "i32", true});
            appendStructLayoutField("/pkg/Other", {"value", "i64", "", false});
          });
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(layoutFieldIndex);

  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "static_slot");
  CHECK(fieldsOut[1].isStatic);

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "value");

  const primec::ir_lowerer::UninitializedFieldBindingIndex emptyIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex({});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled struct and uninitialized field indexes") {
  const primec::ir_lowerer::StructAndUninitializedFieldIndexes indexes =
      primec::ir_lowerer::buildStructAndUninitializedFieldIndexes(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendStructLayoutField("/pkg/Container", {"value", "i32", "", false});
            appendStructLayoutField("/pkg/Other", {"field", "uninitialized", "f64", false});
          });

  REQUIRE(indexes.structLayoutFieldIndex.size() == 2);
  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(
      indexes.structLayoutFieldIndex, "/pkg/Container", layoutFields));
  REQUIRE(layoutFields.size() == 2);
  CHECK(layoutFields[0].name == "slot");
  CHECK(layoutFields[1].name == "value");

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      indexes.uninitializedFieldBindingIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "value");

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      indexes.uninitializedFieldBindingIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "field");
  CHECK(fieldsOut[0].typeTemplateArg == "f64");

  const primec::ir_lowerer::StructAndUninitializedFieldIndexes emptyIndexes =
      primec::ir_lowerer::buildStructAndUninitializedFieldIndexes(1, {});
  CHECK(emptyIndexes.structLayoutFieldIndex.empty());
  CHECK(emptyIndexes.uninitializedFieldBindingIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled struct and uninitialized resolution setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    if (namespacePrefix == "/pkg" && typeName == "Container") {
      resolvedOut = "/pkg/Container";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };

  std::string error;
  primec::ir_lowerer::StructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildStructAndUninitializedResolutionSetup(
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      resolveExprPath,
      setup,
      error));

  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(
      setup.fieldIndexes.structLayoutFieldIndex, "/pkg/Container", layoutFields));
  REQUIRE(layoutFields.size() == 1);
  CHECK(layoutFields[0].name == "slot");

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldBindings;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      setup.fieldIndexes.uninitializedFieldBindingIndex, "/pkg/Container", fieldBindings));
  REQUIRE(fieldBindings.size() == 1);
  CHECK(fieldBindings[0].name == "slot");
  CHECK(fieldBindings[0].typeTemplateArg == "MyStruct");

  primec::ir_lowerer::StructSlotFieldInfo fieldSlot;
  REQUIRE(setup.structLayoutResolutionAdapters.structSlotResolution.resolveStructFieldSlot(
      "/pkg/Container", "slot", fieldSlot));
  CHECK(fieldSlot.name == "slot");
  CHECK(fieldSlot.slotOffset == 1);
  CHECK(fieldSlot.slotCount == 2);
  CHECK(fieldSlot.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
      "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo access;
  bool resolved = false;
  REQUIRE(setup.uninitializedResolutionAdapters.resolveUninitializedStorage(
      fieldExpr, locals, access, resolved));
  CHECK(resolved);
  CHECK(access.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(access.receiver == &receiverIt->second);
  CHECK(access.fieldSlot.slotOffset == 1);
  CHECK(access.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled setup-type, struct-type, and uninitialized setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  std::string error;
  primec::ir_lowerer::SetupTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildSetupTypeStructAndUninitializedResolutionSetup(
      structNames,
      importAliases,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      [](const primec::Expr &) { return std::string(); },
      setup,
      error));
  CHECK(error.empty());

  CHECK(setup.setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);
  CHECK(setup.setupTypeAndStructTypeAdapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) ==
        ValueKind::Int64);

  std::string resolvedStructPath;
  REQUIRE(setup.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters.resolveStructTypeName(
      "MyStruct", "/pkg", resolvedStructPath));
  CHECK(resolvedStructPath == "/pkg/MyStruct");

  primec::ir_lowerer::StructSlotFieldInfo fieldSlot;
  REQUIRE(setup.structAndUninitializedResolutionSetup.structLayoutResolutionAdapters.structSlotResolution
              .resolveStructFieldSlot("/pkg/Container", "slot", fieldSlot));
  CHECK(fieldSlot.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.structAndUninitializedResolutionSetup.uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
      "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled setup-math, setup-type, and uninitialized setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  std::string error;
  primec::ir_lowerer::SetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildSetupMathTypeStructAndUninitializedResolutionSetup(
      true,
      structNames,
      importAliases,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      [](const primec::Expr &) { return std::string(); },
      setup,
      error));
  CHECK(error.empty());

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setup.setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Expr stringBinding;
  primec::Transform stringType;
  stringType.name = "string";
  stringBinding.transforms.push_back(stringType);
  CHECK(setup.setupMathAndBindingAdapters.bindingTypeAdapters.isStringBinding(stringBinding));
  CHECK_FALSE(setup.setupMathAndBindingAdapters.bindingTypeAdapters.isFileErrorBinding(stringBinding));

  CHECK(setup.setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters.valueKindFromTypeName(
            "i32") == ValueKind::Int32);
  std::string resolvedStructPath;
  REQUIRE(setup.setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters
              .structTypeResolutionAdapters.resolveStructTypeName("MyStruct", "/pkg", resolvedStructPath));
  CHECK(resolvedStructPath == "/pkg/MyStruct");
  CHECK_FALSE(setup.setupMathAndBindingAdapters.setupMathResolvers.getMathConstantName("phi", builtinName));
}

TEST_CASE("ir lowerer uninitialized type helpers check field index struct path") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          2,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Container", {"slot", "uninitialized", "i64", false});
          });
  CHECK(primec::ir_lowerer::hasUninitializedFieldBindingsForStructPath(
      fieldIndex, "/pkg/Container"));
  CHECK_FALSE(primec::ir_lowerer::hasUninitializedFieldBindingsForStructPath(
      fieldIndex, "/pkg/Missing"));
}

TEST_CASE("ir lowerer uninitialized type helpers infer call target struct paths from field index") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto inferDefinitionStructReturnPath = [](const std::string &path) {
    if (path == "/pkg/factory") {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            ctorCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath) == "/pkg/Ctor");

  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath) == "/pkg/FromDef");

  primec::Expr unknownCall;
  unknownCall.kind = primec::Expr::Kind::Call;
  unknownCall.name = "unknown";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            unknownCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath).empty());
}


TEST_SUITE_END();
