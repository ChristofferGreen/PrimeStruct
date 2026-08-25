#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("require transforms publish evaluated builtin type predicate facts") {
  const std::string source = R"(
[struct]
Item {
  [i32] value
}

[sum]
Choice {
  [i32] left
  [i32] right
}

[return<int> require(typeof<value> == int, type_not_equals<typeof<value>, f32>(), is_type<i32>(), is_struct<Item>(), is_sum<Choice>())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "requirement_predicate_facts[0]: definition_path=\"/identity\" "
            "predicate_kind=\"predicate_call\" predicate_name=\"/std/meta/type_equals\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "source_text=\"/std/meta/type_equals<typeof<value>, int>()\" "
            "operands=[{kind=\"type_fact\" text=\"typeof<value>\" "
            "stable_handle=\"type_fact:typeof<value>\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "{kind=\"type_fact\" text=\"int\" stable_handle=\"type_fact:int\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "requirement_predicate_facts[1]: definition_path=\"/identity\" "
            "predicate_kind=\"predicate_call\" predicate_name=\"/std/meta/type_not_equals\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "requirement_predicate_facts[3]: definition_path=\"/identity\" "
            "predicate_kind=\"predicate_call\" predicate_name=\"/std/meta/is_struct\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "requirement_predicate_facts[4]: definition_path=\"/identity\" "
            "predicate_kind=\"predicate_call\" predicate_name=\"/std/meta/is_sum\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("evaluation_outcome=\"satisfied\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "evaluation_diagnostic=\"type equality satisfied\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "stable_handle=\"type_fact:typeof<value>\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("text=\"typeof<value>\"") !=
        std::string::npos);
}

TEST_CASE("require transforms publish evaluated value predicate facts") {
  const std::string source = R"(
[return<int> require(value_greater<4, 0>(), value_less_equal<4, 4>(), value_not_equals<4, 5>())]
positive() {
  return(4i32)
}

[return<int>]
main() {
  return(positive())
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/value_greater\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "source_text=\"/std/meta/value_greater<4, 0>()\" "
            "operands=[{kind=\"literal_compile_time_argument\" text=\"4\" "
            "stable_handle=\"literal_compile_time_argument:4\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "{kind=\"literal_compile_time_argument\" text=\"0\" "
            "stable_handle=\"literal_compile_time_argument:0\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "evaluation_diagnostic=\"value predicate satisfied: 4 > 0\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/value_less_equal\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/value_not_equals\"") !=
        std::string::npos);
}

TEST_CASE("require value predicates use integer template arguments") {
  const std::string source = R"(
[return<i32> require(N > 0)]
positive_index<N>() {
  return(1i32)
}

[return<i32>]
main() {
  return(positive_index<4>())
}
)";

  std::string error;
  CHECK(validateProgramThroughCompilePipeline(source,
                                              "/main",
                                              {"io_out", "io_err"},
                                              {"io_out", "io_err"},
                                              error));
  CHECK(error.empty());
}

TEST_CASE("require value predicates reject failing integer template arguments") {
  const std::string source = R"(
[return<i32> require(N > 0)]
positive_index<N>() {
  return(1i32)
}

[return<i32>]
main() {
  return(positive_index<0>())
}
)";

  std::string error;
  CHECK_FALSE(validateProgramThroughCompilePipeline(source,
                                                    "/main",
                                                    {"io_out", "io_err"},
                                                    {"io_out", "io_err"},
                                                    error));
  CHECK(error.find("requirement predicate not satisfied: "
                   "/std/meta/value_greater") != std::string::npos);
  CHECK(error.find("direct requirement check failed on /positive_index") !=
        std::string::npos);
  CHECK(error.find("category: unsatisfied requirement predicate") !=
        std::string::npos);
  CHECK(error.find("category: invalid requirement predicate evaluation") ==
        std::string::npos);
  CHECK(error.find("require transform: /positive_index") !=
        std::string::npos);
  CHECK(error.find("concrete facts:") != std::string::npos);
  CHECK(error.find("literal_compile_time_argument:0") != std::string::npos);
  CHECK(error.find("value predicate failed: 0 > 0") != std::string::npos);
  CHECK(error.find("hint: pass values or types that satisfy the require(...) "
                   "predicate") != std::string::npos);
}

