TEST_SUITE_BEGIN("primestruct.semantics.maybe");

namespace {
const std::string kMaybePrelude = R"(
[public struct]
Maybe<T>() {
  [public bool] empty{true}
  [public uninitialized<T>] value{uninitialized<T>()}

  [public]
  Create() {
  }

  [public]
  Destroy() {
    if(not(this.empty), then() { drop(this.value) }, else() { })
  }
}

[public return<Maybe<T>>]
some<T>([T] value) {
  [Maybe<T> mut] out{Maybe<T>()}
  [Reference<Maybe<T>> mut] ref{location(out)}
  init(ref.value, value)
  assign(ref.empty, false)
  return(out)
}

[public return<Maybe<T>>]
none<T>() {
  return(Maybe<T>())
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
  [Maybe<Widget>] value{some<Widget>(Widget())}
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

TEST_SUITE_END();
