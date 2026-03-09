TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("count builtin validates on array literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on map literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on method calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("ref helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ref requires soa_vector target") != std::string::npos);
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

TEST_CASE("count method validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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

TEST_CASE("reordered access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
[return<map<string, i32>>]
wrapMapText() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 21i32)}
  return(values)
}

[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(values.at_unsafe(key))
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

TEST_CASE("map wrapper temporary count call validates target classification") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(count(wrapMapAuto()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary access call validates map target classification") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  at(wrapMapAuto(), 1i32)
  at_unsafe(wrapMapAuto(), 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary capacity reports vector target diagnostic") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(capacity(wrapMapAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("count preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("capacity preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("at preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  at(missing(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("capacity builtin validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity does not accept template arguments") != std::string::npos);
}

TEST_CASE("capacity rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("capacity rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin capacity") != std::string::npos);
}

TEST_CASE("capacity method validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 50i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(capacity(wrapVector()).tag(), wrapVector().capacity().tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method rejects extra arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin capacity") != std::string::npos);
}

TEST_CASE("capacity rejects non-vector target") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("capacity method rejects array target") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(counter.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(capacity(counter))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(capacity(makeCounter()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("at call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  at(counter, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("at_unsafe call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  at_unsafe(makeCounter(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at_unsafe") != std::string::npos);
}

TEST_CASE("push call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Counter mut] counter{Counter()}
  push(counter, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("reserve method keeps unknown method on wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter mut] counter{Counter()}
  return(counter)
}

[effects(heap_alloc), return<int>]
main() {
  makeCounter().reserve(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/reserve") != std::string::npos);
}

TEST_CASE("at method validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(count(values))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(plus(count(values), 33i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 10i32))
}

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

TEST_CASE("count call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 11i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(plus(count(values), 12i32))
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(94i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(count(text))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(95i32)
}

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

TEST_CASE("count method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(plus(count(values), 20i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(plus(count(values), 21i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(plus(count(values), 20i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(73i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(74i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(75i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(76i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(65i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(79i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(80i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(83i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(84i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires mutable vector binding") != std::string::npos);
}

TEST_CASE("push requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("push on array reports vector binding before effect requirement") {
  const auto checkInvalidPush = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("push requires vector binding") != std::string::npos);
  };

  checkInvalidPush("push(values, 2i32)");
  checkInvalidPush("values.push(2i32)");
}

TEST_CASE("push validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  push(values, Particle(2i32))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection syntax parity validates for call and method forms") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] viaCall{vector<i32>(10i32, 20i32, 30i32)}
  [vector<i32> mut] viaMethod{vector<i32>(10i32, 20i32, 30i32)}
  pop(viaCall)
  viaMethod.pop()
  reserve(viaCall, 3i32)
  viaMethod.reserve(3i32)
  push(viaCall, 40i32)
  viaMethod.push(40i32)
  return(plus(
      plus(at(viaCall, 2i32), viaMethod.at(2i32)),
      plus(viaCall[2i32], viaMethod[2i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection indexing syntax parity keeps integer-index diagnostics") {
  const auto checkInvalidIndex = [](const std::string &exprText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32>] values{array<i32>(1i32, 2i32)}\n"
        "  return(" +
        exprText +
        ")\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("at requires integer index") != std::string::npos);
  };

  checkInvalidIndex("at(values, \"oops\"utf8)");
  checkInvalidIndex("values.at(\"oops\"utf8)");
  checkInvalidIndex("values[\"oops\"utf8]");
}

TEST_CASE("push rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push<i32>(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push does not accept template arguments") != std::string::npos);
}

TEST_CASE("push rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires exactly two arguments") != std::string::npos);
}

TEST_CASE("push call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires mutable vector binding") != std::string::npos);
}

TEST_CASE("reserve requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("reserve on array reports vector binding before effect requirement") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("reserve requires vector binding") != std::string::npos);
  };

  checkInvalidReserve("reserve(values, 8i32)");
  checkInvalidReserve("values.reserve(8i32)");
}

TEST_CASE("reserve requires integer capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires integer capacity") != std::string::npos);
}

TEST_CASE("reserve rejects bool capacity in call and method forms") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("reserve requires integer capacity") != std::string::npos);
  };

  checkInvalidReserve("reserve(values, true)");
  checkInvalidReserve("values.reserve(true)");
}

TEST_CASE("reserve rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve<i32>(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve does not accept template arguments") != std::string::npos);
}