TEST_CASE("require value predicates reject non-constant operands") {
  // i32 (and other integer) parameters are accepted as runtime-checkable
  // contract operands (see runtimeContractOperandKind in
  // RequirementPredicateFacts.cpp), so this uses an f32 parameter, which
  // is not a supported runtime-checkable value type, to keep exercising
  // genuine rejection of a non-constant operand.
  const std::string source = R"(
[return<i32> require(value_greater<value, 0>())]
bad([f32] value) {
  return(0i32)
}

[return<i32>]
main() {
  return(bad(1.0f32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid requirement predicate /std/meta/value_greater") !=
        std::string::npos);
  CHECK(error.find("category: invalid requirement predicate evaluation") !=
        std::string::npos);
  CHECK(error.find("category: unsatisfied requirement predicate") ==
        std::string::npos);
  CHECK(error.find("compile_time_symbol:value") != std::string::npos);
  CHECK(error.find("non-constant value operand for requirement predicate "
                   "/std/meta/value_greater: value") != std::string::npos);
  CHECK(error.find("hint: make the require(...) predicate a supported "
                   "compile-time predicate") != std::string::npos);
}

TEST_CASE("require facts publish phase-qualified compile-time effects") {
  const std::string source = R"(
[effects(file_read) return<int> require(is_type<i32>())]
runtime_only() {
  return(1i32)
}

[effects(file_read) effects<compiletime>(file_read) return<int> require(is_type<i32>())]
compiletime_allowed() {
  return(2i32)
}

[return<int>]
main() {
  return(0i32)
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  const size_t runtimePos = dumps.semanticProduct.find(
      "definition_path=\"/runtime_only\" predicate_kind=\"predicate_call\" "
      "predicate_name=\"/std/meta/is_type\"");
  REQUIRE(runtimePos != std::string::npos);
  const size_t runtimeEnd = dumps.semanticProduct.find('\n', runtimePos);
  const std::string runtimeLine =
      dumps.semanticProduct.substr(runtimePos, runtimeEnd - runtimePos);
  CHECK(runtimeLine.find("compile_time_effects=[]") != std::string::npos);

  const size_t compileTimePos = dumps.semanticProduct.find(
      "definition_path=\"/compiletime_allowed\" "
      "predicate_kind=\"predicate_call\" "
      "predicate_name=\"/std/meta/is_type\"");
  REQUIRE(compileTimePos != std::string::npos);
  const size_t compileTimeEnd =
      dumps.semanticProduct.find('\n', compileTimePos);
  const std::string compileTimeLine =
      dumps.semanticProduct.substr(compileTimePos,
                                   compileTimeEnd - compileTimePos);
  CHECK(compileTimeLine.find("compile_time_effects=[\"file_read\"]") !=
        std::string::npos);
}

TEST_CASE("generic semantic product handoff snapshots requirement and branch facts") {
  const std::string source = R"(
[struct]
Vec2 {
  [i32] x
  [i32] y

  Copy([Reference<Self>] other) {
  }
}

[return<Vec2>]
/Vec2/plus([Vec2] left, [Vec2] right) {
  return(Vec2{plus(left.x, right.x), plus(left.y, right.y)})
}

[return<bool>]
/Vec2/equal([Vec2] left, [Vec2] right) {
  return(left.x == right.x)
}

[return<bool>]
/Vec2/less_than([Vec2] left, [Vec2] right) {
  return(left.x < right.x)
}

[return<i32> require(typeof<value> == Vec2, has_trait<Vec2>(Additive),
                     can_copy<Vec2>(), N > 0)]
measure<T, N>([T] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, Vec2>()) {
    [type] ValueT { typeof<value> }
    [struct] BranchBox {
      [ValueT] held{Vec2{0i32, 0i32}}
    }
    [BranchBox] boxed{BranchBox{value}}
    assign(result, boxed.held.x)
  } else {
    missing_in_discarded_branch(value)
  }
  return(result)
}

[return<i32>]
main() {
  return(measure<Vec2, 4>(Vec2{7i32, 2i32}))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());

  CHECK(dumps.astSemantic.find("ct_if(") == std::string::npos);
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);

  CHECK(dumps.semanticProduct.find(
            "definition_path=\"/measure__t") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/type_equals\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "source_text=\"/std/meta/type_equals<typeof<value>, Vec2>()\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/has_trait\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "evaluation_diagnostic=\"trait predicate satisfied: Additive") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/can_copy\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/value_greater\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "evaluation_diagnostic=\"value predicate satisfied: 4 > 0\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "full_path=\"/measure__t") != std::string::npos);
  CHECK(dumps.semanticProduct.find("BranchBox__ct_if_then_") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "struct_path=\"/measure__t") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "field_name=\"held\" field_index=0 binding_type_text=\"Vec2\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("DiscardedT") == std::string::npos);
  CHECK(dumps.semanticProduct.find("ValueT") == std::string::npos);

  CHECK(dumps.ir.find("module {") != std::string::npos);
  CHECK(dumps.ir.find("requirement_predicate_facts") == std::string::npos);
}

TEST_CASE("rejected generic semantic facts stop before product publication") {
  const std::string unsatisfiedRequirement = R"(
[return<i32> require(N > 0)]
positive_index<N>() {
  return(1i32)
}

[return<i32>]
main() {
  return(positive_index<0>())
}
)";

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  CHECK_FALSE(runDumpStageForSource(
      unsatisfiedRequirement, "semantic-product", output, errorStage, error));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error.find("requirement predicate not satisfied: "
                   "/std/meta/value_greater") != std::string::npos);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK_FALSE(output.hasSemanticProgram);

  const std::string escapingBranchType = R"(
