#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers resolve file handle Result payload metadata") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo fileLocal;
  fileLocal.isFileHandle = true;
  fileLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("file", fileLocal);

  primec::ir_lowerer::LocalInfo resultLocal;
  resultLocal.isResult = true;
  resultLocal.resultHasValue = true;
  resultLocal.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  resultLocal.resultValueIsFileHandle = true;
  resultLocal.resultErrorType = "FileError";
  locals.emplace("source", resultLocal);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr fileExpr;
  fileExpr.kind = primec::Expr::Kind::Name;
  fileExpr.name = "file";

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, fileExpr};

  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "source";

  primec::Expr paramExpr;
  paramExpr.kind = primec::Expr::Kind::Name;
  paramExpr.name = "opened";

  primec::Expr lambdaExpr;
  lambdaExpr.isLambda = true;
  lambdaExpr.args.push_back(paramExpr);
  lambdaExpr.bodyArguments.push_back(paramExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceExpr, lambdaExpr};

  primec::Expr okParamExpr;
  okParamExpr.kind = primec::Expr::Kind::Call;
  okParamExpr.isMethodCall = true;
  okParamExpr.name = "ok";
  okParamExpr.args = {resultName, paramExpr};

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceExpr, lambdaExpr};
  andThenExpr.args[2].bodyArguments.clear();
  andThenExpr.args[2].bodyArguments.push_back(okParamExpr);

  primec::ir_lowerer::LocalInfo otherResultLocal = resultLocal;
  locals.emplace("other", otherResultLocal);

  primec::Expr otherExpr;
  otherExpr.kind = primec::Expr::Kind::Name;
  otherExpr.name = "other";

  primec::Expr leftParamExpr;
  leftParamExpr.kind = primec::Expr::Kind::Name;
  leftParamExpr.name = "left";

  primec::Expr rightParamExpr;
  rightParamExpr.kind = primec::Expr::Kind::Name;
  rightParamExpr.name = "right";

  primec::Expr map2LambdaExpr;
  map2LambdaExpr.isLambda = true;
  map2LambdaExpr.args = {leftParamExpr, rightParamExpr};
  map2LambdaExpr.bodyArguments.push_back(leftParamExpr);

  primec::Expr map2Expr;
  map2Expr.kind = primec::Expr::Kind::Call;
  map2Expr.isMethodCall = true;
  map2Expr.name = "map2";
  map2Expr.args = {resultName, sourceExpr, otherExpr, map2LambdaExpr};

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
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.valueIsFileHandle);
  CHECK(out.valueStructType.empty());

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.valueIsFileHandle);
  CHECK(out.valueStructType.empty());

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.valueIsFileHandle);
  CHECK(out.valueStructType.empty());

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      map2Expr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.valueIsFileHandle);
  CHECK(out.valueStructType.empty());
}

