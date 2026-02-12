#pragma once

TEST_CASE("import brings immediate children into root") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves struct types and constructors") {
  const std::string source = R"(
import /util
namespace util {
  [struct]
  Widget() {
    [i32] value{1i32}
  }
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import resolves methods on struct types") {
  const std::string source = R"(
import /util
[struct]
/util/Widget() {
  [i32] value{1i32}
}
[return<int>]
/util/Widget/get([Widget] self, [i32] value) {
  return(value)
}
[return<int>]
main() {
  [Widget] item{Widget()}
  return(item.get(5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import does not alias nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(inner())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: inner") != std::string::npos);
}

TEST_CASE("import does not alias namespace blocks") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(nested())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: nested") != std::string::npos);
}

TEST_CASE("import rejects namespace-only path") {
  const std::string source = R"(
import /util
namespace util {
  namespace nested {
    [return<int>]
    inner() {
      return(1i32)
    }
  }
}
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util") != std::string::npos);
}

TEST_CASE("import accepts whitespace-separated paths") {
  const std::string source = R"(
import /util /math/*
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts wildcard math and util paths") {
  const std::string source = R"(
import /math/* /util/*
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<f64>]
main() {
  [i32] value{inc(1i32)}
  return(sin(0.0f64))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple explicit math paths") {
  const std::string source = R"(
import /math/sin /math/pi
[return<f64>]
main() {
  return(plus(pi, sin(0.0f64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects unknown wildcard path") {
  const std::string source = R"(
import /missing/*
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing/*") != std::string::npos);
}

TEST_CASE("import rejects unknown single-segment path") {
  const std::string source = R"(
import /missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /missing") != std::string::npos);
}

TEST_CASE("import rejects missing definition") {
  const std::string source = R"(
import /util/missing
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /util/missing") != std::string::npos);
}

TEST_CASE("import conflicts with existing root definitions") {
  const std::string source = R"(
import /util
[return<int>]
dup() {
  return(1i32)
}
namespace util {
  [return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import conflicts between namespaces") {
  const std::string source = R"(
import /util, /tools
namespace util {
  [return<int>]
  dup() {
    return(1i32)
  }
}
namespace tools {
  [return<int>]
  dup() {
    return(2i32)
  }
}
[return<int>]
main() {
  return(dup())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: dup") != std::string::npos);
}

TEST_CASE("import resolves execution targets") {
  const std::string source = R"(
import /util
namespace util {
  [return<void>]
  run([i32] count) {
    return()
  }
}
[return<int>]
main() {
  return(1i32)
}
run(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import accepts multiple paths in one statement") {
  const std::string source = R"(
import /util, /math/*
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(1i32), 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("duplicate imports are ignored") {
  const std::string source = R"(
import /util, /util
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import works after definitions") {
  const std::string source = R"(
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(2i32))
}
import /util
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import ignores nested definitions") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  first() {
    return(1i32)
  }
  namespace nested {
    [return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(first())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import rejects nested definitions in root") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  immediate() {
    return(0i32)
  }
  namespace nested {
    [return<int>]
    second() {
      return(2i32)
    }
  }
}
[return<int>]
main() {
  return(second())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: second") != std::string::npos);
}