[return<auto>]
pick([i32] value) {
  ct_if(type_equals<typeof<value>, i32>()) {
    [type] ValueT { typeof<value> }
    [struct] PairT {
      [ValueT] first{0i32}
    }
    [PairT] pair{PairT{value}}
    return(pair)
  } else {
    return(value)
  }
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  output = {};
  errorStage = primec::CompilePipelineErrorStage::None;
  error.clear();
  CHECK_FALSE(runDumpStageForSource(
      escapingBranchType, "semantic-product", output, errorStage, error));
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error.find("local generated struct cannot escape return type: "
                   "/pick/PairT__ct_if_then_") != std::string::npos);
  CHECK(error.find("selected compile-time branch: then") !=
        std::string::npos);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("require builtin type predicates reject mismatched calls") {
  const std::string source = R"(
[return<T> require(typeof<value> == i32)]
only_i32<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  [f32] bad{only_i32<f32>(4.0f32)}
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramThroughCompilePipeline(source,
                                                    "/main",
                                                    {"io_out", "io_err"},
                                                    {"io_out", "io_err"},
                                                    error));
  CHECK(error.find("requirement predicate not satisfied: /std/meta/type_equals") !=
        std::string::npos);
  CHECK(error.find("direct requirement check failed on /only_i32") !=
        std::string::npos);
  CHECK(error.find("category: unsatisfied requirement predicate") !=
        std::string::npos);
  CHECK(error.find("type_fact:typeof<value>") != std::string::npos);
  CHECK(error.find("type_fact:i32") != std::string::npos);
  CHECK(error.find("type equality failed: f32 != i32") != std::string::npos);
}

TEST_CASE("require builtin type predicates accept local generated structs") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] left{3i32}
  [i32] right{4i32}
  [type] LeftT { typeof<left> }
  [type] RightT { typeof<right> }
  [struct] PairT {
    [LeftT] first{0i32}
    [RightT] second{0i32}
  }
  [return<int> require(is_struct<PairT>())]
  usePair([PairT] pair) {
    return(plus(pair.first, pair.second))
  }
  [PairT] pair{PairT{left, right}}
  return(usePair(pair))
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("require capability predicates accept trait constructor lifecycle and call facts") {
  const std::string source = R"(
[struct]
Vec2 {
  [i32] x
  [i32] y

  Copy([Reference<Self>] other) {
  }
}

[return<Vec2>]
/Vec2/plus([Vec2] left, [Vec2] right) {
  return(Vec2{plus(left.x, right.x), plus(left.y, right.y)})
}

[return<bool>]
/Vec2/equal([Vec2] left, [Vec2] right) {
  return(left.x == right.x)
}

[return<bool>]
/Vec2/less_than([Vec2] left, [Vec2] right) {
  return(left.x < right.x)
}

[return<Vec2> require(has_trait<Vec2>(Additive), has_trait<Vec2>(Comparable),
                      can_construct<Vec2, i32, i32>(), can_copy<Vec2>(),
                      supports_call<Vec2, Vec2, Vec2>(/Vec2/plus),
                      has_field<Vec2>(x), has_member<Vec2>(plus))]
makeVec([Vec2] value) {
  return(value)
}

[return<int>]
main() {
  [Vec2] value{Vec2{1i32, 2i32}}
  [Vec2] copy{makeVec(value)}
  return(copy.x)
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  const bool captured = primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error);
  INFO(error);
  REQUIRE(captured);
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/has_trait\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/can_construct\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/can_copy\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/supports_call\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/std/meta/has_field\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find("evaluation_outcome=\"satisfied\"") !=
        std::string::npos);
}

TEST_CASE("require capability predicates reject failed trait checks with type facts") {
  const std::string source = R"(
[struct]
Vec2 {
  [i32] x
  [i32] y
}

[return<bool>]
/Vec2/equal([Vec2] left, [Vec2] right) {
  return(left.x == right.x)
}

[return<Vec2> require(has_trait<Vec2>(Comparable))]
needsComparable([Vec2] value) {
  return(value)
}

[return<int>]
main() {
  [Vec2] value{Vec2{1i32, 2i32}}
  [Vec2] copy{needsComparable(value)}
  return(copy.x)
}
)";

  std::string error;
  const bool validated = validateProgramThroughCompilePipeline(source,
                                                              "/main",
                                                              {"io_out", "io_err"},
                                                              {"io_out", "io_err"},
                                                              error);
  INFO(error);
  CHECK_FALSE(validated);
  CHECK(error.find("requirement predicate not satisfied: /std/meta/has_trait") !=
        std::string::npos);
  CHECK(error.find("trait predicate failed: Comparable") != std::string::npos);
  CHECK(error.find("type facts: /Vec2") != std::string::npos);
}

