#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

const std::string RetiredSoaVectorDiagnostic =
    "soa_vector<T> is not supported; use soa<T>";
const std::string NonTemplatedSoaVectorDiagnostic =
    "template arguments are only supported on templated definitions: /soa_vector";

} // namespace

TEST_CASE("to_soa rejects named arguments while retired to_aos spelling rejects before named args") {
  const auto checkReject = [](const std::string &setup,
                              const std::string &callExpr,
                              const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        + setup +
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  const std::string namedArgs = "named arguments not supported for builtin calls";
  checkReject("  [vector<Particle>] values{vector<Particle>()}\n",
              "to_soa([values] values)", namedArgs);
  checkReject("  [soa_vector<Particle>] packed{soa_vector<Particle>()}\n",
              "to_aos([values] packed)", RetiredSoaVectorDiagnostic);
  checkReject("  [vector<Particle>] values{vector<Particle>()}\n",
              "values.to_soa([values] values)", namedArgs);
  checkReject("  [soa_vector<Particle>] packed{soa_vector<Particle>()}\n",
              "packed.to_aos([values] packed)", RetiredSoaVectorDiagnostic);
}

TEST_CASE("retired soa_vector access forms reject before named arguments") {
  const auto checkReject = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
  };

  checkReject("get([index] 0i32, [values] values)");
  checkReject("get_ref([index] 0i32, [values] location(values))");
  checkReject("location(values).get_ref([index] 0i32)");
  checkReject("ref([index] 0i32, [values] values)");
  checkReject("ref_ref([index] 0i32, [values] values)");
  checkReject("values.ref_ref([index] 0i32)");
  checkReject("/soa_vector/get([index] 0i32, [values] values)");
  checkReject("/soa_vector/get_ref([index] 0i32, [values] location(values))");
  checkReject("/soa_vector/ref([index] 0i32, [values] values)");
  checkReject("/soa_vector/ref_ref([index] 0i32, [values] values)");
}

TEST_CASE("retired soa_vector index forms reject before integer index checks") {
  const auto checkIndexType = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
  };

  checkIndexType("get(values, true)");
  checkIndexType("get_ref(location(values), true)");
  checkIndexType("values.ref(1.0f32)");
  checkIndexType("ref_ref(values, true)");
  checkIndexType("values.ref_ref(1.0f32)");
}

TEST_CASE("old-surface soa_vector access forms reject retired spelling before indices") {
  const auto checkValidate = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
  };

  checkValidate("/soa_vector/get(values, true)");
  checkValidate("/soa_vector/get_ref(location(values), 1.0f32)");
  checkValidate("/soa_vector/ref(values, 1.0f32)");
  checkValidate("/soa_vector/ref_ref(values, 1.0f32)");
}

TEST_CASE("soa_vector conversion and access builtins reject template arguments") {
  const auto checkReject = [](const std::string &setup, const std::string &callExpr, const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n" +
        setup +
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa<i32>(values)",
              "to_soa does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "to_aos<i32>(values)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "values.to_soa<i32>()",
              "to_soa does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "values.to_aos<i32>()",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "get<i32>(values, 0i32)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "get_ref<i32>(location(values), 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref<i32>(values, 0i32)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref_ref<i32>(values, 0i32)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "/soa_vector/get<i32>(values, 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "/soa_vector/ref<i32>(values, 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "/soa_vector/ref_ref<i32>(values, 0i32)", RetiredSoaVectorDiagnostic);
}

TEST_CASE("soa_vector canonical get_ref rejects retired spelling before expression-only check") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [auto] item{/soa_vector/get_ref<i32>(location(values), 0i32)}
  return(item.x)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector conversion and access builtins reject block arguments") {
  const auto checkAccept = [](const std::string &setup, const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n" +
        setup +
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.empty());
  };
  const auto checkReject = [](const std::string &setup, const std::string &callExpr, const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n" +
        setup +
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkAccept("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa(values) { return(values) }");
  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "values.to_soa() { return(values) }",
              "block arguments require a definition target");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "to_aos(values) { return(values) }", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "values.to_aos() { return(values) }", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "get(values, 0i32) { return(values) }", RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "ref(values, 0i32) { return(values) }", RetiredSoaVectorDiagnostic);
}

