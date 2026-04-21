#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup inference helper resolves reordered named access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
  locals.emplace("values", vectorInfo);

  primec::ir_lowerer::LocalInfo indexInfo;
  indexInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  indexInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("indexValue", indexInfo);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "indexValue";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {indexExpr, valuesExpr};
  accessExpr.argNames = {std::string("index"), std::string("values")};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer setup inference helper keeps leading collection receiver for positional access") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leadingMapInfo;
  leadingMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  leadingMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("values", leadingMapInfo);

  primec::ir_lowerer::LocalInfo fallbackMapInfo;
  fallbackMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  fallbackMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("fallback", fallbackMapInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr fallbackExpr;
  fallbackExpr.kind = primec::Expr::Kind::Name;
  fallbackExpr.name = "fallback";

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesExpr, fallbackExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper keeps leading soa receiver for positional access") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leadingSoaInfo;
  leadingSoaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  leadingSoaInfo.isSoaVector = true;
  locals.emplace("values", leadingSoaInfo);

  primec::ir_lowerer::LocalInfo fallbackVectorInfo;
  fallbackVectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  fallbackVectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("fallback", fallbackVectorInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr fallbackExpr;
  fallbackExpr.kind = primec::Expr::Kind::Name;
  fallbackExpr.name = "fallback";

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "get";
  accessExpr.args = {valuesExpr, fallbackExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper keeps labeled named receiver for access") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leadingMapInfo;
  leadingMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  leadingMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("values", leadingMapInfo);

  primec::ir_lowerer::LocalInfo fallbackMapInfo;
  fallbackMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  fallbackMapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("fallback", fallbackMapInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr fallbackExpr;
  fallbackExpr.kind = primec::Expr::Kind::Name;
  fallbackExpr.name = "fallback";

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {valuesExpr, fallbackExpr};
  accessExpr.argNames = {std::string("values"), std::string("index")};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper handles invalid and templated access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr nonAccess;
  nonAccess.kind = primec::Expr::Kind::Call;
  nonAccess.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            nonAccess,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr entryArgsTarget;
  entryArgsTarget.kind = primec::Expr::Kind::Name;
  entryArgsTarget.name = "entry_args";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args = {entryArgsTarget, indexExpr};
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.kind == primec::Expr::Kind::Name && expr.name == "entry_args";
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr vectorTarget;
  vectorTarget.kind = primec::Expr::Kind::Call;
  vectorTarget.name = "vector";
  vectorTarget.templateArgs = {"string"};
  accessExpr.args = {vectorTarget, indexExpr};
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned canonical map access string kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "/std/collections/map/at";
  accessExpr.args = {wrapMapCall, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            [](const primec::Expr &candidate,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
              if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
                return false;
              }
              candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
              return true;
            }) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.isMethodCall = true;
  accessExpr.name = "at";
  accessExpr.args = {wrapMapCall, indexExpr};
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            [](const primec::Expr &candidate,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
              if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
                return false;
              }
              candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
              return true;
            }) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.name = "at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            [](const primec::Expr &candidate,
               const primec::ir_lowerer::LocalMap &,
               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
              if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
                return false;
              }
              candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
              return true;
            }) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves canonical vector access string kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("values", vectorInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "/std/collections/vector/at";
  accessExpr.args = {valuesExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.name = "/std/collections/vector/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves bare vector method access string kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("values", vectorInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.isMethodCall = true;
  accessExpr.name = "at";
  accessExpr.args = {valuesExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.name = "at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves std-namespaced vector method access string kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("values", vectorInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.isMethodCall = true;
  accessExpr.name = "/std/collections/vector/at";
  accessExpr.args = {valuesExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.name = "/std/collections/vector/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves slash-method vector access string kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("values", vectorInfo);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.isMethodCall = true;
  accessExpr.name = "/vector/at";
  accessExpr.args = {valuesExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  accessExpr.name = "/vector/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  accessExpr.name = "/std/collections/vector/at";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);

  accessExpr.name = "/std/collections/vector/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned vector access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapValuesCall;
  wrapValuesCall.kind = primec::Expr::Kind::Call;
  wrapValuesCall.name = "wrapValues";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;

  primec::Expr directAccessExpr;
  directAccessExpr.kind = primec::Expr::Kind::Call;
  directAccessExpr.name = "/vector/at";
  directAccessExpr.args = {wrapValuesCall, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  auto resolveWrapValuesKind = [](const primec::Expr &candidate,
                                  const primec::ir_lowerer::LocalMap &,
                                  primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
    if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapValues") {
      return false;
    }
    candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::String;
    return true;
  };

  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            directAccessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapValuesKind) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr methodAccessExpr;
  methodAccessExpr.kind = primec::Expr::Kind::Call;
  methodAccessExpr.isMethodCall = true;
  methodAccessExpr.name = "at_unsafe";
  methodAccessExpr.args = {wrapValuesCall, indexExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            methodAccessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapValuesKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup inference helper infers body value kinds with locals scaffolding") {
  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "x";
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Literal;
  initializer.literalValue = 1;
  bindingExpr.args = {initializer};

  primec::Expr readExpr;
  readExpr.kind = primec::Expr::Kind::Name;
  readExpr.name = "x";

  int applyArrayInfoCalls = 0;
  int applyValueInfoCalls = 0;
  const primec::ir_lowerer::LocalInfo::ValueKind kind = primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
      {bindingExpr, readExpr},
      {},
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &locals) {
        if (expr.kind == primec::Expr::Kind::Name) {
          auto it = locals.find(expr.name);
          if (it != locals.end() && !it->second.structTypeName.empty()) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
          }
          if (it != locals.end()) {
            return it->second.valueKind;
          }
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](const primec::Expr &, primec::ir_lowerer::LocalInfo &) { ++applyArrayInfoCalls; },
      [&](const primec::Expr &, primec::ir_lowerer::LocalInfo &) { ++applyValueInfoCalls; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string("/pkg/Vec3"); });
  CHECK(kind == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
  CHECK(applyArrayInfoCalls == 1);
  CHECK(applyValueInfoCalls == 1);
}

TEST_CASE("ir lowerer setup inference helper recovers bool body comparison tails") {
  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  const primec::ir_lowerer::LocalInfo::ValueKind tailKind =
      primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
          {comparisonExpr},
          {},
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); });
  CHECK(tailKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "return";
  returnExpr.args = {comparisonExpr};

  const primec::ir_lowerer::LocalInfo::ValueKind returnKind =
      primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
          {returnExpr},
          {},
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); });
  CHECK(returnKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer setup inference helper prefers semantic binding facts for body locals scaffolding") {
  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "x";
  bindingExpr.semanticNodeId = 321;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Literal;
  initializer.literalValue = 1;
  bindingExpr.args = {initializer};

  primec::Expr readExpr;
  readExpr.kind = primec::Expr::Kind::Name;
  readExpr.name = "x";

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "x",
      .bindingTypeText = "i64",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 321,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/x"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const primec::ir_lowerer::LocalInfo::ValueKind kind =
      primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
          {bindingExpr, readExpr},
          {},
          [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &locals) {
            if (expr.kind == primec::Expr::Kind::Name) {
              auto it = locals.find(expr.name);
              if (it != locals.end()) {
                return it->second.valueKind;
              }
            }
            if (expr.kind == primec::Expr::Kind::Literal) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            }
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
          [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
          &semanticTargets);

  CHECK(kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup inference helper validates body locals scaffolding diagnostics") {
  primec::Expr badBindingExpr;
  badBindingExpr.isBinding = true;
  badBindingExpr.name = "x";
  badBindingExpr.args = {};

  CHECK(primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
            {badBindingExpr},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); }) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "return";
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Literal;
  returnExpr.args = {arg, arg};

  CHECK(primec::ir_lowerer::inferBodyValueKindWithLocalsScaffolding(
            {returnExpr},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
            },
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); }) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers math builtin return kinds") {
  using Resolution = primec::ir_lowerer::MathBuiltinReturnKindResolution;

  primec::Expr x;
  x.kind = primec::Expr::Kind::Name;
  x.name = "x";
  primec::Expr y;
  y.kind = primec::Expr::Kind::Name;
  y.name = "y";
  primec::Expr z;
  z.kind = primec::Expr::Kind::Name;
  z.name = "z";

  primec::Expr clampExpr;
  clampExpr.kind = primec::Expr::Kind::Call;
  clampExpr.name = "clamp";
  clampExpr.args = {x, y, z};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferMathBuiltinReturnKind(
            clampExpr,
            {},
            true,
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.name == "x") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              }
              if (expr.name == "y") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              }
              if (expr.name == "z") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
              return (left == primec::ir_lowerer::LocalInfo::ValueKind::Int64 ||
                      right == primec::ir_lowerer::LocalInfo::ValueKind::Int64)
                         ? primec::ir_lowerer::LocalInfo::ValueKind::Int64
                         : primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr predicateExpr;
  predicateExpr.kind = primec::Expr::Kind::Call;
  predicateExpr.name = "is_nan";
  predicateExpr.args = {x};
  CHECK(primec::ir_lowerer::inferMathBuiltinReturnKind(
            predicateExpr,
            {},
            true,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_SUITE_END();
