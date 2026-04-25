#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("variadic wrapped FileError packs accept canonical named free builtin at receivers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<FileError>>] values) {
  [auto] head{at([values] values, [index] 0i32).why()}
  [auto] tail{at([values] values, [index] minus(count(values), 1i32)).why()}
  return(plus(count(head), count(tail)))
}

[return<int>]
forward_refs([args<Reference<FileError>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_refs_mixed([args<Reference<FileError>>] values) {
  [FileError] extra{15i32}
  [Reference<FileError>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
score_ptrs([args<Pointer<FileError>>] values) {
  [auto] head{at([values] values, [index] 0i32).why()}
  [auto] tail{at([values] values, [index] minus(count(values), 1i32)).why()}
  return(plus(count(head), count(tail)))
}

[return<int>]
forward_ptrs([args<Pointer<FileError>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_ptrs_mixed([args<Pointer<FileError>>] values) {
  [FileError] extra{16i32}
  [Pointer<FileError>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [FileError] a0{13i32}
  [FileError] a1{14i32}
  [Reference<FileError>] r0{location(a0)}
  [Reference<FileError>] r1{location(a1)}

  [FileError] b0{17i32}
  [FileError] b1{18i32}
  [Reference<FileError>] s0{location(b0)}
  [Reference<FileError>] s1{location(b1)}

  [FileError] c0{19i32}
  [Reference<FileError>] t0{location(c0)}

  [FileError] d0{20i32}
  [FileError] d1{21i32}
  [Pointer<FileError>] p0{location(d0)}
  [Pointer<FileError>] p1{location(d1)}

  [FileError] e0{22i32}
  [FileError] e1{23i32}
  [Pointer<FileError>] q0{location(e0)}
  [Pointer<FileError>] q1{location(e1)}

  [FileError] f0{24i32}
  [Pointer<FileError>] u0{location(f0)}

  return(plus(score_refs(r0, r1),
              plus(forward_refs(s0, s1),
                   plus(forward_refs_mixed(t0),
                        plus(score_ptrs(p0, p1),
                             plus(forward_ptrs(q0, q1),
                                  forward_ptrs_mixed(u0)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("variadic wrapped FileError packs still reject noncanonical named free builtin at labels") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<FileError>>] values) {
  [auto] head{at([value] values, [index] 0i32).why()}
  return(count(head))
}

[return<int>]
main() {
  [FileError] value{13i32}
  [Reference<FileError>] ref{location(value)}
  return(score_refs(ref))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("variadic wrapped File handle packs accept canonical named free builtin at receivers") {
  const std::string source = R"(
[effects(file_write)]
swallow_file_error([FileError] err) {}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
score_refs([args<Reference<File<Write>>>] values) {
  at([values] values, [index] 0i32).write_line("alpha"utf8)?
  at([values] values, [index] minus(count(values), 1i32)).write_line("omega"utf8)?
  at([values] values, [index] 0i32).flush()?
  at([values] values, [index] minus(count(values), 1i32)).flush()?
  return(plus(count(values), 10i32))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_refs([args<Reference<File<Write>>>] values) {
  return(score_refs([spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_refs_mixed([args<Reference<File<Write>>>] values) {
  [File<Write>] extra{File<Write>("extra_named_ref.txt"utf8)?}
  [Reference<File<Write>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
score_ptrs([args<Pointer<File<Write>>>] values) {
  at([values] values, [index] 0i32).write_line("alpha"utf8)?
  at([values] values, [index] minus(count(values), 1i32)).write_line("omega"utf8)?
  at([values] values, [index] 0i32).flush()?
  at([values] values, [index] minus(count(values), 1i32)).flush()?
  return(plus(count(values), 10i32))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_ptrs([args<Pointer<File<Write>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_ptrs_mixed([args<Pointer<File<Write>>>] values) {
  [File<Write>] extra{File<Write>("extra_named_ptr.txt"utf8)?}
  [Pointer<File<Write>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
main() {
  [File<Write>] a0{File<Write>("a0_named.txt"utf8)?}
  [File<Write>] a1{File<Write>("a1_named.txt"utf8)?}
  [Reference<File<Write>>] r0{location(a0)}
  [Reference<File<Write>>] r1{location(a1)}

  [File<Write>] b0{File<Write>("b0_named.txt"utf8)?}
  [File<Write>] b1{File<Write>("b1_named.txt"utf8)?}
  [Reference<File<Write>>] s0{location(b0)}
  [Reference<File<Write>>] s1{location(b1)}

  [File<Write>] c0{File<Write>("c0_named.txt"utf8)?}
  [Reference<File<Write>>] t0{location(c0)}

  [File<Write>] d0{File<Write>("d0_named.txt"utf8)?}
  [File<Write>] d1{File<Write>("d1_named.txt"utf8)?}
  [Pointer<File<Write>>] p0{location(d0)}
  [Pointer<File<Write>>] p1{location(d1)}

  [File<Write>] e0{File<Write>("e0_named.txt"utf8)?}
  [File<Write>] e1{File<Write>("e1_named.txt"utf8)?}
  [Pointer<File<Write>>] q0{location(e0)}
  [Pointer<File<Write>>] q1{location(e1)}

  [File<Write>] f0{File<Write>("f0_named.txt"utf8)?}
  [Pointer<File<Write>>] u0{location(f0)}

  return(plus(score_refs(r0, r1),
              plus(forward_refs(s0, s1),
                   plus(forward_refs_mixed(t0),
                        plus(score_ptrs(p0, p1),
                             plus(forward_ptrs(q0, q1),
                                  forward_ptrs_mixed(u0)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("variadic wrapped read File handle packs accept canonical named free builtin at read_byte try") {
  const std::string source = R"(
[effects(file_read)]
swallow_file_error([FileError] err) {}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
score_refs([args<Reference<File<Read>>>] values) {
  [i32 mut] first{0i32}
  [i32 mut] last{0i32}
  at([values] values, [index] 0i32).read_byte(first)?
  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?
  return(plus(plus(first, last), count(values)))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_refs([args<Reference<File<Read>>>] values) {
  return(score_refs([spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_refs_mixed([args<Reference<File<Read>>>] values) {
  [File<Read>] extra{File<Read>("extra_named_ref.txt"utf8)?}
  [Reference<File<Read>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
score_ptrs([args<Pointer<File<Read>>>] values) {
  [i32 mut] first{0i32}
  [i32 mut] last{0i32}
  at([values] values, [index] 0i32).read_byte(first)?
  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?
  return(plus(plus(first, last), count(values)))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_ptrs([args<Pointer<File<Read>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_ptrs_mixed([args<Pointer<File<Read>>>] values) {
  [File<Read>] extra{File<Read>("extra_named_ptr.txt"utf8)?}
  [Pointer<File<Read>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
main() {
  [File<Read>] a0{File<Read>("a0_named.txt"utf8)?}
  [File<Read>] a1{File<Read>("a1_named.txt"utf8)?}
  [Reference<File<Read>>] r0{location(a0)}
  [Reference<File<Read>>] r1{location(a1)}

  [File<Read>] b0{File<Read>("b0_named.txt"utf8)?}
  [File<Read>] b1{File<Read>("b1_named.txt"utf8)?}
  [Reference<File<Read>>] s0{location(b0)}
  [Reference<File<Read>>] s1{location(b1)}

  [File<Read>] c0{File<Read>("c0_named.txt"utf8)?}
  [Reference<File<Read>>] t0{location(c0)}

  [File<Read>] d0{File<Read>("d0_named.txt"utf8)?}
  [File<Read>] d1{File<Read>("d1_named.txt"utf8)?}
  [Pointer<File<Read>>] p0{location(d0)}
  [Pointer<File<Read>>] p1{location(d1)}

  [File<Read>] e0{File<Read>("e0_named.txt"utf8)?}
  [File<Read>] e1{File<Read>("e1_named.txt"utf8)?}
  [Pointer<File<Read>>] q0{location(e0)}
  [Pointer<File<Read>>] q1{location(e1)}

  [File<Read>] f0{File<Read>("f0_named.txt"utf8)?}
  [Pointer<File<Read>>] u0{location(f0)}

  return(plus(score_refs(r0, r1),
              plus(forward_refs(s0, s1),
                   plus(forward_refs_mixed(t0),
                        plus(score_ptrs(p0, p1),
                             plus(forward_ptrs(q0, q1),
                                  forward_ptrs_mixed(u0)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("canonical map helper bodies reject conflicting imported count alias") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  count([map<i32, i32>] values) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/size([map<i32, i32>] values) {
  return(count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/size(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: count") != std::string::npos);
}

TEST_CASE("canonical map helper bodies keep import conflict ahead of missing canonical count fallback") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  count([map<i32, i32>] values) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/size([map<i32, i32>] values) {
  return(count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/size(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: count") != std::string::npos);
}

TEST_CASE("canonical map helper bodies keep import conflict ahead of missing canonical access fallback") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  at([map<i32, i32>] values, [i32] key) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/first([map<i32, i32>] values) {
  return(at(values, 1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/first(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map at call requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("bare map at_unsafe auto inference requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("map namespaced count method resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count method resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count method supports auto inference") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values./std/collections/map/count()}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map count method requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map count method auto inference requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("canonical map count call does not inherit alias-only helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map contains method requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("canonical map contains call does not inherit alias-only helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  if(/std/collections/map/contains(values, 1i32),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("bare map tryAt method auto inference keeps deterministic unknown method diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.tryAt(1i32)}
  return(try(inferred))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map access methods require imported canonical helpers or explicit definitions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("bare map access method auto inference keeps deterministic unknown call target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.at_unsafe(1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("map namespaced at method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/at(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced at_unsafe method auto inference keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values./map/at_unsafe(1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map namespaced contains method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/contains(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/contains") != std::string::npos);
}

TEST_CASE("map stdlib namespaced contains method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/contains(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/contains") != std::string::npos);
}

TEST_CASE("map namespaced tryAt method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[return<Result<int, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(io_out)]
panic([ContainerError] err) {}

[return<int> effects(heap_alloc, io_out) on_error<ContainerError, /panic>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(values./map/tryAt(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/tryAt") != std::string::npos);
}

TEST_CASE("map stdlib namespaced tryAt method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[return<Result<int, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(io_out)]
panic([ContainerError] err) {}

[return<int> effects(heap_alloc, io_out) on_error<ContainerError, /panic>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(values./std/collections/map/tryAt(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/tryAt") != std::string::npos);
}

TEST_CASE("map stdlib namespaced at_unsafe method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/at_unsafe(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("stdlib canonical map contains and tryAt helpers resolve in method-call sugar") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Result<i32, ContainerError>] found{values.tryAt(1i32)}
  return(values.contains(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map helpers resolve in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count(true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map insert resolves in method-call sugar") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32> mut] values{map<i32, i32>()}
  values.insert(1i32, 4i32)
  values.insert(2i32, 7i32)
  values.insert(1i32, 9i32)
  return(plus(values.count(), plus(values.at(1i32), values.at_unsafe(2i32))))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK_MESSAGE(error.empty(), error);
}

TEST_CASE("stdlib canonical map insert resolves in direct-call form") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32> mut] values{map<i32, i32>()}
  /std/collections/map/insert<i32, i32>(values, 1i32, 4i32)
  /std/collections/map/insert<i32, i32>(values, 2i32, 7i32)
  /std/collections/map/insert<i32, i32>(values, 1i32, 9i32)
  return(plus(values.count(), plus(values.at(1i32), values.at_unsafe(2i32))))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK_MESSAGE(error.empty(), error);
}

TEST_CASE("stdlib canonical map insert resolves on non-local field and borrowed receivers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Holder() {
  [map<i32, i32> mut] values{map<i32, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  /std/collections/map/insert<i32, i32>(holder.values, 1i32, 4i32)
  holder.values.insert(2i32, 7i32)
  [Reference<map<i32, i32>> mut] ref{location(holder.values)}
  /std/collections/map/insert_ref<i32, i32>(ref, 3i32, 11i32)
  ref.insert(2i32, 13i32)
  return(plus(holder.values.count(), plus(holder.values.at(1i32), plus(holder.values.at_unsafe(2i32), holder.values.at(3i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map insert resolves on nested non-local and helper-return borrowed receivers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Holder() {
  [map<i32, i32> mut] values{map<i32, i32>()}
}

[struct]
Outer() {
  [Holder mut] holder{Holder()}
}

[return<Reference<map<i32, i32>>>]
borrowValues([Reference<map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Outer mut] outer{Outer()}
  /std/collections/map/insert<i32, i32>(outer.holder.values, 1i32, 4i32)
  /std/collections/map/insert<i32, i32>(outer.holder.values, 2i32, 7i32)
  [Reference<map<i32, i32>> mut] ref{borrowValues(location(outer.holder.values))}
  /std/collections/map/insert_ref<i32, i32>(ref, 3i32, 11i32)
  ref.insert(2i32, 13i32)
  return(plus(outer.holder.values.count(), plus(outer.holder.values.at(1i32), plus(outer.holder.values.at_unsafe(2i32), outer.holder.values.at(3i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map insert resolves on helper-return borrowed method receivers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Holder() {
  [map<i32, i32> mut] values{map<i32, i32>()}
}

[return<Reference<map<i32, i32>>>]
borrowValues([Reference<map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  [Reference<map<i32, i32>> mut] ref{borrowValues(location(holder.values))}
  ref.insert(1i32, 4i32)
  ref.insert(2i32, 7i32)
  ref.insert(1i32, 9i32)
  return(plus(holder.values.count(), plus(holder.values.at(1i32), holder.values.at_unsafe(2i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map helper resolves method-call sugar for slash return type") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
