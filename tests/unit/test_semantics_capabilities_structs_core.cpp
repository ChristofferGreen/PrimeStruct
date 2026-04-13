#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.effects");


TEST_CASE("execution effects transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}


TEST_CASE("execution capabilities transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), capabilities(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution capabilities rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out, io_out)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution effects rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution effects rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io, io)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), effects(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out), capabilities(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}

TEST_CASE("capabilities transform validates asset and gpu identifiers") {
  const std::string source = R"(
[effects(asset_read, gpu_queue), capabilities(asset_read, gpu_queue), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities transform rejects duplicate gpu capability") {
  const std::string source = R"(
[capabilities(gpu, gpu), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.struct_transforms");

TEST_CASE("align_bytes validates integer argument") {
  const std::string source = R"(
[align_bytes(16), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_bytes accepts digit separators") {
  const std::string source = R"(
[align_bytes(1,024), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_bytes rejects non-integer argument") {
  const std::string source = R"(
[align_bytes(foo), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("align_bytes rejects wrong argument count") {
  const std::string source = R"(
[align_bytes(4, 8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("align_kbytes rejects wrong argument count") {
  const std::string source = R"(
[align_kbytes(4, 8), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires exactly one integer argument") != std::string::npos);
}

TEST_CASE("align_kbytes rejects template arguments") {
  const std::string source = R"(
[align_kbytes<i32>(4), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes does not accept template arguments") != std::string::npos);
}

TEST_CASE("align_kbytes validates integer argument") {
  const std::string source = R"(
[align_kbytes(4), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_kbytes accepts hex literal") {
  const std::string source = R"(
[align_kbytes(0x10), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_kbytes rejects non-integer argument") {
  const std::string source = R"(
[align_kbytes(foo), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("struct transform validates without args") {
  const std::string source = R"(
[struct]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("no_padding transform validates without args") {
  const std::string source = R"(
[no_padding]
Thing() {
  [i32] a{1i32}
  [i32] b{2i32}
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

TEST_CASE("no_padding rejects alignment padding") {
  const std::string source = R"(
[no_padding]
Thing() {
  [i32] a{1i32}
  [i64] b{2i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no_padding disallows alignment padding on field /Thing/b") != std::string::npos);
}

TEST_CASE("platform_independent_padding rejects implicit padding") {
  const std::string source = R"(
[platform_independent_padding]
Thing() {
  [i32] a{1i32}
  [i64] b{2i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("platform_independent_padding requires explicit alignment on field /Thing/b") != std::string::npos);
}

TEST_CASE("platform_independent_padding allows explicit alignment") {
  const std::string source = R"(
[platform_independent_padding]
Thing() {
  [i32] a{1i32}
  [align_bytes(8) i64] b{2i64}
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

TEST_CASE("struct layout accepts explicit canonical map field") {
  const std::string source = R"(
[struct]
Thing() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [bool] ready{true}
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

TEST_CASE("no_padding keeps canonical map field padding diagnostics") {
  const std::string source = R"(
[no_padding]
Thing() {
  [bool] ready{true}
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("no_padding disallows alignment padding on field /Thing/values") != std::string::npos);
}

TEST_CASE("struct alignment rejects smaller field requirement") {
  const std::string source = R"(
[struct]
Thing() {
  [align_bytes(4) i64] value{1i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment requirement on field /Thing/value") != std::string::npos);
}

TEST_CASE("struct alignment rejects smaller struct requirement") {
  const std::string source = R"(
[struct align_bytes(4)]
Thing() {
  [i64] value{1i64}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment requirement on struct /Thing") != std::string::npos);
}

TEST_CASE("static fields do not affect struct alignment") {
  const std::string source = R"(
[struct align_bytes(4)]
Thing() {
  [static i64] shared{1i64}
  [i32] value{2i32}
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

TEST_CASE("struct constructors ignore static fields") {
  const std::string source = R"(
[struct]
Thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
}

[return<int>]
main() {
  Thing(3i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod transform validates without args") {
  const std::string source = R"(
[pod]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod transform rejects handle tag") {
  const std::string source = R"(
[pod, handle]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("pod transform rejects handle fields") {
  const std::string source = R"(
[pod]
main() {
  [handle<PathNode>] target{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot contain handle or gpu_lane fields") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on definition") {
  const std::string source = R"(
[handle, gpu_lane]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("handle definitions cannot be tagged as gpu_lane") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on field") {
  const std::string source = R"(
[struct]
main() {
  [handle<PathNode> gpu_lane] target{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as handle and gpu_lane") != std::string::npos);
}

TEST_CASE("struct transform rejects template arguments") {
  const std::string source = R"(
[struct<i32>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("struct transform rejects arguments") {
  const std::string source = R"(
[struct(foo)]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept arguments") != std::string::npos);
}


TEST_SUITE_END();
