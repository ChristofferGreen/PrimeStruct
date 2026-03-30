TEST_CASE("to_soa and to_aos reject named arguments for builtin calls") {
  const auto checkNamedArgs = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [vector<Particle>] values{vector<Particle>()}\n"
        "  [soa_vector<Particle>] packed{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkNamedArgs("to_soa([values] values)");
  checkNamedArgs("to_aos([values] packed)");
}

TEST_CASE("soa_vector get and ref reject named arguments for builtin calls") {
  const auto checkNamedArgs = [](const std::string &callExpr) {
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
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkNamedArgs("get([index] 0i32, [values] values)");
  checkNamedArgs("ref([index] 0i32, [values] values)");
  checkNamedArgs("/soa_vector/get([index] 0i32, [values] values)");
  checkNamedArgs("/soa_vector/ref([index] 0i32, [values] values)");
}

TEST_CASE("soa_vector get and ref require integer indices for builtin calls") {
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
    CHECK(error.find("requires integer index") != std::string::npos);
  };

  checkIndexType("get(values, true)");
  checkIndexType("values.ref(1.0f32)");
  checkIndexType("/soa_vector/get(values, true)");
  checkIndexType("/soa_vector/ref(values, 1.0f32)");
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
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa<i32>(values)",
              "to_soa does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "to_aos<i32>(values)",
              "to_aos does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "get<i32>(values, 0i32)",
              "get does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref<i32>(values, 0i32)",
              "ref does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "/soa_vector/get<i32>(values, 0i32)", "get does not accept template arguments");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "/soa_vector/ref<i32>(values, 0i32)", "ref does not accept template arguments");
}

TEST_CASE("soa_vector conversion and access builtins reject block arguments") {
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
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa(values) { return(values) }",
              "block arguments require a definition target");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "to_aos(values) { return(values) }", "block arguments require a definition target");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "get(values, 0i32) { return(values) }", "block arguments require a definition target");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n",
              "ref(values, 0i32) { return(values) }", "block arguments require a definition target");
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
    CHECK(error.find(expected) != std::string::npos);
  };

  checkReject("  [vector<Particle>] values{vector<Particle>()}\n", "to_soa()",
              "argument count mismatch for builtin to_soa");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "to_aos(values, values)",
              "argument count mismatch for builtin to_aos");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "get(values)",
              "argument count mismatch for builtin get");
  checkReject("  [soa_vector<Particle>] values{soa_vector<Particle>()}\n", "ref(values, 0i32, 1i32)",
              "argument count mismatch for builtin ref");
}

TEST_CASE("soa_vector builtin construction allows empty constructor and empty braces") {
  const std::string source = R"(
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector get helper call-form accepts labeled named receiver") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector get method-form prefers user helper over builtin") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector get helper call-form does not fall back to non-receiver label") {
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
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("soa_vector ref method-form prefers user helper over builtin") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector field-view method reports unsupported diagnostic") {
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
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form reports unsupported diagnostic") {
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
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view index syntax reports unsupported diagnostic") {
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
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view builtin rejects named arguments") {
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
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form falls back to user helper") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector field-view method falls back to user helper") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
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

TEST_CASE("namespaced access wrapper temporary chained method reports i32 path diagnostics") {
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
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("slash-method access wrapper temporaries keep removed alias diagnostics") {
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
  CHECK(error.find("unknown method: /vector/at_unsafe") != std::string::npos);
}
