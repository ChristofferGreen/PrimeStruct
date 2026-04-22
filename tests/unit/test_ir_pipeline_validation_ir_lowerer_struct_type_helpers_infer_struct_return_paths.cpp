#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer struct type helpers infer struct return paths") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (namespacePrefix != "/pkg") {
      return false;
    }
    if (typeName == "Foo") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    if (typeName == "Bar") {
      resolvedOut = "/pkg/Bar";
      return true;
    }
    return false;
  };
  auto inferExpr = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "factory") {
      return std::string("/pkg/FromExpr");
    }
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "mk") {
      return std::string("/pkg/FromStmt");
    }
    return std::string();
  };

  primec::Definition returnTransformDef;
  returnTransformDef.namespacePrefix = "/pkg";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Foo"};
  returnTransformDef.transforms.push_back(returnTransform);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnTransformDef, resolveStruct, inferExpr) ==
        "/pkg/Foo");

  primec::Definition namedTransformDef;
  namedTransformDef.namespacePrefix = "/pkg";
  primec::Transform qualifier;
  qualifier.name = "public";
  namedTransformDef.transforms.push_back(qualifier);
  primec::Transform namedType;
  namedType.name = "Bar";
  namedTransformDef.transforms.push_back(namedType);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(namedTransformDef, resolveStruct, inferExpr) ==
        "/pkg/Bar");

  primec::Definition returnExprDef;
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "factory";
  returnExprDef.returnExpr = returnExpr;
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnExprDef, resolveStruct, inferExpr) ==
        "/pkg/FromExpr");

  primec::Definition returnStmtDef;
  primec::Expr nestedCall;
  nestedCall.kind = primec::Expr::Kind::Call;
  nestedCall.name = "mk";
  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {nestedCall};
  returnStmtDef.statements.push_back(returnStmt);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnStmtDef, resolveStruct, inferExpr) ==
        "/pkg/FromStmt");

  primec::Definition unresolvedDef;
  unresolvedDef.namespacePrefix = "/pkg";
  primec::Transform unresolvedReturn;
  unresolvedReturn.name = "return";
  unresolvedReturn.templateArgs = {"Missing"};
  unresolvedDef.transforms.push_back(unresolvedReturn);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(unresolvedDef, resolveStruct, inferExpr).empty());
}

TEST_CASE("ir lowerer struct type helpers infer call target struct paths") {
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto isKnownStructPath = [](const std::string &path) { return path == "/pkg/Ctor"; };
  auto inferDefinitionStructReturnPath = [](const std::string &path) {
    if (path == "/pkg/factory") {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            ctorCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath) == "/pkg/Ctor");

  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            factoryCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath) == "/pkg/FromDef");

  primec::Expr unknownCall;
  unknownCall.kind = primec::Expr::Kind::Call;
  unknownCall.name = "unknown";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            unknownCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr methodCall = ctorCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            methodCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr fieldAccessCall = ctorCall;
  fieldAccessCall.isFieldAccess = true;
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            fieldAccessCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            nameExpr, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());
}

TEST_CASE("ir lowerer struct type helpers infer name-expression struct paths") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo structInfo;
  structInfo.structTypeName = "/pkg/Point";
  locals.emplace("point", structInfo);
  primec::ir_lowerer::LocalInfo fileInfo;
  fileInfo.isFileHandle = true;
  fileInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  fileInfo.structTypeName = "/std/file/File<Read>";
  locals.emplace("file", fileInfo);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "point";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(nameExpr, locals) == "/pkg/Point");

  nameExpr.name = "file";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(nameExpr, locals).empty());

  nameExpr.name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(nameExpr, locals).empty());

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "point";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(callExpr, locals).empty());
}

TEST_CASE("ir lowerer struct type helpers infer field-access struct paths") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiverInfo;
  receiverInfo.structTypeName = "/pkg/Container";
  locals.emplace("self", receiverInfo);

  auto inferStructExprPath = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    if (expr.kind != primec::Expr::Kind::Name) {
      return std::string();
    }
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return std::string();
    }
    return it->second.structTypeName;
  };
  auto resolveStructFieldSlot =
      [](const std::string &structPath, const std::string &fieldName, primec::ir_lowerer::StructSlotFieldInfo &out) {
        if (structPath == "/pkg/Container" && fieldName == "nested") {
          out.structPath = "/pkg/Nested";
          return true;
        }
        return false;
      };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr fieldAccess;
  fieldAccess.kind = primec::Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = "nested";
  fieldAccess.args = {receiverExpr};
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            fieldAccess, locals, inferStructExprPath, resolveStructFieldSlot) == "/pkg/Nested");

  primec::Expr missingReceiver = fieldAccess;
  missingReceiver.args.front().name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            missingReceiver, locals, inferStructExprPath, resolveStructFieldSlot).empty());

  primec::Expr missingField = fieldAccess;
  missingField.name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            missingField, locals, inferStructExprPath, resolveStructFieldSlot).empty());

  primec::Expr notFieldAccess = fieldAccess;
  notFieldAccess.isFieldAccess = false;
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            notFieldAccess, locals, inferStructExprPath, resolveStructFieldSlot).empty());
}

