#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer expr emit setup dispatches upload/readback passthrough step") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 9;

  int emitExprCalls = 0;
  auto emitExpr = [&](const primec::Expr &argExpr, const primec::ir_lowerer::LocalMap &) {
    ++emitExprCalls;
    return argExpr.kind == primec::Expr::Kind::Literal;
  };

  primec::Expr uploadExpr;
  uploadExpr.kind = primec::Expr::Kind::Call;
  uploadExpr.name = "upload";
  uploadExpr.args.push_back(literalExpr);
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);

  primec::Expr readbackExpr = uploadExpr;
  readbackExpr.name = "readback";
  emitExprCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            readbackExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);

  primec::Expr unknownExpr = uploadExpr;
  unknownExpr.name = "unknown";
  emitExprCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            unknownExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
}

TEST_CASE("ir lowerer expr emit setup validates upload/readback dispatch dependencies") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr uploadExpr;
  uploadExpr.kind = primec::Expr::Kind::Call;
  uploadExpr.name = "upload";
  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 1;
  uploadExpr.args.push_back(literalExpr);

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            {},
            emitReadbackPassthroughCall,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitUploadPassthroughCall");

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            {},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitReadbackPassthroughCall");

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            {},
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");
}

TEST_CASE("emitter emit setup math-import step detects supported imports") {
  primec::Program noImports;
  CHECK_FALSE(primec::emitter::runEmitterEmitSetupMathImport(noImports));

  primec::Program wildcardImport;
  wildcardImport.imports.push_back("/std/math/*");
  CHECK(primec::emitter::runEmitterEmitSetupMathImport(wildcardImport));

  primec::Program namespacedImport;
  namespacedImport.imports.push_back("/std/math/trig");
  CHECK(primec::emitter::runEmitterEmitSetupMathImport(namespacedImport));

  primec::Program unrelatedImport;
  unrelatedImport.imports.push_back("/std/io/*");
  unrelatedImport.imports.push_back("/std/math/");
  CHECK_FALSE(primec::emitter::runEmitterEmitSetupMathImport(unrelatedImport));
}

