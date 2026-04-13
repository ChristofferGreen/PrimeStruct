#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers resolve final-if Result.and_then metadata") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo sourceResult;
  sourceResult.isResult = true;
  sourceResult.resultHasValue = true;
  sourceResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  sourceResult.resultErrorType = "FileError";
  locals.emplace("sourceResult", sourceResult);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr sourceResultExpr;
  sourceResultExpr.kind = primec::Expr::Kind::Name;
  sourceResultExpr.name = "sourceResult";

  primec::Expr valueParamExpr;
  valueParamExpr.kind = primec::Expr::Kind::Name;
  valueParamExpr.name = "value";

  primec::Expr okThenExpr;
  okThenExpr.kind = primec::Expr::Kind::Call;
  okThenExpr.isMethodCall = true;
  okThenExpr.name = "ok";
  okThenExpr.args = {resultName, valueParamExpr};

  primec::Expr thenReturnExpr;
  thenReturnExpr.kind = primec::Expr::Kind::Call;
  thenReturnExpr.name = "return";
  thenReturnExpr.args = {okThenExpr};

  primec::Expr elseLiteralExpr;
  elseLiteralExpr.kind = primec::Expr::Kind::Literal;
  elseLiteralExpr.literalValue = 0;

  primec::Expr okElseExpr;
  okElseExpr.kind = primec::Expr::Kind::Call;
  okElseExpr.isMethodCall = true;
  okElseExpr.name = "ok";
  okElseExpr.args = {resultName, elseLiteralExpr};

  primec::Expr elseReturnExpr;
  elseReturnExpr.kind = primec::Expr::Kind::Call;
  elseReturnExpr.name = "return";
  elseReturnExpr.args = {okElseExpr};

  primec::Expr thenBlockExpr;
  thenBlockExpr.kind = primec::Expr::Kind::Call;
  thenBlockExpr.name = "then";
  thenBlockExpr.hasBodyArguments = true;
  thenBlockExpr.bodyArguments.push_back(thenReturnExpr);

  primec::Expr elseBlockExpr;
  elseBlockExpr.kind = primec::Expr::Kind::Call;
  elseBlockExpr.name = "else";
  elseBlockExpr.hasBodyArguments = true;
  elseBlockExpr.bodyArguments.push_back(elseReturnExpr);

  primec::Expr condExpr;
  condExpr.kind = primec::Expr::Kind::Literal;
  condExpr.literalValue = 1;

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {condExpr, thenBlockExpr, elseBlockExpr};

  primec::Expr andThenLambdaExpr;
  andThenLambdaExpr.isLambda = true;
  andThenLambdaExpr.args.push_back(valueParamExpr);
  andThenLambdaExpr.bodyArguments.push_back(ifExpr);

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceResultExpr, andThenLambdaExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    if (expr.kind == primec::Expr::Kind::Name) {
      auto localIt = localsIn.find(expr.name);
      if (localIt != localsIn.end()) {
        return localIt->second.valueKind;
      }
    }
    if (expr.kind == primec::Expr::Kind::Literal) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve packed error struct Result combinator metadata") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo sourceResult;
  sourceResult.isResult = true;
  sourceResult.resultHasValue = true;
  sourceResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  sourceResult.resultErrorType = "FileError";
  locals.emplace("sourceResult", sourceResult);
  locals.emplace("leftResult", sourceResult);
  locals.emplace("rightResult", sourceResult);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr sourceResultExpr;
  sourceResultExpr.kind = primec::Expr::Kind::Name;
  sourceResultExpr.name = "sourceResult";

  primec::Expr leftResultExpr;
  leftResultExpr.kind = primec::Expr::Kind::Name;
  leftResultExpr.name = "leftResult";

  primec::Expr rightResultExpr;
  rightResultExpr.kind = primec::Expr::Kind::Name;
  rightResultExpr.name = "rightResult";

  primec::Expr valueParamExpr;
  valueParamExpr.kind = primec::Expr::Kind::Name;
  valueParamExpr.name = "value";

  primec::Expr leftParamExpr;
  leftParamExpr.kind = primec::Expr::Kind::Name;
  leftParamExpr.name = "left";

  primec::Expr rightParamExpr;
  rightParamExpr.kind = primec::Expr::Kind::Name;
  rightParamExpr.name = "right";

  primec::Expr containerCtorExpr;
  containerCtorExpr.kind = primec::Expr::Kind::Call;
  containerCtorExpr.name = "ContainerError";
  containerCtorExpr.args = {valueParamExpr};

  primec::Expr mapLambdaExpr;
  mapLambdaExpr.isLambda = true;
  mapLambdaExpr.args.push_back(valueParamExpr);
  mapLambdaExpr.bodyArguments.push_back(containerCtorExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceResultExpr, mapLambdaExpr};

  primec::Expr imageCtorExpr;
  imageCtorExpr.kind = primec::Expr::Kind::Call;
  imageCtorExpr.name = "ImageError";
  imageCtorExpr.args = {valueParamExpr};

  primec::Expr okImageExpr;
  okImageExpr.kind = primec::Expr::Kind::Call;
  okImageExpr.isMethodCall = true;
  okImageExpr.name = "ok";
  okImageExpr.args = {resultName, imageCtorExpr};

  primec::Expr andThenLambdaExpr;
  andThenLambdaExpr.isLambda = true;
  andThenLambdaExpr.args.push_back(valueParamExpr);
  andThenLambdaExpr.bodyArguments.push_back(okImageExpr);

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceResultExpr, andThenLambdaExpr};

  primec::Expr plusExpr;
  plusExpr.kind = primec::Expr::Kind::Call;
  plusExpr.name = "plus";
  plusExpr.args = {leftParamExpr, rightParamExpr};

  primec::Expr gfxCtorExpr;
  gfxCtorExpr.kind = primec::Expr::Kind::Call;
  gfxCtorExpr.name = "GfxError";
  gfxCtorExpr.args = {plusExpr};

  primec::Expr map2LambdaExpr;
  map2LambdaExpr.isLambda = true;
  map2LambdaExpr.args = {leftParamExpr, rightParamExpr};
  map2LambdaExpr.bodyArguments.push_back(gfxCtorExpr);

  primec::Expr map2Expr;
  map2Expr.kind = primec::Expr::Kind::Call;
  map2Expr.isMethodCall = true;
  map2Expr.name = "map2";
  map2Expr.args = {resultName, leftResultExpr, rightResultExpr, map2LambdaExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    if (expr.kind == primec::Expr::Kind::Name) {
      auto localIt = localsIn.find(expr.name);
      if (localIt != localsIn.end()) {
        return localIt->second.valueKind;
      }
    }
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "plus") {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueStructType == "/std/collections/ContainerError");
  CHECK(out.errorType == "FileError");

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueStructType == "/std/image/ImageError");
  CHECK(out.errorType == "FileError");

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      map2Expr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueStructType == "/std/gfx/GfxError");
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve function-returned array Result payload metadata") {
  primec::Definition makeNumbersDef;
  makeNumbersDef.fullPath = "/make_numbers";

  primec::Expr makeNumbersExpr;
  makeNumbersExpr.kind = primec::Expr::Kind::Call;
  makeNumbersExpr.name = "make_numbers";

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [&](const primec::Expr &expr) -> const primec::Definition * {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "make_numbers") {
      return &makeNumbersDef;
    }
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
    if (path != "/make_numbers") {
      return false;
    }
    infoOut = primec::ir_lowerer::ReturnInfo{};
    infoOut.isResult = true;
    infoOut.resultHasValue = true;
    infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    infoOut.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Array;
    infoOut.resultErrorType = "FileError";
    return true;
  };
  auto inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      makeNumbersExpr, {}, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve function-returned map Result payload metadata") {
  primec::Definition makeValuesDef;
  makeValuesDef.fullPath = "/make_values";

  primec::Expr makeValuesExpr;
  makeValuesExpr.kind = primec::Expr::Kind::Call;
  makeValuesExpr.name = "make_values";

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [&](const primec::Expr &expr) -> const primec::Definition * {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "make_values") {
      return &makeValuesDef;
    }
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
    if (path != "/make_values") {
      return false;
    }
    infoOut = primec::ir_lowerer::ReturnInfo{};
    infoOut.isResult = true;
    infoOut.resultHasValue = true;
    infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    infoOut.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Map;
    infoOut.resultValueMapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    infoOut.resultErrorType = "FileError";
    return true;
  };
  auto inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      makeValuesExpr, {}, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve function-returned Buffer Result payload metadata") {
  primec::Definition makeBufferDef;
  makeBufferDef.fullPath = "/make_buffer";

  primec::Expr makeBufferExpr;
  makeBufferExpr.kind = primec::Expr::Kind::Call;
  makeBufferExpr.name = "make_buffer";

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [&](const primec::Expr &expr) -> const primec::Definition * {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "make_buffer") {
      return &makeBufferDef;
    }
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
    if (path != "/make_buffer") {
      return false;
    }
    infoOut = primec::ir_lowerer::ReturnInfo{};
    infoOut.isResult = true;
    infoOut.resultHasValue = true;
    infoOut.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    infoOut.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
    infoOut.resultErrorType = "GfxError";
    return true;
  };
  auto inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      makeBufferExpr, {}, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "GfxError");
}