TEST_CASE("soa_vector conversion and access builtins enforce argument counts") {
  const auto checkReject = [](const std::string &setup, const std::string &callExpr, const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n" +
        setup +
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(callExpr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa()",
              "argument count mismatch for builtin to_soa");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "to_aos(values, values)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "values.to_soa(values)",
              "argument count mismatch for builtin to_soa");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "values.to_aos(values)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "get(values)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "get_ref(location(values))",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "get_ref(location(values), 0i32, 1i32)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref(values, 0i32, 1i32)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref_ref(values)",
              RetiredSoaVectorDiagnostic);
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref_ref(values, 0i32, 1i32)",
              RetiredSoaVectorDiagnostic);
}

TEST_CASE("soa_vector builtin construction rejects retired spelling") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] ctorValues{soa_vector<Particle>()}
  [soa_vector<Particle>] braceValues{}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector builtin ref local bindings reject retired spelling") {
  const auto checkReject = [](const std::string &bindingType, const std::string &bindingInit) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  [" + bindingType + "] item{" + bindingInit + "}\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(bindingInit);
    INFO(error);
    CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
  };

  checkReject("auto", "ref(values, 0i32)");
  checkReject("auto", "values.ref(0i32)");
  checkReject("auto", "/soa_vector/ref(values, 0i32)");
  checkReject("Particle", "ref(values, 0i32)");
  checkReject("Particle", "values.ref(0i32)");
  checkReject("Particle", "/soa_vector/ref(values, 0i32)");
}