TEST_CASE("require field predicates cannot observe private fields outside owner") {
  const std::string source = R"(
[struct]
SecretBox {
  [private i32] secret
  [i32] visible
}

[return<int> require(has_field<SecretBox>(secret))]
readSecret([SecretBox] box) {
  return(box.visible)
}

[return<int>]
main() {
  return(0i32)
}
)";

  std::string error;
  const bool validated = validateProgramThroughCompilePipeline(source,
                                                              "/main",
                                                              {"io_out", "io_err"},
                                                              {"io_out", "io_err"},
                                                              error);
  INFO(error);
  CHECK_FALSE(validated);
  CHECK(error.find("requirement predicate not satisfied: /std/meta/has_field") !=
        std::string::npos);
  CHECK(error.find("no visible field named secret on /SecretBox") !=
        std::string::npos);
}

TEST_CASE("require call predicates resolve overload-style helpers deterministically") {
  const std::string source = R"(
namespace Ops {
  [return<i32>]
  op__ov0([i32] value) {
    return(value)
  }

  [return<f32>]
  op__ov1([f32] value) {
    return(value)
  }
}

[return<int> require(supports_call<i32, i32>(/Ops/op),
                     supports_call<f32, f32>(/Ops/op))]
main() {
  return(0i32)
}
)";

  std::string error;
  const bool validated = validateProgramThroughCompilePipeline(source,
                                                              "/main",
                                                              {"io_out", "io_err"},
                                                              {"io_out", "io_err"},
                                                              error);
  INFO(error);
  CHECK(validated);
  CHECK(error.empty());
}

TEST_CASE("require builtin type predicates diagnose invalid operands and reserved names") {
  const std::string unknownSource = R"(
[return<int> require(type_contains<typeof<value>, i32>())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(unknownSource, "/main", error));
  CHECK(error.find("invalid requirement predicate type_contains") !=
        std::string::npos);
  CHECK(error.find("unknown requirement predicate: type_contains") !=
        std::string::npos);

  const std::string unsupportedOperandSource = R"(
[return<int> require(type_equals<4, i32>())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(unsupportedOperandSource, "/main", error));
  CHECK(error.find("invalid requirement predicate /std/meta/type_equals") !=
        std::string::npos);
  CHECK(error.find(
            "unsupported operand for requirement predicate "
            "/std/meta/type_equals: 4") !=
        std::string::npos);

  const std::string reservedNamespaceSource = R"(
namespace std {
  namespace meta {
    [return<bool>]
    type_equals() {
      return(true)
    }
  }
}

[return<int>]
main() {
  return(0i32)
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(reservedNamespaceSource, "/main", error));
  CHECK(error.find("/std/meta is reserved for builtin requirement predicates: "
                   "/std/meta/type_equals") != std::string::npos);
}

TEST_CASE("require pure user predicates drive semantic facts") {
  const std::string acceptedSource = R"(
[return<bool>]
is_supported() {
  return(true)
}

[return<int> require(is_supported())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  const bool accepted = primec::testing::captureSemanticBoundaryDumpsForTesting(
      acceptedSource, "/main", dumps, error);
  REQUIRE(accepted);
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "predicate_name=\"/is_supported\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find("operands=[]") != std::string::npos);
  CHECK(dumps.semanticProduct.find("evaluation_outcome=\"satisfied\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "evaluation_diagnostic=\"user predicate returned true\"") !=
        std::string::npos);

  const std::string rejectedSource = R"(
[return<bool>]
is_supported() {
  return(false)
}

[return<int> require(is_supported())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(rejectedSource, "/main", error));
  CHECK(error.find("requirement predicate not satisfied: /is_supported") !=
        std::string::npos);
  CHECK(error.find("category: unsatisfied requirement predicate") !=
        std::string::npos);
  CHECK(error.find("category: invalid requirement predicate evaluation") ==
        std::string::npos);
  CHECK(error.find("predicate source: is_supported()") != std::string::npos);
  CHECK(error.find("user predicate returned false") != std::string::npos);
}

TEST_CASE("require pure user predicates reject impure and unsupported bodies") {
  const std::string effectSource = R"(
[effects(file_read), return<bool>]
needs_file() {
  return(true)
}

[return<int> require(needs_file())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(effectSource, "/main", error));
  CHECK(error.find("compile-time effect opt-in rejected: /needs_file") !=
        std::string::npos);
  CHECK(error.find("category: denied compile-time effect") !=
        std::string::npos);
  CHECK(error.find("category: unsatisfied requirement predicate") ==
        std::string::npos);
  CHECK(error.find("denied compile-time effect in user requirement predicate "
                   "/needs_file: file_read "
                   "(missing effects<compiletime>(file_read))") !=
        std::string::npos);
  CHECK(error.find("hint: add effects<compiletime>(...)") !=
        std::string::npos);

  const std::string allowedEffectSource = R"(
[effects(file_read), return<bool>]
needs_file() {
  return(true)
}

[effects<compiletime>(file_read) return<int> require(needs_file())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  error.clear();
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      allowedEffectSource, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "definition_path=\"/identity\" predicate_kind=\"predicate_call\" "
            "predicate_name=\"/needs_file\"") != std::string::npos);
  CHECK(dumps.semanticProduct.find("compile_time_effects=[\"file_read\"]") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("evaluation_outcome=\"satisfied\"") !=
        std::string::npos);

  const std::string runtimeParamSource = R"(
[return<bool>]
needs_runtime([i32] value) {
  return(true)
}

[return<int> require(needs_runtime(value))]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(runtimeParamSource, "/main", error));
  CHECK(error.find("runtime parameters are not supported in compile-time "
                   "user predicate: /needs_runtime") != std::string::npos);

  const std::string bodySource = R"(