TEST_CASE("emitter emit setup lifecycle helper step matches helper suffixes") {
  const auto createMatch = primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo/Create");
  REQUIRE(createMatch.has_value());
  CHECK(createMatch->parentPath == "/Foo");
  CHECK(createMatch->kind == primec::emitter::EmitterLifecycleHelperKind::Create);
  CHECK(createMatch->placement.empty());

  const auto destroyStackMatch = primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo/DestroyStack");
  REQUIRE(destroyStackMatch.has_value());
  CHECK(destroyStackMatch->parentPath == "/Foo");
  CHECK(destroyStackMatch->kind == primec::emitter::EmitterLifecycleHelperKind::Destroy);
  CHECK(destroyStackMatch->placement == "stack");

  const auto copyMatch = primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo/Copy");
  REQUIRE(copyMatch.has_value());
  CHECK(copyMatch->kind == primec::emitter::EmitterLifecycleHelperKind::Copy);

  const auto moveMatch = primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo/Move");
  REQUIRE(moveMatch.has_value());
  CHECK(moveMatch->kind == primec::emitter::EmitterLifecycleHelperKind::Move);

  CHECK_FALSE(primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo"));
  CHECK_FALSE(primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/Foo/CreateExtra"));
  CHECK_FALSE(primec::emitter::runEmitterEmitSetupLifecycleHelperMatchStep("/FooCreate"));
}

TEST_CASE("emitter expr control name step resolves constants and locals") {
  primec::Expr notNameExpr;
  notNameExpr.kind = primec::Expr::Kind::Literal;
  notNameExpr.literalValue = 1;
  CHECK_FALSE(primec::emitter::runEmitterExprControlNameStep(
      notNameExpr,
      {},
      true).has_value());

  primec::Expr bareConstantExpr;
  bareConstantExpr.kind = primec::Expr::Kind::Name;
  bareConstantExpr.name = "pi";
  const auto bareNoMath = primec::emitter::runEmitterExprControlNameStep(
      bareConstantExpr,
      {},
      false);
  REQUIRE(bareNoMath.has_value());
  CHECK(*bareNoMath == "pi");

  const auto bareWithMath = primec::emitter::runEmitterExprControlNameStep(
      bareConstantExpr,
      {},
      true);
  REQUIRE(bareWithMath.has_value());
  CHECK(*bareWithMath == "ps_const_pi");

  primec::Expr namespacedConstantExpr;
  namespacedConstantExpr.kind = primec::Expr::Kind::Name;
  namespacedConstantExpr.name = "/std/math/tau";
  const auto namespaced = primec::emitter::runEmitterExprControlNameStep(
      namespacedConstantExpr,
      {},
      false);
  REQUIRE(namespaced.has_value());
  CHECK(*namespaced == "ps_const_tau");

  primec::Expr parserShapedConstantExpr;
  parserShapedConstantExpr.kind = primec::Expr::Kind::Name;
  parserShapedConstantExpr.name = "tau";
  parserShapedConstantExpr.namespacePrefix = "/std/math";
  const auto parserShaped = primec::emitter::runEmitterExprControlNameStep(
      parserShapedConstantExpr,
      {},
      false);
  REQUIRE(parserShaped.has_value());
  CHECK(*parserShaped == "ps_const_tau");

  primec::Emitter::BindingInfo localInfo;
  localInfo.typeName = "f64";
  std::unordered_map<std::string, primec::Emitter::BindingInfo> localTypes = {
      {"e", localInfo},
  };
  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "e";
  const auto localResolved = primec::emitter::runEmitterExprControlNameStep(
      localExpr,
      localTypes,
      true);
  REQUIRE(localResolved.has_value());
  CHECK(*localResolved == "e");
}

TEST_CASE("emitter primitive return inference keeps scoped builtin control calls") {
  std::unordered_map<std::string, primec::Emitter::BindingInfo> localTypes;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Name;
  lhs.name = "value";
  primec::Emitter::BindingInfo lhsInfo;
  lhsInfo.typeName = "i32";
  localTypes.emplace("value", lhsInfo);

  primec::Expr rhs;
  rhs.kind = primec::Expr::Kind::Literal;
  rhs.literalValue = 7;

  primec::Expr namespacedAssignCall;
  namespacedAssignCall.kind = primec::Expr::Kind::Call;
  namespacedAssignCall.name = "assign";
  namespacedAssignCall.namespacePrefix = "/std/collections/internal_soa_storage";
  namespacedAssignCall.args = {lhs, rhs};
  CHECK(primec::emitter::inferPrimitiveReturnKind(
            namespacedAssignCall, localTypes, returnKinds, false) ==
        primec::emitter::ReturnKind::Int);

  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr thenExpr;
  thenExpr.kind = primec::Expr::Kind::Literal;
  thenExpr.literalValue = 1;

  primec::Expr elseExpr;
  elseExpr.kind = primec::Expr::Kind::Literal;
  elseExpr.literalValue = 2;

  primec::Expr generatedIfCall;
  generatedIfCall.kind = primec::Expr::Kind::Call;
  generatedIfCall.name =
      "/std/collections/internal_soa_storage/SoaColumn__tabcdef01/if";
  generatedIfCall.args = {cond, thenExpr, elseExpr};
  CHECK(primec::emitter::inferPrimitiveReturnKind(
            generatedIfCall, localTypes, returnKinds, false) ==
        primec::emitter::ReturnKind::Int);
}

TEST_CASE("emitter expr control float-literal step formats literals") {
  primec::Expr notFloatExpr;
  notFloatExpr.kind = primec::Expr::Kind::Literal;
  notFloatExpr.literalValue = 1;
  CHECK_FALSE(primec::emitter::runEmitterExprControlFloatLiteralStep(notFloatExpr).has_value());

  primec::Expr float64Expr;
  float64Expr.kind = primec::Expr::Kind::FloatLiteral;
  float64Expr.floatWidth = 64;
  float64Expr.floatValue = "3.5";
  const auto float64Value = primec::emitter::runEmitterExprControlFloatLiteralStep(float64Expr);
  REQUIRE(float64Value.has_value());
  CHECK(*float64Value == "3.5");

  primec::Expr float32WholeExpr = float64Expr;
  float32WholeExpr.floatWidth = 32;
  float32WholeExpr.floatValue = "2";
  const auto float32Whole = primec::emitter::runEmitterExprControlFloatLiteralStep(float32WholeExpr);
  REQUIRE(float32Whole.has_value());
  CHECK(*float32Whole == "2.0f");

  primec::Expr float32FractionExpr = float32WholeExpr;
  float32FractionExpr.floatValue = "2.25";
  const auto float32Fraction = primec::emitter::runEmitterExprControlFloatLiteralStep(float32FractionExpr);
  REQUIRE(float32Fraction.has_value());
  CHECK(*float32Fraction == "2.25f");

  primec::Expr float32ExponentExpr = float32WholeExpr;
  float32ExponentExpr.floatValue = "1e3";
  const auto float32Exponent = primec::emitter::runEmitterExprControlFloatLiteralStep(float32ExponentExpr);
  REQUIRE(float32Exponent.has_value());
  CHECK(*float32Exponent == "1e3f");
}

TEST_CASE("emitter expr control integer-literal step formats literals") {
  primec::Expr notIntegerExpr;
  notIntegerExpr.kind = primec::Expr::Kind::FloatLiteral;
  notIntegerExpr.floatValue = "1.0";
  CHECK_FALSE(primec::emitter::runEmitterExprControlIntegerLiteralStep(notIntegerExpr).has_value());

  primec::Expr signed32Expr;
  signed32Expr.kind = primec::Expr::Kind::Literal;
  signed32Expr.isUnsigned = false;
  signed32Expr.intWidth = 32;
  signed32Expr.literalValue = static_cast<uint64_t>(static_cast<int32_t>(-3));
  const auto signed32Value = primec::emitter::runEmitterExprControlIntegerLiteralStep(signed32Expr);
  REQUIRE(signed32Value.has_value());
  CHECK(*signed32Value == "-3");

  primec::Expr unsignedExpr = signed32Expr;
  unsignedExpr.isUnsigned = true;
  unsignedExpr.literalValue = 7;
  const auto unsignedValue = primec::emitter::runEmitterExprControlIntegerLiteralStep(unsignedExpr);
  REQUIRE(unsignedValue.has_value());
  CHECK(*unsignedValue == "static_cast<uint64_t>(7)");

  primec::Expr signed64Expr = signed32Expr;
  signed64Expr.intWidth = 64;
  signed64Expr.literalValue = 11;
  const auto signed64Value = primec::emitter::runEmitterExprControlIntegerLiteralStep(signed64Expr);
  REQUIRE(signed64Value.has_value());
  CHECK(*signed64Value == "static_cast<int64_t>(11)");
}

TEST_CASE("emitter expr control bool-literal step formats literals") {
  primec::Expr notBoolExpr;
  notBoolExpr.kind = primec::Expr::Kind::Literal;
  notBoolExpr.literalValue = 1;
  CHECK_FALSE(primec::emitter::runEmitterExprControlBoolLiteralStep(notBoolExpr).has_value());

  primec::Expr boolExpr;
  boolExpr.kind = primec::Expr::Kind::BoolLiteral;
  boolExpr.boolValue = true;
  const auto trueValue = primec::emitter::runEmitterExprControlBoolLiteralStep(boolExpr);
  REQUIRE(trueValue.has_value());
  CHECK(*trueValue == "true");

  boolExpr.boolValue = false;
  const auto falseValue = primec::emitter::runEmitterExprControlBoolLiteralStep(boolExpr);
  REQUIRE(falseValue.has_value());
  CHECK(*falseValue == "false");
}

TEST_CASE("emitter expr control string-literal step formats literals") {
  primec::Expr notStringExpr;
  notStringExpr.kind = primec::Expr::Kind::Literal;
  notStringExpr.literalValue = 1;
  CHECK_FALSE(primec::emitter::runEmitterExprControlStringLiteralStep(notStringExpr).has_value());

  primec::Expr stringExpr;
  stringExpr.kind = primec::Expr::Kind::StringLiteral;
  stringExpr.stringValue = "\"hello\"utf8";
  const auto stringValue = primec::emitter::runEmitterExprControlStringLiteralStep(stringExpr);
  REQUIRE(stringValue.has_value());
  CHECK(*stringValue == "std::string_view(\"hello\")");

  primec::Expr singleQuotedExpr = stringExpr;
  singleQuotedExpr.stringValue = "'ok'utf8";
  const auto singleQuotedValue = primec::emitter::runEmitterExprControlStringLiteralStep(singleQuotedExpr);
  REQUIRE(singleQuotedValue.has_value());
  CHECK(*singleQuotedValue == "std::string_view(\"ok\")");
}

TEST_CASE("emitter expr control field-access step formats receiver access") {
  primec::Expr notCallExpr;
  notCallExpr.kind = primec::Expr::Kind::Name;
  notCallExpr.name = "value";
  CHECK_FALSE(primec::emitter::runEmitterExprControlFieldAccessStep(
      notCallExpr,
      [&](const primec::Expr &) { return "unused"; },
      {}).has_value());

  primec::Expr fieldAccessNoReceiver;
  fieldAccessNoReceiver.kind = primec::Expr::Kind::Call;
  fieldAccessNoReceiver.isFieldAccess = true;
  fieldAccessNoReceiver.name = "count";
  CHECK_FALSE(primec::emitter::runEmitterExprControlFieldAccessStep(
      fieldAccessNoReceiver,
      [&](const primec::Expr &) { return "unused"; },
      {}).has_value());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "buffer";

  primec::Expr fieldAccessExpr = fieldAccessNoReceiver;
  fieldAccessExpr.args.push_back(receiverExpr);
  int receiverCalls = 0;
  const auto fieldAccessValue = primec::emitter::runEmitterExprControlFieldAccessStep(
      fieldAccessExpr,
      [&](const primec::Expr &receiver) {
        ++receiverCalls;
        CHECK(receiver.kind == primec::Expr::Kind::Name);
        CHECK(receiver.name == "buffer");
        return "buffer";
      },
      {});
  REQUIRE(fieldAccessValue.has_value());
  CHECK(*fieldAccessValue == "buffer.count");
  CHECK(receiverCalls == 1);

  CHECK_FALSE(primec::emitter::runEmitterExprControlFieldAccessStep(fieldAccessExpr, {}, {}).has_value());

  const auto staticFieldAccessValue = primec::emitter::runEmitterExprControlFieldAccessStep(
      fieldAccessExpr,
      [&](const primec::Expr &) { return "unused"; },
      [&](const primec::Expr &receiver) -> std::optional<std::string> {
        CHECK(receiver.kind == primec::Expr::Kind::Name);
        CHECK(receiver.name == "buffer");
        return std::string("BufferType");
      });
  REQUIRE(staticFieldAccessValue.has_value());
  CHECK(*staticFieldAccessValue == "BufferType::count");
}

TEST_CASE("emitter expr control call-path step rewrites non-method call names") {
  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.name = "count";
  methodExpr.isMethodCall = true;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      methodExpr,
      "count",
      {},
      {}).has_value());

  primec::Expr emptyNameExpr = methodExpr;
  emptyNameExpr.isMethodCall = false;
  emptyNameExpr.name.clear();
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      emptyNameExpr,
      "",
      {},
      {}).has_value());

  primec::Expr absoluteNameExpr = emptyNameExpr;
  absoluteNameExpr.name = "/count";
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      absoluteNameExpr,
      "/count",
      {},
      {}).has_value());

  primec::Expr nestedNameExpr = emptyNameExpr;
  nestedNameExpr.name = "a/b";
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      nestedNameExpr,
      "a/b",
      {},
      {}).has_value());

  primec::Expr namespacedExpr = emptyNameExpr;
  namespacedExpr.name = "Vec3";
  namespacedExpr.namespacePrefix = "pkg";
  const auto namespacedAlias = primec::emitter::runEmitterExprControlCallPathStep(
      namespacedExpr,
      "Vec3",
      {},
      {{"Vec3", "/pkg/Vec3"}});
  REQUIRE(namespacedAlias.has_value());
  CHECK(*namespacedAlias == "/pkg/Vec3");
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      namespacedExpr,
      "Vec3",
      {},
      {}).has_value());

  primec::Expr bareExpr = emptyNameExpr;
  bareExpr.name = "Vec3";
  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {{"Vec3", "/already/resolved"}},
      {{"Vec3", "/pkg/Vec3"}}).has_value());

  const auto rootPath = primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {{"/Vec3", "/Vec3"}},
      {{"Vec3", "/pkg/Vec3"}});
  REQUIRE(rootPath.has_value());
  CHECK(*rootPath == "/Vec3");

  const auto aliasPath = primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {},
      {{"Vec3", "/pkg/Vec3"}});
  REQUIRE(aliasPath.has_value());
  CHECK(*aliasPath == "/pkg/Vec3");

  const auto slashlessCanonicalMapAliasPath =
      primec::emitter::runEmitterExprControlCallPathStep(
          bareExpr,
          "Vec3",
          {},
          {{"Vec3", "std/collections/map/count"}});
  REQUIRE(slashlessCanonicalMapAliasPath.has_value());
  CHECK(*slashlessCanonicalMapAliasPath == "/std/collections/map/count");

  const auto slashlessMapAliasPath = primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {},
      {{"Vec3", "map/at"}});
  REQUIRE(slashlessMapAliasPath.has_value());
  CHECK(*slashlessMapAliasPath == "/map/at");

  const auto slashlessNonMapAliasPath = primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {},
      {{"Vec3", "pkg/Vec3"}});
  REQUIRE(slashlessNonMapAliasPath.has_value());
  CHECK(*slashlessNonMapAliasPath == "pkg/Vec3");

  CHECK_FALSE(primec::emitter::runEmitterExprControlCallPathStep(
      bareExpr,
      "Vec3",
      {},
      {}).has_value());
}