TEST_CASE("ir lowerer result helpers resolve array and vector Result payload metadata") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo arrayLocal;
  arrayLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("numbers", arrayLocal);

  primec::ir_lowerer::LocalInfo arrayResult;
  arrayResult.isResult = true;
  arrayResult.resultHasValue = true;
  arrayResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  arrayResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayResult.resultErrorType = "FileError";
  locals.emplace("sourceArray", arrayResult);

  primec::ir_lowerer::LocalInfo vectorResult;
  vectorResult.isResult = true;
  vectorResult.resultHasValue = true;
  vectorResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  vectorResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorResult.resultErrorType = "FileError";
  locals.emplace("sourceVector", vectorResult);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr numbersExpr;
  numbersExpr.kind = primec::Expr::Kind::Name;
  numbersExpr.name = "numbers";

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, numbersExpr};

  primec::Expr sourceArrayExpr;
  sourceArrayExpr.kind = primec::Expr::Kind::Name;
  sourceArrayExpr.name = "sourceArray";

  primec::Expr arrayParamExpr;
  arrayParamExpr.kind = primec::Expr::Kind::Name;
  arrayParamExpr.name = "value";

  primec::Expr mapLambdaExpr;
  mapLambdaExpr.isLambda = true;
  mapLambdaExpr.args.push_back(arrayParamExpr);
  mapLambdaExpr.bodyArguments.push_back(arrayParamExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceArrayExpr, mapLambdaExpr};

  primec::Expr sourceVectorExpr;
  sourceVectorExpr.kind = primec::Expr::Kind::Name;
  sourceVectorExpr.name = "sourceVector";

  primec::Expr vectorParamExpr;
  vectorParamExpr.kind = primec::Expr::Kind::Name;
  vectorParamExpr.name = "items";

  primec::Expr okVectorExpr;
  okVectorExpr.kind = primec::Expr::Kind::Call;
  okVectorExpr.isMethodCall = true;
  okVectorExpr.name = "ok";
  okVectorExpr.args = {resultName, vectorParamExpr};

  primec::Expr andThenLambdaExpr;
  andThenLambdaExpr.isLambda = true;
  andThenLambdaExpr.args.push_back(vectorParamExpr);
  andThenLambdaExpr.bodyArguments.push_back(okVectorExpr);

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceVectorExpr, andThenLambdaExpr};

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
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer result helpers resolve Buffer Result payload metadata") {
  primec::ir_lowerer::LocalMap locals;

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr bufferCtorExpr;
  bufferCtorExpr.kind = primec::Expr::Kind::Call;
  bufferCtorExpr.name = "Buffer";
  bufferCtorExpr.templateArgs.push_back("i32");
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.literalValue = 3;
  bufferCtorExpr.args.push_back(countExpr);

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, bufferCtorExpr};

  primec::Expr scopedBufferCtorExpr = bufferCtorExpr;
  scopedBufferCtorExpr.namespacePrefix = "/std/gfx";

  primec::Expr scopedOkExpr = okExpr;
  scopedOkExpr.args = {resultName, scopedBufferCtorExpr};

  primec::ir_lowerer::LocalInfo bufferResult;
  bufferResult.isResult = true;
  bufferResult.resultHasValue = true;
  bufferResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bufferResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferResult.resultErrorType = "GfxError";
  locals.emplace("sourceBuffer", bufferResult);
  locals.emplace("sourceBufferRight", bufferResult);

  primec::Expr sourceBufferExpr;
  sourceBufferExpr.kind = primec::Expr::Kind::Name;
  sourceBufferExpr.name = "sourceBuffer";

  primec::Expr sourceBufferRightExpr;
  sourceBufferRightExpr.kind = primec::Expr::Kind::Name;
  sourceBufferRightExpr.name = "sourceBufferRight";

  primec::Expr bufferParamExpr;
  bufferParamExpr.kind = primec::Expr::Kind::Name;
  bufferParamExpr.name = "value";

  primec::Expr mapLambdaExpr;
  mapLambdaExpr.isLambda = true;
  mapLambdaExpr.args.push_back(bufferParamExpr);
  mapLambdaExpr.bodyArguments.push_back(bufferParamExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceBufferExpr, mapLambdaExpr};

  primec::Expr okBufferExpr;
  okBufferExpr.kind = primec::Expr::Kind::Call;
  okBufferExpr.isMethodCall = true;
  okBufferExpr.name = "ok";
  okBufferExpr.args = {resultName, bufferParamExpr};

  primec::Expr andThenLambdaExpr;
  andThenLambdaExpr.isLambda = true;
  andThenLambdaExpr.args.push_back(bufferParamExpr);
  andThenLambdaExpr.bodyArguments.push_back(okBufferExpr);

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceBufferExpr, andThenLambdaExpr};

  primec::Expr leftParamExpr;
  leftParamExpr.kind = primec::Expr::Kind::Name;
  leftParamExpr.name = "left";

  primec::Expr rightParamExpr;
  rightParamExpr.kind = primec::Expr::Kind::Name;
  rightParamExpr.name = "right";

  primec::Expr map2LambdaExpr;
  map2LambdaExpr.isLambda = true;
  map2LambdaExpr.args.push_back(leftParamExpr);
  map2LambdaExpr.args.push_back(rightParamExpr);
  map2LambdaExpr.bodyArguments.push_back(rightParamExpr);

  primec::Expr map2Expr;
  map2Expr.kind = primec::Expr::Kind::Call;
  map2Expr.isMethodCall = true;
  map2Expr.name = "map2";
  map2Expr.args = {resultName, sourceBufferExpr, sourceBufferRightExpr, map2LambdaExpr};

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
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      scopedOkExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      map2Expr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Buffer);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer result helpers resolve map Result payload metadata") {
  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo mapLocal;
  mapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapLocal.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", mapLocal);

  primec::ir_lowerer::LocalInfo mapResult;
  mapResult.isResult = true;
  mapResult.resultHasValue = true;
  mapResult.resultValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapResult.resultValueCollectionKind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapResult.resultValueMapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  mapResult.resultErrorType = "FileError";
  locals.emplace("sourceMap", mapResult);
  locals.emplace("sourceMapRight", mapResult);

  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr directMapCtorExpr;
  directMapCtorExpr.kind = primec::Expr::Kind::Call;
  directMapCtorExpr.name = "map";
  directMapCtorExpr.namespacePrefix = "/std/collections/map";
  directMapCtorExpr.templateArgs = {"i32", "i32"};

  primec::Expr okCtorExpr;
  okCtorExpr.kind = primec::Expr::Kind::Call;
  okCtorExpr.isMethodCall = true;
  okCtorExpr.name = "ok";
  okCtorExpr.args = {resultName, directMapCtorExpr};

  primec::Expr okExpr;
  okExpr.kind = primec::Expr::Kind::Call;
  okExpr.isMethodCall = true;
  okExpr.name = "ok";
  okExpr.args = {resultName, valuesExpr};

  primec::Expr sourceMapExpr;
  sourceMapExpr.kind = primec::Expr::Kind::Name;
  sourceMapExpr.name = "sourceMap";

  primec::Expr sourceMapRightExpr;
  sourceMapRightExpr.kind = primec::Expr::Kind::Name;
  sourceMapRightExpr.name = "sourceMapRight";

  primec::Expr mapParamExpr;
  mapParamExpr.kind = primec::Expr::Kind::Name;
  mapParamExpr.name = "value";

  primec::Expr mapLambdaExpr;
  mapLambdaExpr.isLambda = true;
  mapLambdaExpr.args.push_back(mapParamExpr);
  mapLambdaExpr.bodyArguments.push_back(mapParamExpr);

  primec::Expr mapExpr;
  mapExpr.kind = primec::Expr::Kind::Call;
  mapExpr.isMethodCall = true;
  mapExpr.name = "map";
  mapExpr.args = {resultName, sourceMapExpr, mapLambdaExpr};

  primec::Expr okMapExpr;
  okMapExpr.kind = primec::Expr::Kind::Call;
  okMapExpr.isMethodCall = true;
  okMapExpr.name = "ok";
  okMapExpr.args = {resultName, mapParamExpr};

  primec::Expr andThenLambdaExpr;
  andThenLambdaExpr.isLambda = true;
  andThenLambdaExpr.args.push_back(mapParamExpr);
  andThenLambdaExpr.bodyArguments.push_back(okMapExpr);

  primec::Expr andThenExpr;
  andThenExpr.kind = primec::Expr::Kind::Call;
  andThenExpr.isMethodCall = true;
  andThenExpr.name = "and_then";
  andThenExpr.args = {resultName, sourceMapExpr, andThenLambdaExpr};

  primec::Expr leftParamExpr;
  leftParamExpr.kind = primec::Expr::Kind::Name;
  leftParamExpr.name = "left";

  primec::Expr rightParamExpr;
  rightParamExpr.kind = primec::Expr::Kind::Name;
  rightParamExpr.name = "right";

  primec::Expr map2LambdaExpr;
  map2LambdaExpr.isLambda = true;
  map2LambdaExpr.args.push_back(leftParamExpr);
  map2LambdaExpr.args.push_back(rightParamExpr);
  map2LambdaExpr.bodyArguments.push_back(rightParamExpr);

  primec::Expr map2Expr;
  map2Expr.kind = primec::Expr::Kind::Call;
  map2Expr.isMethodCall = true;
  map2Expr.name = "map2";
  map2Expr.args = {resultName, sourceMapExpr, sourceMapRightExpr, map2LambdaExpr};

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
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      okCtorExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      mapExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      andThenExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  out = {};
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      map2Expr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.valueCollectionKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueMapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}


TEST_SUITE_END();
