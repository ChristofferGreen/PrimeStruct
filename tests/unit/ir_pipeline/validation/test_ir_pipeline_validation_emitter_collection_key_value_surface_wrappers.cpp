#include "test_ir_pipeline_validation_helpers.h"

// TODO-4694: isCollectionSurfaceValue is a behavior-preserving,
// generically-named wrapper unioning today's collection value-
// classification helpers:
//   isCollectionSurfaceValue(target, localTypes) ==
//       isCollectionVectorValue(target, localTypes) ||
//       isArrayValue(target, localTypes)
// isKeyValueSurfaceValue used to be a thin forward to a separate
// isKeyValueStorageValue primitive; TODO-4698 inlined that primitive's body
// directly into isKeyValueSurfaceValue and deleted it (it had no other
// callers left after TODO-4697's migration), so isKeyValueSurfaceValue is
// now tested directly against pinned true/false expectations per case
// (below) rather than against a separate primitive.
// These tests prove that equivalence/expectation across a representative
// set of currently-known Expr/BindingInfo inputs, covering both true and
// false cases (Name-bound locals, unbound names, and Call-expr shapes).

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
  // Pinned expected result for isKeyValueSurfaceValue on this case. Used
  // directly (not via equivalence with a since-deleted primitive) by the
  // "isKeyValueSurfaceValue matches pinned expectations" test below.
  bool expectedKeyValue;
};

std::vector<WrapperCase> buildCollectionKeyValueSurfaceCases() {
  std::vector<WrapperCase> cases;

  // Name-bound local with an array binding: known-true for isArrayValue,
  // known-false for the key-value classifier.
  {
    BindingInfo info;
    info.typeName = "array";
    cases.push_back({"array-bound name", nameExprFor("items"), {{"items", info}}, false});
  }
  // Name-bound local with an args-pack binding: known-true for isArrayValue,
  // known-false for the key-value classifier.
  {
    BindingInfo info;
    info.typeName = "args";
    cases.push_back({"args-bound name", nameExprFor("packed"), {{"packed", info}}, false});
  }
  // Name-bound local with a vector-family binding: known-false for the
  // key-value classifier.
  {
    BindingInfo info;
    info.typeName = "Vector";
    info.typeTemplateArg = "i32";
    cases.push_back({"vector-bound name", nameExprFor("values"), {{"values", info}}, false});
  }
  // Name-bound local with a key-value/map-family binding: known-true for
  // the key-value classifier.
  {
    BindingInfo info;
    info.typeName = "Map";
    info.typeTemplateArg = "i32, string";
    cases.push_back({"map-bound name", nameExprFor("lookup"), {{"lookup", info}}, true});
  }
  // Name-bound local with an unrelated scalar binding: known-false.
  {
    BindingInfo info;
    info.typeName = "i32";
    cases.push_back({"scalar-bound name", nameExprFor("count"), {{"count", info}}, false});
  }
  // Name-bound local with a Reference-to-array binding: known-false for the
  // key-value classifier.
  {
    BindingInfo info;
    info.typeName = "Reference";
    info.typeTemplateArg = "array<i32>";
    cases.push_back({"reference-to-array name", nameExprFor("refItems"), {{"refItems", info}}, false});
  }
  // Name expr not present in localTypes at all: known-false for all three.
  cases.push_back({"unbound name", nameExprFor("missing"), {}, false});
  // Name expr present in a non-empty map that doesn't contain it either.
  {
    BindingInfo info;
    info.typeName = "i32";
    cases.push_back({"unbound name amid unrelated bindings", nameExprFor("other"), {{"count", info}}, false});
  }
  // Call-expr shapes that are not any recognized constructor/method form.
  cases.push_back({"unrelated call", callExprFor("doSomething"), {}, false});
  cases.push_back({"empty-name call", Expr{}, {}, false});
  // A Call expr whose name resembles a collection helper but has no
  // resolvable builtin collection name (getBuiltinCollectionName will
  // reject it since it isn't a recognized constructor path).
  {
    Expr call = callExprFor("vector");
    cases.push_back({"bare vector-named call, no template args", call, {}, false});
  }
  {
    Expr call = callExprFor("map");
    cases.push_back({"bare map-named call, no template args", call, {}, false});
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

TEST_CASE("isKeyValueSurfaceValue matches pinned expectations") {
  // TODO-4698: isKeyValueStorageValue (the primitive this wrapper used to
  // forward to) was deleted -- it had no other callers left after
  // TODO-4697's migration, so its body now lives directly in
  // isKeyValueSurfaceValue. With no separate primitive left to compare
  // against, each case's expected result is pinned directly instead of
  // computed via equivalence.
  for (const auto &testCase : buildCollectionKeyValueSurfaceCases()) {
    CAPTURE(testCase.label);
    CHECK(primec::emitter::isKeyValueSurfaceValue(testCase.target, testCase.localTypes) ==
          testCase.expectedKeyValue);
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