TEST_CASE("emitter helpers types normalize slashless map import aliases") {
  const std::unordered_map<std::string, std::string> importAliases = {
      {"MapCanonicalAlias", "std/collections/map/at"},
      {"MapCompatAlias", "map/at"},
      {"NonMapAlias", "pkg/Thing"},
  };
  const std::unordered_map<std::string, std::string> structTypeMap = {
      {"/std/collections/map/at", "CanonicalMapAtType"},
      {"/map/at", "CompatMapAtType"},
      {"/pkg/Thing", "PkgThingType"},
  };

  CHECK(primec::emitter::bindingTypeToCpp("MapCanonicalAlias", "/pkg", importAliases, structTypeMap) ==
        "CanonicalMapAtType");
  CHECK(primec::emitter::bindingTypeToCpp("MapCompatAlias", "/pkg", importAliases, structTypeMap) ==
        "CompatMapAtType");
  CHECK(primec::emitter::bindingTypeToCpp("NonMapAlias", "/pkg", importAliases, structTypeMap) == "int");
}

TEST_CASE("emitter helper path preference normalizes slashless map helper candidates") {
  const std::unordered_map<std::string, std::string> mapAliasOnlyNameMap = {
      {"/map/count", "ps_map_count"},
      {"/map/at", "ps_map_at"},
      {"/map/count_ref", "ps_map_count_ref"},
      {"/map/insert_ref", "ps_map_insert_ref"},
  };
  CHECK(primec::emitter::preferVectorStdlibHelperPath("map/count", mapAliasOnlyNameMap) == "/map/count");
  CHECK(primec::emitter::preferVectorStdlibHelperPath("std/collections/map/at", mapAliasOnlyNameMap) ==
        "/std/collections/map/at");
  CHECK(primec::emitter::preferVectorStdlibHelperPath("map/count_ref", mapAliasOnlyNameMap) ==
        "/map/count_ref");
  CHECK(primec::emitter::preferVectorStdlibHelperPath("std/collections/map/insert_ref", mapAliasOnlyNameMap) ==
        "/std/collections/map/insert_ref");

  const std::unordered_map<std::string, std::string> mapStdlibOnlyNameMap = {
      {"/std/collections/map/count", "ps_std_map_count"},
      {"/std/collections/map/insert", "ps_std_map_insert"},
  };
  CHECK(primec::emitter::preferVectorStdlibHelperPath("map/count", mapStdlibOnlyNameMap) == "/map/count");
  CHECK(primec::emitter::preferVectorStdlibHelperPath("map/insert", mapStdlibOnlyNameMap) == "/map/insert");

  CHECK(primec::emitter::preferVectorStdlibHelperPath("pkg/Thing/tag", mapAliasOnlyNameMap) == "pkg/Thing/tag");
}

