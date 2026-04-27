#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.maybe");

namespace {
const std::string kMaybePrelude = R"(
[public struct]
Maybe<T>() {
  [public bool mut] empty{true}
  [public uninitialized<T> mut] value{uninitialized<T>()}

  [public]
  Create() {
  }

[public]
Destroy() {
  if(not(this.empty), then() { drop(this.value) }, else() { })
}

[public return<bool>]
isEmpty() {
  return(this.empty)
}

[public return<bool>]
isSome() {
  return(not(this.empty))
}

[public return<bool>]
is_empty() {
  return(this.isEmpty())
}

[public return<bool>]
is_some() {
  return(this.isSome())
}

[public mut return<void>]
clear() {
  if(not(this.empty), then() { drop(this.value) }, else() { })
  assign(this.empty, true)
}

[public mut return<void>]
set([T] v) {
  if(not(this.empty), then() { drop(this.value) }, else() { })
  init(this.value, v)
  assign(this.empty, false)
}

[public mut return<T>]
take() {
  [T] out{take(this.value)}
  assign(this.empty, true)
  return(out)
}
}

[public return<Maybe<T>>]
some<T>([T] value) {
  [Maybe<T> mut] out{Maybe<T>{}}
  [Reference<Maybe<T>> mut] ref{location(out)}
  init(ref.value, value)
  assign(ref.empty, false)
  return(out)
}

[public return<Maybe<T>>]
none<T>() {
  return(Maybe<T>{})
}
)";
} // namespace

TEST_CASE("maybe some constructs present value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(1i32)}
  if(value.empty) { return(0i32) } else { return(1i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe none constructs empty value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  if(value.empty) { return(1i32) } else { return(0i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe some supports struct values") {
  const std::string source = kMaybePrelude + R"(
[struct]
Widget() {
  [i32] id{7i32}
}

[return<int>]
main() {
  [Maybe<Widget>] value{some<Widget>(Widget{})}
  if(value.empty) { return(0i32) } else { return(1i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe some returns from helper") {
  const std::string source = kMaybePrelude + R"(
[return<Maybe<i32>>]
make() {
  return(some<i32>(5i32))
}

[return<int>]
main() {
  [Maybe<i32>] value{make()}
  if(value.empty) { return(0i32) } else { return(1i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe none used in branches") {
  const std::string source = kMaybePrelude + R"(
[return<Maybe<i32>>]
choose([bool] flag) {
  if(flag) {
    return(some<i32>(2i32))
  } else {
    return(none<i32>())
  }
}

[return<int>]
main() {
  [Maybe<i32>] value{choose(false)}
  if(value.empty) { return(1i32) } else { return(0i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe helpers report empty and some") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  if(value.is_some()) {
    return(0i32)
  }
  if(value.is_empty()) {
    return(1i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe camelCase helpers report empty and some") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  if(value.isSome()) {
    return(0i32)
  }
  if(value.isEmpty()) {
    return(1i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe set and clear mutate value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  value.set(9i32)
  if(value.is_empty()) {
    return(0i32)
  }
  value.clear()
  if(value.is_empty()) {
    return(1i32)
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe take returns stored value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(7i32)}
  [i32] out{value.take()}
  return(out)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe set overwrites existing value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(1i32)}
  value.set(2i32)
  if(value.is_some()) { return(1i32) } else { return(0i32) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe direct constructor rejects payload shorthand without some") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{Maybe{1i32}}
  if(value.empty) { return(0i32) } else { return(1i32) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /Maybe") != std::string::npos);
}

TEST_CASE("maybe direct constructor rejects payload shorthand with explicit template") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{Maybe<i32>{1i32}}
  if(value.empty) { return(0i32) } else { return(1i32) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /Maybe__t") != std::string::npos);
  CHECK(error.find("parameter empty") != std::string::npos);
  CHECK(error.find("expected bool got i32") != std::string::npos);
}

TEST_SUITE_END();