TEST_CASE("soa_vector helper-return ref/ref_ref local bindings reject non-templated spelling") {
  const auto checkReject = [](const std::string &bindingType,
                              const std::string &bindingInit,
                              const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<soa_vector<Particle>>]\n"
        "cloneValues() {\n"
        "  return(soa_vector<Particle>())\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [" + bindingType + "] item{" + bindingInit + "}\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(bindingInit);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("auto", "ref(cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("auto", "cloneValues().ref(0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("Particle", "ref(cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("Particle", "cloneValues().ref(0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("auto", "ref_ref(cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("auto", "cloneValues().ref_ref(0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("Particle", "ref_ref(cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("Particle", "cloneValues().ref_ref(0i32)", NonTemplatedSoaVectorDiagnostic);
}

TEST_CASE("soa_vector ref helper binding rejects retired spelling before user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] idx{vector<i32>(0i32)}
  [auto] item{values.ref(idx)}
  return(item)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector builtin ref call argument escapes use pending diagnostic") {
  const auto checkReject = [](const std::string &expr, const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "consume<T>([T] value) {\n"
        "  return(0i32)\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  return(consume(" + expr + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(expr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("ref(values, 0i32)",
              "unknown method: /std/collections/soa_vector/ref");
  checkReject("values.ref(0i32)",
              "unknown method: /std/collections/soa_vector/ref");
  checkReject("/soa_vector/ref(values, 0i32)", "unknown method: /std/collections/soa_vector/ref");
  checkReject("ref_ref(values, 0i32)", "unknown method: /std/collections/soa_vector/ref_ref");
  checkReject("values.ref_ref(0i32)", "unknown method: /std/collections/soa_vector/ref_ref");
  checkReject("/soa_vector/ref_ref(values, 0i32)",
              "unknown method: /std/collections/soa_vector/ref_ref");
}

TEST_CASE("soa_vector helper-return ref/ref_ref call argument escapes reject non-templated spelling") {
  const auto checkReject = [](const std::string &expr,
                              const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "Holder() {}\n\n"
        "[return<soa_vector<Particle>>]\n"
        "/Holder/cloneValues([Holder] self) {\n"
        "  return(soa_vector<Particle>())\n"
        "}\n\n"
        "[return<int>]\n"
        "consume<T>([T] value) {\n"
        "  return(0i32)\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [Holder] holder{Holder()}\n"
        "  return(consume(" + expr + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(expr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("ref(holder.cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("ref_ref(holder.cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
}

TEST_CASE("soa_vector builtin ref/ref_ref return escapes reject retired spelling") {
  const auto checkReject = [](const std::string &expr,
                              const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<auto>]\n"
        "pick([soa_vector<Particle>] values) {\n"
        "  return(" + expr + ")\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  pick(values)\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/pick", error));
    INFO(expr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("ref(values, 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("values.ref(0i32)", RetiredSoaVectorDiagnostic);
  checkReject("/soa_vector/ref(values, 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("ref_ref(values, 0i32)", RetiredSoaVectorDiagnostic);
  checkReject("values.ref_ref(0i32)", RetiredSoaVectorDiagnostic);
  checkReject("/soa_vector/ref_ref(values, 0i32)", RetiredSoaVectorDiagnostic);
}

TEST_CASE("soa_vector helper-return ref/ref_ref return escapes reject non-templated spelling") {
  const auto checkReject = [](const std::string &expr,
                              const std::string &expected) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "Holder() {}\n\n"
        "[return<soa_vector<Particle>>]\n"
        "/Holder/cloneValues([Holder] self) {\n"
        "  return(soa_vector<Particle>())\n"
        "}\n\n"
        "[return<auto>]\n"
        "pick([Holder] holder) {\n"
        "  return(" + expr + ")\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [Holder] holder{Holder()}\n"
        "  pick(holder)\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/pick", error));
    INFO(expr);
    INFO(error);
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("ref(holder.cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
  checkReject("ref_ref(holder.cloneValues(), 0i32)", NonTemplatedSoaVectorDiagnostic);
}

TEST_CASE("soa_vector ref helper rejects retired spelling before same-path helper escapes") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(11i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([soa_vector<Particle>] values, [vector<i32>] idx) {
  return(ref(values, idx))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] idx{vector<i32>(0i32)}
  return(plus(consume(ref(values, idx)), plus(consume(values.ref(idx)), pick(values, idx))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector ref_ref helper rejects retired spelling before same-path helper escapes") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/ref_ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(13i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([soa_vector<Particle>] values, [vector<i32>] idx) {
  return(ref_ref(values, idx))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] idx{vector<i32>(0i32)}
  return(plus(consume(ref_ref(values, idx)), plus(consume(values.ref_ref(idx)), pick(values, idx))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector helper-return bare ref rejects non-templated spelling before helper escapes") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[effects(heap_alloc), return<int>]
/soa_vector/ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(7i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([vector<i32>] idx) {
  return(ref(cloneValues(), idx))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [auto] item{ref(cloneValues(), idx)}
  return(plus(item, plus(consume(ref(cloneValues(), idx)), pick(idx))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector helper-return bare ref_ref rejects non-templated spelling before helper escapes") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[effects(heap_alloc), return<int>]
/soa_vector/ref_ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(13i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([vector<i32>] idx) {
  return(ref_ref(cloneValues(), idx))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [auto] item{ref_ref(cloneValues(), idx)}
  return(plus(item, plus(consume(ref_ref(cloneValues(), idx)), pick(idx))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector helper-return bare get rejects non-templated spelling before helper escapes") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<int>]
/soa_vector/get([soa_vector<Particle>] values, [int] index) {
  return(7i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick() {
  return(get(cloneValues(), 0i32))
}

[return<int>]
main() {
  [auto] item{get(cloneValues(), 0i32)}
  return(plus(item, plus(consume(get(cloneValues(), 0i32)), pick())))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector helper-return bare get_ref rejects non-templated spelling before helper escapes") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Reference<soa_vector<Particle>>>]
borrowValues([Reference<soa_vector<Particle>>] values) {
  return(values)
}

[return<int>]
/soa_vector/get_ref([Reference<soa_vector<Particle>>] values, [int] index) {
  return(7i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([Reference<soa_vector<Particle>>] values) {
  return(get_ref(borrowValues(values), 0i32))
}

[return<int>]
main() {
  [soa_vector<Particle>] values{cloneValues()}
  [auto] item{get_ref(location(values), 0i32)}
  return(plus(item,
              plus(consume(get_ref(borrowValues(location(values)), 0i32)),
                   plus(consume(location(values).get_ref(0i32)),
                        pick(location(values))))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector builtin get_ref rejects non-templated helper-return spelling before auto inference") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Reference<soa_vector<Particle>>>]
borrowValues([Reference<soa_vector<Particle>>] values) {
  return(values)
}

[return<auto>]
pick([Reference<soa_vector<Particle>>] values) {
  return(get_ref(borrowValues(values), 0i32).x)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{cloneValues()}
  [auto] item{pick(location(values))}
  return(item)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector builtin get rejects non-templated helper-return spelling before field inference") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Reference<soa_vector<Particle>>>]
borrowValues([Reference<soa_vector<Particle>>] values) {
  return(values)
}

[return<int>]
pick([Reference<soa_vector<Particle>>] values) {
  return(get(borrowValues(values), 0i32).x)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{cloneValues()}
  return(pick(location(values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector builtin method get_ref rejects non-templated helper-return spelling before field inference") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Reference<soa_vector<Particle>>>]
borrowValues([Reference<soa_vector<Particle>>] values) {
  return(values)
}

[return<int>]
pick([Reference<soa_vector<Particle>>] values) {
  return(borrowValues(values).get_ref(0i32).x)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{cloneValues()}
  return(pick(location(values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("uppercase SoaVector helper-return count keeps current meta diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{7i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(soaVectorSingle<Particle>(Particle(7i32)))
}

[return<int>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(7i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick() {
  return(count(cloneValues()))
}

[return<int>]
main() {
  [auto] item{count(cloneValues())}
  return(plus(item, plus(consume(count(cloneValues())), pick())))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("meta.field_count requires struct type argument: type:Particle") !=
        std::string::npos);
}

TEST_CASE("vector-target count get get_ref and ref keep same-path soa helpers") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/count([vector<Particle>] values) {
  return(6i32)
}

[return<int>]
/soa_vector/get([vector<Particle>] values, [i32] index) {
  return(7i32)
}

[return<int>]
/soa_vector/get_ref([Reference<vector<Particle>>] values, [i32] index) {
  return(11i32)
}

[return<int>]
/soa_vector/ref([vector<Particle>] values, [i32] index) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle>] values{vector<Particle>(Particle(1i32))}
  [int mut] total{plus(count(values), values.count())}
  assign(total, plus(total, values./soa_vector/count()))
  assign(total, plus(total, get(values, 0i32)))
  assign(total, plus(total, get_ref(location(values), 0i32)))
  assign(total, plus(total, ref(values, 0i32)))
  assign(total, plus(total, values.get(0i32)))
  assign(total, plus(total, location(values).get_ref(0i32)))
  assign(total, plus(total, values./soa_vector/get(0i32)))
  assign(total, plus(total, /soa_vector/get_ref(location(values), 0i32)))
  assign(total, plus(total, values.ref(0i32)))
  assign(total, plus(total, values./soa_vector/ref(0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector-target to_aos keeps same-path helper shadow") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([vector<Particle>] values) {
  return(9i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([vector<Particle>] values) {
  return(values.to_aos())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle>] values{vector<Particle>(Particle(1i32))}
  [auto] bare{to_aos(values)}
  [auto] direct{/to_aos(values)}
  [auto] method{values.to_aos()}
  [auto] slash{values./to_aos()}
  return(plus(bare,
              plus(direct,
                   plus(method,
                        plus(slash,
                             plus(consume(values.to_aos()),
                                  pick(values)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("raw builtin soa_vector canonical to_aos bindings keep experimental vector contract") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<void>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{/std/collections/soa_vector/to_aos<Particle>(values)}
  [vector<Particle>] unpackedB{location(values)./std/collections/soa_vector/to_aos_ref<Particle>()}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) !=
        std::string::npos);
}

TEST_CASE("vector-target old-explicit push and reserve keep same-path soa helpers") {
  const std::string source = R"(
[return<int>]
/soa_vector/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa_vector/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[return<auto>]
pickPush([vector<i32>] values) {
  return(values./soa_vector/push(4i32))
}

[return<auto>]
pickReserve([vector<i32>] values) {
  return(values./soa_vector/reserve(6i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values./soa_vector/push(4i32),
              plus(values./soa_vector/reserve(6i32),
                   plus(pickPush(values), pickReserve(values)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector-target method push and reserve keep same-path soa helpers") {
  const std::string source = R"(
[return<int>]
/soa_vector/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa_vector/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[return<auto>]
pickPush([vector<i32>] values) {
  return(values.push(4i32))
}

[return<auto>]
pickReserve([vector<i32>] values) {
  return(values.reserve(6i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values.push(4i32),
              plus(values.reserve(6i32),
                   plus(pickPush(values), pickReserve(values)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical soa helper-return push and reserve keep same-path helper across escapes") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa<Particle>>]
cloneValues() {
  return(soa<Particle>())
}

[return<int>]
/soa_vector/push([soa<Particle>] values, [Particle] value) {
  return(7i32)
}

[return<int>]
/soa_vector/reserve([soa<Particle>] values, [int] count) {
  return(8i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pickPush() {
  return(cloneValues().push(Particle(1i32)))
}

[return<auto>]
pickReserve() {
  return(reserve(cloneValues(), 2i32))
}

[return<int>]
main() {
  [auto] pushBare{push(cloneValues(), Particle(1i32))}
  [auto] pushMethod{cloneValues().push(Particle(1i32))}
  [auto] reserveBare{reserve(cloneValues(), 2i32)}
  [auto] reserveMethod{cloneValues().reserve(2i32)}
  return(plus(pushBare,
              plus(pushMethod,
                   plus(reserveBare,
                        plus(reserveMethod,
                             plus(consume(push(cloneValues(), Particle(1i32))),
                                  plus(consume(cloneValues().reserve(2i32)),
                                       plus(pickPush(), pickReserve()))))))))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("canonical soa method-like helper-return push and reserve keep same-path helper across escapes") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa<Particle>())
}

[return<int>]
/soa_vector/push([soa<Particle>] values, [Particle] value) {
  return(7i32)
}

[return<int>]
/soa_vector/reserve([soa<Particle>] values, [int] count) {
  return(8i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pickPush([Holder] holder) {
  return(holder.cloneValues().push(Particle(1i32)))
}

[return<auto>]
pickReserve([Holder] holder) {
  return(reserve(holder.cloneValues(), 2i32))
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] pushBare{push(holder.cloneValues(), Particle(1i32))}
  [auto] pushMethod{holder.cloneValues().push(Particle(1i32))}
  [auto] reserveBare{reserve(holder.cloneValues(), 2i32)}
  [auto] reserveMethod{holder.cloneValues().reserve(2i32)}
  return(plus(pushBare,
              plus(pushMethod,
                   plus(reserveBare,
                        plus(reserveMethod,
                             plus(consume(push(holder.cloneValues(), Particle(1i32))),
                                  plus(consume(holder.cloneValues().reserve(2i32)),
                                       plus(pickPush(holder), pickReserve(holder)))))))))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("soa_vector builtin field view call argument escapes report escape diagnostics") {
  const auto checkReject = [](const std::string &expr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "consume<T>([T] value) {\n"
        "  return(0i32)\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  return(consume(" + expr + "))\n"
        "}\n";
    std::string error;
    CHECK_MESSAGE(!validateProgram(source, "/main", error), error);
    CHECK(error.find("field-view escapes via argument") != std::string::npos);
  };

  checkReject("x(values)");
  checkReject("values.x()");
}

TEST_CASE("soa_vector builtin field view return escapes reject retired spelling") {
  const auto checkReject = [](const std::string &expr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<auto>]\n"
        "pick([soa_vector<Particle>] values) {\n"
        "  return(" + expr + ")\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  pick(values)\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_MESSAGE(!validateProgram(source, "/pick", error), error);
    INFO(expr);
    INFO(error);
    CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
  };

  checkReject("x(values)");
  checkReject("values.x()");
}

TEST_CASE("legacy soa_vector builtin field-view bindings reject retired spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [auto] methodView{values.x()}
  [auto] callView{x(values)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("canonical soa field-view bindings track borrow roots") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [auto] methodView{values.x()}
  [auto] callView{x(values)}
  return(0i32)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("canonical soa field-view binding blocks structural mutation while live") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
consume<T>([T] value) {
  return(0i32)
}

[return<int> effects(heap_alloc)]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  [auto] view{values.x()}
  values.push(Particle(2i32))
  return(consume(view))
}
)";
  std::string error;
  CHECK_MESSAGE(!validateProgram(source, "/main", error), error);
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("legacy soa_vector field-view binding rejects retired spelling before mutation borrow check") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
consume<T>([T] value) {
  return(0i32)
}

[return<int> effects(heap_alloc)]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  [auto] view{values.x()}
  values.push(Particle(2i32))
  return(consume(view))
}
)";
  std::string error;
  CHECK_MESSAGE(!validateProgram(source, "/main", error), error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("legacy soa_vector field-view bindings reject non-templated helper-return spelling") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<Reference<soa_vector<Particle>>>]
pickBorrowed([Reference<soa_vector<Particle>>] values) {
  return(values)
}

[return<int>]
consume<T>([T] value) {
  return(0i32)
}

[return<int>]
main() {
  [soa_vector<Particle> mut] values{soa_vector<Particle>()}
  [auto] view{pickBorrowed(location(values)).x()}
  return(consume(view))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(NonTemplatedSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view helper rejects retired spelling before returned field-view arithmetic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/x([soa_vector<Particle>] values) {
  return(13i32)
}

[return<int>]
consume([int] value) {
  return(value)
}

[return<auto>]
pick([soa_vector<Particle>] values) {
  return(values.x())
}

[return<auto>]
pick_call([soa_vector<Particle>] values) {
  return(x(values))
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(plus(plus(consume(values.x()), consume(x(values))), plus(pick(values), pick_call(values))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector get helper call-form rejects retired labeled receiver") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] index{vector<i32>(0i32)}
  return(get([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector get method-form rejects retired spelling before helper preference") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] idx{vector<i32>(0i32)}
  return(values.get(idx))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector get helper call-form rejects retired named-argument helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([vector<i32>] values, [soa_vector<Particle>] index) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(0i32)}
  [soa_vector<Particle>] index{soa_vector<Particle>()}
  return(get([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector ref method-form rejects retired spelling before helper preference") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] idx{vector<i32>(0i32)}
  return(values.ref(idx))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view method statement rejects retired helper substrate") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.x()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form statement rejects retired helper substrate") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  x(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view index syntax rejects retired helper substrate") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.x()[0i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view builtin rejects retired spelling before named arguments") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.x([index] 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form rejects retired spelling before user helper fallback") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/x([soa_vector<Particle>] values) {
  return(7i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(x(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("soa_vector field-view method rejects retired spelling before user helper fallback") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/x([soa_vector<Particle>] values, [i32] index) {
  return(index)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.x(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find(RetiredSoaVectorDiagnostic) != std::string::npos);
}

TEST_CASE("count rejects vector named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count([value] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("count method validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method on map binding ignores explicit alias helper without canonical definition") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(7i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("count method validates on string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 40i32))
}

[return<int>]
main() {
  return(plus(count(wrapText()).tag(), wrapText().count().tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector count method resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector count method requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("count wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(count(wrapText()).missing_tag())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(at(wrapText(), 1i32).tag(), wrapText().at_unsafe(2i32).tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32).tag(),
              /std/collections/vector/at_unsafe(wrapText(), 2i32).tag()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("reordered access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
import /std/collections/*

[return<map<string, i32>>]
wrapMapText() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 21i32)}
  return(values)
}

[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(/std/collections/map/at_unsafe(values, key))
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 1i32))
}

[return<int>]
main() {
  return(at_unsafe("only"raw_utf8, wrapMapText()).tag())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("access wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(at(wrapText(), 1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("namespaced access wrapper temporary chained method reports vector at mismatch") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(/std/collections/vector/at(wrapText(), 1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("slash-method access wrapper temporaries report missing namespaced at target") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32).tag(),
              wrapText()./vector/at_unsafe(2i32).tag()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_SUITE_END();