TEST_CASE("emitter expr control method-path step rewrites eligible method calls") {
  primec::Expr nonMethodExpr;
  nonMethodExpr.kind = primec::Expr::Kind::Call;
  nonMethodExpr.name = "value";
  nonMethodExpr.isMethodCall = false;
  CHECK_FALSE(primec::emitter::runEmitterExprControlMethodPathStep(
      nonMethodExpr,
      {},
      {},
      {},
      {},
      [&](std::string &) { return true; }).has_value());

  primec::Expr methodExpr = nonMethodExpr;
  methodExpr.isMethodCall = true;
  bool resolverCalled = false;
  auto countLikeResolvedPath = primec::emitter::runEmitterExprControlMethodPathStep(
      methodExpr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, primec::Emitter::BindingInfo> &) { return true; },
      {},
      {},
      [&](std::string &pathOut) {
        resolverCalled = true;
        pathOut = "/Vec3/count";
        return true;
      });
  REQUIRE(countLikeResolvedPath.has_value());
  CHECK(*countLikeResolvedPath == "/Vec3/count");
  CHECK(resolverCalled);

  auto resolvedPath = primec::emitter::runEmitterExprControlMethodPathStep(
      methodExpr,
      {},
      {},
      {},
      {},
      [&](std::string &pathOut) {
        pathOut = "/Vec3/count";
        return true;
      });
  REQUIRE(resolvedPath.has_value());
  CHECK(*resolvedPath == "/Vec3/count");

  CHECK_FALSE(primec::emitter::runEmitterExprControlMethodPathStep(
      methodExpr,
      {},
      {},
      {},
      {},
      [&](std::string &) { return false; }).has_value());

  CHECK_FALSE(primec::emitter::runEmitterExprControlMethodPathStep(methodExpr, {}, {}, {}, {}, {}).has_value());
}

