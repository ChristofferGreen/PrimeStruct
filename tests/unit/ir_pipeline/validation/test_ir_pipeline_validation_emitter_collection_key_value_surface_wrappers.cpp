#include "test_ir_pipeline_validation_helpers.h"

// TODO-4694: isCollectionSurfaceValue and isKeyValueSurfaceValue are
// behavior-preserving, generically-named wrappers unioning today's
// collection/key-value value-classification helpers:
//   isCollectionSurfaceValue(target, localTypes) ==
//       isCollectionVectorValue(target, localTypes) ||
//       isArrayValue(target, localTypes)
//   isKeyValueSurfaceValue(target, localTypes) ==
//       isKeyValueStorageValue(target, localTypes)
// These tests prove that equivalence across a representative set of
// currently-known Expr/BindingInfo inputs, covering both true and false
// cases (Name-bound locals, unbound names, and Call-expr shapes).

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

using primec::Expr;
using primec::emitter::BindingInfo;

Expr nameExprFor(std::string name) {
  Expr expr;
  expr.kind = Expr::Kind::Name;
  expr.name = std::move(name);
  return expr;
}

Expr callExprFor(std::string name) {
  Expr expr;
  expr.kind = Expr::Kind::Call;
  expr.name = std::move(name);
  return expr;
}

struct WrapperCase {
  const char *label;
  Expr target;
  std::unordered_map<std::string, BindingInfo> localTypes;
};

std::vector<WrapperCase> buildCollectionKeyValueSurfaceCases() {
  std::vector<WrapperCase> cases;

  // Name-bound local with an array binding: known-true for isArrayValue.
  {
    BindingInfo info;
    info.typeName = "array";
    cases.push_back({"array-bound name", nameExprFor("items"), {{"items", info}}});
  }
  // Name-bound local with an args-pack binding: known-true for isArrayValue.
  {
    BindingInfo info;
    info.typeName = "args";
    cases.push_back({"args-bound name", nameExprFor("packed"), {{"packed", info}}});
  }
  // Name-bound local with a vector-family binding.
  {
    BindingInfo info;
    info.typeName = "Vector";
    info.typeTemplateArg = "i32";
    cases.push_back({"vector-bound name", nameExprFor("values"), {{"values", info}}});
  }
  // Name-bound local with a key-value/map-family binding.
  {
    BindingInfo info;
    info.typeName = "Map";
    info.typeTemplateArg = "i32, string";
    cases.push_back({"map-bound name", nameExprFor("lookup"), {{"lookup", info}}});
  }
  // Name-bound local with an unrelated scalar binding: known-false.
  {
    BindingInfo info;
    info.typeName = "i32";
    cases.push_back({"scalar-bound name", nameExprFor("count"), {{"count", info}}});
  }
  // Name-bound local with a Reference-to-array binding.
  {
    BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = "array<i32>";
    cases.push_back({"reference-to-array name", nameExprFor("refItems"), {{"refItems", info}}});
  }
  // Name expr not present in localTypes at all: known-false for all three.
  cases.push_back({"unbound name", nameExprFor("missing"), {}});
  // Name expr present in a non-empty map that doesn't contain it either.
  {
    BindingInfo info;
    info.typeName = "i32";
    cases.push_back({"unbound name amid unrelated bindings", nameExprFor("other"), {{"count", info}}});
  }
  // Call-expr shapes that are not any recognized constructor/method form.
  cases.push_back({"unrelated call", callExprFor("doSomething"), {}});
  cases.push_back({"empty-name call", Expr{}, {}});
  // A Call expr whose name resembles a collection helper but has no
  // resolvable builtin collection name (getBuiltinCollectionName will
  // reject it since it isn't a recognized constructor path).
  {
    Expr call = callExprFor("vector");
    cases.push_back({"bare vector-named call, no template args", call, {}});
  }
  {
    Expr call = callExprFor("map");
    cases.push_back({"bare map-named call, no template args", call, {}});
  }

  return cases;
}

} // namespace

TEST_CASE("isCollectionSurfaceValue matches the union of isCollectionVectorValue and isArrayValue") {
  for (const auto &testCase : buildCollectionKeyValueSurfaceCases()) {
    CAPTURE(testCase.label);
    const bool expected =
        primec::emitter::isCollectionVectorValue(testCase.target, testCase.localTypes) ||
        primec::emitter::isArrayValue(testCase.target, testCase.localTypes);
    CHECK(primec::emitter::isCollectionSurfaceValue(testCase.target, testCase.localTypes) == expected);
  }
}

TEST_CASE("isKeyValueSurfaceValue matches isKeyValueStorageValue") {
  for (const auto &testCase : buildCollectionKeyValueSurfaceCases()) {
    CAPTURE(testCase.label);
    const bool expected =
        primec::emitter::isKeyValueStorageValue(testCase.target, testCase.localTypes);
    CHECK(primec::emitter::isKeyValueSurfaceValue(testCase.target, testCase.localTypes) == expected);
  }
}

TEST_CASE("isCollectionSurfaceValue/isKeyValueSurfaceValue cover known-true and known-false anchor cases") {
  // Anchor cases pinned to concrete expected results, so a degenerate
  // wrapper (e.g. one that always returns false) cannot pass purely by
  // matching a similarly-degenerate comparison helper.
  BindingInfo arrayInfo;
  arrayInfo.typeName = "array";
  std::unordered_map<std::string, BindingInfo> arrayLocals = {{"items", arrayInfo}};
  CHECK(primec::emitter::isCollectionSurfaceValue(nameExprFor("items"), arrayLocals));
  CHECK_FALSE(primec::emitter::isKeyValueSurfaceValue(nameExprFor("items"), arrayLocals));

  BindingInfo scalarInfo;
  scalarInfo.typeName = "i32";
  std::unordered_map<std::string, BindingInfo> scalarLocals = {{"count", scalarInfo}};
  CHECK_FALSE(primec::emitter::isCollectionSurfaceValue(nameExprFor("count"), scalarLocals));
  CHECK_FALSE(primec::emitter::isKeyValueSurfaceValue(nameExprFor("count"), scalarLocals));

  CHECK_FALSE(primec::emitter::isCollectionSurfaceValue(nameExprFor("missing"), {}));
  CHECK_FALSE(primec::emitter::isKeyValueSurfaceValue(nameExprFor("missing"), {}));
}

TEST_SUITE_END();