TEST_CASE("reserve rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires exactly two arguments") != std::string::npos);
}

TEST_CASE("reserve validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  reserve(values, 8i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop still rejects soa_vector helper target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
touch([soa_vector<Particle> mut] values) {
  pop(values)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires vector binding") != std::string::npos);
}

TEST_CASE("reserve call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.reserve(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires mutable vector binding") != std::string::npos);
}

TEST_CASE("pop validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop does not accept block arguments") != std::string::npos);
}

TEST_CASE("pop rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop does not accept template arguments") != std::string::npos);
}

TEST_CASE("pop rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires exactly one argument") != std::string::npos);
}

TEST_CASE("pop call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires mutable vector binding") != std::string::npos);
}

TEST_CASE("clear validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear does not accept template arguments") != std::string::npos);
}

TEST_CASE("clear rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires exactly one argument") != std::string::npos);
}

TEST_CASE("remove_at requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires mutable vector binding") != std::string::npos);
}

TEST_CASE("remove_at requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires integer index") != std::string::npos);
}

TEST_CASE("remove_at rejects bool index in call and method forms") {
  const auto checkInvalidRemoveAt = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("remove_at requires integer index") != std::string::npos);
  };

  checkInvalidRemoveAt("remove_at(values, true)");
  checkInvalidRemoveAt("values.remove_at(true)");
}

TEST_CASE("remove_at rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at<i32>(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at does not accept template arguments") != std::string::npos);
}

TEST_CASE("remove_at rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires exactly two arguments") != std::string::npos);
}

TEST_CASE("remove_at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_swap(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires integer index") != std::string::npos);
}

TEST_CASE("remove_swap rejects bool index in call and method forms") {
  const auto checkInvalidRemoveSwap = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("remove_swap requires integer index") != std::string::npos);
  };

  checkInvalidRemoveSwap("remove_swap(values, true)");
  checkInvalidRemoveSwap("values.remove_swap(true)");
}

TEST_CASE("remove_swap requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires mutable vector binding") != std::string::npos);
}

TEST_CASE("remove_swap rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap<i32>(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap does not accept template arguments") != std::string::npos);
}

TEST_CASE("remove_swap rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires exactly two arguments") != std::string::npos);
}

TEST_CASE("remove_swap validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helpers are statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},       {"pop", "values"},       {"reserve", "values, 8i32"},
      {"clear", "values"},            {"remove_at", "values, 0i32"}, {"remove_swap", "values, 0i32"}};
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("vector helper named args on array targets report vector binding") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("requires vector binding") != std::string::npos);
  };

  checkInvalidStatement("push([values] values, [value] 3i32)");
  checkInvalidStatement("reserve([values] values, [capacity] 8i32)");
  checkInvalidStatement("remove_at([values] values, [index] 0i32)");
  checkInvalidStatement("remove_swap([values] values, [index] 0i32)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
  checkInvalidStatement("values.push([value] 3i32)");
  checkInvalidStatement("values.reserve([capacity] 8i32)");
  checkInvalidStatement("values.remove_at([index] 0i32)");
  checkInvalidStatement("values.remove_swap([index] 0i32)");
}

TEST_CASE("vector helper named args keep named-arg error on vector targets") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkInvalidStatement("push([value] 3i32, [payload] values)");
  checkInvalidStatement("reserve([capacity] 8i32, [payload] values)");
  checkInvalidStatement("remove_at([index] 0i32, [payload] values)");
  checkInvalidStatement("remove_swap([index] 0i32, [payload] values)");
  checkInvalidStatement("pop([payload] values)");
  checkInvalidStatement("clear([payload] values)");
}