TEST_CASE("emitter count rewrite skips explicit canonical vector capacity helpers") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr capacityExpr;
  capacityExpr.kind = primec::Expr::Kind::Call;
  capacityExpr.name = "/std/collections/vector/capacity";
  capacityExpr.args = {receiverExpr};

  int resolveCalls = 0;
  auto resolvedPath = primec::emitter::runEmitterExprControlCountRewriteStep(
      capacityExpr,
      "/std/collections/vector/capacity",
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &methodCandidate, std::string &pathOut) {
        ++resolveCalls;
        CHECK(methodCandidate.isMethodCall);
        CHECK(methodCandidate.name == "capacity");
        REQUIRE(methodCandidate.args.size() == 1);
        CHECK(methodCandidate.args.front().kind == primec::Expr::Kind::Name);
        CHECK(methodCandidate.args.front().name == "values");
        pathOut = "/vector/capacity";
        return true;
      });
  CHECK_FALSE(resolvedPath.has_value());
  CHECK(resolveCalls == 0);
}

TEST_CASE("emitter count rewrite rejects removed array capacity alias") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr capacityExpr;
  capacityExpr.kind = primec::Expr::Kind::Call;
  capacityExpr.name = "/array/capacity";
  capacityExpr.args = {receiverExpr};

  int resolveCalls = 0;
  CHECK_FALSE(primec::emitter::runEmitterExprControlCountRewriteStep(
                  capacityExpr,
                  "/array/capacity",
                  {},
                  {},
                  {},
                  {},
                  {},
                  [&](const primec::Expr &, std::string &) {
                    ++resolveCalls;
                    return true;
                  })
                  .has_value());
  CHECK(resolveCalls == 0);
}

TEST_SUITE_END();