TEST_CASE("ir lowerer result helpers build locals-aware resolver adapters") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localResult;
  localResult.isResult = true;
  localResult.resultHasValue = false;
  localResult.resultErrorType = "LocalError";
  locals.emplace("resultLocal", localResult);

  primec::Expr localNameExpr;
  localNameExpr.kind = primec::Expr::Kind::Name;
  localNameExpr.name = "resultLocal";

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  const auto resolveResultExprInfo = primec::ir_lowerer::makeResolveResultExprInfoFromLocals(
      resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind);

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(resolveResultExprInfo(localNameExpr, locals, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "LocalError");

  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK_FALSE(resolveResultExprInfo(unknownName, locals, out));
}

TEST_CASE("ir lowerer result helpers resolve indexed args-pack Result expressions") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo resultPack;
  resultPack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  resultPack.isArgsPack = true;
  resultPack.isResult = true;
  resultPack.resultHasValue = true;
  resultPack.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  resultPack.resultErrorType = "ParseError";
  locals.emplace("values", resultPack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      accessExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.errorType == "ParseError");
}

TEST_CASE("ir lowerer result helpers resolve indexed args-pack file handle method results") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo handlePack;
  handlePack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  handlePack.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  handlePack.isArgsPack = true;
  handlePack.isFileHandle = true;
  handlePack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  locals.emplace("values", handlePack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  primec::Expr lineExpr;
  lineExpr.kind = primec::Expr::Kind::StringLiteral;
  lineExpr.stringValue = "msg";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.isMethodCall = true;
  writeExpr.name = "write_line";
  writeExpr.args = {accessExpr, lineExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      writeExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");

  primec::Expr wrappedWriteExpr = writeExpr;
  wrappedWriteExpr.args = {accessExpr, lineExpr};

  primec::ir_lowerer::ResultExprInfo wrappedOut;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      wrappedWriteExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, wrappedOut));
  CHECK(wrappedOut.isResult);
  CHECK_FALSE(wrappedOut.hasValue);
  CHECK(wrappedOut.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve indexed borrowed args-pack file handle method results") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo handlePack;
  handlePack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  handlePack.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  handlePack.isArgsPack = true;
  handlePack.isFileHandle = true;
  handlePack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  locals.emplace("values", handlePack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  primec::Expr derefExpr;
  derefExpr.kind = primec::Expr::Kind::Call;
  derefExpr.name = "dereference";
  derefExpr.args = {accessExpr};

  primec::Expr lineExpr;
  lineExpr.kind = primec::Expr::Kind::StringLiteral;
  lineExpr.stringValue = "msg";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.isMethodCall = true;
  writeExpr.name = "write_line";
  writeExpr.args = {derefExpr, lineExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      writeExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");

  primec::Expr wrappedWriteExpr = writeExpr;
  wrappedWriteExpr.args = {accessExpr, lineExpr};

  primec::ir_lowerer::ResultExprInfo wrappedOut;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      wrappedWriteExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, wrappedOut));
  CHECK(wrappedOut.isResult);
  CHECK_FALSE(wrappedOut.hasValue);
  CHECK(wrappedOut.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve indexed pointer args-pack file handle method results") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo handlePack;
  handlePack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  handlePack.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  handlePack.isArgsPack = true;
  handlePack.isFileHandle = true;
  handlePack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  locals.emplace("values", handlePack);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesName, indexExpr};

  primec::Expr derefExpr;
  derefExpr.kind = primec::Expr::Kind::Call;
  derefExpr.name = "dereference";
  derefExpr.args = {accessExpr};

  primec::Expr lineExpr;
  lineExpr.kind = primec::Expr::Kind::StringLiteral;
  lineExpr.stringValue = "msg";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.isMethodCall = true;
  writeExpr.name = "write_line";
  writeExpr.args = {derefExpr, lineExpr};

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  const primec::ir_lowerer::InferExprKindWithLocalsFn inferExprKind = {};

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      writeExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");
}


TEST_SUITE_END();
