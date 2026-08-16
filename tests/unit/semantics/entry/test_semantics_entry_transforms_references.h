TEST_CASE("reference return allows direct parameter reference") {
  const std::string source = R"(
[return<Reference<i32>>]
identity([Reference<i32>] input) {
  return(input)
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(identity(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference return allows parameter-rooted borrow expression") {
  const std::string source = R"(
[return<Reference<i32>>]
peek([Reference<uninitialized<i32>>] slot) {
  return(borrow(dereference(slot)))
}

[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] slot{location(storage)}
  init(dereference(slot), 7i32)
  [i32] out{dereference(peek(slot))}
  drop(dereference(slot))
  return(out)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference return rejects local reference escape") {
  const std::string source = R"(
[return<Reference<i32>>]
bad() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(ref)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return requires direct parameter reference") != std::string::npos);
}

TEST_CASE("reference return rejects local borrow escape") {
  const std::string source = R"(
[return<Reference<i32>>]
bad() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  init(storage, 1i32)
  return(borrow(storage))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return requires direct parameter reference or parameter-rooted borrow") !=
        std::string::npos);
}

TEST_CASE("reference return rejects derived parameter reference") {
  const std::string source = R"(
[return<Reference<i32>>]
bad([Reference<i32>] input) {
  [Reference<i32>] alias{location(input)}
  return(alias)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return requires direct parameter reference") != std::string::npos);
}

TEST_CASE("reference return rejects mismatched parameter-rooted borrow target") {
  const std::string source = R"(
[return<Reference<i32>>]
bad([Reference<uninitialized<i64>>] slot) {
  return(borrow(dereference(slot)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return type mismatch") != std::string::npos);
}

TEST_CASE("reference return requires matching template target") {
  const std::string source = R"(
[return<Reference<i32>>]
bad([Reference<i64>] input) {
  return(input)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/bad", error));
  CHECK(error.find("reference return type mismatch") != std::string::npos);
}

TEST_CASE("unsafe definitions allow overlapping mutable references") {
  const std::string source = R"(
[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] refA{location(value)}
  [Reference<i32> mut] refB{location(value)}
  [i32] observed{dereference(refA)}
  assign(refA, 2i32)
  assign(refB, 3i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects safe-call boundary escape") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference allows unsafe-call boundary") {
  const std::string source = R"(
[unsafe, return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe parameter reference allows named safe-call boundary") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
forward([Reference<i32>] input) {
  consume([input] input)
  return()
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  forward(ref)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects named safe-call boundary escape through call result") {
  const std::string source = R"(
[unsafe, return<Reference<i32>>]
forward([Reference<i32>] input) {
  return(input)
}

[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume([input] forward([input] ref))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe transform rejects duplicate markers") {
  const std::string source = R"(
[unsafe, unsafe, return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate unsafe transform") != std::string::npos);
}

TEST_CASE("unsafe transform rejects arguments") {
  const std::string source = R"(
[unsafe(flag), return<void>]
main() {
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe does not accept arguments") != std::string::npos);
}

TEST_CASE("unsafe scope accepts reference location initialization") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  assign(value, 2i32)
  return(dereference(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe scope allows pointer to reference conversion initializer") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  return(dereference(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe scope rejects mismatched pointer to reference conversion") {
  const std::string source = R"(
[unsafe, return<int>]
main() {
  [i64 mut] value{1i64}
  [Pointer<i64>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe Reference binding type mismatch") != std::string::npos);
}

TEST_CASE("unsafe pointer-converted reference rejects safe-call boundary escape") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  consume(ref)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through call result") {
  const std::string source = R"(
[unsafe, return<Reference<i32>>]
forward([Reference<i32>] input) {
  return(input)
}

[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(forward(ref))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through if expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(if(true, then(){ ref }, else(){ ref }))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through block return expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  consume(block(){ return(ref) })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe reference rejects safe-call boundary escape through match expression") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [i32] selector{0i32}
  consume(match(selector, case(0i32) { ref }, else() { ref }))
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes across safe boundary to /consume") != std::string::npos);
}

TEST_CASE("unsafe parameter reference allows safe-call boundary") {
  const std::string source = R"(
[return<void>]
consume([Reference<i32>] input) {
  return()
}

[unsafe, return<void>]
forward([Reference<i32>] input) {
  consume(input)
  return()
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  forward(ref)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects assignment escape to outer binding") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32>] local{ptr}
  assign(out, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference allows assignment through local alias") {
  const std::string source = R"(
[unsafe, return<void>]
useLocal([Pointer<i32>] ptr) {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] localOut{location(sinkValue)}
  [Reference<i32> mut] alias{location(localOut)}
  [Reference<i32>] local{ptr}
  assign(alias, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  useLocal(location(sourceValue))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference allows assignment through parameter-rooted pointer alias") {
  const std::string source = R"(
[unsafe, return<void>]
forward([Reference<i32> mut] out, [Reference<i32>] input) {
  [Pointer<i32>] aliasPtr{location(input)}
  [Reference<i32>] alias{aliasPtr}
  assign(out, alias)
  return()
}

[return<int>]
main() {
  [i32 mut] first{1i32}
  [i32 mut] second{2i32}
  [Reference<i32> mut] out{location(first)}
  [Reference<i32>] input{location(second)}
  forward(out, input)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe reference rejects assignment escape through parameter alias") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32> mut] alias{location(out)}
  [Reference<i32>] local{ptr}
  assign(alias, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference rejects pointer-write escape through parameter alias") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32> mut] alias{location(out)}
  [Reference<i32>] local{ptr}
  assign(dereference(location(alias)), local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("unsafe reference rejects direct pointer-write escape to parameter") {
  const std::string source = R"(
[unsafe, return<void>]
leak([Pointer<i32>] ptr, [Reference<i32> mut] out) {
  [Reference<i32>] local{ptr}
  assign(dereference(out), local)
  return()
}

[return<int>]
main() {
  [i32 mut] sourceValue{1i32}
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(location(sourceValue), out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsafe reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("safe reference rejects assignment escape to parameter") {
  const std::string source = R"(
[return<void>]
leak([Reference<i32> mut] out) {
  [i32 mut] localValue{1i32}
  [Reference<i32>] local{location(localValue)}
  assign(out, local)
  return()
}

[return<int>]
main() {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reference escapes via assignment to out") != std::string::npos);
  CHECK(error.find("root: localValue") != std::string::npos);
  CHECK(error.find("sink: out") != std::string::npos);
}

TEST_CASE("safe reference rejects assignment escape through match expression") {
  const std::string source = R"(
[return<void>]
leak([Reference<i32> mut] out) {
  [i32 mut] localValue{1i32}
  [Reference<i32>] local{location(localValue)}
  [i32] selector{0i32}
  assign(out, match(selector, case(0i32) { local }, else() { local }))
  return()
}

[return<int>]
main() {
  [i32 mut] sinkValue{2i32}
  [Reference<i32> mut] out{location(sinkValue)}
  leak(out)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reference escapes via assignment to out") != std::string::npos);
}

TEST_CASE("safe reference allows assignment from parameter to parameter") {
  const std::string source = R"(
[return<void>]
forward([Reference<i32> mut] out, [Reference<i32>] input) {
  assign(out, input)
  return()
}

[return<int>]
main() {
  [i32 mut] first{1i32}
  [i32 mut] second{2i32}
  [Reference<i32> mut] out{location(first)}
  [Reference<i32>] input{location(second)}
  forward(out, input)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects assign before pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  assign(value, 2i32)
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] observed{dereference(ptr)}
  assign(value, 2i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects move before pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] moved{move(value)}
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows move after pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] observed{dereference(ptr)}
  [i32] moved{move(value)}
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects increment before pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  increment(dereference(ptr))
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows increment after pointer-alias last use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32] observed{dereference(ptr)}
  increment(dereference(ptr))
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}