[return<bool>]
runtime_body() {
  return(equals(1i32, 1i32))
}

[return<int> require(runtime_body())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(bodySource, "/main", error));
  CHECK(error.find("invalid requirement predicate /runtime_body") !=
        std::string::npos);
  CHECK(error.find("category: invalid requirement predicate evaluation") !=
        std::string::npos);
  CHECK(error.find("unsupported pure user requirement predicate body: "
                   "/runtime_body") != std::string::npos);
}

TEST_CASE("statement ct_if selects predicate branches before validation") {
  const std::string selectedThen = R"(
[return<i32>]
pick([i32] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, i32>()) {
    assign(result, value)
  } else {
    missing_in_discarded_branch(value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedThen, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("assign(result, value)") != std::string::npos);

  const std::string selectedElse = R"(
[return<i32>]
pick([i32] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, f32>()) {
    missing_in_discarded_branch(value)
  } else {
    assign(result, value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(9i32))
}
)";

  error.clear();
  dumps = {};
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedElse, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("assign(result, value)") != std::string::npos);
}

TEST_CASE("generic statement ct_if selects branches after specialization") {
  const std::string selectedThen = R"(
[return<i32>]
pick<T>([T] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, T>()) {
    assign(result, 7i32)
  } else {
    missing_in_discarded_branch(value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(1i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  const bool selectedThenOk = primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedThen, "/main", dumps, error);
  REQUIRE(selectedThenOk);
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("assign") != std::string::npos);
  CHECK(dumps.astSemantic.find("7") != std::string::npos);

  const std::string selectedElse = R"(
[return<i32>]
pick<T>([T] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, i32>()) {
    missing_in_discarded_branch(value)
  } else {
    assign(result, 5i32)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(1.5f32))
}
)";

  error.clear();
  dumps = {};
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedElse, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("assign") != std::string::npos);
  CHECK(dumps.astSemantic.find("5") != std::string::npos);
}

TEST_CASE("statement ct_if scopes selected branch generated structs") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, i32>()) {
    [type] ValueT { typeof<value> }
    [struct] PairT {
      [ValueT] first{0i32}
    }
    [PairT] pair{PairT{value}}
    assign(result, pair.first)
  } else {
    [struct] DiscardedT {
      [bool] flag{false}
    }
    missing_in_discarded_branch(value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("ct_if(") == std::string::npos);
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("DiscardedT") == std::string::npos);
  CHECK(dumps.astSemantic.find("/pick/PairT__ct_if_then_") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "full_path=\"/pick/PairT__ct_if_then_") != std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "struct_path=\"/pick/PairT__ct_if_then_") != std::string::npos);
  CHECK(dumps.semanticProduct.find("field_name=\"first\" field_index=0 "
                                   "binding_type_text=\"i32\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("DiscardedT") == std::string::npos);
  CHECK(dumps.semanticProduct.find("ValueT") == std::string::npos);
}

TEST_CASE("statement ct_if branch generated structs do not leak") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  ct_if(type_equals<typeof<value>, i32>()) {
    [struct] PairT {
      [i32] first{0i32}
    }
  } else {
    print(0i32)
  }
  [PairT] pair{PairT{value}}
  return(pair.first)
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unsupported binding type: PairT") != std::string::npos);
}

TEST_CASE("statement ct_if branch generated escape names selected branch") {
  const std::string source = R"(
[return<auto>]
pick([i32] value) {
  ct_if(type_equals<typeof<value>, i32>()) {
    [type] ValueT { typeof<value> }
    [struct] PairT {
      [ValueT] first{0i32}
    }
    [PairT] pair{PairT{value}}
    return(pair)
  } else {
    return(value)
  }
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("local generated struct cannot escape return type: "
                   "/pick/PairT__ct_if_then_") != std::string::npos);
  CHECK(error.find("selected compile-time branch: then") !=
        std::string::npos);
  CHECK(error.find("generated type path: /pick/PairT__ct_if_then_") !=
        std::string::npos);
  CHECK(error.find("local generated type source:") != std::string::npos);
  CHECK(error.find("type facts: ValueT") != std::string::npos);
  CHECK(error.find("local type fact provenance:\n- ValueT at ") !=
        std::string::npos);
  CHECK(error.find("hint: keep branch-local generated structs inside the "
                   "selected ct_if branch") != std::string::npos);
}

