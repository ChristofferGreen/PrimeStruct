#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.maybe");

namespace {
const std::string kMaybePrelude = R"(
[public sum]
Maybe<T> {
  none
  [T] some
}

[public return<Maybe<T>>]
some<T>([T] value) {
  return(Maybe<T>{[some] value})
}

[public return<Maybe<T>>]
none<T>() {
  return(Maybe<T>{none})
}

[public return<bool>]
/Maybe/isEmpty<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(true)
    }
    some(value) {
      return(false)
    }
  })
}

[public return<bool>]
/Maybe/isSome<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(false)
    }
    some(value) {
      return(true)
    }
  })
}

[public return<bool>]
/Maybe/is_empty<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(true)
    }
    some(value) {
      return(false)
    }
  })
}

[public return<bool>]
/Maybe/is_some<T>([Maybe<T>] self) {
  return(pick(self) {
    none {
      return(false)
    }
    some(value) {
      return(true)
    }
  })
}
)";
} // namespace

TEST_CASE("maybe some constructs present sum value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{some<i32>(1i32)}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe none constructs default sum value") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{none<i32>()}
  return(pick(value) {
    none {
      return(1i32)
    }
    some(v) {
      return(0i32)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe default and explicit none are equivalent") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] implicit{}
  [Maybe<i32>] explicit{none}
  [i32] left{pick(implicit) {
    none {
      1i32
    }
    some(v) {
      0i32
    }
  }}
  [i32] right{pick(explicit) {
    none {
      1i32
    }
    some(v) {
      0i32
    }
  }}
  return(left + right)
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
  return(pick(value) {
    none {
      return(0i32)
    }
    some(widget) {
      return(widget.id)
    }
  })
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
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
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
  return(pick(value) {
    none {
      return(1i32)
    }
    some(v) {
      return(0i32)
    }
  })
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

TEST_CASE("maybe direct constructor accepts explicit present variant") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{Maybe<i32>{[some] 1i32}}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe direct constructor accepts unique inferred payload") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32>] value{Maybe<i32>{1i32}}
  return(pick(value) {
    none {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("maybe mutable struct helpers are retired on sum values") {
  const std::string source = kMaybePrelude + R"(
[return<int>]
main() {
  [Maybe<i32> mut] value{none<i32>()}
  value.set(9i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/Maybe__t") != std::string::npos);
  CHECK(error.find("/set") != std::string::npos);
}

TEST_SUITE_END();