TEST_CASE("vector helper expressions with named arguments stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"pop", "[values] values"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"clear", "[values] values"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("vector helper method expressions with named arguments stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[value] 2i32"},
      {"reserve", "[capacity] 8i32"},
      {"remove_at", "[index] 0i32"},
      {"remove_swap", "[index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(values." +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("namespaced vector helper accepts statement form") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced vector helper is statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push(values, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper accepts statement form") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector helper is statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(values, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push([values] values, [value] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor is treated as builtin collection") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>([first] 4i32, [second] 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access and count helpers are builtin-alias validated") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/std/collections/vector/count(values)}
  [i32] k{/std/collections/vector/capacity(values)}
  [i32] first{/std/collections/vector/at(values, 0i32)}
  [i32] second{/std/collections/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical vector helpers resolve in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated stdlib canonical vector helpers resolve in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated stdlib canonical vector helper keeps precedence diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced call aliases forward to canonical stdlib helper precedence") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/at(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count alias keeps canonical helper diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced call alias forwards to templated canonical helper with explicit template args") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced alias implicitly forwards to templated canonical helper on arity mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced alias implicitly forwards on bool type mismatch with same arity") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced alias implicit template forwarding keeps canonical diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced bool mismatch fallback keeps canonical diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [string] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias implicitly forwards with named arguments") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count([values] values, [marker] true),
              values.count([marker] true)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced named-arg implicit forwarding keeps canonical diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count([marker] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("array namespaced alias forwards templated vector helper to canonical stdlib path") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced templated alias keeps canonical helper diagnostics") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced templated alias call forwards past non-templated compatibility helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method templated call keeps canonical diagnostics past non-templated compatibility helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("wrapper temporary templated vector method call forwards to canonical helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>(true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper temporary templated vector method keeps canonical diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias keeps templated canonical helper diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments required for /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced access and count helpers are builtin-alias validated") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/at([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector namespaced access helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("vector namespaced capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("namespaced vector helper with named arguments is statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(/vector/" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("user definition named push is not treated as builtin vector helper") {
  const std::string source = R"(
[return<void>]
push([i32] value) {
}

[return<int>]
main() {
  push([value] 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  [return<int>]
  push([i32] next) {
    assign(this.value, next)
    return(this.value)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  return(counter.push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper assign to immutable field is rejected") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("struct push helper wrapper temporary statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  push([i32] next) {
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  makeCounter().push(7i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper wrapper temporary expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [return<int>]
  push([i32] next) {
    return(next)
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push can be used in expressions") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push(3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression uses user-defined method target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(3i32, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts bool positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/vector/push([vector<i32> mut] values, [bool] value) {
  return(value)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(true, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression stays statement-only without user helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper expression skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
  return(value)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  return(push(makeCounter(), values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper call-form expression user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([value] 3i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  return(push([value] payload, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  [auto] inferred{push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/get([vector<i32>] values, [string] index) {
  return(12i32)
}

[effects(heap_alloc), return<int>]
/string/get([string] values, [vector<i32>] index) {
  return(98i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [string] index{"payload"raw_utf8}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(13i32)
}

[effects(heap_alloc), return<bool>]
/vector/get([vector<i32>] values, [soa_vector<Particle>] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] index{vector<i32>(0i32)}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression builtin stays statement-only with named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([values] values, [value] 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper statement call-form user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push([value] 3i32, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement prefers labeled named receiver") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<void>]
/soa_vector/push([soa_vector<Particle> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/soa_vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector helper statement skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<void>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  push(makeCounter(), values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("vector at call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call-form helper shadow prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/map/at([map<string, i32>] values, [vector<i32>] index) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call-form labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(92i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector at call-form helper shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(1i32, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(81i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at_unsafe call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(82i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at_unsafe call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at call-form helper shadow skips temp-leading positional probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/at([vector<Counter>] values, [Counter] index) {
  return(index)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter>] values{vector<Counter>(Counter())}
  return(at(makeCounter(), values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("user definition named push with positional args is not treated as builtin") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(3i32, 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push accepts named arguments") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push([value] 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method helper remains statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper builtin still rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear([values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named count accepts named arguments") {
  const std::string source = R"(
[return<int>]
count([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(count([value] 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named capacity accepts named arguments") {
  const std::string source = R"(
[return<int>]
capacity([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(capacity([value] 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity builtin still rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity([value] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named at accepts named arguments") {
  const std::string source = R"(
[return<int>]
at([i32] value, [i32] index) {
  return(plus(value, index))
}

[return<int>]
main() {
  return(at([value] 3i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named at_unsafe accepts named arguments") {
  const std::string source = R"(
[return<int>]
at_unsafe([i32] value, [i32] index) {
  return(minus(value, index))
}

[return<int>]
main() {
  return(at_unsafe([value] 7i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at([value] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named vector accepts named arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named map accepts named arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts named arguments") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named vector accepts block arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts block arguments") {
  const std::string source = R"(
[return<int>]
array<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array<i32>(2i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named map accepts block arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("builtin at validates on array literal call target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(4i32, 5i32), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named vector call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
vector<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(vector<i32>(9i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named map call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(map<i32, i32>(7i32, 8i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_SUITE_END();