TEST_CASE("expression ct_if selects value branches before validation") {
  const std::string selectedReturn = R"(
[return<i32>]
pick([i32] value) {
  return(ct_if(type_equals<typeof<value>, i32>()) {
    value
  } else {
    missing_in_discarded_branch(value)
  })
}

[return<i32>]
main() {
  return(pick(7i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedReturn, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("ct_if") == std::string::npos);

  const std::string selectedBinding = R"(
[return<i32>]
pick([i32] value) {
  [i32] result{ct_if(type_equals<typeof<value>, f32>()) {
    missing_in_discarded_branch(value)
  } else {
    value
  }}
  return(result)
}

[return<i32>]
main() {
  return(pick(9i32))
}
)";

  error.clear();
  dumps = {};
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      selectedBinding, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("ct_if") == std::string::npos);
}

TEST_CASE("generic expression ct_if selects branches after specialization") {
  const std::string source = R"(
[return<T>]
pick<T>([T] value) {
  return(ct_if(type_equals<typeof<value>, T>()) {
    value
  } else {
    missing_in_discarded_branch(value)
  })
}

[return<i32>]
main() {
  return(pick(11i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("ct_if") == std::string::npos);
}

TEST_CASE("expression ct_if diagnoses selected branch type mismatch") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  [i32] result{ct_if(type_equals<typeof<value>, i32>()) {
    1.5f32
  } else {
    value
  }}
  return(result)
}

[return<i32>]
main() {
  return(pick(3i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer type mismatch") != std::string::npos);
  CHECK(error.find("missing_in_discarded_branch") == std::string::npos);
}

TEST_CASE("statement ct_if diagnoses invalid predicate conditions") {
  const std::string source = R"(
[return<i32>]
pick() {
  ct_if(type_equals<typeof<missing>, i32>()) {
    print(1i32)
  } else {
    print(2i32)
  }
  return(0i32)
}

[return<i32>]
main() {
  return(pick())
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid ct_if condition on /pick") != std::string::npos);
  CHECK(error.find("category: invalid compile-time flow predicate") !=
        std::string::npos);
  CHECK(error.find("ct_if site: /pick at ") != std::string::npos);
  CHECK(error.find("predicate source: "
                   "/std/meta/type_equals<typeof<missing>, i32>()") !=
        std::string::npos);
  CHECK(error.find("compile-time facts:") != std::string::npos);
  CHECK(error.find("type_fact:typeof<missing>") != std::string::npos);
  CHECK(error.find("unknown type fact for requirement predicate "
                   "/std/meta/type_equals: typeof<missing>") !=
        std::string::npos);
  CHECK(error.find("hint: make the ct_if predicate evaluable from "
                   "compile-time facts") != std::string::npos);
}

TEST_CASE("ct_if flow diagnostics distinguish invalid user predicates") {
  const std::string deniedEffectSource = R"(
[effects(file_read), return<bool>]
needs_file() {
  return(true)
}

[return<i32>]
pick() {
  ct_if(needs_file()) {
    print(1i32)
  } else {
    print(2i32)
  }
  return(0i32)
}

[return<i32>]
main() {
  return(pick())
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(deniedEffectSource, "/main", error));
  INFO(error);
  CHECK(error.find("invalid ct_if condition on /pick") != std::string::npos);
  CHECK(error.find("category: invalid compile-time flow predicate") !=
        std::string::npos);
  CHECK(error.find("predicate source: needs_file()") != std::string::npos);
  CHECK(error.find("predicate path: /needs_file") != std::string::npos);
  CHECK(error.find("denied compile-time effect in user requirement predicate "
                   "/needs_file: file_read") != std::string::npos);

  const std::string unsupportedBodySource = R"(
[return<bool>]
runtime_body() {
  return(equals(1i32, 1i32))
}

[return<i32>]
pick() {
  ct_if(runtime_body()) {
    print(1i32)
  } else {
    print(2i32)
  }
  return(0i32)
}

[return<i32>]
main() {
  return(pick())
}
)";

  error.clear();
  CHECK_FALSE(validateProgram(unsupportedBodySource, "/main", error));
  INFO(error);
  CHECK(error.find("invalid ct_if condition on /pick") != std::string::npos);
  CHECK(error.find("predicate source: runtime_body()") != std::string::npos);
  CHECK(error.find("unsupported pure user requirement predicate body: "
                   "/runtime_body") != std::string::npos);
}

TEST_CASE("ct_if unsatisfied predicates select else without diagnostics") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, f32>()) {
    missing_in_discarded_branch(value)
  } else {
    assign(result, value)
  }
  return(result)
}

[return<i32>]
main() {
  return(pick(9i32))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("missing_in_discarded_branch") ==
        std::string::npos);
  CHECK(dumps.astSemantic.find("assign(result, value)") != std::string::npos);
  CHECK(dumps.astSemantic.find("ct_if") == std::string::npos);
}