TEST_CASE("ir lowerer struct type helpers build layout field index and collect fields") {
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "array<i32>", "", true});
            appendStructLayoutField("/pkg/Bar", {"x", "f64", "", false});
          });
  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(fieldIndex, "/pkg/Foo", layoutFields));
  REQUIRE(layoutFields.size() == 2);
  CHECK(layoutFields[0].name == "a");
  CHECK(layoutFields[1].name == "b");
  CHECK(layoutFields[1].isStatic);

  std::vector<primec::ir_lowerer::StructArrayFieldInfo> arrayFields;
  REQUIRE(primec::ir_lowerer::collectStructArrayFieldsFromLayoutIndex(fieldIndex, "/pkg/Foo", arrayFields));
  REQUIRE(arrayFields.size() == 2);
  CHECK(arrayFields[0].typeName == "i32");
  CHECK(arrayFields[1].typeName == "array<i32>");
  CHECK(arrayFields[1].isStatic);

  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(fieldIndex, "/pkg/Missing", layoutFields));
  CHECK_FALSE(primec::ir_lowerer::collectStructArrayFieldsFromLayoutIndex(fieldIndex, "/pkg/Missing", arrayFields));

  const primec::ir_lowerer::StructLayoutFieldIndex emptyIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(1, {});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer struct type helpers resolve struct array info from path") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    if (typeName == "string") {
      return ValueKind::String;
    }
    return ValueKind::Unknown;
  };

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::StructArrayFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Foo") {
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", true});
      return true;
    }
    if (structPath == "/pkg/Mixed") {
      out.push_back({"i32", "", false});
      out.push_back({"i64", "", false});
      return true;
    }
    if (structPath == "/pkg/Templated") {
      out.push_back({"array", "i32", false});
      return true;
    }
    if (structPath == "/pkg/Strings") {
      out.push_back({"string", "", false});
      return true;
    }
    return false;
  };

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Foo", collectFields, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);

  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Mixed", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Templated", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Strings", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Missing", collectFields, valueKindFromTypeName, out));
}

TEST_CASE("ir lowerer struct type helpers resolve and apply struct array info from layout field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    if (typeName == "string") {
      return ValueKind::String;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
            appendStructLayoutField("/pkg/Templated", {"c", "array", "i32", false});
          });

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Foo", fieldIndex, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Templated", fieldIndex, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Missing", fieldIndex, valueKindFromTypeName, out));

  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);

  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromBindingWithLayoutFieldIndex(
      expr, resolveStructTypeName, fieldIndex, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBindingWithLayoutFieldIndex(
      expr, resolveStructTypeName, fieldIndex, valueKindFromTypeName, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers build struct array resolvers from layout field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
          });
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  auto resolveStructArrayInfoFromPath =
      primec::ir_lowerer::makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
          fieldIndex, valueKindFromTypeName);
  auto applyStructArrayInfo =
      primec::ir_lowerer::makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
          resolveStructTypeName, fieldIndex, valueKindFromTypeName);

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(resolveStructArrayInfoFromPath("/pkg/Foo", out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(resolveStructArrayInfoFromPath("/pkg/Missing", out));

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  applyStructArrayInfo(expr, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers build bundled struct array adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
          });
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  auto adapters = primec::ir_lowerer::makeStructArrayInfoAdapters(
      fieldIndex, resolveStructTypeName, valueKindFromTypeName);

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(adapters.resolveStructArrayTypeInfoFromPath("/pkg/Foo", out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(adapters.resolveStructArrayTypeInfoFromPath("/pkg/Missing", out));

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  adapters.applyStructArrayInfo(expr, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers apply struct array info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };
  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::StructArrayFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Foo") {
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", false});
      return true;
    }
    return false;
  };

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBinding(
      expr, resolveStructTypeName, collectFields, valueKindFromTypeName, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);

  primec::Expr pointerExpr;
  pointerExpr.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerExpr.transforms.push_back(pointerType);
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  pointerInfo.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBinding(
      pointerExpr, resolveStructTypeName, collectFields, valueKindFromTypeName, pointerInfo);
  CHECK(pointerInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(pointerInfo.structTypeName.empty());
}

TEST_CASE("ir lowerer struct type helpers resolve struct slot field by name") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::vector<primec::ir_lowerer::StructSlotFieldInfo> fields = {
      {"x", 1, 1, ValueKind::Int32, ""},
      {"nested", 2, 3, ValueKind::Unknown, "/pkg/Nested"},
  };

  primec::ir_lowerer::StructSlotFieldInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructSlotFieldByName(fields, "nested", out));
  CHECK(out.name == "nested");
  CHECK(out.slotOffset == 2);
  CHECK(out.slotCount == 3);
  CHECK(out.structPath == "/pkg/Nested");

  CHECK_FALSE(primec::ir_lowerer::resolveStructSlotFieldByName(fields, "missing", out));
}

TEST_CASE("ir lowerer struct type helpers resolve field slot from layout") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto resolveStructSlotFields = [](const std::string &structPath,
                                    std::vector<primec::ir_lowerer::StructSlotFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Container") {
      out.push_back({"value", 1, 1, ValueKind::Int32, ""});
      out.push_back({"nested", 2, 3, ValueKind::Unknown, "/pkg/Nested"});
      return true;
    }
    return false;
  };

  primec::ir_lowerer::StructSlotFieldInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Container", "nested", resolveStructSlotFields, out));
  CHECK(out.name == "nested");
  CHECK(out.slotOffset == 2);
  CHECK(out.structPath == "/pkg/Nested");

  CHECK_FALSE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Container", "missing", resolveStructSlotFields, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Missing", "nested", resolveStructSlotFields, out));
}


TEST_SUITE_END();