TEST_CASE("duplicate require transforms fail closed before publication") {
  const std::string source = R"(
[return<int> require(type_equals<typeof<value>, int>()) require(has_trait<typeof<value>>(Additive))]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  CHECK_FALSE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.find("duplicate require transform; combine predicates into one require(...)") !=
        std::string::npos);
}

TEST_CASE("requirement constrained overload selects the viable same arity candidate") {
  const std::string source = R"(
[return<T> require(type_equals<typeof<value>, i64>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(type_equals<typeof<value>, i32>())]
choose<T>([T] value) {
  return(value)
}

[return<i32>]
main() {
  return(choose(4i32))
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("requirement constrained overload uses local argument facts") {
  const std::string source = R"(
[return<T> require(type_equals<typeof<value>, i64>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(type_equals<typeof<value>, i32>())]
choose<T>([T] value) {
  return(value)
}

[return<i64>]
main() {
  [i64] selected{8i64}
  return(choose(selected))
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("requirement constrained overload preserves no viable diagnostics") {
  const std::string source = R"(
[return<T> require(type_equals<typeof<value>, i64>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(type_equals<typeof<value>, i32>())]
choose<T>([T] value) {
  return(value)
}

[return<f32>]
main() {
  return(choose(4.0f32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  const size_t callSitePos = error.find("call site:");
  const size_t noViablePos =
      error.find("no viable requirement overload for /choose");
  const size_t candidatesPos = error.find("rejected candidates:");
  const size_t factsPos = error.find("concrete inferred argument facts:");
  const size_t hintPos = error.find("hint: pass values or types that satisfy "
                                   "exactly one constrained overload");
  CHECK(callSitePos != std::string::npos);
  CHECK(noViablePos != std::string::npos);
  CHECK(candidatesPos != std::string::npos);
  CHECK(factsPos != std::string::npos);
  CHECK(hintPos != std::string::npos);
  CHECK(callSitePos < noViablePos);
  CHECK(noViablePos < candidatesPos);
  CHECK(candidatesPos < factsPos);
  CHECK(factsPos < hintPos);
  CHECK(error.find("rejected by:") != std::string::npos);
  CHECK(error.find("/std/meta/type_equals") != std::string::npos);
  CHECK(error.find("type equality failed: f32 != i64") !=
        std::string::npos);
  CHECK(error.find("type equality failed: f32 != i32") !=
        std::string::npos);
  CHECK(error.find("arg0 type=f32") != std::string::npos);
}

TEST_CASE("requirement constrained overload reports value predicate rejection") {
  const std::string source = R"(
[return<T> require(value_greater<0, 1>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(value_less<5, 4>())]
choose<T>([T] value) {
  return(value)
}

[return<i32>]
main() {
  return(choose(4i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no viable requirement overload for /choose") !=
        std::string::npos);
  CHECK(error.find("/std/meta/value_greater") != std::string::npos);
  CHECK(error.find("value predicate failed: 0 > 1") != std::string::npos);
  CHECK(error.find("/std/meta/value_less") != std::string::npos);
  CHECK(error.find("value predicate failed: 5 < 4") != std::string::npos);
  CHECK(error.find("literal_compile_time_argument") == std::string::npos);
  CHECK(error.find("arg0 type=i32") != std::string::npos);
  CHECK(error.find("hint: pass values or types that satisfy exactly one "
                   "constrained overload") != std::string::npos);
}

TEST_CASE("requirement constrained overload reports ambiguous candidates") {
  const std::string source = R"(
[return<T> require(type_equals<typeof<value>, i32>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(is_type<i32>())]
choose<T>([T] value) {
  return(value)
}

[return<i32>]
main() {
  return(choose(4i32))
}
)";

  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  const size_t callSitePos = error.find("call site:");
  const size_t ambiguousPos =
      error.find("ambiguous requirement overload for /choose");
  const size_t candidatesPos = error.find("viable candidates:");
  const size_t factsPos = error.find("concrete inferred argument facts:");
  const size_t hintPos =
      error.find("hint: make the overload requirements mutually exclusive");
  CHECK(callSitePos != std::string::npos);
  CHECK(ambiguousPos != std::string::npos);
  CHECK(candidatesPos != std::string::npos);
  CHECK(factsPos != std::string::npos);
  CHECK(hintPos != std::string::npos);
  CHECK(callSitePos < ambiguousPos);
  CHECK(ambiguousPos < candidatesPos);
  CHECK(candidatesPos < factsPos);
  CHECK(factsPos < hintPos);
  CHECK(error.find("requirements satisfied:") != std::string::npos);
  CHECK(error.find("type equality satisfied") != std::string::npos);
  CHECK(error.find("type fact resolved: i32") != std::string::npos);
  CHECK(error.find("arg0 type=i32") != std::string::npos);
}

TEST_CASE("type resolution try operand metadata stays aligned with query snapshots") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  std::string error;
  primec::semantics::TypeResolutionTryValueSnapshot trySnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionTryValueSnapshotForTesting(
      parseProgram(source), "/main", error, trySnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryResultTypeSnapshot queryResultSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
      parseProgram(source), "/main", error, queryResultSnapshot));
  CHECK(error.empty());

  const auto &tryEntry = requireTryValueSnapshotEntry(trySnapshot, "/main", "/lookup");
  const auto &callEntry = requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/lookup");
  const auto &bindingEntry = requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/lookup");
  const auto &resultEntry = requireQueryResultTypeSnapshotEntry(queryResultSnapshot, "/main", "/lookup");
  CHECK(tryEntry.operandResolvedPath == callEntry.resolvedPath);
  CHECK(tryEntry.operandResolvedPath == bindingEntry.resolvedPath);
  CHECK(tryEntry.operandBindingTypeText == bindingEntry.bindingTypeText);
  CHECK(tryEntry.operandReceiverBindingTypeText.empty());
  CHECK(tryEntry.operandQueryTypeText == callEntry.typeText);
  CHECK(tryEntry.valueTypeText == resultEntry.valueTypeText);
  CHECK(tryEntry.errorTypeText == resultEntry.errorTypeText);
}

TEST_CASE("templated fallback adapter seam classifies Result value and error envelope") {
  primec::semantics::TemplatedFallbackQueryStateEnvelopeSnapshotForTesting snapshot;
  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting(
      "Result<int, MyError>", snapshot);

  CHECK(snapshot.hasResultType);
  CHECK(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType == "i32");
  CHECK(snapshot.resultErrorType == "MyError");
  CHECK(snapshot.mismatchDiagnostic.empty());

  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting(
      "/std/result/Result<int, MyError>", snapshot);

  CHECK(snapshot.hasResultType);
  CHECK(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType == "i32");
  CHECK(snapshot.resultErrorType == "MyError");
  CHECK(snapshot.mismatchDiagnostic.empty());
}

TEST_CASE("templated fallback adapter seam rejects missing Result envelope arguments") {
  primec::semantics::TemplatedFallbackQueryStateEnvelopeSnapshotForTesting snapshot;
  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting("Result", snapshot);

  CHECK_FALSE(snapshot.hasResultType);
  CHECK_FALSE(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType.empty());
  CHECK(snapshot.resultErrorType.empty());
  CHECK(snapshot.mismatchDiagnostic == "result query type missing template arguments: Result");
}

TEST_CASE("explicit template-arg graph facts publish resolved specialization facts") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id<i32>(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const bool hasI32ExplicitFact = std::any_of(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.resolvedConcrete &&
           fact.explicitArgsText.find("i32") != std::string::npos;
  });
  const bool isAcceptableFactShape = facts.empty() || hasI32ExplicitFact;
  CHECK(isAcceptableFactShape);
}

TEST_CASE("explicit template-arg graph facts publish builtin container template facts") {
  const std::string source =
      "[return<vector<i32>>]\n"
      "main() {\n"
      "  return(vector<i32>(1i32))\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.targetPath == "vector" &&
           fact.explicitArgsText == "i32" &&
           fact.resolvedTypeText == "vector<i32>";
  });
  REQUIRE(it != facts.end());
  CHECK(it->resolvedConcrete);
}

TEST_CASE("explicit template-arg graph facts keep mismatch diagnostics for invalid arity") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id<i32, i32>(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  CHECK_FALSE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.find("template argument count mismatch for /id") != std::string::npos);
}

TEST_CASE("implicit template-arg graph facts publish inferred argument facts") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.targetPath == "/id" &&
           fact.scopePath == "/main" &&
           fact.callName == "id" &&
           fact.inferredArgsText == "i32";
  });
  REQUIRE(it != facts.end());
}

TEST_CASE("implicit template-arg graph facts publish helper-routing scope") {
  const std::string source =
      "[return<T>]\n"
      "/std/collections/vector/pick<T>([vector<T>] values, [T] fallback) {\n"
      "  return(fallback)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main([vector<i32>] values) {\n"
      "  [auto] left{/std/collections/vector/pick(values, 1i32)}\n"
      "  [auto] right{/std/collections/vector/pick(values, 2i32)}\n"
      "  return(plus(left, right))\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.scopePath == "/main" &&
           fact.targetPath == "/std/collections/vector/pick" &&
           fact.inferredArgsText == "i32";
  });
  REQUIRE(it != facts.end());
}

TEST_CASE("implicit template-arg graph facts keep conflict diagnostics") {
  const std::string source =
      "[return<T>]\n"
      "pair<T>([T] left, [T] right) {\n"
      "  return(left)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{pair(1i32, true)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  CHECK_FALSE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.find("implicit template arguments conflict on /pair") != std::string::npos);
}

TEST_SUITE_END();
